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
	uint32_t start_len = 0;

	{
		uint32_t at = 0;
		uint32_t letter = 0;
		for (auto c : portmantout) {
			assert(at < graph.nodes);
			while (at < graph.nodes) {
				bool found = false;
				assert(graph.child_start[at+1] <= graph.children);
				for (uint32_t i = graph.child_start[at]; i < graph.child_start[at+1]; ++i) {
					if (graph.child_char[i] == c) {
						at = graph.child[i];
						found = true;
						break;
					}
				}
				if (found) break;
				at = graph.rewind[at];
			}
			assert(at < graph.nodes);
			assert(at != 0);
			if (graph.maximal[at]) {
				assert(maximal_index[at] < maximal.size());
				path.emplace_back(maximal_index[at]);
				if (path.size() == 1) {
					start_len = letter + 1;
				}
			}
			++letter;
		}
	}
	stopwatch("compute path");

	std::cout << "Path has " << path.size() << " maximals (of " << maximal.size() << ")" << std::endl;

	{
		uint32_t len = start_len;
		for (uint32_t i = 0; i + 1 < path.size(); ++i) {
			len += distances[path[i] * maximal.size() + path[i+1]];
		}
		std::cout << "Expected length " << len << " vs real length " << portmantout.size() << std::endl;
	}

	bool done = false;
	while (!done) {
		done = true;
		{ //See if there are any removable words:
			std::vector< uint8_t > counts(maximal.size());
			for (auto p : path) {
				counts[p] += 1;
			}
			for (uint32_t i = 1; i + 1 < path.size(); ++i) {
				assert(counts[path[i]] >= 1);
				if (counts[path[i]] == 1) continue;
				uint32_t start = i;
				uint32_t end = i;
				while (end + 1 < path.size()) {
					assert(counts[path[end]] > 1);
					counts[path[end]] -= 1;
					end += 1;
					assert(counts[path[end]] >= 1);
					if (counts[path[end]] == 1) break;
				}

			}
		}


	}

	{
		uint32_t len = start_len;
		for (uint32_t i = 0; i + 1 < path.size(); ++i) {
			len += distances[path[i] * maximal.size() + path[i+1]];
		}
		std::vector< uint32_t > dump;
		for (auto p : path) {
			assert(p < maximal.size());
			dump.emplace_back(maximal[p]);
		}
		std::string filename = "improved-" + std::to_string(len) + ".dump";
		std::cout << "Writing to '" << filename << "'" << std::endl;
		std::ofstream out(filename);
		out.write(reinterpret_cast< const char * >(&dump[0]), 4 * dump.size());
	}


}
