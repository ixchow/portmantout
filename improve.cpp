#include <limits>
#include <algorithm>
#include <map>
#include <chrono>
#include <random>
#include <unordered_set>

#include "graph.hpp"
#include "stopwatch.hpp"

struct {
	void describe() {
	}
} options;

int main(int argc, char **argv) {

	for (int a = 1; a < argc; ++a) {
		std::string arg = argv[a];
		std::string tag;
		std::string value;
		auto pos = arg.find(':');
		tag = arg.substr(0, pos+1);
		value = arg.substr(pos+1);
		{
			std::cerr << "Unknown tag '" << tag << "'" << std::endl;
			return 1;
		}
	}

	options.describe();

	stopwatch("start");
	Graph graph;
	if (!graph.read("wordlist.graph")) {
		std::cerr << "Failed to read graph." << std::endl;
		exit(1);
	}
	stopwatch("read graph");

	std::vector< uint32_t > maximal;

	for (auto m = graph.maximal; m != graph.maximal + graph.nodes; ++m) {
		if (*m) {
			maximal.emplace_back(m - graph.maximal);
		}
	}
	std::cout << "Have " << maximal.size() << " maximal nodes." << std::endl;


	std::unique_ptr< uint8_t[] > distances(new uint8_t[maximal.size() * maximal.size()]);

	std::ifstream in("distances.table");
	if (!in.read(reinterpret_cast< char * >(distances.get()), maximal.size() * maximal.size())) {
		std::cerr << "failed to read distance table." << std::endl;
		return false;
	}

	stopwatch("read distances");

	//------------------------------------------
	
	std::string portmantout;
	if (!std::getline(std::cin, portmantout) || portmantout.size() == 0) {
		std::cerr << "Please pass a portmantout on stdin." << std::endl;
		return 1;
	}

	std::cout << "Attempting to improve portmantout of " << portmantout.size() << " letters." << std::endl;

	stopwatch("read word");

	//------------------------------------------

	std::vector< uint32_t > path;

	{
		std::vector< std::vector< uint32_t > > possible;
		possible.reserve(portmantout.size() + 1);
		possible.emplace_back(1, 0);
		for (auto c : portmantout) {
			assert(!possible.back().empty());
			std::unordered_set< uint32_t > next;
			for (auto n : possible.back()) {
				assert(n < graph.nodes);
				assert(graph.adj_start[n+1] <= graph.adjacencies);
				for (uint32_t a = graph.adj_start[n]; a < graph.adj_start[a+1]; ++a) {
					if (graph.adj_char[a] == c) {
						next.insert(graph.adj[a]);
					}
				}
			}
			possible.emplace_back(next.begin(), next.end());
			std::cout << "[" << possible.back().size() << "] - " << possible.size() << " / " << portmantout.size() << std::endl;
		}
		assert(!possible.back().empty());
	}

	stopwatch("compute path");
/*
	//goal: greedy (randomized) unification from lowest to highest cost.
	{
		uint32_t len = start_len;
		uint32_t at = start;
		while (1) {
			path.emplace_back(maximal[at]);
			auto f = merges.find(at);
			if (f == merges.end()) break;
			len += distances[at * maximal.size() + f->second];
			at = f->second;
		}
		std::cout << "Path of " << path.size() << " steps (extracted from " << merges.size() << " merges) -- len " << len << std::endl;
		std::string filename = "path-" + std::to_string(len) + ".dump";
		std::cout << "Writing to '" << filename << "'" << std::endl;
		std::ofstream out(filename);
		out.write(reinterpret_cast< const char * >(&path[0]), 4 * path.size());
	}
*/


}
