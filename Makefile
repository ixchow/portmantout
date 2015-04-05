.PHONY : all

all : check check-fast check-faster

CPP = g++ -Wall -Werror -std=c++11

check : check.cpp stopwatch.hpp
	$(CPP) -o $@ $<

check-fast : check-fast.cpp stopwatch.hpp
	$(CPP) -o $@ $<

check-faster : check-faster.cpp stopwatch.hpp
	$(CPP) -o $@ $<
