#include <assert.h>
#include <iostream>
#include <fstream>
#include <list>
#include <memory>

#include <ncurses.h>

#define CTRL_X 24

using namespace std;

typedef list<list<char>>::iterator MULTILINE_ITER;

list<list<char>> input;
WINDOW *win;
int x, y, max_x, max_y;

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

void init_tui() {
  initscr();
  raw();
  noecho();

  keypad(stdscr, TRUE); 

  win = newwin(0, 0, 0, 0);
  getmaxyx(win, max_y, max_x);
  wrefresh(win);
}

void populate_input(fstream &file) {
  input.push_back(list<char>());

  auto it = input.begin();
  char c;
  int num_lines = 1;
  // TODO: Remove max_y limit
  while (file.get(c) && num_lines < max_y + 1) {
    if ('\n' == c) {
      ++num_lines;

      input.push_back(list<char>());
      ++it;
    } else {
      it->push_back(c);
    }
  }

  assert(it->empty());
  // Get rid of final newline
  input.erase(it);
}

MULTILINE_ITER print_text() {
  erase();
  move(0, 0);
  refresh();

  auto it = input.begin();
  for (; it != input.end(); ++it) {
    for (char c : *it) {
      addch(c);
    }

    if (it != std::prev(input.end())) {
      addch('\n');
    }
  }

  refresh();

  return it;
}

void do_tui() {
  auto it = print_text();

  // Get cursor pos from the main window
  getyx(stdscr, y, x);

  // Assume the cursor is at the last char in the file 
  assert(input.end() == it);
  
  // Now place the iterators at the last char of the file
  --it;
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
      if (y > 0) {
        --y;
        --it;
        if (0 == x) {
          line_it = it->begin();
        } else if (x < (int) it->size()) {
          // x within line size, don't change it
          line_it = it->begin();
          std::advance(line_it, x);
        } else {
          line_it = it->end();
          --line_it;
          if (it->empty()) {
            x = 0;
          } else {
            x = (int) it->size() - 1;
          }
        }
      }
    } else if (KEY_DOWN == ch) {
      if (y < (int) input.size() - 1 && y < max_y) {
        ++y;
        ++it;
        if (0 == x) {
          line_it = it->begin();
        } else if (x < (int) it->size()) {
          // x within line size, don't change it
          line_it = it->begin();
          std::advance(line_it, x);
        } else {
          line_it = it->end();
          --line_it;
          if (it->empty()) {
            x = 0;
          } else {
            x = (int) it->size() - 1;
          }
        }
      }
    } else if (KEY_BACKSPACE == ch) {
      if (0 == x) {
        // Can't delete before start
        if (0 == y) continue;

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
        --y;

        // Place iterator one char after the previous line's last char
        line_it = std::next(previous_line_last_char_it);
      } else {
        // Erase the previous character
        --line_it;
        line_it = it->erase(line_it);

        --x;
      }

      print_text();
    } else if ('\n' == ch) {
      // TODO: Remove temporary limit
      if (max_x + 1 == (int) input.size()) continue;

      auto prev_it = it;
      auto prev_line_end = prev_it->end();

      // A newline is expected to appear under the current line
      // Insert prepends, so insert from the next line
      ++it;
      it = input.insert(it, list<char>());

      // Move characters from cursor to the end of the line to the next line
      it->splice(it->begin(), *prev_it,  line_it, prev_line_end);
      ++y;

      // New line => cursor goes to start of line
      x = 0;
      line_it = it->begin();

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
