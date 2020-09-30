main: main.cpp
	g++ main.cpp -o main -Wall -g -lncurses

clean:
	rm main
