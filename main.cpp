#include <assert.h>
#include <iostream>
#include <iterator>
#include <fstream>
#include <list>
#include <memory>

#include <ncurses.h>

#define CTRL_X 24

using namespace std;

inline unsigned ceiling_divide(unsigned a, unsigned b) {
  return (a + b - 1) / b;
}

// An iterator to traverse the input lines
typedef list<list<char>>::iterator MULTILINE_ITER;

// A list of lines in the input
list<list<char>> input;
WINDOW *win;
// max_x is the width size, so x only goes up to max_x - 1, and similarly for max_y
unsigned x, y, max_x, max_y;

// The line number of the line displayed at the top of the screen
unsigned start_line = 0;
unsigned start_sub_line = 0;
MULTILINE_ITER start_it;

unsigned current_line_sub_line = 0;

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

/*
 * Initializes NCurses and the TUI window, gets the max x and y
 */
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

  // Return early if there's nothing to print in the line and there's no more
  // lines to print
  if (0 == it->size() || input.end() == it) return it;

  std::advance(it, start_line);

  // Handle first line separately to account for sub-line start
  auto line_it = it->begin();

  unsigned curr_num_lines = 1;
  unsigned curr_num_chars_in_sub_line = 0;
  std::advance(line_it, start_sub_line * max_x);

  // Iterating over chars, so allow filling the last line of the screen
  for (; line_it != it->end() && curr_num_lines <= max_y; ++line_it) {
    // Handles having more characters than can fit in the current line
    if (max_x == curr_num_chars_in_sub_line) {
      // Don't print past screen
      if (max_y == curr_num_lines) break;

      // Reached the width of the screen, printing on a new line
      ++curr_num_lines;
      curr_num_chars_in_sub_line = 0;
    }

    addch(*line_it);
    ++curr_num_chars_in_sub_line;
  }

  ++it;

  // Iterating over lines, so if we're at max_y, then no more lines can be displayed
  for (; it != input.end() && curr_num_lines < max_y; ++it) {
    // Reset subline since we moved to a new line
    curr_num_chars_in_sub_line = 0;

    addch('\n');
    ++curr_num_lines;

    for (char c : *it) {
      if (max_x == curr_num_chars_in_sub_line) {
        // Don't print past screen
        if (max_y == curr_num_lines) break;

        // Reached the width of the screen, printing on a new line
        ++curr_num_lines;
        curr_num_chars_in_sub_line = 0;
      }
      addch(c);
      ++curr_num_chars_in_sub_line;
    }
  }

  refresh();

  // For consistency, have print_text return an iterator to the last line
  if (input.end() == it || max_y == curr_num_lines) {
    it = std::prev(it);
  }

  return it;
}

