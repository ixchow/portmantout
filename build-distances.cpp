#include "graph.hpp"
#include "stopwatch.hpp"

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

	stopwatch("setup");

	//run lots of BFS's:
	for (auto const &seed : maximal) {
		std::vector< uint8_t > distance(graph.nodes, 0xff);
		std::vector< uint32_t > ply;
		ply.emplace_back(seed);
		distance[seed] = 0;
		uint32_t dis = 0;
		while (!ply.empty()) {
			std::vector< uint32_t > next_ply;
			dis += 1;
			assert(dis < 0xff);
			for (auto i : ply) {
				for (uint32_t a = graph.adj_start[i]; a < graph.adj_start[i+1]; ++a) {
					uint32_t n = graph.adj[a];
					if (distance[n] == 0xff) {
						distance[n] = dis;
						next_ply.emplace_back(n);
					}
				}
			}
			ply = std::move(next_ply);
		}

		static uint8_t max = 0;

		{ //copy to distances table
			uint8_t *base = distances.get() + (&seed - &maximal[0]) * maximal.size();
			for (auto const &m : maximal) {
				assert(distance[m] != 0xff);
				max = std::max(max, distance[m]);
				*(base++) = distance[m];
			}
		}

		static uint32_t step = 0;
		step += 1;
		if (step == 500) {
			stopwatch("bfs * 500");
			std::cout << (&seed - &maximal[0]) << " / " << maximal.size()
				<< " -- longest path so far: " << int32_t(max) << "."
				<< std::endl;
			step = 0;
		}
	}

	stopwatch("last bit of calculation");

	std::ofstream out("distances.table");
	out.write(reinterpret_cast< const char * >(distances.get()), maximal.size() * maximal.size());

	stopwatch("write");

	return 0;
}
