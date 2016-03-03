.PHONY : all

all : check check-fast check-faster check-fasterer

clean :
	rm -f check check-fast check-faster check-fasterer


OS := $(shell uname)

ifeq ($(OS), Linux)
	CPP = g++ -Wall -Werror -O2 -std=c++11 -g
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

compress : compress.cpp Coder.cpp Coder.hpp
	$(CPP) -o $@ -I/usr/include/eigen3 compress.cpp Coder.cpp

compress-tests : compress-tests.cpp
	$(CPP) -o $@ $<

search-gen : search-gen.cpp
	$(CPP) -o $@ $<

build-graph : build-graph.cpp stopwatch.hpp graph.hpp
	$(CPP) -o $@ $<

wordlist.graph : build-graph wordlist.asc
	./build-graph

build-distances : build-distances.cpp stopwatch.hpp graph.hpp
	$(CPP) -o $@ $<

distances.table : build-distances wordlist.graph
	./build-distances


search-match : search-match.cpp graph.hpp stopwatch.hpp
	$(CPP) -o $@ $< blossom5/PMduals.o blossom5/PMexpand.o blossom5/PMinit.o blossom5/PMinterface.o blossom5/PMmain.o blossom5/PMrepair.o blossom5/PMshrink.o blossom5/MinCost/MinCost.o

search-match-ply : search-match-ply.cpp graph.hpp stopwatch.hpp
	$(CPP) -o $@ $< #blossom5/PMduals.o blossom5/PMexpand.o blossom5/PMinit.o blossom5/PMinterface.o blossom5/PMmain.o blossom5/PMrepair.o blossom5/PMshrink.o blossom5/MinCost/MinCost.o

path-to-word : path-to-word.cpp graph.hpp stopwatch.hpp
	$(CPP) -o $@ $<
