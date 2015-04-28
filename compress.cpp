#include <string>
#include <iostream>
#include <fstream>
#include <cassert>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

class Unique;

class Node : public std::map< char, Node * > {
public:
	Node() : terminal(false), src('\0'), id(-1U) { }
	void add_string(std::string s) {
		if (s.empty()) {
			terminal = true;
			return;
		}
		auto f = find(s[0]);
		if (f == end()) f = insert(std::make_pair(s[0], new Node())).first;
		f->second->add_string(s.substr(1));
	}
	void dagify_children(Unique &u);
	uint32_t count() {
		uint32_t ret = 1;
		for (auto &c : *this) {
			ret += c.second->count();
		}
		return ret;
	}
	bool terminal;

	char src; //used in unique2 code
	uint32_t id; //used in unique code
};


class Unique {
public:
	Unique() : fresh_id(0) { }
	uint32_t fresh_id;
	class NodeComp {
	public:
		bool operator()(Node *a, Node *b) {
			assert(a->src != '\0' && b->src != '\0');
			if (a->src != b->src) return a->src < b->src;
			if (a->terminal != b->terminal) return a->terminal < b->terminal;
			if (a->size() != b->size()) return a->size() < b->size();
			auto ia = a->begin(); auto ib = b->begin();
			while (ia != a->end()) {
				assert(ib != b->end());

				if (*ia != *ib) return *ia < *ib;

				++ia;
				++ib;
			}
			assert(ib == b->end());
			return false;
		}
	};
	std::set< Node *, NodeComp > store;
	Node *unique(char c, Node *n) {
		assert(n->id == -1U);
		assert(n->src == '\0');
		n->src = c;
		auto res = store.insert(n);
		if (res.second == false) delete n;
		else n->id = fresh_id++;
		return *res.first;
	}
	uint32_t count() {
		return store.size();
	}
};


void Node::dagify_children(Unique &u) {
	for (auto &c : *this) {
		c.second->dagify_children(u);
		c.second = u.unique(c.first, c.second);
	}
}

class Params {
public:
	Params() : verbose(false), letter_map(ByFrequency), peel(-1U) { }
	bool verbose;
	enum {
		NoMap,
		ByFrequency
	} letter_map;
	uint32_t peel;
};

double est_bits_helper(std::vector< uint32_t > const &data) {
	std::map< uint32_t, uint32_t > counts;
	for (auto d : data) {
		counts.insert(std::make_pair(d, 0)).first->second += 1;
	}
	double bits = 0.0;
	for (auto c : counts) {
		bits += c.second * -std::log2(double(c.second) / data.size());
	}
	return bits;
}

double est_bits(std::vector< uint32_t > const &data) {
	double basic = est_bits_helper(data);
	if (data.empty()) return basic;

	std::vector< uint32_t > deltas;
	for (auto d = data.begin(); d + 1 < data.end(); ++d) {
		deltas.push_back(*(d+1) - *d);
	}

	double delta = est_bits_helper(deltas);

	if (delta < basic) {
		std::cout << "Delta was better." << std::endl;
		return delta;
	} else {
		return basic;
	}
}