void do_tui() {
  auto it = print_text();

  // Get cursor pos from the main window
  getyx(stdscr, y, x);

  // Now place the line iterator at the last char of the file
  auto line_it = it->end();

  // The iterator is at the last char of the line
  // There's it->size() chars in the line, and max_x chars per lines
  current_line_sub_line = (it->size()) / max_x;

  start_it = input.begin();
  std::advance(start_it, start_line);
  
  std::locale loc;
  wchar_t ch;
  while ((ch = getch()) != CTRL_X) {
    // Using if-else's to catch printable characters
    if (KEY_LEFT == ch) {
      if (x > 0) {
        --x;
        --line_it;
      } else if (current_line_sub_line > 0) {
        --line_it;
        --current_line_sub_line;

        x = max_x - 1;

        if (y > 0) {
          --y;
        } else {
          --start_sub_line;
        }
        
      }
    } else if (KEY_RIGHT == ch) {
      if (x + max_x * current_line_sub_line >= it->size()) continue;

      ++line_it;
      if (max_x - 1 == x) {
        x = 0;

        // Handle empty line separately, else current_line_sub_line would be -1
        unsigned curr_line_num_sub_lines = 1;
        if (it->size() > 0) {
          curr_line_num_sub_lines = ceiling_divide(it->size(), max_x);
        }

        if (current_line_sub_line + 1 < curr_line_num_sub_lines) {
          ++current_line_sub_line;
        }

        if (max_y - 1 == y) {
          // Going past the bottom of the screen, scroll down
          unsigned start_num_sub_lines = ceiling_divide(start_it->size(), max_x);

          // Check whether the next second line from the top is a sub-line of the
          // current start line, or if it's a different line
          if (start_num_sub_lines - 1 == start_sub_line) {
            ++start_line;
            ++start_it;
          } else {
            ++start_sub_line;
          }

          print_text();
        } else {
          ++y;
        }
      } else {
        ++x;
      }
    } else if (KEY_UP == ch) {
      // Continue if there is no line or subline before current line
      if (0 == y && 0 == start_line && 0 == current_line_sub_line) continue;

      if (0 == current_line_sub_line) {
        // Line above is a different line
        --it;

        // Handle empty line separately, else current_line_sub_line would be -1
        unsigned curr_line_num_sub_lines = 1;
        if (it->size() > 0) {
          curr_line_num_sub_lines = ceiling_divide(it->size(), max_x);
        }

        current_line_sub_line = curr_line_num_sub_lines - 1;

        // Calculate the length of the last sub_line
        unsigned last_sub_line_length = it->size() % max_x;
        if (x <= last_sub_line_length) {
          // x within line size, don't change it
          line_it = it->begin();
          std::advance(line_it, x + max_x * current_line_sub_line);

        } else {
          line_it = it->end();

          if (it->empty()) {
            x = 0;
          } else {
            x = last_sub_line_length;
          }
        }
      } else {
        // Line above is a subline from the same line
        // Move backwards by the width of the screen
        unsigned new_it_pos = (current_line_sub_line - 1) * max_x + x;
        line_it = it->begin();
        std::advance(line_it, new_it_pos);

        --current_line_sub_line;
      }

      if (0 == y) {
        // Going past the top of the screen, scroll up
        if (start_sub_line > 0) {
          --start_sub_line;
        } else {
          --start_line;
          --start_it;
        }

        print_text();
      } else {
        --y;
      }
    } else if (KEY_DOWN == ch) {
      // Handle empty line separately, else current_line_sub_line would be -1
      unsigned curr_line_num_sub_lines = 1;
      if (it->size() > 0) {
        curr_line_num_sub_lines = ceiling_divide(it->size(), max_x);
      }

      // Don't scroll past end of file
      if (input.end() == std::next(it) &&
        curr_line_num_sub_lines - 1 == current_line_sub_line)
      {
        continue;
      }


      if (current_line_sub_line + 1 < curr_line_num_sub_lines) {
        // The current line has another sub-line, go down to it
        ++current_line_sub_line;

        // Check that if you move the cursor one line down, the cursor still
        // points to at character in the line
        if (max_x * (current_line_sub_line) + x <= it->size()) {
          // Move by as many characters as the width of a line to end up just
          // under the cursor's previous position
          std::advance(line_it, max_x);
        } else {
          // The last character in the line is less than max_x characters away,
          // place the cursor after the last character
          unsigned distance_to_end = std::distance(line_it, it->end());
          // x -= distance_to_end - max_x;
          std::cerr << distance_to_end << std::endl;
          x = x - (max_x - distance_to_end);
          line_it = it->end();
        }
      } else {
        ++it;

        // Moving down to a new line, so we're at the first sub-line
        current_line_sub_line = 0;

        if (0 == x) {
          line_it = it->begin();
        } else if (x <= it->size()) {
          // x within line size, don't change it
          line_it = it->begin();
          std::advance(line_it, x);
        } else {
          // The new line is shorter than the x-position of the cursor
          line_it = it->end();

          if (it->empty()) {
            x = 0;
          } else {
            x = it->size();
          }
        }
      }

      if (max_y - 1 == y) {
        // Going past the bottom of the screen, scroll down
        unsigned start_num_sub_lines = ceiling_divide(start_it->size(), max_x);

        // Check whether the next second line from the top is a sub-line of the
        // current start line, or if it's a different line
        if (start_num_sub_lines - 1 == start_sub_line) {
          ++start_line;
          ++start_it;
        } else {
          ++start_sub_line;
        }

        print_text();
      } else {
        ++y;
      }
    } else if (KEY_BACKSPACE == ch) {
      if (0 == x) {
        // Can't delete before start
        if (0 == y && 0 == start_line && 0 == current_line_sub_line) continue;

        if (0 == current_line_sub_line) {
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
        } else {
          // Erase the previous character
          --line_it;
          line_it = it->erase(line_it);

          --current_line_sub_line;

          x = max_x - 1;
        }

        if (0 == y) {
          // Going past the top of the screen, scroll up
          if (start_sub_line > 0) {
            --start_sub_line;
          } else {
            --start_line;
            --start_it;
          }

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
      // Moving down to a new line, so we're at the first sub-line
      current_line_sub_line = 0;

      auto prev_it = it;
      auto prev_line_end = prev_it->end();

      // A newline is expected to appear under the current line
      // Insert prepends, so insert from the next line
      ++it;
      it = input.insert(it, list<char>());

      // Only if there were characters after the cursor
      if (prev_line_end != line_it) {
        // Move characters from cursor to the end of the line to the next line
        it->splice(it->begin(), *prev_it,  line_it, prev_line_end);
        std::cerr << "haaa" << endl;
      }

      // New line => cursor goes to start of line
      x = 0;
      line_it = it->begin();

      if (max_y - 1 == y) {
        // Going past the bottom of the screen, scroll down
        unsigned start_num_sub_lines = ceiling_divide(start_it->size(), max_x);

        // Check whether the next second line from the top is a sub-line of the
        // current start line, or if it's a different line
        if (start_num_sub_lines - 1 == start_sub_line) {
          ++start_line;
          ++start_it;
        } else {
          ++start_sub_line;
        }

        print_text();
      } else {
        ++y;
      }

      print_text();
    } else if (std::isprint(ch, loc)) {
      it->insert(line_it, ch);
      if (max_x - 1 == x) {
        x = 0;

        ++current_line_sub_line;

        if (max_y - 1 == y) {
          // Going past the bottom of the screen, scroll down
          unsigned start_num_sub_lines = ceiling_divide(start_it->size(), max_x);

          // Check whether the next second line from the top is a sub-line of the
          // current start line, or if it's a different line
          if (start_num_sub_lines - 1 == start_sub_line) {
            ++start_line;
            ++start_it;
          } else {
            ++start_sub_line;
          }

          print_text();
        } else {
          ++y;
        }
      } else {
        ++x;
      }
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
