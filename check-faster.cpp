#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <cassert>
#include <vector>
#include <deque>
#include "stopwatch.hpp"

class Level : public std::map< char, Level * > {
public:
	Level() : terminal(false), depth(-1U), safe(-1U), rewind(NULL), visited(false) { }
	//set during initial build:
	bool terminal;

	//set during second build phase:
	uint32_t depth; //how long is the prefix?
	uint32_t safe; //how much of the prefix is a word?
	Level *rewind;

	//set during evaluation:
	bool visited;
};

class Counts {
public:
	Counts() : nodes(0), terminals(0) {
	}
	uint32_t nodes;
	uint32_t terminals;
	std::vector< uint32_t > depth;
};

void count_tree(Counts &counts, Level *level, uint32_t depth) {
	while (counts.depth.size() <= depth) {
		counts.depth.push_back(0);
	}
	counts.depth[depth] += 1;

	if (level->terminal) {
		counts.terminals += 1;
	}

	counts.nodes += 1;
	for (auto c : *level) {
		count_tree(counts, c.second, depth + 1);
	}
}

void dump_missing(std::string const &prefix, Level *level) {
	if (level->terminal) {
		std::cout << prefix << std::endl;
	}
	for (auto c : *level) {
		dump_missing(prefix + c.first, c.second);
	}
}

void count_visited(Level &root, uint32_t *found_words, uint32_t *missed_words) {
	//propagate visited information up the strata:
	std::vector< std::vector< Level * > > strata;
	strata.emplace_back(1, &root);
	while (!strata.back().empty()) {
		std::vector< Level * > next;
		for (auto l : strata.back()) {
			for (auto cl : *l) {
				next.emplace_back(cl.second);
			}
		}
		strata.emplace_back(std::move(next));
	}
	for (auto s = strata.rbegin(); s != strata.rend(); ++s) {
		for (auto l : *s) {
			if (!l->visited) {
				for (auto cl : *l) {
					if (cl.second->visited) {
						l->visited = true;
						break;
					}
				}
			}
			if (l->visited) {
				if (l->rewind) {
					l->rewind->visited = true;
				}
			}
			if (l->terminal) {
				if (l->visited) {
					++(*found_words);
				} else {
					++(*missed_words);
				}
			}
		}
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
			assert(at->terminal == false);
			at->terminal = true;
		}
		std::cout << "Have " << symbols.size() << " symbols." << std::endl;

		//set up rewind pointers in tree, level-by-level:
		{
			std::vector< Level * > layer;
			layer.push_back(&root);
			root.rewind = NULL;
			root.depth = 0;
			root.safe = 0;
			while (!layer.empty()) {
				std::vector< Level * > next_layer;
				for (auto l : layer) {
					//update rewind pointers for children based on current rewind pointer.
					for (auto cl : *l) {
						next_layer.emplace_back(cl.second);
						//depth:
						cl.second->depth = l->depth + 1;

						//safe:
						if (cl.second->terminal) {
							cl.second->safe = cl.second->depth;
						} else {
							cl.second->safe = l->safe;
						}

						//rewind:
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

			//set up safe characters in tree:
		}

		//unroll tree for some stats:
		Counts counts;
		count_tree(counts, &root, 0);
		std::cout << "Depths:" << std::endl;
		for (uint32_t i = 0; i < counts.depth.size(); ++i) {
			std::cout << i << ": " << counts.depth[i] << std::endl;
		}
		std::cout << "Terminals: " << counts.terminals << std::endl;
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

	Level *at = &root;
	for (auto c : portmantout) {
		while (at != NULL) {
			at->visited = true;
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
	at->visited = true;

	stopwatch("test");

	{
		//TODO: count terminals that are marked 'visited':
		uint32_t found_words = 0;
		uint32_t missed_words = 0;
		count_visited(root, &found_words, &missed_words);
		std::cout << "Found " << found_words << " words." << std::endl;
		std::cout << "Missed " << missed_words << " words." << std::endl;
	}

	//dump_missing("", &root); //DEBUG

	stopwatch("test - eval");

	Counts counts;
	count_tree(counts, &root, 0);

	return 0;

}
