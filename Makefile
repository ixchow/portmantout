.PHONY : all

all : check check-fast check-faster check-fasterer

clean :
	rm -f check check-fast check-faster check-fasterer


OS := $(shell uname)

ifeq ($(OS), Linux)
	CPP = g++ -Wall -Werror -O2 -std=c++11
else
	CPP = clang++ -Wall -Werror -O2 -std=c++11
endif



check : check.cpp stopwatch.hpp
	$(CPP) -o $@ $<

check-fast : check-fast.cpp stopwatch.hpp
	$(CPP) -o $@ $<

check-faster : check-faster.cpp stopwatch.hpp
	$(CPP) -o $@ $<

check-fasterer : check-fasterer.cpp stopwatch.hpp
	$(CPP) -o $@ $<
