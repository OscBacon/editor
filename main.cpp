#include <assert.h>
#include <iostream>
#include <fstream>
#include <list>
#include <memory>

#include <ncurses.h>

#define CTRL_X 24

using namespace std;

list<unique_ptr<list<char>>> input;
WINDOW *win;
int x, y, max_x, max_y;

void traverse_list() {
  for (auto const &line : input) {
    for (char c : *line) {
      cout << c;
    }
    cout << endl;
  }
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

void do_tui() {
  auto it = input.begin();
  for (; it != input.end(); ++it) {
    for (auto line_it = (*it)->begin(); line_it != (*it)->end(); ++line_it) {
      addch(*line_it);
    }

    if (*it != input.back()) {
      addch('\n');
    }
  }

  // Get cursor pos from the main window
  getyx(stdscr, y, x);

  // Assume the cursor is at the last char in the file 
  assert(input.end() == it);
  
  // Now place the iterators at the last char of the file
  --it;
  auto line_it = (*it)->end();
  --line_it; // Gets to last char
  
  std::locale loc;
  wchar_t ch;
  while ((ch = getch()) != CTRL_X) {
    switch (ch) {
      case KEY_LEFT:
        if (x > 0) {
          --x;
          --line_it;
        }
        break;
      case KEY_RIGHT:
        // TODO: Don't assume lines don't overflow
        if (x < (int) (*it)->size() - 1 && x < max_x) {
          ++x;
          ++line_it;
        }
        break;
      case KEY_UP:
        if (y > 0) {
          --y;
          --it;
          if (x == 0) {
            line_it = (*it)->begin();
          } else if (x < (int) (*it)->size()) {
            // x within line size, don't change it
            line_it = (*it)->begin();
            std::advance(line_it, x - 1);
          } else {
            line_it = (*it)->end();
            --line_it;
            if ((*it)->empty()) {
              x = 0;
            } else {
              x = (int) (*it)->size() - 1;
            }
          }
        }
        break;
      case KEY_DOWN:
        if (y < (int) input.size() - 1 && y < max_y) {
          ++y;
          ++it;
          if (x == 0) {
            line_it = (*it)->begin();
          } else if (x < (int) (*it)->size()) {
            // x within line size, don't change it
            line_it = (*it)->begin();
            std::advance(line_it, x - 1);
          } else {
            line_it = (*it)->end();
            --line_it;
            if ((*it)->empty()) {
              x = 0;
            } else {
              x = (int) (*it)->size() - 1;
            }
          }
        }
        break;
      default:
        // Check if alpha numeric character was pressed
        if (!std::isalpha(ch, loc)) {
          break;
        }
        
        (*it)->insert(line_it, ch);
        break;
    }

    wmove(win, y, x);
    wrefresh(win);
  }

  endwin();
}

int main(int argc, char **argv) {
  if (argc != 2) {
    cout << "Usage: " << argv[0] << " <filename>" << endl;
    exit(1);
  }

  fstream file;
  file.open(argv[1]);

  init_tui();

  input.push_back(unique_ptr<list<char>>(new list<char>));

  auto it = input.begin();
  char c;
  int num_lines = 1;
  // TODO: Remove max_y limit
  while (file.get(c) && num_lines < max_y) {
    // TODO: Remove max_y limit
    if (c == '\n') {
      ++num_lines;

      if (num_lines == max_y) break;

      input.push_back(unique_ptr<list<char>>(new list<char>));
      ++it;
    } else {
      (*it)->push_back(c);
    }
  }

  do_tui();

  traverse_list();

  return 0;
}
