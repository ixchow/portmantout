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
	Level() : length(0), depth(0), rewind(NULL), index(-1U), visited(false) { }
	//set during initial build:
	uint32_t length; //longest word that is a suffix of this context
	uint32_t depth; //how long is the prefix?

	bool is_terminal() const {
		return length > 0 && length == depth;
	}

	//set during second build phase:
	Level *rewind;
	uint32_t index;

	//to be removed at some point:
	bool visited;
};


//Want to pack:
// [char] [length] [depth] [first child] [child count]
// some options:

// (a) node looks like:
// [length] [depth] [child count] [rewind] (5 / 5 / 5 / 17 -- doesn't quite work, I think?)
// [rewind] (always negative offset?)
// [char offset] ... [char offset] (32 bits / child)

// (MOD: child offset relative to some base idx is at most
//      ~26 * ([2? 1?] + 26)-ish so could probably pack into 16-bit gloms )

// (b) node looks like:
//  (store children in contiguous block elsewhere)
// [char] [length] [depth] [rewind] (64 bits, probably?)
// [first child] [last child]

// At a first cut, let's do option (a) because it seems like it would touch less memory.


struct CompLevel {
	uint16_t length; uint16_t depth : 15; bool visited : 1;
	uint8_t child_count; uint32_t rewind : 24;
};
static_assert(sizeof(CompLevel) == 8, "CompLevel is two ints.");
struct CompChild {
	uint32_t index : 24;
	char c;
};
static_assert(sizeof(CompChild) == 4, "CompChild is one int.");

std::vector< uint32_t > compressed;

