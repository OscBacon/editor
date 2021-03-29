#include <assert.h>
#include <iostream>
#include <fstream>
#include <list>
#include <memory>

#include <ncurses.h>

#define CTRL_X 24

using namespace std;

// An iterator to traverse the input lines
typedef list<list<char>>::iterator MULTILINE_ITER;

// A list of lines in the input
list<list<char>> input;
WINDOW *win;
int x, y, max_x, max_y;

// The line number of the line displayed at the top of the screen
int start_line = 0;

void traverse_list(ostream &out = cout) {
  for (auto const &line : input) {
    for (char c : line) {
      out << c;
    }
    out << endl;
  }
}

void write_output(char *filename) {
  ofstream out;
  out.open(filename);

  traverse_list(out);

  out.close();
}

// Initializes NCurses and the TUI window, gets the max x and y
void init_tui() {
  initscr();
  raw();
  noecho();

  keypad(stdscr, TRUE);

  win = newwin(0, 0, 0, 0);
  getmaxyx(win, max_y, max_x);
  wrefresh(win);
}

/*
 * Populates input list with a list of chars for each line in the file.
 * If the file is empty, adds an empty line.
 *
 */
void populate_input(fstream &file) {
  input.push_back(list<char>());

  auto it = input.begin();
  char c;
  while (file.get(c)) {
    if ('\n' == c) {
      input.push_back(list<char>());
      ++it;
    } else {
      it->push_back(c);
    }
  }

  assert(it->empty());

  // Get rid of final newline if the file is not empty
  if (1 != input.size()) {
    input.erase(it);
  }
}

MULTILINE_ITER print_text() {
  erase();
  move(0, 0);
  refresh();

  auto it = input.begin();
  std::advance(it, start_line);

  // Only print lines up to the bottom of the window, or the number of lines
  // left
  int num_lines = input.size();
  int max_lines = min(num_lines - start_line, max_y);

  int curr_num_lines = 0;
  for (; it != input.end() && curr_num_lines < max_lines; ++it) {
    ++curr_num_lines;

    for (char c : *it) {
      addch(c);
    }

    if (max_lines == curr_num_lines) break;

    addch('\n');
  }

  refresh();

  // For consistency, have print_text return an iterator to the last line
  if (it == input.end())
    it = std::prev(it);

  return it;
}

void do_tui() {
  auto it = print_text();

  // Get cursor pos from the main window
  getyx(stdscr, y, x);

  // Now place the line iterator at the last char of the file
  auto line_it = it->end();
  
  std::locale loc;
  wchar_t ch;
  while ((ch = getch()) != CTRL_X) {
    // Using if-else's to catch printable characters
    if (KEY_LEFT == ch) {
      if (x > 0) {
        --x;
        --line_it;
      }
    } else if (KEY_RIGHT == ch) {
      // TODO: Don't assume lines don't overflow
      if (x < (int) it->size() && x < max_x) {
        ++x;
        ++line_it;
      }
    } else if (KEY_UP == ch) {
      // If at top of file, do nothing
      if (0 == y && 0 == start_line) continue;

      --it;
      if (0 == x) {
        line_it = it->begin();
      } else if (x <= (int) it->size()) {
        // x within line size, don't change it
        line_it = it->begin();
        std::advance(line_it, x);
      } else {
        line_it = it->end();

        if (it->empty()) {
          x = 0;
        } else {
          x = (int) it->size();
        }
      }

      if (0 == y) {
        // Going past the top of the screen, scroll up
        --start_line;
        print_text();
      } else {
        --y;
      }
    } else if (KEY_DOWN == ch) {
      // Don't scroll past end of file
      if (y == (int) input.size() - 1) continue;

      ++it;
      if (0 == x) {
        line_it = it->begin();
      } else if (x <= (int) it->size()) {
        // x within line size, don't change it
        line_it = it->begin();
        std::advance(line_it, x);
      } else {
        line_it = it->end();

        if (it->empty()) {
          x = 0;
        } else {
          x = (int) it->size();
        }
      }

      if (y == max_y - 1) {
        // Going past the bottom of the screen, scroll down
        ++start_line;
        print_text();
      } else {
        ++y;
      }
    } else if (KEY_BACKSPACE == ch) {
      if (0 == x) {
        // Can't delete before start
        if (0 == start_line) continue;

        // Merge current line with previous
        auto previous_it = std::prev(it);

        // Get current # of chars in previous line.
        // After merging lines, the cursor will be one char right of that
        x = previous_it->size();
        auto previous_line_last_char_it = std::prev(previous_it->end());

        // Append current line to previous line
        previous_it->splice(previous_it->end(), *it);

        // Get rid of current line since it was merged
        input.erase(it);
        it = previous_it;

        // Place iterator one char after the previous line's last char
        line_it = std::next(previous_line_last_char_it);

        if (0 == y) {
          // Going past the top of the screen, scroll up
          --start_line;
        } else {
          --y;
        }
      } else {
        // Erase the previous character
        --line_it;
        line_it = it->erase(line_it);

        --x;
      }

      print_text();
    } else if ('\n' == ch) {
      auto prev_it = it;
      auto prev_line_end = prev_it->end();

      // A newline is expected to appear under the current line
      // Insert prepends, so insert from the next line
      ++it;
      it = input.insert(it, list<char>());

      // Move characters from cursor to the end of the line to the next line
      it->splice(it->begin(), *prev_it,  line_it, prev_line_end);

      // New line => cursor goes to start of line
      x = 0;
      line_it = it->begin();

      if (y == max_y - 1) {
        // Going past the bottom of the screen, scroll down
        ++start_line;
      } else {
        ++y;
      }

      print_text();
    } else if (std::isprint(ch, loc)) {
      // TODO: let lines overflow
      if (max_x == (int) it->size()) continue;

      it->insert(line_it, ch);
      ++x;

      print_text();
    } else {
      continue;
    }

    wmove(win, y, x);
    wrefresh(win);
  }

  endwin();
}

int main(int argc, char **argv) {
  if (!(argc == 2 || argc == 3)) {
    cout << "Usage: " << argv[0] << " <filename> <output>" << endl;
    exit(1);
  }


  init_tui();

  fstream file;
  file.open(argv[1]);
  populate_input(file);
  file.close();


  do_tui();

  if (3 == argc) {
    write_output(argv[2]);
  } else {
    traverse_list();
  }

  return 0;
}
