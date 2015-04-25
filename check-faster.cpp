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
	Level() : length(0), depth(0), rewind(NULL), visited(false) { }
	//set during initial build:
	uint32_t length; //longest word that is a suffix of this context
	uint32_t depth; //how long is the prefix?

	bool is_terminal() const {
		return length > 0 && length == depth;
	}

	//set during second build phase:
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
	std::vector< uint32_t > children;
};

void count_tree(Counts &counts, Level *level, uint32_t depth) {
	while (counts.depth.size() <= depth) {
		counts.depth.push_back(0);
		counts.children.push_back(0);
	}
	assert(counts.depth.size() == counts.children.size());
	counts.depth[depth] += 1;
	counts.children[depth] += level->size();

	if (level->is_terminal()) {
		counts.terminals += 1;
	}

	counts.nodes += 1;
	for (auto c : *level) {
		count_tree(counts, c.second, depth + 1);
	}
}

void dump_missing(std::string const &prefix, Level *level) {
	if (level->is_terminal()) {
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
			if (l->is_terminal()) {
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
		std::ifstream wordlist("wordlist.asc");
		std::string word;
		while (std::getline(wordlist, word)) {
			Level *at = &root;
			for (auto c : word) {
				auto f = at->insert(std::make_pair(c, nullptr)).first;
				if (f->second == NULL) {
					f->second = new Level();
					f->second->depth = at->depth + 1;
				}
				at = f->second;
			}
			assert(at);
			assert(at->is_terminal() == false);
			at->length = word.size();
			assert(at->is_terminal() == true);
		}

		//set up rewind pointers in tree, level-by-level:
		{
			std::vector< Level * > layer;
			layer.push_back(&root);
			while (!layer.empty()) {
				std::vector< Level * > next_layer;
				for (auto l : layer) {
					//update rewind pointers for children based on current rewind pointer.
					for (auto cl : *l) {
						next_layer.emplace_back(cl.second);

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

						//length:
						// (a) length is already set to depth ['cause this is a terminal]
						// (b) length can be set based on rewind ['cause if there is a word in the current context, rewind certainly is at least that long]
						cl.second->length = std::max(cl.second->length, cl.second->rewind->length);
					}
				}
				layer = next_layer;
			}
		}
#ifdef STATS
		//unroll tree for some stats:
		Counts counts;
		count_tree(counts, &root, 0);
		std::cout << "Depths:" << std::endl;
		for (uint32_t i = 0; i < counts.depth.size(); ++i) {
			std::cout << i << ": " << counts.depth[i] << " avg children: " << double(counts.children[i]) / counts.depth[i] << std::endl;
		}
		std::cout << "Terminals: " << counts.terminals << std::endl;
		std::cout << counts.nodes << " nodes." << std::endl;
#endif //STATS
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

	//for each character in the current context, how many (including this one) remain in a word?
	std::deque< uint32_t > lengths;
	uint32_t count = 0;

	uint32_t uncovered_characters = 0;
	uint32_t uncovered_transitions = 0;
	uint32_t missing_characters = 0;

	std::string ctx = "";

	for (auto iter = portmantout.begin(); iter <= portmantout.end(); ++iter) {
		char c = (iter == portmantout.end() ? '\0' : *iter);

		while (1) {
			assert(at != NULL);
			at->visited = true;

			//std::cout << count << " / " << safe << " / " << at->depth << ": " << ctx << std::endl;
			assert(ctx.size() == at->depth);

			auto f = at->find(c);
			if (f != at->end()) {
				at = f->second;
				lengths.push_back(0);
				if (at->length > 0) {
					assert(at->length <= lengths.size());
					uint32_t &l = lengths[lengths.size() - at->length];
					assert(at->length > l);
					l = at->length;
				}
				ctx += c;
				break;
			} else if (at->rewind) {
				//not at the root, so move up by dropping characters:
				uint32_t drop = at->depth - at->rewind->depth;
				at = at->rewind;

				for (uint32_t i = 0; i < drop; ++i) {
					assert(!lengths.empty());
					if (lengths[0] == 0) {
						//std::cout << "Dropping uncovered character." << std::endl;
						++uncovered_characters;
						++uncovered_transitions; //because transition from uncovered is clearly uncovered
					} else if (lengths[0] == 1) {
						//std::cout << "Found uncovered transition." << std::endl;
						++uncovered_transitions;
					} else {
						assert(lengths.size() >= 2);
						lengths[1] = std::max(lengths[1], lengths[0] - 1);
					}
					lengths.pop_front();
					++count;
				}

				ctx.erase(0, drop);
			} else {
				//at the root, so evict character:
				assert(ctx == "");
				assert(lengths.size() == 0);
				//std::cout << "Character not found: " << (int)c << "." << std::endl;
				missing_characters += 1;
				uncovered_characters += 1;
				count += 1;
				break;
			}
		}
	}

	//always have one of each of these because dropping the last character
	// (assuming non-null portmantout)
	assert(missing_characters > 0 && uncovered_characters > 0 && uncovered_transitions > 0);
	assert(count == portmantout.size() + 1);

	missing_characters -= 1;
	uncovered_characters -= 1;
	uncovered_transitions -= 1;

	std::cout << "Uncovered characters: " << uncovered_characters << std::endl;
	std::cout << "Uncovered transitions: " << uncovered_transitions << std::endl;
	std::cout << "Missing characters: " << missing_characters << std::endl;


	{
		uint32_t found_words = 0;
		uint32_t missed_words = 0;
		count_visited(root, &found_words, &missed_words);
		std::cout << "Found " << found_words << " words." << std::endl;
		std::cout << "Missed " << missed_words << " words." << std::endl;
	}

	//dump_missing("", &root); //DEBUG

	stopwatch("test");

	return 0;

}