void compress(std::vector< std::string > const &_wordlist, Params const &params) {
	std::vector< std::string > wordlist = _wordlist;

	//re-order letters by frequency:
	if (params.letter_map == Params::ByFrequency) {
		std::map< char, uint32_t > counts;

		for (auto word : wordlist) {
			for (auto c : word) {
				counts.insert(std::make_pair(c, 0)).first->second += 1;
			}
		}

		std::vector< std::pair< uint32_t, char > > by_count;
		for (auto c : counts) {
			by_count.emplace_back(c.second, c.first);
		}
		std::sort(by_count.rbegin(), by_count.rend());
		std::map< char, char > reorder;
		for (auto bc : by_count) {
			assert(!reorder.count(bc.second));
			reorder[bc.second] = 'a' + reorder.size();
		}
		for (auto &word : wordlist) {
			for (auto &c : word) {
				assert(reorder.count(c));
				c = reorder[c];
			}
		}
	}


	/*
	std::sort(wordlist.begin(), wordlist.end(), [](std::string const &a, std::string const &b){
		for (uint32_t i = 0; i < a.size() && i < b.size(); ++i) {
			if (a[i] != b[i]) return a[i] < b[i];
		}
		return a.size() > b.size(); //longer words first 
	});*/

	//Build a tree from the words:
	Node root;
	for (auto w : wordlist) {
		root.add_string(w);
	}

	//Store some of the tree straight-up:

	//each node is represented by:
	// a bit saying if it's terminal,
	// count of children,
	// and then letters of children (as deltas)
	std::vector< uint32_t > p1_terminals;
	std::vector< uint32_t > p1_child_counts;
	std::vector< uint32_t > p1_child_first_letters;
	std::vector< uint32_t > p1_child_letter_deltas;

	std::vector< Node * > remaining;
	remaining.emplace_back(&root);
	for (uint32_t level = 0; level < params.peel; ++level) {
		if (remaining.empty()) break;
		std::vector< Node * > todo = std::move(remaining);
		assert(remaining.empty());
		for (auto n : todo) {
			p1_terminals.push_back(n->terminal ? 1 : 0);
			p1_child_counts.push_back(n->size());
			char prev = 'a';
			bool first = true;
			for (auto cn : *n) {
				remaining.emplace_back(cn.second);
				if (first) {
					p1_child_first_letters.push_back(cn.first);
					first = false;
				} else {
					p1_child_letter_deltas.push_back(cn.first - prev);
				}
				prev = cn.first;
			}
		}
	}
	#define REPORT(V) \
		do { \
			printf("%-24s: %6.0f items in %6.0f bytes.\n", #V, double(V.size()), std::ceil(est_bits(V) / 8.0)); \
		} while (0)

	if (params.verbose) {
		REPORT(p1_terminals);
		REPORT(p1_child_counts);
		REPORT(p1_child_first_letters);
		REPORT(p1_child_letter_deltas);
	}


	Unique unique;

	uint32_t pre_count = 0;
	for (auto &n : remaining) {
		pre_count += n->count();
		n->dagify_children(unique);
		//n = unique.unique('!', n);
	}

	/*if (params.verbose) {
		std::cout << "   Unique from " << pre_count << " to " << unique.store.size() << std::endl;
	}*/

	//TODO: consider generating IDs via topological-sort-esque ordering

	std::vector< uint32_t > p2_terminals;
	std::vector< uint32_t > p2_child_counts;
	std::vector< uint32_t > p2_child_ids;
	std::vector< uint32_t > p2_child_firsts;
	std::vector< uint32_t > p2_child_deltas;

	//TODO: consider storing deltas
	for (auto n : remaining) {
		p2_terminals.push_back(n->terminal ? 1 : 0);
		p2_child_counts.push_back(n->size());
		std::vector< std::pair< uint32_t, Node * > > by_id;
		for (auto cn : *n) {
			by_id.emplace_back(cn.second->id, cn.second);
		}
		std::sort(by_id.begin(), by_id.end());

		bool first = true;
		uint32_t prev = 0;
		for (auto cn : by_id) {
			assert(cn.second->id < unique.store.size());
			if (first) {
				p2_child_firsts.push_back(cn.second->id);
				first = false;
			} else {
				p2_child_deltas.push_back(cn.second->id - prev);
			}
			p2_child_ids.push_back(cn.second->id);
			prev = cn.second->id;
		}
	}

	if (params.verbose) {
		REPORT(p2_terminals);
		REPORT(p2_child_counts);
		REPORT(p2_child_ids);
		REPORT(p2_child_firsts);
		REPORT(p2_child_deltas);
	}


	//In this phase, store in order by id:
	//  - each node gets its source letter
	//  - if it's a terminal
	//  - child count
	//  - child ids
	std::vector< uint32_t > p3_letters;
	std::vector< uint32_t > p3_terminals;
	std::vector< uint32_t > p3_child_counts;
	std::vector< uint32_t > p3_child_firsts;
	std::vector< uint32_t > p3_child_deltas;


	std::vector< std::pair< uint32_t, Node * > > by_id;
	for (auto n : unique.store) {
		assert(n->id != -1U);
		by_id.emplace_back(n->id, n);
	}
	std::sort(by_id.rbegin(), by_id.rend()); //decreasing order for some reason
	assert(by_id.empty() || by_id.size() == by_id[0].second->id + 1);


	for (auto n_id : by_id) {
		Node *n = n_id.second;
		p3_letters.push_back(n->src);
		p3_terminals.push_back(n->terminal ? 1 : 0);
		p3_child_counts.push_back(n->size());
			
		std::vector< std::pair< uint32_t, std::pair< char, Node * > > > c_by_id;
		for (auto c : *n) {
			c_by_id.emplace_back(c.second->id, c);
		}
		std::sort(c_by_id.begin(), c_by_id.end()); //increasing

		bool first = true;
		uint32_t prev = 0;
		for (auto c_id : c_by_id) {
			auto c = c_id.second;
			assert(c.second->id >= prev);
			if (first) {
				p3_child_firsts.push_back(c.second->id - n->id);
				first = false;
			} else {
				p3_child_deltas.push_back(c.second->id - prev);
			}
			prev = c.second->id;
		}
	}

	if (params.verbose) {
		REPORT(p3_letters);
		REPORT(p3_terminals);
		REPORT(p3_child_counts);
		REPORT(p3_child_firsts);
		REPORT(p3_child_deltas);
	}

	const char *flag = "???";
	if (params.letter_map == Params::ByFrequency) {
		flag = "[f]";
	} else if (params.letter_map == Params::NoMap) {
		flag = "[ ]";
	}

	double p1_bits =
		est_bits(p1_terminals)
		+ est_bits(p1_child_counts)
		+ est_bits(p1_child_first_letters)
		+ est_bits(p1_child_letter_deltas)
		;

	double p2_bits =
		est_bits(p2_terminals)
		+ est_bits(p2_child_counts)
		+ std::min(
			est_bits(p2_child_firsts) + est_bits(p2_child_deltas),
			est_bits(p2_child_ids)
		);

	double p3_bits =
		est_bits(p3_terminals)
		+ est_bits(p3_letters)
		+ est_bits(p3_child_counts)
		+ est_bits(p3_child_firsts)
		+ est_bits(p3_child_deltas)
		;

	printf("%s %2d --> %6.0f == %6.0f + %6.0f + %6.0f\n", flag, params.peel,
		std::ceil((p1_bits + p2_bits + p3_bits) / 8.0),
		std::ceil(p1_bits / 8.0),
		std::ceil(p2_bits / 8.0),
		std::ceil(p3_bits / 8.0) );

}



int main(int argc, char **argv) {

	std::vector< std::string > wordlist;
	{ //load:
		std::ifstream file("wordlist.asc");
		std::string word;
		while (std::getline(file, word)) {
			wordlist.emplace_back(word);
		}
	}
	std::cout << "Have " << wordlist.size() << " words." << std::endl;

	Params params;

#ifdef SINGLE
	params.verbose = true;
	params.peel = 5;
	compress(wordlist, params);
#else
	for (params.peel = 3; params.peel < 13; ++params.peel) {
		compress(wordlist, params);
	}
#endif


	return 0;

}
