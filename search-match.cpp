#include "graph.hpp"
#include "stopwatch.hpp"
#include "blossom5/PerfectMatching.h"

int main(int argc, char **argv) {

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

	int64_t match_nodes = maximal.size();
	if (match_nodes % 2 == 1) {
		match_nodes += 1;
	}

	PerfectMatching pm(match_nodes, match_nodes * (match_nodes - 1) / 2);

	for (uint32_t i = 0; i < maximal.size(); ++i) {
		if (match_nodes > int64_t(maximal.size())) {
			pm.AddEdge(i, match_nodes - 1, graph.depth[maximal[i]]); //"don't pair" edge costs length
		}
		for (uint32_t j = 0; j < i; ++j) {
			assert(i * maximal.size() + j < maximal.size() * maximal.size());
			assert(j * maximal.size() + i < maximal.size() * maximal.size());
			//uint8_t cost = std::min(distances[i * maximal.size() + j], distances[j * maximal.size() + i]);
			//pm.AddEdge(i, j, cost);
		}
	}
	stopwatch("build problem");

	pm.Solve();

	stopwatch("solve problem");


}
