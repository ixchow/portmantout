#include <limits>
#include <algorithm>
#include <map>
#include <chrono>
#include <random>
#include <unordered_set>
#include <set>

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
	const uint32_t size = maximal.size();

	//Compute overlaps:
	// for each maximal node, want to find which other maximal nodes are prefixes.

	std::vector< std::string > words;
	uint32_t total_words_length = 0;
	{
		words.reserve(size);
		for (auto m : maximal) {
			std::string word;
			uint32_t at = m;
			while (at != 0) {
				assert(at < graph.nodes);
				assert(graph.parent[at] < graph.nodes);
				uint32_t p = graph.parent[at];
				bool found = false;
				for (uint32_t i = graph.child_start[p]; i < graph.child_start[p+1]; ++i) {
					if (graph.child[i] == at) {
						assert(!found);
						found = true;
						word += graph.child_char[i];
					}
				}
				assert(found);
				at = p;
			}
			total_words_length += word.size();
			std::reverse(word.begin(), word.end());
			words.emplace_back(word);
			//std::cout << word << std::endl; //sanity? I guess.
		}
		stopwatch("words");
	}
	std::unique_ptr< uint8_t[] > overlaps(new uint8_t[size*size]);
	stopwatch("alloc");

	bool compute_overlaps;

	{
		std::ifstream in("overlaps.table");
		if (in.read(reinterpret_cast< char * >(overlaps.get()), size * size)) {
			std::cerr << "Read overlaps from table." << std::endl;
			compute_overlaps = false;
		} else {
			std::cerr << "Failed to read overlaps table, will compute" << std::endl;
			compute_overlaps = true;
		}
		stopwatch("read");
	}

	if (compute_overlaps) {
		for (uint32_t r = 0; r < size; ++r) {
			std::vector< bool > word(graph.nodes, false);
			//std::unordered_set< uint32_t > word; //nodes in the word
			{
				uint32_t at = maximal[r];
				while (at != 0) {
					assert(at < graph.nodes);
					word[at] = true; //.insert(at);
					at = graph.parent[at];
				}
				word[0] = true; //word.insert(at);
			}
			for (uint32_t c = 0; c < size; ++c) {
				//uint32_t a, b;
				//{
				//prefix way:
					uint32_t over = 0;
					uint32_t at = maximal[c];
					while (!word[at]) {
						at = graph.rewind[at];
					}
					while (at != 0) {
						at = graph.parent[at];
						++over;
					}
					//a = over;
				//}
				/*{
					//slow way:
					uint32_t over = std::min(words[r].size(), words[c].size());
					while (over > 0 && words[c].substr(words[c].size()-over, over) != words[r].substr(0, over)) --over;
					b = over;
					a = b;
				}*/
				/*if (a != b) {
					std::cout << words[c] << " to " << words[r] << " " << a << " vs " << b << std::endl;
				}*/
				overlaps[r * size + c] = over;
			}
			if ((r+1) % 1000 == 0) {
				std::cout << r + 1 << " of " << size << std::endl;
				stopwatch("1k overlaps");
			}
		}
		stopwatch("fin");
		std::ofstream out("overlaps.table");
		out.write(reinterpret_cast< char * >(overlaps.get()), size * size);
		stopwatch("write");
	}

	std::vector< bool > sel(256, false);
	{
		for (uint32_t i = 0; i < size; ++i) {
			overlaps[i * size + i] = 0;
		}
		stopwatch("diag");
		std::cout << "Self-edges discouraged." << std::endl;

		uint8_t *start = overlaps.get();
		uint8_t *end = overlaps.get() + size * size;

		for (uint8_t *o = start; o != end; ++o) {
			sel[*o] = true;
		}
		stopwatch("Selected distances.");

	}

	//------------------------------------------

	std::vector< bool > r_assigned(size, false), c_assigned(size, false);

	uint32_t total = 0;
	for (uint32_t o = 0xff; o != 0; --o) {
		if (!sel[o]) continue;
		uint32_t assignments = 0;
		for (uint32_t r = 0; r < size; ++r) {
			if (r_assigned[r]) continue;
			for (uint32_t c = 0; c < size; ++c) {
				if (c_assigned[c]) continue;
				assert(overlaps[r * size + c] <= o);
				if (overlaps[r * size + c] == o) {
					++assignments;
					total += overlaps[r * size + c];
					c_assigned[c] = true;
					r_assigned[r] = true;
					break;
				}
			}
		}
		std::cout << "After " << assignments << " assignments at overlap '" << (int)o << "', total is " << total << " (bound: " << total_words_length - total << ")" << std::endl;
	}
	return 0;
	
}
