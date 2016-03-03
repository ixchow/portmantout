#include <limits>
#include <algorithm>
#include <map>
#include <chrono>
#include <random>

#include "blossom5/PerfectMatching.h"

#include "graph.hpp"
#include "stopwatch.hpp"

struct {
	std::string prefix = "portmanteaux";
	std::string merge = "max min"; //"matching"; //"min, matching"; //"matching"; //"min"; //"max min";
	std::string order = "reverse"; //"random";
	bool block = true;
	uint32_t chunk = 2000;
	void describe() {
		std::cout << "Options:\n";
		std::cout << "\tStarting prefix: " << prefix << "\n";
		std::cout << "\tMerge cost: " << merge << "\n";
		std::cout << "\tMerge order: " << order << "\n";
		std::cout << "\tBlocking: " << (block ? "yes" : "no") << "\n";
		if (merge == "matching") {
			std::cout << "\tMatching chunk size: " << chunk << "\n";
		}
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
		if (tag == "prefix:") {
			options.prefix = value;
		} else if (tag == "merge:") {
			options.merge = value;
		} else if (tag == "order:") {
			options.order = value;
		} else if (tag == "chunk:") {
			options.chunk = std::atoi(value.c_str());
		} else if (tag == "block:") {
			options.block = (value == "true" || value == "yes" || value == "t" || value == "1" || value =="y");
		} else {
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

	uint32_t start = -1U;
	uint32_t start_len = 0;
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
		if (prefix.substr(0, options.prefix.size()) == options.prefix) {
			if (start == -1U) {
				start = &m - &maximal[0];
				start_len = prefix.size();
				std::cout << "Using: " << prefix << std::endl;
			} else {
				std::cout << "(Alternate prefix: " << prefix << ")" << std::endl;
			}
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
			options.describe();
			std::cout << "At " << particles.size() << " particles." << std::endl;

			std::vector< std::pair< uint32_t, uint32_t > > next_particles;

			std::vector< std::pair< uint32_t, uint32_t > > best; //*words* to merge, not particles
			if (options.merge == "max min") {
				int32_t best_cost = std::numeric_limits< int32_t >::max();
				for (auto const &p1 : particles) {
					int32_t p1_best_cost = std::numeric_limits< int32_t >::max();
					std::vector< std::pair< uint32_t, uint32_t > > p1_best;
					for (auto const &p2 : particles) {
						if (&p2 == &p1) continue;
						if (p2.first == start) continue; //can't merge with start symbol, no matter how much one might wish to

						//p1 then p2 incurs:
						int32_t cost = distances[p1.second * maximal.size() + p2.first];
						cost -= int32_t(graph.depth[maximal[p2.first]]); //basically, cost is -overlap

						if (cost < p1_best_cost) {
							p1_best_cost = cost;
							p1_best.clear();
						}
						if (cost == p1_best_cost) {
							p1_best.emplace_back(p1.second, p2.first);
						}
					}
					p1_best_cost = -p1_best_cost;
					if (!p1_best.empty() && p1_best_cost < best_cost) {
						best_cost = p1_best_cost;
						best.clear();
					}
					if (p1_best_cost == best_cost) {
						best.insert(best.end(), p1_best.begin(), p1_best.end());
					}
				}
				std::cout << "   " << best.size() << " have cost " << best_cost << std::endl;
			} else if (options.merge == "greedy") {
				for (auto const &p1 : particles) {
					int32_t p1_best_cost = std::numeric_limits< int32_t >::max();
					std::vector< std::pair< uint32_t, uint32_t > > p1_best;
					for (auto const &p2 : particles) {
						if (&p2 == &p1) continue;
						if (p2.first == start) continue; //can't merge with start symbol, no matter how much one might wish to

						//p1 then p2 incurs:
						int32_t cost = distances[p1.second * maximal.size() + p2.first];
						cost -= int32_t(graph.depth[maximal[p2.first]]); //basically, cost is -overlap

						if (cost < p1_best_cost) {
							p1_best_cost = cost;
							p1_best.clear();
						}
						if (cost == p1_best_cost) {
							p1_best.emplace_back(p1.second, p2.first);
						}
					}
					best.insert(best.end(), p1_best.begin(), p1_best.end());
				}
				std::cout << "   " << best.size() << " have optimal cost" << std::endl;

			} else if (options.merge == "min" || (options.merge == "min, matching" && particles.size() > options.chunk)) {
int32_t best_cost = std::numeric_limits< int32_t >::max();
				for (auto const &p1 : particles) {
					for (auto const &p2 : particles) {
						if (&p2 == &p1) continue;
						if (p2.first == start) continue; //can't merge with start symbol, no matter how much one might wish to

						//p1 then p2 incurs:
						int32_t cost = distances[p1.second * maximal.size() + p2.first];
						cost -= int32_t(graph.depth[maximal[p2.first]]); //basically, cost is -overlap

						if (cost < best_cost) {
							best_cost = cost;
							best.clear();
						}
						if (cost == best_cost) {
							best.emplace_back(p1.second, p2.first);
						}
					}
				}
				std::cout << "   " << best.size() << " have cost " << best_cost << std::endl;
			} else if (options.merge == "matching" || (options.merge.substr(options.merge.size() - 10) == ", matching" && particles.size() <= options.chunk)) {
				//assign particles to chunks:
				std::vector< std::vector< uint32_t > > chunks(1);
				while (particles.size() / chunks.size() > options.chunk) {
					chunks.emplace_back();
				}
				std::cout << "Using " << chunks.size() << " chunks." << std::endl;
				for (uint32_t p = 0; p < particles.size(); ++p) {
					chunks[size_t(p) * chunks.size() / particles.size()].push_back(p);
				}
				for (auto const &chunk : chunks) {
					std::cout << "  matching chunk " << &chunk - &chunks[0] << " of " << chunks.size() << std::endl; //DEBUG
					PerfectMatching pm(chunk.size() * 2, chunk.size() * chunk.size());
					pm.options.verbose = false;
					for (uint32_t i1 = 0; i1 < chunk.size(); ++i1) {
						auto const &p1 = particles[chunk[i1]];
						for (uint32_t i2 = 0; i2 < chunk.size(); ++i2) {
							auto const &p2 = particles[chunk[i2]];
							if (&p2 == &p1) continue;

							int32_t cost = distances[p1.second * maximal.size() + p2.first];
							cost -= int32_t(graph.depth[maximal[p2.first]]); //basically, cost is -overlap

							if (p2.first == start) {
								cost = -10; //uniformly cheap because we never actually do it
							}

							pm.AddEdge(i1, chunk.size() + i2, cost); //matching end to start, I guess?
						}
					}
					pm.Solve();
					int64_t total_cost = 0;
					for (uint32_t i1 = 0; i1 < chunk.size(); ++i1) {
						uint32_t m = pm.GetMatch(i1);
						assert(m >= chunk.size());
						m -= chunk.size();
						if (particles[chunk[m]].first == start) continue;
						int32_t cost = distances[particles[chunk[i1]].second * maximal.size() + particles[chunk[m]].first];
						cost -= int32_t(graph.depth[maximal[particles[chunk[m]].first]]); //basically, cost is -overlap
						total_cost += cost;


						best.emplace_back(particles[chunk[i1]].second, particles[chunk[m]].first);
					}
					std::cout << "Average cost: " << total_cost / double(chunk.size()) << std::endl;
				}
			} else {
				assert(0 && "Unknown merge option");
			}
			std::vector< uint32_t > starts(maximal.size(), -1U);
			std::vector< uint32_t > ends(maximal.size(), -1U);
			for (uint32_t p = 0; p < particles.size(); ++p) {
				assert(particles[p].first < starts.size());
				assert(particles[p].second < ends.size());
				starts[particles[p].first] = p;
				ends[particles[p].second] = p;
			}

			if (options.order == "default") {
			} else if (options.order == "reverse") {
				std::reverse(best.begin(), best.end()); //who knows?
			} else if (options.order == "random") {
				for (auto const &b : best) {
					static std::mt19937 mt(std::chrono::high_resolution_clock::now().time_since_epoch().count());
					auto idx = &b - &best[0];
					std::swap(best[idx], best[mt() % (best.size() - idx) + idx]);
					assert(b == best[idx]);
				}
			} else {
				assert(0 && "Unknown order option.");
			}

			uint32_t performed = 0;

			std::vector< bool > blocked(particles.size(), false);

			for (auto const &b : best) {
				assert(b.first < ends.size());
				assert(b.second < starts.size());
				if (ends[b.first] == -1U || starts[b.second] == -1U) continue;
				++performed;
				uint32_t p1 = ends[b.first];
				uint32_t p2 = starts[b.second];
				assert(p1 < particles.size());
				assert(p2 < particles.size());

				if (p1 == p2) continue; //no making loops. not allowed!
				if (options.block) {
					if (blocked[p1] || blocked[p2]) continue;
					blocked[p1] = true;
					blocked[p2] = true;
				}


				particles[p1].second = particles[p2].second;
				assert(particles[p2].second < ends.size());
				assert(ends[particles[p2].second] == p2);
				ends[particles[p2].second] = p1;

				particles[p2] = std::make_pair(-1U, -1U); //no longer relevant

				ends[b.first] = -1U;
				starts[b.second] = -1U;

				merges.insert(b);
				total_length += distances[b.first * maximal.size() + b.second];
			}
			std::cout << "  (did " << performed << " merges.)" << std::endl;
			for (uint32_t p = 0; p < particles.size(); ++p) {
				if (particles[p] != std::make_pair(-1U, -1U)) {
					next_particles.emplace_back(particles[p]);
				}
			}
			particles = std::move(next_particles);

			std::cout << "particles: "  << particles.size() << " total length so far: " << total_length << std::endl;
		}
	}

	stopwatch("merge");

	//------------------------------------------

	options.describe();

	std::vector< uint32_t > path;
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


}
