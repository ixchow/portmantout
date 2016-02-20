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
#include "graph.hpp"


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

int main(int argc, char **argv) {

	Node root;
	Node *start = nullptr;

	stopwatch("start");

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

	stopwatch("read");

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

	Graph graph;
	graph.resize(nodes.size(), adjacencies);

	{
		auto adj_start = graph.adj_start;
		auto adj = graph.adj;
		auto adj_char = graph.adj_char;

		for (auto const &n : nodes) {
			assert(&n - &nodes[0] == n->index);
			*(adj_start++) = adj - graph.adj;
			std::vector< std::pair< char, Node * > > valid(n->begin(), n->end());
			if (n->terminal) {
				for (Node *r = n->rewind; r != &root; r = r->rewind) {
					assert(r);
					valid.insert(valid.end(), r->begin(), r->end());
				}
			}
			std::stable_sort(valid.begin(), valid.end());
			for (auto v : valid) {
				*(adj_char++) = v.first;
				*(adj++) = v.second->index;
			}
		}
		*(adj_start++) = adj - graph.adj;

		assert(adj == graph.adj + adjacencies);
		assert(adj_char == graph.adj_char + adjacencies);
		assert(adj_start == graph.adj_start + nodes.size() + 1);
	}

	{
		auto depth = graph.depth;
		auto maximal = graph.maximal;
		auto parent = graph.parent;

		for (uint32_t i = 0; i < nodes.size(); ++i) {
			assert(nodes[i]->index == i);
			*(depth++) = nodes[i]->depth;
			*(maximal++) = nodes[i]->maximal;
			if (nodes[i]->parent) {
				*(parent++) = nodes[i]->parent->index;
			} else {
				assert(i == 0);
				*(parent++) = -1U;
			}
		}
		assert(depth   == graph.depth + nodes.size());
		assert(maximal == graph.maximal + nodes.size());
		assert(parent  == graph.parent + nodes.size());
	}

	stopwatch("build");

	graph.write("wordlist.graph");

	stopwatch("write");

	return 0;
}