void count_visited(uint32_t *found_words, uint32_t *missed_words) {
	//propagate visited information up the strata:
	std::vector< std::vector< uint32_t > > strata;
	strata.emplace_back(1, 0);
	while (!strata.back().empty()) {
		std::vector< uint32_t > next;
		for (auto at_idx : strata.back()) {
			CompLevel *at = reinterpret_cast< CompLevel * >(&compressed[at_idx]);

			auto at_begin = &compressed[at_idx+2];
			auto at_end = &compressed[at_idx+2+at->child_count];

			for (auto c = at_begin; c != at_end; ++c) {
				next.push_back(reinterpret_cast< CompChild * >(c)->index);
			}
		}
		strata.emplace_back(std::move(next));
	}

	for (auto s = strata.rbegin(); s != strata.rend(); ++s) {
		for (auto l_idx : *s) {
			CompLevel *l = reinterpret_cast< CompLevel * >(&compressed[l_idx]);
			if (!l->visited) {
				auto at_begin = &compressed[l_idx+2];
				auto at_end = &compressed[l_idx+2+l->child_count];

				for (auto c = at_begin; c != at_end; ++c) {
					CompLevel *cl = reinterpret_cast< CompLevel * >(&compressed[reinterpret_cast< CompChild * >(c)->index]);
					if (cl->visited) {
						l->visited = true;
					}
				}
			}

			if (l->visited) {
				if (l->rewind < compressed.size()) {
					reinterpret_cast< CompLevel * >(&compressed[l->rewind])->visited = true;
				}
			}
			if (l->length > 0 && l->depth == l->length) {
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

	{
		Level root;
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
		
		//store tree into compressed:
		{
			//allocate indices (depth-first):
			uint32_t next_index = 0;
			std::deque< Level * > todo;
			todo.push_back(&root);
			while (!todo.empty()) {
#define DFS
#ifdef DFS
				Level *l = todo.back(); todo.pop_back();
				for (auto cl = l->rbegin(); cl != l->rend(); ++cl) {
#else
				Level *l = todo.front(); todo.pop_front();
				for (auto cl = l->begin(); cl != l->end(); ++cl) {
#endif
					todo.push_back(cl->second);
				}
				l->index = next_index;
				next_index += 2; //64 bits of header
				next_index += l->size(); //32-bits per child
			}
			std::cout << "Need " << next_index << " 32-bit storage locations for tree." << std::endl;
			compressed.resize(next_index, 0);
		}
		{ //actually store data:
			std::deque< Level * > todo;
			todo.push_back(&root);
			while (!todo.empty()) {
#ifdef DFS
				Level *l = todo.back(); todo.pop_back();
				for (auto cl = l->rbegin(); cl != l->rend(); ++cl) {
#else
				Level *l = todo.front(); todo.pop_front();
				for (auto cl = l->begin(); cl != l->end(); ++cl) {
#endif
					todo.push_back(cl->second);
				}
				assert(l->index + 1 < compressed.size());
				CompLevel *comp = reinterpret_cast< CompLevel * >(&compressed[l->index]);
				comp->length = l->length;
				comp->depth = l->depth;
				comp->visited = false;
				comp->child_count = l->size();
				if (l->rewind) {
					assert(l->rewind->index < 0xffffff);
					comp->rewind = l->rewind->index;
				} else {
					comp->rewind = 0xffffff;
				}
				uint32_t i = 0;
				for (auto ci = l->begin(); ci != l->end(); ++ci) {
					assert(l->index + 2 + i < compressed.size());
					CompChild *c = reinterpret_cast< CompChild * >(&compressed[l->index + 2 + i]);
					c->c = ci->first;
					assert(ci->second->index <= 0xffffff);
					c->index = ci->second->index;
					assert(uint32_t(c->c) == *reinterpret_cast< uint32_t * >(c) >> 24);
					++i;
				}
			}
		}
	}

	stopwatch("build");

	std::string portmantout;
	if (!std::getline(std::cin, portmantout) || portmantout.size() == 0) {
		std::cerr << "Please pass a portmantout on stdin." << std::endl;
		return 1;
	}

	stopwatch("read");

	std::cout << "Testing portmantout of " << portmantout.size() << " letters." << std::endl;

	uint32_t at_idx = 0;

	//for each character in the current context, how many (including this one) remain in a word?
	std::deque< uint32_t > lengths;
	uint32_t count = 0;

	uint32_t uncovered_characters = 0;
	uint32_t uncovered_transitions = 0;
	uint32_t missing_characters = 0;

	for (auto iter = portmantout.begin(); iter <= portmantout.end(); ++iter) {
		char c = (iter == portmantout.end() ? '\0' : *iter);

		while (1) {
			assert(at_idx + 1 < compressed.size());
			CompLevel *at = reinterpret_cast< CompLevel * >(&compressed[at_idx]);
			at->visited = true;


			auto at_begin = &compressed[at_idx+2];
			auto at_end = &compressed[at_idx+2+at->child_count];
			auto f = std::lower_bound(at_begin, at_end, uint32_t(c) << 24);
			if (f != at_end && (*f & 0xff000000) != uint32_t(c) << 24) f = at_end;

			if (f != at_end) {
				at_idx = reinterpret_cast< CompChild * >(f)->index;
				at = reinterpret_cast< CompLevel * >(&compressed[at_idx]);

				lengths.push_back(0);
				if (at->length > 0) {
					assert(at->length <= lengths.size());
					uint32_t &l = lengths[lengths.size() - at->length];
					assert(at->length > l);
					l = at->length;
				}
				break;
			} else if (at->rewind < compressed.size()) {
				CompLevel *rw = reinterpret_cast< CompLevel * >(&compressed[at->rewind]);
				
				//not at the root, so move up by dropping characters:
				uint32_t drop = at->depth - rw->depth;
				at_idx = at->rewind;
				at = rw;

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

			} else {
				//at the root, so evict character:
				assert(lengths.size() == 0);
				//std::cout << "Character not found: " << (int)c << "." << std::endl;
				missing_characters += 1;
				uncovered_characters += 1;
				count += 1;
				break;
			}
		}
	}
	assert(count == portmantout.size() + 1);

	//always have one of each of these because dropping the last character
	// (assuming non-null portmantout)
	assert(missing_characters > 0 && uncovered_characters > 0 && uncovered_transitions > 0);

	missing_characters -= 1;
	uncovered_characters -= 1;
	uncovered_transitions -= 1;

	std::cout << "Uncovered characters: " << uncovered_characters << std::endl;
	std::cout << "Uncovered transitions: " << uncovered_transitions << std::endl;
	std::cout << "Missing characters: " << missing_characters << std::endl;

	{
		uint32_t found_words = 0;
		uint32_t missed_words = 0;
		count_visited(&found_words, &missed_words);
		std::cout << "Found " << found_words << " words." << std::endl;
		std::cout << "Missed " << missed_words << " words." << std::endl;
	}

	//dump_missing("", &root); //DEBUG

	stopwatch("test");

	return 0;

}
