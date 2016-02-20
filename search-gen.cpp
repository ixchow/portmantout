#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <cassert>
#include <vector>
#include <deque>
#include <functional>
#include <random>
#include <algorithm>
#include <chrono>
#include "stopwatch.hpp"


class Node : public std::map< char, Node * > {
public:
	Node *rewind = nullptr;
	uint32_t index = -1U;
	bool terminal = false;
	bool maximal = false; //terminal that isn't a substring

	uint32_t depth = 0;

	//------ DEBUG ------
	Node *parent = nullptr;
	std::string prefix() const {
		if (!parent) return "";
		for (auto cn : *parent) {
			if (cn.second == this) return parent->prefix() + cn.first;
		}
		assert(0 && "parent pointer should be correct, y'know.");
	}
};


void build_tree() {
	Node root;
	Node *start = nullptr;

	//initial tree:
	std::ifstream wordlist("wordlist.asc");
	std::string word;
	while (std::getline(wordlist, word)) {
		Node *at = &root;
		for (auto c : word) {
			auto f = at->insert(std::make_pair(c, nullptr)).first;
			if (f->second == NULL) {
				f->second = new Node();
				f->second->parent = at;
				f->second->depth = f->second->parent->depth + 1;
			}
			at = f->second;
		}
		assert(at);
		assert(at->terminal == false);
		at->terminal = true;
		if (word == "portmanteau") {
			start = at;
		}
	}
	assert(start);

	//set rewind pointers:
	{
		std::vector< Node * > layer;
		layer.push_back(&root);
		while (!layer.empty()) {
			std::vector< Node * > next_layer;
			for (auto n : layer) {
				//update rewind pointers for children based on current rewind pointer.
				for (auto cn : *n) {
					next_layer.emplace_back(cn.second);

					//rewind:
					Node *r = n->rewind;
					while (r != nullptr) {
						assert(r != nullptr);
						auto f = r->find(cn.first);
						if (f != r->end()) {
							//great, can extend this rewind:
							r = f->second;
							break;
						} else {
							//have to rewind further:
							r = r->rewind;
						}
					}
					assert(n == &root || r != nullptr); //for everything but the root, rewind should always hit root and bounce down
					if (r == nullptr) r = &root;
					cn.second->rewind = r;
				}
			}
			layer = next_layer;
		}
	}

	{ //dump some info about the tree:
		uint32_t nodes = 0;
		uint32_t edges = 0;
		uint32_t rewinds = 0;
		uint32_t terminal = 0;
		uint32_t steps = 0;
		std::function< void(Node *) > count = [&](Node *n) {
			nodes += 1;
			if (n->rewind) rewinds += 1;
			if (n->terminal) terminal += 1;
			for (auto &c : *n) {
				steps += 1;
				edges += 1;
				if (c.first != '\0') {
					count(c.second);
				}
			}
			//add valid next steps from rewind pointers:
			if (n->terminal) {
				for (Node *r = n->rewind; r != &root; r = r->rewind) {
					assert(r);
					steps += r->size();
				}
			}
		};
		count(&root);
		std::cout << "Built tree with " << nodes << " nodes and " << edges << " edges." << std::endl;
		std::cout << "Have " << terminal << " terminal nodes." << std::endl;
		std::cout << "Have " << rewinds << " rewind pointers." << std::endl;
		std::cout << "Have " << steps << " 'next step' edges (includes rewinds at terminals)." << std::endl;
	}

	//set indices:
	std::vector< Node * > nodes;
	uint32_t adjacencies = 0;
	{
		std::function< void(Node *) > index = [&](Node *n) {
			n->index = nodes.size();
			nodes.emplace_back(n);

			adjacencies += n->size();
			//add valid next steps from rewind pointers:
			if (n->terminal) {
				for (Node *r = n->rewind; r != &root; r = r->rewind) {
					assert(r);
					adjacencies += r->size();
				}
			}
			for (auto cn : *n) {
				index(cn.second);
			}
		};
		index(&root);
	}

	for (auto n : nodes) {
		if (n->terminal && n->size() == 0) n->maximal = true;
	}
	{
		uint32_t maximal = 0;
		for (auto n : nodes) {
			if (n->maximal) ++maximal;
		}
		std::cout << "Have " << maximal << " maximal words based on child counting." << std::endl;
	}
	for (auto n : nodes) {
		if (n->rewind) n->rewind->maximal = false;
	}
	{
		uint32_t maximal = 0;
		for (auto n : nodes) {
			if (n->maximal) ++maximal;
		}
		std::cout << "Have " << maximal << " maximal words after rewind culling." << std::endl;
	}

	//okay, a valid step is:
	// - (at any node) a marked next letter for this node
	// - (at a terminal node) a marked next letter for some [non-root!] rewind of this node

	//For searches and stuff, probably should compress and linearize tree.
	//method: straight-up adjacency list w/ valid edges unwound.
	// --> avoids storing anything else but edge info
	// --> can we compress to 16 bits per adj [using signed ints]

	std::vector< uint32_t > adj_start;
	std::vector< uint32_t > adj;
	std::vector< char > adj_char;


	adj_start.reserve(nodes.size() + 1);
	adj.reserve(adjacencies);
	adj_char.reserve(adjacencies);

	for (auto const &n : nodes) {
		assert(&n - &nodes[0] == n->index);
		adj_start.emplace_back(adj.size());
		std::vector< std::pair< char, Node * > > valid(n->begin(), n->end());
		if (n->terminal) {
			for (Node *r = n->rewind; r != &root; r = r->rewind) {
				assert(r);
				valid.insert(valid.end(), r->begin(), r->end());
			}
		}
		std::stable_sort(valid.begin(), valid.end());
		for (auto v : valid) {
			adj.emplace_back(v.second->index);
			adj_char.emplace_back(v.first);
		}
	}
	adj_start.emplace_back(adj.size());

	assert(adj.size() == adjacencies);
	assert(adj_char.size() == adjacencies);
	assert(adj_start.size() == nodes.size() + 1);

	/* possible optimization
	std::vector< uint32_t > inv_adj_start;
	std::vector< uint32_t > inv_adj;
	std::vector< char > inv_adj_char;
	{ //invert edges (for reading back bfs paths)
	}
	*/

	std::cout << "Built " << adj.size() << "-entry adjacency list." << std::endl;

	std::vector< uint8_t > depth;
	depth.reserve(nodes.size());
	std::vector< bool > wanted;
	wanted.reserve(nodes.size());
	std::vector< bool > terminal;
	terminal.reserve(nodes.size());
	for (uint32_t i = 0; i < nodes.size(); ++i) {
		assert(nodes[i]->index == i);
		depth.emplace_back(nodes[i]->depth);
		terminal.emplace_back(nodes[i]->terminal);
		wanted.emplace_back(nodes[i]->maximal);
	}
	assert(terminal.size() == nodes.size());
	assert(wanted.size() == nodes.size());

/*
	stopwatch("before bfs");
	uint32_t max_terminal_distance = 0;
	for (uint32_t seed = 0; seed < nodes.size(); ++seed) {
		if (!(seed == 0 || wanted[seed])) continue;
		std::vector< bool > visited(nodes.size(), false);
		std::vector< uint8_t > distance(nodes.size(), 0xff);
		std::vector< uint32_t > ply;
		ply.emplace_back(seed);
		visited[seed] = true;
		uint32_t dis = 0;
		while (!ply.empty()) {
			std::vector< uint32_t > next_ply;
			//next_ply.reserve(nodes.size()); //maybe?
			for (auto i : ply) {
				distance[i] = dis;
				if (terminal[i]) {
					max_terminal_distance = std::max(max_terminal_distance, dis);
				}
				for (uint32_t a = adj_start[i]; a < adj_start[i+1]; ++a) {
					uint32_t n = adj[a];
					if (!visited[n]) {
						visited[n] = true;
						next_ply.emplace_back(n);
					}
				}
			}
			ply = std::move(next_ply);
			dis += 1;
		}
		static uint32_t step = 0;
		step += 1;
		if (step == 500) {
			stopwatch("bfs * 500");
			std::cout << seed << " / " << nodes.size()
				<< " -- longest path so far: " << max_terminal_distance << "."
				<< std::endl;
			step = 0;
			exit(0); //DEBUG
		}
	}
	stopwatch("bfs");
*/

	//To try: various noodlings with what it means to find a good path, I guess.

	//One way to find a path is to randomly pick among the shortest paths from the current location to next things.

	class Path {
	public:
		uint32_t at = 0;
		std::string so_far = "";
		std::vector< bool > wanted;
		uint32_t wanted_remain = -1U;
	};

	auto randomly_extend = [&](Path const &path, Path &into) {
		assert(path.at + 1 < adj_start.size());
		assert(path.wanted_remain > 0);
		assert(!path.wanted[path.at]);

		std::vector< uint32_t > from(nodes.size(), -1U);
		std::vector< uint8_t > sum(nodes.size(), 0);
		std::vector< uint8_t > length(nodes.size(), 0xff);
		std::vector< uint32_t > ply;

		ply.emplace_back(path.at);
		from[path.at] = path.at;
		sum[path.at] = 0;
		length[path.at] = 0;
		uint32_t len = 0;
		while (!ply.empty()) {
			std::vector< uint32_t > next_ply;
			for (auto i : ply) {
				length[i] = len;
				if (path.wanted[i]) {
					assert(sum[i] + depth[i] < 255);
					sum[i] += depth[i];
				}
				for (uint32_t a = adj_start[i]; a < adj_start[i+1]; ++a) {
					uint32_t n = adj[a];
					if (from[n] == -1U) {
						from[n] = i;
						sum[n] = sum[i];
						next_ply.emplace_back(n);
					}
				}
			}
			ply = std::move(next_ply);
			len += 1;
		}

		{ //DEBUG:
			/*std::cout << "At: " << nodes[path.at]->prefix() << std::endl;
			{
				if (nodes[path.at]->terminal) {
					Node *r = nodes[path.at]->rewind;
					while (r != &root) {
						std::cout << "  rewinds to " << r->prefix() << " (" << r->index << ") { ";
						for (auto cn : *r) {
							std::cout << cn.first << " ";
						}
						std::cout << "}" << std::endl;
						r = r->rewind;
					}
				} else {
					std::cout << "  (not terminal)" << std::endl;
				}
			}*/

			bool unreach = false;

			for (uint32_t i = 0; i < nodes.size(); ++i) {
				if (from[i] == -1U) {
					if (nodes[i]->maximal) {
						unreach = true;
						std::cout << "Can't reach '" << nodes[i]->prefix() << "' (" << i << ") from '" << nodes[path.at]->prefix() << "'" << std::endl;
					}
				}
			}

			assert(!unreach);

		}

		uint32_t selected;
		{
			int32_t best_savings = std::numeric_limits< int32_t >::min();
			std::vector< uint32_t > best;
			for (uint32_t i = 0; i < nodes.size(); ++i) {
				if (path.wanted[i]) {
					assert(from[i] != -1U); //everything is reachable
	
					int32_t savings = int32_t(sum[i]) - int32_t(length[i]);
					if (savings > best_savings) {
						best.clear();
						best_savings = savings;
					}
					if (savings == best_savings) {
						best.emplace_back(i);
					}
				}
			}

			assert(!best.empty());

			static std::mt19937 mt(std::chrono::high_resolution_clock::now().time_since_epoch().count());
			selected = best[mt() % best.size()];
		}
/*
		assert(!possible.empty());

		static std::mt19937 mt(0xfeed1007);
		float sample = mt() / float(mt.max()) * possible_sum;

		uint32_t selected = -1U;
		for (auto const &p : possible) {
			sample -= p.first;
			if (sample <= 0.0f) {
				selected = p.second;
			}
		}
		if (selected >= nodes.size()) {
			std::cout << "Failed to sample properly; have " << sample << " leftover." << std::endl;
			selected = possible.back().second;
		}
		*/

		assert(selected < nodes.size());
		assert(path.wanted[selected]);

		//read back:
		into = path;
		std::vector< uint32_t > list;
		uint32_t at = selected;
		while (from[at] != at) {
			list.emplace_back(at);
			at = from[at];
			assert(at < from.size());
		}
		assert(at == path.at);
		std::reverse(list.begin(), list.end());

		//move 'into' along path:
		for (auto n : list) {
			bool found = false;
			for (uint32_t a = adj_start[into.at]; a < adj_start[into.at+1]; ++a) {
				if (adj[a] == n) {
					found = true;
					into.so_far += adj_char[a];
					into.at = n;
					if (into.wanted[n]) {
						into.wanted[n] = false;
						assert(into.wanted_remain > 0);
						into.wanted_remain -= 1;
					}
					break;
				}
			}
			assert(found);
		}

	};

	stopwatch("step init");
	std::vector< Path > paths;
	{
		Path origin;
		origin.wanted = wanted;

		origin.so_far = start->prefix();
		origin.at = start->index;
		origin.wanted[start->index] = false; //not needed, it turns out

		origin.wanted_remain = 0;
		for (auto w : origin.wanted) {
			if (w) origin.wanted_remain += 1;
		}
		while (origin.wanted_remain) {
			Path temp;
			randomly_extend(origin, temp);
			origin = temp;

			static uint32_t step = 0;
			step += 1;
			if (step == 500) {
				stopwatch("step * 500");
				std::cout << origin.wanted_remain << " after " << origin.so_far.size() << std::endl;
				step = 0;
			}

		}
		std::string filename = "ix-" + std::to_string(origin.so_far.size()) + ".txt";
		std::ofstream out(filename);
		out << origin.so_far;
		std::cout << "Wrote " << filename << "." << std::endl;
/*
		paths.resize(100);
		for (auto const &p : paths) {
			randomly_extend(origin, p);
		}
	*/
	}
}

