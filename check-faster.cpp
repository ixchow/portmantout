#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <cassert>
#include <vector>
#include <deque>
#include <time.h>

class Level : public std::map< char, Level * > {
public:
	Level() : terminal(0), rewind(NULL) { }
	uint32_t terminal;
	Level *rewind;
};

class Counts {
public:
	Counts() : nodes(0), terminals(0) {
	}
	uint32_t nodes;
	std::vector< uint32_t > terminals;
	std::vector< uint32_t > depth;
};

void count_tree(Counts &counts, Level *level, uint32_t depth) {
	while (counts.depth.size() <= depth) {
		counts.depth.push_back(0);
	}
	counts.depth[depth] += 1;

	while (counts.terminals.size() <= level->terminal) {
		counts.terminals.push_back(0);
	}
	counts.terminals[level->terminal] += 1;

	counts.nodes += 1;
	for (auto c : *level) {
		count_tree(counts, c.second, depth + 1);
	}
}

void stopwatch(const char *name) {
	struct timespec ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	double now = double(ts.tv_sec) + 1.0e-9 * double(ts.tv_nsec);
	static double prev = now;

	std::cout << name << ": " << (now - prev) * 1000.0 << "ms" << std::endl;

	prev = now;
}

void dump_missing(std::string const &prefix, Level *level) {
	if (level->terminal) {
		std::cout << prefix << std::endl;
	}
	for (auto c : *level) {
		dump_missing(prefix + c.first, c.second);
	}
}

int main(int argc, char **argv) {
	stopwatch("start");

	//This is a tree-based matcher with explicit backtracking.
	// Doing this KMP-style (adding extra child edges that backtrack) would be slicker.
	//compressing and localizing the tree (since all nodes are at most 26-out),
	//  would almost certainly also be a performance win.
	Level root;
	{
		std::set< char > symbols;

		std::ifstream wordlist("wordlist.asc");
		std::string word;
		while (std::getline(wordlist, word)) {
			Level *at = &root;
			for (auto c : word) {
				symbols.insert(c);
				auto f = at->insert(std::make_pair(c, nullptr)).first;
				if (f->second == NULL) {
					f->second = new Level();
				}
				at = f->second;
			}
			assert(at);
			at->terminal += 1;
		}
		std::cout << "Have " << symbols.size() << " symbols." << std::endl;

		//set up rewind pointers in tree, level-by-level:
		{
			std::vector< Level * > layer;
			layer.push_back(&root);
			root.rewind = NULL;
			while (!layer.empty()) {
				std::vector< Level * > next_layer;
				for (auto l : layer) {
					//update rewind pointers for children based on current rewind pointer.
					for (auto cl : *l) {
						next_layer.emplace_back(cl.second);

						Level *r = l->rewind;
						while (r != NULL) {
							assert(r != NULL);
							auto f = r->find(cl.first);
							if (f != r->end()) {
								//great, can extend this rewind:
								r = f->second;
								break;
							} else {
								//have to rewind further:
								r = r->rewind;
							}
						}
						assert(l == &root || r != NULL); //for everything but the root, rewind should always hit root and bounce down
						if (r == NULL) r = &root;
						cl.second->rewind = r;
					}
				}
				layer = next_layer;
			}
		}

		//unroll tree for some stats:
		Counts counts;
		count_tree(counts, &root, 0);
		std::cout << "Depths:" << std::endl;
		for (uint32_t i = 0; i < counts.depth.size(); ++i) {
			std::cout << i << ": " << counts.depth[i] << std::endl;
		}
		std::cout << "Terminal counts: " << std::endl;
		for (uint32_t i = 0; i < counts.terminals.size(); ++i) {
			std::cout << i << ": " << counts.terminals[i] << std::endl;
		}
		std::cout << counts.nodes << " nodes." << std::endl;
	}

	stopwatch("build");

	std::string portmantout;
	if (!std::getline(std::cin, portmantout) || portmantout.size() == 0) {
		std::cerr << "Please pass a portmantout on stdin." << std::endl;
		return 1;
	}

	stopwatch("read");

	std::cout << "Testing portmantout of " << portmantout.size() << " letters." << std::endl;

	uint32_t found_words = 0;

	Level *at = &root;
	for (auto c : portmantout) {
		while (at != NULL) {
			if (at->terminal > 0) {
				at->terminal -= 1;
				++found_words;
			}
			auto f = at->find(c);
			if (f != at->end()) {
				at = f->second;
				break;
			} else {
				at = at->rewind;
			}
		}
		if (at == NULL) {
			std::cout << "COVERAGE HOLE!" << std::endl;
			at = &root;
		}
	}
	if (at->terminal > 0) {
		at->terminal -= 1;
		++found_words;
	}


	stopwatch("test");

	std::cout << "Found " << found_words << " words." << std::endl;

	Counts counts;
	count_tree(counts, &root, 0);

	if (counts.terminals.size() > 1) {
		std::cout << "Missing at least " << counts.terminals[1] << " words." << std::endl;
		//dump_missing("", &root);
		return 1;
	}

	return 0;

}
