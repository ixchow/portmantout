#pragma once

#include <vector>
#include <fstream>
#include <memory>
#include <cassert>
#include <strings.h> //because bzero is handy

class Graph {
public:
	void resize(uint32_t nodes, uint32_t adjacencies) {
		uint32_t bytes = 0;
		bytes += 8; //nodes, adjacencies storage
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
		DO(adj_start, nodes + 1);
		DO(adj,      adjacencies);
		DO(adj_char, adjacencies);
		#undef DO

		assert(bytes % 4 == 0);
		storage_size = bytes / 4;

		storage.reset(new uint32_t[storage_size]);

		bzero(storage.get(), storage_size * 4);

		storage[0] = nodes;
		storage[1] = adjacencies;

		#define DO(X, C) \
			do { \
				X += reinterpret_cast< decltype(X) >(storage.get()) - reinterpret_cast< decltype(X) >(NULL); \
			} while(0)
		DO(depth,     nodes);
		DO(maximal,   nodes);
		DO(parent,    nodes);
		DO(adj_start, nodes);
		DO(adj,      adjacencies);
		DO(adj_char, adjacencies);
		#undef DO
	}

	//node info:
	uint32_t nodes = 0;
	uint8_t  *depth = nullptr;
	bool     *maximal = nullptr;
	uint32_t *parent = nullptr;
	uint32_t *adj_start = nullptr;

	//edge info:
	uint32_t adjacencies = 0;
	uint32_t *adj = nullptr;
	char     *adj_char = nullptr;

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
		resize(nodes, adjacencies);
		in.seekg(0);
		if (!in.read(reinterpret_cast< char * >(storage.get()), 4 * storage_size)) return false;
		return true;
	}

};