void build_joins() {
	std::vector< std::string > words;
	{
		std::ifstream wordlist("wordlist.asc");
		std::string word;
		while (std::getline(wordlist, word)) {
			words.emplace_back(std::move(word));
		}
	}

	{
		uint32_t maximal = 0;
		uint32_t i = 0;
		for (auto const &a : words) {
			++i;
			if (i % 1000 == 0) {
				std::cout << i << " / " << words.size() << std::endl;
			}
			bool found = false;
			for (auto const &b : words) {
				if (a == b) continue;
				if (b.find(a) != std::string::npos) {
					found = true;
					break;
				}
			}
			if (!found) {
				++maximal;
			}
		}
		std::cout << "Simple tests suggest " << maximal << " maximal words." << std::endl;
		//n.b. Simple tests suggest 64389 maximal words.
	}

	//portmanteau can be thought of as a series of word end -> word end moves.
	//enumerate those moves:
	uint32_t edges = 0;
	uint32_t i = 0;
	for (auto const &at : words) {
		++i;
		std::cout << i << " / " << words.size() << std::endl;
		for (auto const &next : words) {
			for (uint32_t over = 1; over <= at.size() && over <= next.size(); ++over) {
				if (at.substr(at.size()-over) == next.substr(0, over)) {
					++edges;
				}
			}
		}
	}

	std::cout << "Found " << edges << " joints among " << words.size() << " words." << std::endl;
	//n.b. Found 785312434 joints among 108709 words.
}


int main(int argc, char **argv) {

	stopwatch("start");
	build_tree();
	stopwatch("build tree");
	//build_joins(); //really slow!
	//stopwatch("build joins");

	return 0;
}
