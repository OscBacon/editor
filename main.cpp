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

void traverse_list(ostream &out = cout) {
  for (auto const &line : input) {
    for (char c : *line) {
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

void print_text() {
  erase();
  move(0, 0);
  refresh();

  auto it = input.begin();
  for (; it != input.end(); ++it) {
    for (auto line_it = (*it)->begin(); line_it != (*it)->end(); ++line_it) {
      addch(*line_it);
    }

    if (*it != input.back()) {
      addch('\n');
    }
  }

  refresh();
}

void do_tui() {
  // TODO: merge with print_text
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
    // Using if-else's to catch printable characters
    if (ch == KEY_LEFT) {
      if (x > 0) {
        --x;
        --line_it;
      }
    } else if (ch == KEY_RIGHT) {
      // TODO: Don't assume lines don't overflow
      if (x < (int) (*it)->size() - 1 && x < max_x) {
        ++x;
        ++line_it;
      }
    } else if (ch == KEY_UP) {
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
    } else if (ch == KEY_DOWN) {
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
    } else if (ch == KEY_BACKSPACE) {
      // TODO: handle deleting line
      if (x == 0) continue;

      line_it = (*it)->erase(line_it);
      // erase returns the next iterator, go to previous instead
      --line_it;
      --x;
      print_text();
    } else if (std::isprint(ch, loc)) {
      // TODO: let lines overflow
      if ((int) (*it)->size() == max_x) continue;

      (*it)->insert(line_it, ch);
      ++x;
      // auto temp_it = input.begin();
      print_text();
    } else {
      // TODO: handle newline
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

  if (argc == 3) {
    write_output(argv[2]);
  } else {
    traverse_list();
  }

  return 0;
}
