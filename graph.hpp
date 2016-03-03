#pragma once

#include <vector>
#include <fstream>
#include <memory>
#include <cassert>
#include <strings.h> //because bzero is handy

class Graph {
public:
	void resize(uint32_t nodes, uint32_t adjacencies, uint32_t children) {
		uint32_t bytes = 0;
		bytes += 12; //nodes, adjacencies, children storage
		#define DO(X, C) \
			do { \
				assert(bytes % sizeof(*(X)) == 0); \
				(X) = reinterpret_cast< decltype(X) >(NULL) + bytes / sizeof(*(X)); \
				bytes += sizeof(*(X)) * (C); \
				while (bytes % 4) ++bytes; \
			} while(0)
		DO(depth,     nodes);
		DO(maximal,   nodes);
		DO(parent,    nodes);
		DO(rewind,    nodes);
		DO(adj_start, nodes + 1);
		DO(child_start, nodes + 1);
		DO(adj,      adjacencies);
		DO(adj_char, adjacencies);
		DO(child,      children);
		DO(child_char, children);
		#undef DO

		assert(bytes % 4 == 0);
		storage_size = bytes / 4;

		storage.reset(new uint32_t[storage_size]);

		bzero(storage.get(), storage_size * 4);

		storage[0] = nodes;
		storage[1] = adjacencies;
		storage[2] = children;

		#define DO(X) \
			do { \
				X += reinterpret_cast< decltype(X) >(storage.get()) - reinterpret_cast< decltype(X) >(NULL); \
			} while(0)
		DO(depth);
		DO(maximal);
		DO(parent);
		DO(rewind);
		DO(adj_start);
		DO(child_start);
		DO(adj);
		DO(adj_char);
		DO(child);
		DO(child_char);
		#undef DO
	}

	//node info:
	uint32_t nodes = 0;
	uint8_t  *depth = nullptr;
	bool     *maximal = nullptr;
	uint32_t *parent = nullptr;
	uint32_t *rewind = nullptr;
	uint32_t *adj_start = nullptr;
	uint32_t *child_start = nullptr;

	//edge info:
	uint32_t adjacencies = 0;
	uint32_t *adj = nullptr;
	char     *adj_char = nullptr;

	//no-rewind-adjacencies edge info:
	uint32_t children = 0;
	uint32_t *child = nullptr;
	char     *child_char = nullptr;


	//internal use:
	std::unique_ptr< uint32_t[] > storage;
	uint32_t storage_size = 0;

	void write(std::string filename) {
		assert(storage);
		std::ofstream out(filename);
		out.write(reinterpret_cast< const char * >(storage.get()), 4 * storage_size);
	}

	bool read(std::string filename) {
		std::ifstream in(filename);
		if (!in.read(reinterpret_cast< char * >(&nodes), 4)) return false;
		if (!in.read(reinterpret_cast< char * >(&adjacencies), 4)) return false;
		if (!in.read(reinterpret_cast< char * >(&children), 4)) return false;
		resize(nodes, adjacencies, children);
		in.seekg(0);
		if (!in.read(reinterpret_cast< char * >(storage.get()), 4 * storage_size)) return false;
		return true;
	}

};
