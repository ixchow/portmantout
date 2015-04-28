#include <string>
#include <iostream>
#include <fstream>
#include <cassert>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <algorithm>

class Unique;

class Node : public std::map< char, Node * > {
public:
	Node() : terminal(false), src('\0'), id(-1U), refs(0) { }
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
	uint32_t refs;
};


class Unique {
public:
	Unique() : fresh_id(0) { }
	uint32_t fresh_id;
	class NodeComp {
	public:
		bool operator()(Node *a, Node *b) {
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
		if (res.second == false) {
			delete n;
		} else {
			n->id = fresh_id++;
		}
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
	Params() : verbose(false), letter_map(NoMap), re_id(None), inline_single(false) { }
	bool verbose;
	enum {
		NoMap,
		ByFrequency
	} letter_map;
	enum {
		None,
		RefCount,
		Random
	} re_id;
	bool inline_single;
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
	return est_bits_helper(data);
}

double est_bits_delta(std::vector< uint32_t > const &data) {
	if (data.empty()) return 0.0;

	std::vector< uint32_t > deltas;
	for (auto d = data.begin(); d + 1 < data.end(); ++d) {
		deltas.push_back(*(d+1) - *d);
	}

	return est_bits_helper(deltas);
}

void report(const char *name, std::vector< uint32_t > const &data) {
	std::map< uint32_t, uint32_t > counts;
	for (auto d : data) {
		counts.insert(std::make_pair(d, 0)).first->second += 1;
	}
	printf("%-20s : %5d items, %4d unique -> %5.0f [%5.0f] bytes (%3.2f [%3.2f] bits per)\n",
		name,
		(int)data.size(),
		(int)counts.size(),
		std::ceil(est_bits(data) / 8.0),
		std::ceil(est_bits_delta(data) / 8.0),
		est_bits(data) / data.size(),
		est_bits_delta(data) / data.size()
		);
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

	Unique unique;
	root.dagify_children(unique);
	unique.unique('\0', &root);

	for (auto n : unique.store) {
		for (auto cn : *n) {
			cn.second->refs += 1;
		}
	}

	if (params.verbose) {
		std::cout << "   Unique from " << root.count() << " to " << unique.store.size() << std::endl;
		/*
		std::vector< uint32_t > hist;
		for (auto n : unique.store) {
			while (n->refs >= hist.size()) {
				hist.push_back(0);
			}
			hist[n->refs] += 1;
		}
		for (uint32_t i = 0; i < hist.size(); ++i) {
			if (hist[i] != 0) {
				std::cout << "[" << i << "] " << hist[i] << std::endl;
			}
		}*/
	}

	//let's re-id by reference frequency (not sure how useful, but worth a shot:
	if (params.re_id == Params::RefCount) {
		std::multimap< uint32_t, Node * > by_refs;
		for (auto n : unique.store) {
			by_refs.insert(std::make_pair(n->refs, n));
		}
		uint32_t fresh_id = 0;
		for (auto r = by_refs.rbegin(); r != by_refs.rend(); ++r) {
			r->second->id = fresh_id++;
		}
		assert(fresh_id == unique.fresh_id);
	} else if (params.re_id == Params::Random) {
		std::mt19937 mt(rand());
		std::vector< uint32_t > ids;
		ids.reserve(unique.fresh_id);
		for (uint32_t i = 0; i < unique.fresh_id; ++i) {
			ids.push_back(i);
		}
		for (uint32_t i = 0; i < ids.size(); ++i) {
			std::swap(ids[i], ids[i + (mt() % (ids.size() - i))]);
		}
		auto id = ids.begin();
		for (auto n : unique.store) {
			assert(id != ids.end());
			n->id = *id;
			++id;
		}
		assert(id == ids.end());
	} else {
		assert(params.re_id == Params::None);
	}

	std::vector< uint32_t > s_letters;
	std::vector< uint32_t > s_terminals;
	std::vector< uint32_t > s_child_counts;
	std::vector< uint32_t > s_child_counts_t;
	std::vector< uint32_t > s_first_letters;
	std::vector< uint32_t > s_first_letters_t;
	std::vector< uint32_t > s_letter_deltas;
	std::vector< uint32_t > s_letter_deltas_t;
	std::vector< uint32_t > s_id_counts;
	std::vector< uint32_t > s_id_counts_t;
	std::vector< uint32_t > s_first_ids;
	std::vector< uint32_t > s_first_ids_t;
	std::vector< uint32_t > s_id_deltas;
	std::vector< uint32_t > s_id_deltas_t;



	{ //store tree in fragments by refs
		std::vector< uint32_t > size_hist;

		//let's store by (descending) id:
		std::vector< std::pair< uint32_t, Node * > > by_id;
		for (auto n : unique.store) {
			by_id.emplace_back(n->id, n);
		}
		std::sort(by_id.rbegin(), by_id.rend());
		for (auto seed_ : by_id) {
			Node *seed = seed_.second;
			if (seed->refs == 1 || (params.inline_single && seed->size() == 0)) continue; //nope, don't need it

			if (seed->refs == 0) {
				//don't store letter for root
				assert(seed->src == '\0');
			} else {
				assert(seed->src != '\0');
				s_letters.push_back(seed->src);
			}

			uint32_t fragment_size = 0;

			std::deque< Node * > todo;
			todo.push_back(seed);
			while (!todo.empty()) {
				Node *n = todo.front();
				todo.pop_front();
				++fragment_size;

				std::vector< std::pair< char, Node * > > children;
				std::vector< uint32_t > ids;
				for (auto cn : *n) {
					assert(cn.second->refs >= 1);
					//store refs == 1 nodes or nodes with no children inline:
					if (cn.second->refs == 1 || (params.inline_single && cn.second->size() == 0)) {
						//will be stored inline
						children.emplace_back(cn.first, cn.second);
					} else {
						//will be stored by reference
						ids.push_back(cn.second->id);
					}
				}
				//store ids in order:
				std::sort(ids.begin(), ids.end());
				//store children in order as well (should already be sorted -- map):
				std::sort(children.begin(), children.end());

				if (!children.empty()) { //children
					if (n->terminal) {
						s_first_letters_t.push_back(children[0].first);
					} else {
						s_first_letters.push_back(children[0].first);
					}
					for (uint32_t i = 1; i < children.size(); ++i) {
						if (n->terminal) {
							s_letter_deltas_t.push_back(children[i].first - children[i-1].first);
						} else {
							s_letter_deltas.push_back(children[i].first - children[i-1].first);
						}
					}
				}
				if (!ids.empty()) {
					if (n->terminal) {
						s_first_ids_t.push_back(ids[0]);
					} else {
						s_first_ids.push_back(ids[0]);
					}
					for (uint32_t i = 1; i < ids.size(); ++i) {
						if (n->terminal) {
							s_id_deltas_t.push_back(ids[i] - ids[i-1]);
						} else {
							s_id_deltas.push_back(ids[i] - ids[i-1]);
						}
					}
				}

				/*if (children.empty() && ids.empty()) {
					assert(n->terminal);
					s_child_counts_t.push_back(children.size());
					s_id_counts_t.push_back(ids.size());
				} else {*/
					s_terminals.push_back(n->terminal ? 1 : 0);
					if (n->terminal) {
						s_child_counts_t.push_back(children.size());
						s_id_counts_t.push_back(ids.size());
					} else {
						s_child_counts.push_back(children.size());
						s_id_counts.push_back(ids.size());
					}
				//}

				for (auto c : children) {
					todo.push_back(c.second);
				}
			}

			while (fragment_size >= size_hist.size()) {
				size_hist.push_back(0);
			}
			size_hist[fragment_size] += 1;
		}


		if (params.verbose) {
			for (uint32_t i = 0; i < size_hist.size(); ++i) {
				if (size_hist[i] != 0) {
					std::cout << "|" << i << "| " << size_hist[i] << std::endl;
				}
			}
		}

	}

#define REPORT(V) report(#V, V)
	if (params.verbose) {
		REPORT(s_letters);
		REPORT(s_terminals);
		REPORT(s_child_counts);
		REPORT(s_child_counts_t);
		REPORT(s_first_letters);
		REPORT(s_first_letters_t);
		REPORT(s_letter_deltas);
		REPORT(s_letter_deltas_t);
		REPORT(s_id_counts);
		REPORT(s_id_counts_t);
		REPORT(s_first_ids);
		REPORT(s_first_ids_t);
		REPORT(s_id_deltas);
		REPORT(s_id_deltas_t);
	}
#undef REPORT
	double bits =
		std::min(est_bits(s_letters), est_bits_delta(s_letters))
		+ est_bits(s_terminals)
		+ est_bits(s_child_counts)
		+ est_bits(s_child_counts_t)
		+ est_bits(s_first_letters)
		+ est_bits(s_first_letters_t)
		+ est_bits(s_letter_deltas)
		+ est_bits(s_letter_deltas_t)
		+ est_bits(s_id_counts)
		+ est_bits(s_id_counts_t)
		+ est_bits(s_first_ids)
		+ est_bits(s_first_ids_t)
		+ est_bits(s_id_deltas);
		+ est_bits(s_id_deltas_t);
	std::cout << "Have " << std::ceil(bits / 8.0) << " bytes." << std::endl;
}



int main(int argc, char **argv) {
	srand(time(0));

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

#define SINGLE
#ifdef SINGLE
	params.verbose = true;
	params.letter_map = Params::ByFrequency;
	params.re_id = Params::RefCount;
	params.inline_single = false;
	compress(wordlist, params);
#else
	for (params.peel = 3; params.peel < 13; ++params.peel) {
		compress(wordlist, params);
	}
#endif


	return 0;

}
