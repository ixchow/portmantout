#include <limits>
#include <algorithm>
#include <map>

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

	uint32_t start = -1U;
	for (auto const &m : maximal) {
		std::string prefix;
		uint32_t at = m;
		while (graph.parent[at] < graph.nodes) {
			int32_t parent = graph.parent[at];
			bool found = false;
			for (uint32_t a = graph.adj_start[parent]; a != graph.adj_start[parent+1]; ++a) {
				if (graph.adj[a] == at) {
					assert(!found);
					found = true;
					prefix += graph.adj_char[a];
				}
			}
			assert(found);
			at = parent;
		}
		std::reverse(prefix.begin(), prefix.end());
		if (prefix.substr(0, 11) == "portmanteau") {
			start = &m - &maximal[0];
			std::cout << prefix << std::endl; //DEBUG
		}
	}
	assert(start != -1U);

	std::unique_ptr< uint8_t[] > distances(new uint8_t[maximal.size() * maximal.size()]);

	
	std::ifstream in("distances.table");
	if (!in.read(reinterpret_cast< char * >(distances.get()), maximal.size() * maximal.size())) {
		std::cerr << "failed to read distance table." << std::endl;
		return false;
	}

	stopwatch("read distances");

	//------------------------------------------

	//goal: greedy (randomized) unification from lowest to highest cost.

	std::map< uint32_t, uint32_t > merges;

	std::vector< std::pair< uint32_t, uint32_t > > particles;

	for (uint32_t i = 0; i < maximal.size(); ++i) {
		particles.emplace_back(i, i);
	}

	{
		uint32_t total_length = 0;
		//unify in cost order:
		while (particles.size() > 1) {
			std::cout << "At " << particles.size() << " particles." << std::endl;

			std::vector< std::pair< uint32_t, uint32_t > > next_particles;
			std::vector< bool > used(particles.size(), false);
			while (1) {
				std::vector< std::pair< uint32_t, uint32_t > > best;
				int32_t best_cost = std::numeric_limits< int32_t >::max();
				for (auto const &p1 : particles) {
					if (used[&p1 - &particles[0]]) continue;
					for (auto const &p2 : particles) {
						if (&p2 == &p1) continue;
						if (used[&p2 - &particles[0]]) continue;
						if (p2.first == start) continue; //can't merge with start symbol, no matter how much one might wish to
	
						//p1 then p2 incurs:
						int32_t cost = distances[p1.second * maximal.size() + p2.first];
						cost -= int32_t(graph.depth[maximal[p2.first]]); //basically, cost is -overlap

						if (cost < best_cost) {
							best_cost = cost;
							best.clear();
						}
						if (cost == best_cost) {
							best.emplace_back(std::make_pair(&p1 - &particles[0], &p2 - &particles[0]));
						}
					}
				}
				if (best.empty()) break;
				std::cout << "   " << best.size() << " have cost " << best_cost << std::endl;
				for (auto b : best) {
					if (used[b.first] || used[b.second]) continue;
					used[b.first] = true;
					used[b.second] = true;
					merges.insert(std::make_pair(particles[b.first].second, particles[b.second].first));
					next_particles.emplace_back(particles[b.first].first, particles[b.second].second);
					total_length += distances[particles[b.first].second * maximal.size() + particles[b.second].first];
				}
				std::cout << "Total length so far: " << total_length << std::endl;
			}
			uint32_t extra = 0;
			for (uint32_t p = 0; p < particles.size(); ++p) {
				if (!used[p]) {
					next_particles.emplace_back(particles[p]);
					++extra;
				}
			}
			std::cout << "Had " << extra << " leftover." << std::endl;
			particles = std::move(next_particles);
		}
	}

	stopwatch("merge");

	//------------------------------------------

	std::vector< uint32_t > path;
	{
		uint32_t len = 0;
		uint32_t at = start;
		while (1) {
			path.emplace_back(maximal[at]);
			auto f = merges.find(at);
			if (f == merges.end()) break;
			len += distances[at * maximal.size() + f->second];
			at = f->second;
		}
		std::cout << "Path of " << path.size() << " steps (extracted from " << merges.size() << " merges) -- len " << len << " (not counting first word)" << std::endl;
		std::ofstream out("path.dump");
		out.write(reinterpret_cast< const char * >(&path[0]), 4 * path.size());
	}


}
