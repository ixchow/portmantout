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
	Node() : id(-1U) { }
	void add_string(std::string s) {
		if (s.empty()) return;
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
	uint32_t id; //used in unique code
};


class Unique {
public:
	Unique() : fresh_id(0) { }
	uint32_t fresh_id;
	class NodeComp {
	public:
		bool operator()(Node *a, Node *b) {
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
	Node *unique(Node *n) {
		assert(n->id == -1U);
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
		c.second = u.unique(c.second);
	}
}


int main(int argc, char **argv) {

	//Attempt to represent wordlist.asc in as little space as possible:
	std::vector< std::string > wordlist;
	{ //load:
		std::ifstream file("wordlist.asc");
		std::string word;
		std::map< char, uint32_t > counts;
		while (std::getline(file, word)) {
			wordlist.emplace_back(word);
			for (auto c : word) {
				counts.insert(std::make_pair(c, 0)).first->second += 1;
			}
		}


		//re-order by frequency:
		std::vector< std::pair< uint32_t, char > > by_count;
		for (auto c : counts) {
			by_count.emplace_back(c.second, c.first);
		}
		assert(by_count.size() == 26);
		std::sort(by_count.rbegin(), by_count.rend());
		std::map< char, char > reorder;
		for (auto bc : by_count) {
			reorder[bc.second] = 'a' + reorder.size();
		}
		assert(reorder.size() == 26);
		for (auto &word : wordlist) {
			for (auto &c : word) {
				assert(reorder.count(c));
				c = reorder[c];
			}
		}


		//massage:
		std::sort(wordlist.begin(), wordlist.end(), [](std::string const &a, std::string const &b){
			for (uint32_t i = 0; i < a.size() && i < b.size(); ++i) {
				if (a[i] != b[i]) return a[i] < b[i];
			}
			return a.size() > b.size(); //longer words first 
		});
	}
	std::cout << "Have " << wordlist.size() << " words." << std::endl;

	//This version is okay but we need to start weighting the code space properly
	//First idea: each word is represented by removing some number of letters from previous word and adding some number of new letters.

	std::string prev = "";
	std::map< uint32_t, uint32_t > removes;
	std::map< uint32_t, uint32_t > adds;
	std::map< uint32_t, uint32_t > letters;

	std::map< uint32_t, uint32_t > acts;
	std::map< uint32_t, std::map< uint32_t, uint32_t > > ctx_acts;
	uint32_t prev_action = 0;
	auto act = [&](uint32_t action) {
		ctx_acts[prev_action].insert(std::make_pair(action, 0)).first->second += 1;
		acts.insert(std::make_pair(action, 0)).first->second += 1;
		prev_action = action;
	};

	for (auto word : wordlist) {
		uint32_t common = 0;
		while (common < prev.size() && common < word.size() && prev[common] == word[common]) {
			++common;
		}
		uint32_t remove = prev.size() - common;
		removes.insert(std::make_pair(remove, 0)).first->second += 1;

		for (uint32_t i = 0; i < remove; ++i) {
			act('^');
		}

		uint32_t add = word.size() - common;
		adds.insert(std::make_pair(add, 0)).first->second += 1;


		for (uint32_t i = common; i < word.size(); ++i) {
			letters.insert(std::make_pair(word[i], 0)).first->second += 1;
			act(word[i]);
		}

		act('<');

		prev = word;
	}

	auto opt_bits = [](std::map< uint32_t, uint32_t > &counts) {
		uint32_t total = 0;
		for (auto c : counts) {
			total += c.second;
		}
		double bits = 0.0;
		for (auto c : counts) {
			bits += c.second * -std::log2(double(c.second) / total);
		}
		return bits;
	};

	double ctx_acts_bits = 0.0;
	for (auto &at : ctx_acts) {
		ctx_acts_bits += opt_bits(at.second);
	}


	std::cout << "   Storing remove [" << removes.size() << " values] is " << opt_bits(removes) / 8.0 << " bytes total." << std::endl;
	std::cout << "   Storing add [" << adds.size() << " values] is " << opt_bits(adds) / 8.0 << " bytes total." << std::endl;
	std::cout << "   Storing letter [" << letters.size() << " values] is " << opt_bits(letters) / 8.0 << " bytes total." << std::endl;
	std::cout << " => " << (opt_bits(removes) + opt_bits(adds) + opt_bits(letters)) / 8.0 << " bytes altogether." << std::endl;
	std::cout << "\nAction is " << opt_bits(acts) / 8.0 << " bytes total." << std::endl;
	std::cout << "Action+ctx is " << ctx_acts_bits / 8.0 << " bytes total." << std::endl;

	std::cout << "----------" << std::endl;
	//------------------------------------------------------
	//Okay, tree-peeling version.
	// we're going to build a tree
	{
		Node root;
		for (auto w : wordlist) {
			root.add_string(w);
		}
	
		std::map< uint32_t, uint32_t > strata_acts;
		std::map< uint32_t, uint32_t > strata_counts;
		std::map< uint32_t, uint32_t > strata_letters;
		std::map< uint32_t, uint32_t > strata_deltas;
	
		std::vector< std::vector< Node * > > strata;
		strata.emplace_back(1, &root);
		while (1) {
			std::vector< Node * > next;
			for (auto n : strata.back()) {
				strata_counts.insert(std::make_pair(n->size(),0)).first->second += 1;
				char prev = 'a';
				for (auto cn : *n) {
					next.emplace_back(cn.second);
					strata_acts.insert(std::make_pair(cn.first,0)).first->second += 1;
					strata_letters.insert(std::make_pair(cn.first, 0)).first->second +=  1;
					strata_deltas.insert(std::make_pair(cn.first - prev, 0)).first->second +=  1;
					prev = cn.first;
				}
				strata_acts.insert(std::make_pair('<',0)).first->second += 1;
			}
			if (next.empty()) break;
			strata.emplace_back(std::move(next));
		}
		std::cout << "Have " << strata.size() << " strata." << std::endl;
		std::cout << "That would be " << opt_bits(strata_acts) / 8.0 << " bytes to peel [rle]." << std::endl;
		std::cout << "        " << strata_counts.size() << " counts for " << opt_bits(strata_counts) / 8.0 << " bytes.\n";
		std::cout << "        " << strata_letters.size() << " letters for " << opt_bits(strata_letters) / 8.0 << " bytes.\n";
		std::cout << "        " << strata_deltas.size() << " deltas for " << opt_bits(strata_deltas) / 8.0 << " bytes.\n";
		std::cout << "That would be " << (opt_bits(strata_counts) + opt_bits(strata_letters)) / 8.0 << " bytes to peel [count + letter]." << std::endl;
		std::cout << "That would be " << (opt_bits(strata_counts) + opt_bits(strata_deltas)) / 8.0 << " bytes to peel [count + delta]." << std::endl;
	
		std::cout << "----------" << std::endl;
	}

	//------------------------------------------------------
	//I wonder if there's some DAG-ing to be had
	{
		Node root;
		for (auto w : wordlist) {
			root.add_string(w);
		}

		std::cout << "(old count: " << root.count() << ")\n";

		Unique unique;

		root.dagify_children(unique);

		std::cout << "from " << root.count() << " to " << unique.count() + 1 << "." << std::endl;


		std::map< uint32_t, uint32_t > unique_letters;
		std::map< uint32_t, uint32_t > unique_counts;
		std::map< uint32_t, uint32_t > unique_ids;
		std::map< uint32_t, uint32_t > unique_deltas;

		std::vector< std::pair< uint32_t, Node * > > by_id;
		for (auto n : unique.store) {
			assert(n->id != -1U);
			by_id.emplace_back(n->id, n);
		}
		std::sort(by_id.rbegin(), by_id.rend());

		//TODO: need to write down list of root's children
		for (auto n_id : by_id) {
			Node *n = n_id.second;
			
			std::vector< std::pair< uint32_t, std::pair< char, Node * > > > c_by_id;
			for (auto c : *n) {
				c_by_id.emplace_back(c.second->id, c);
			}
			std::sort(c_by_id.rbegin(), c_by_id.rend());

			unique_counts.insert(std::make_pair(c_by_id.size(),0)).first->second += 1;
			uint32_t prev = n->id;
			for (auto c_id : c_by_id) {
				auto c = c_id.second;
				unique_letters.insert(std::make_pair(c.first, 0)).first->second +=  1;
				unique_ids.insert(std::make_pair(c.second->id, 0)).first->second +=  1;
				assert(c.second->id <= prev);
				unique_deltas.insert(std::make_pair(prev - c.second->id, 0)).first->second +=  1;
				prev = c.second->id;
			}
		}

		std::cout << "        " << unique_counts.size() << " counts for " << opt_bits(unique_counts) / 8.0 << " bytes.\n";
		std::cout << "        " << unique_letters.size() << " letters for " << opt_bits(unique_letters) / 8.0 << " bytes.\n";
		std::cout << "        " << unique_ids.size() << " ids for " << opt_bits(unique_ids) / 8.0 << " bytes.\n";
		std::cout << "        " << unique_deltas.size() << " deltas for " << opt_bits(unique_deltas) / 8.0 << " bytes.\n";
		std::cout << "That would be " << (opt_bits(unique_counts) + opt_bits(unique_letters) + opt_bits(unique_deltas)) / 8.0 << " bytes to peel [counts + letters + deltas]." << std::endl;
	


	}

	return 0;

}
