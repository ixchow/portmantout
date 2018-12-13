#include <limits>
#include <algorithm>
#include <map>
#include <chrono>
#include <random>
#include <unordered_set>

#include "graph.hpp"
#include "stopwatch.hpp"

#include <dlib/optimization.h>

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

	std::vector< uint32_t > maximal_index(graph.nodes, -1U);
	std::vector< uint32_t > maximal;

	for (auto m = graph.maximal; m != graph.maximal + graph.nodes; ++m) {
		if (*m) {
			maximal_index[m - graph.maximal] = maximal.size();
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

	uint8_t max_dis = 0;
	{
		uint8_t *start = distances.get();
		uint8_t *end = distances.get() + maximal.size() * maximal.size();
		for (uint8_t *d = start; d != end; ++d) {
			max_dis = std::max(max_dis, *d);
		}
		std::cout << "Max distance is " << int(max_dis) << std::endl;
		for (uint8_t *d = start; d != end; ++d) {
			*d = max_dis - *d + 1;
		}
		std::cout << "Distances inverted." << std::endl;

		for (uint32_t i = 0; i < maximal.size(); ++i) {
			distances[i * maximal.size() + i] = 0;
		}
		std::cout << "Self-edges discouraged." << std::endl;
	}

	//------------------------------------------

	//Let's try the lazy approach:
	std::vector< long > assignment = max_cost_assignment(dlib::mat(distances.get(), maximal.size(), maximal.size()));

	stopwatch("did assignment");

	uint32_t total_cost = 0;
	for (uint32_t i = 0; i < assignment.size(); ++i) {
		total_cost += distances[ maximal.size() * i + assignment[i] ];
	}
	std::cout << "Total cost is: " << total_cost << " corresponding to a bound of " << maximal.size() * uint32_t(max_dis) - total_cost << std::endl;

	return 0;
	
}
