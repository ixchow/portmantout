.PHONY : all

all : check check-fast

CPP = g++ -Wall -Werror -std=c++11

check : check.cpp
	$(CPP) -o $@ $<

check-fast : check-fast.cpp
	$(CPP) -o $@ $<
