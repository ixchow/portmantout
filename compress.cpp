#include <string>
#include <iostream>
#include <fstream>
#include <cassert>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <algorithm>
#include <cmath>
#include <random>

#include "Coder.hpp"

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
	Params() : verbose(false), verbose_split(false), letter_map(NoMap), re_id(None), split(NoSplit), inline_single(false), reverse(false) { }
	bool verbose;
	bool verbose_split;
	enum LetterMap : int {
		NoMap,
		ByFrequency,
		MapCount
	} letter_map;
	enum ReID : int {
		None,
		RefCount,
		Random,
		IdCount
	} re_id;
	enum Split : int {
		NoSplit,
		SingleSplit,
		SplitCount
	} split;
	bool inline_single;
	bool reverse;
	std::string describe() const {
		std::string desc = "";
		if (letter_map == NoMap) {
			desc += "-";
		} else if (letter_map == ByFrequency) {
			desc += "f";
		} else {
			assert(0 && "Unknown letter map.");
		}

		if (re_id == None) {
			desc += "-";
		} else if (re_id == RefCount) {
			desc += "c";
		} else if (re_id == Random) {
			desc += "*";
		} else {
			assert(0 && "Unknown re-id mode.");
		}

		if (inline_single) {
			desc += "i";
		} else {
			desc += "-";
		}
		if (reverse) {
			desc += "~";
		} else {
			desc += "-";
		}
		return desc;
	}
};

class Context : public std::map< const char *, int32_t > {
public:
	void operator()(const char *label, int32_t value) {
		insert(std::make_pair(label, value));
	}
};

double est_bits_helper(std::map< int32_t, uint32_t > const &counts) {
	uint32_t total = 0;
	for (auto c : counts) {
		total += c.second;
	}
	double bits = 0.0;
	for (auto c : counts) {
		bits += c.second * -std::log2(double(c.second) / total);
	}
	return bits;
}

double est_bits_helper(std::vector< int32_t > const &values) {
	std::map< int32_t, uint32_t > counts;
	for (auto v : values) {
		counts.insert(std::make_pair(v, 0)).first->second += 1;
	}
	return est_bits_helper(counts);
}

/*
double est_bits_delta(std::vector< uint32_t > const &data) {
	if (data.empty()) return 0.0;

	std::vector< uint32_t > deltas;
	for (auto d = data.begin(); d + 1 < data.end(); ++d) {
		deltas.push_back(*(d+1) - *d);
	}

	return est_bits_helper(deltas);
}
*/

double est_bits_split(std::vector< const char * > features, std::vector< std::pair< Context, int32_t > > const &data, Params const &params, std::string &desc) {
	std::map< int32_t, uint32_t > counts;
	for (auto d : data) {
		counts.insert(std::make_pair(d.second, 0)).first->second += 1;
	}
	double basic_bits = est_bits_helper(counts);
	if (params.verbose_split) {
		printf("  %6.0f bytes w/ no split\n", std::ceil(basic_bits / 8.0));
	}

	double best_bits = basic_bits;
	desc = "-";

	if (params.split == Params::NoSplit) {
		return best_bits;
	}

	//estimate entropy storage given split along each feature
	for (auto feature : features) {
		std::map< int32_t, std::map< int32_t, uint32_t > > bins;
		for (auto const &d : data) {
			auto f = d.first.find(feature);
			assert(f != d.first.end());
			bins[f->second].insert(std::make_pair(d.second, 0)).first->second += 1;
		}
		assert(!bins.empty());
		double bits = 16.0 + 16.0; //charging for first value + bin count
		int32_t prev = bins.begin()->first;
		std::vector< int32_t > bin_deltas;
		for (auto &bin : bins) {
			bits += est_bits_helper(bin.second);
			bin_deltas.push_back(bin.first - prev);
			prev = bin.first;
		}
		double label_bits = est_bits_helper(bin_deltas);
		if (params.verbose_split) {
			printf("  % 6.0f saved [inc %2.0f label] w/ %s\n",
				std::floor((basic_bits - bits + label_bits) / 8.0),
				std::ceil(label_bits / 8.0),
				feature);
		}
		bits += label_bits;
		if (bits < best_bits) {
			best_bits = bits;
			desc = feature;
		}
/*
		//TODO: recursive splitting
		if (bins.size() > 1) {
			std::string rec_desc = "";
			double rec_bits = est_bits_split(features, data, params, rec_desc);
			rec_desc = feature + std::string("|") + rec_desc;
			rec_bits += label_bits;
			printf("  % 6.0f saved [inc %2.0f label] w/ %s\n",
				std::floor((basic_bits - rec_bits + label_bits) / 8.0),
				std::ceil(label_bits / 8.0),
				rec_desc(std::string("|" + feature));
			if (rec_bits
		}
*/
	}
	return best_bits;
}

double report(const char *name, std::vector< std::pair< Context, int32_t > > const &data, Params const &params) {
	if (data.empty()) {
		if (params.verbose) {
			printf("%-20s : empty.\n", name);
		}
		return 0.0;
	}

	std::set< const char * > features;
	for (auto f : data[0].first) {
		features.insert(f.first);
	}

	double best_bits = std::numeric_limits< double >::infinity();
	std::string best_desc = "";
	best_bits = est_bits_split(std::vector< const char * >(features.begin(), features.end()), data, params, best_desc);



	/*double bits = est_bits(data);
	double delta_bits = est_bits_delta(data);*/


	std::set< int32_t > unique;
	for (auto d : data) {
		unique.insert(d.second);
	}

	if (params.verbose) {
		printf("%-20s : %5d items, %4d unique -> %5.0f bytes (%5.2f bits per) via %s\n",
			name,
			(int)data.size(),
			(int)unique.size(),
			std::ceil(best_bits / 8.0),
			best_bits / data.size(),
			best_desc.c_str()
			);
	}

#ifdef EXACT
	Coder init, equal, adapt;
	{
		std::map< int32_t, uint32_t > fixed(counts.begin(), counts.end());
		Distribution dist(fixed);
		std::cout << "f"; std::cout.flush();
		for (auto d : data) {
			init.write(d, dist);
		}
	}
	{
		std::map< int32_t, uint32_t > blank(counts.begin(), counts.end());
		for (auto &b : blank) {
			b.second = 1;
		}
		Distribution dist(blank);
		std::cout << "e"; std::cout.flush();
		for (auto d : data) {
			equal.write(d, dist);
		}
		std::cout << "a"; std::cout.flush();
		for (auto d : data) {
			adapt.write(d, dist);
			dist.update_for_val(d);
		}
	}
	printf("   init: %d, equal: %d, adapt: %d\n", (int)init.output.size(), (int)equal.output.size(), (int)adapt.output.size());
#endif //EXACT
	return best_bits;
}

#define REPORT(V) report(#V, V, params)

void compress(std::vector< std::string > const &_wordlist, Params const &params) {
	std::vector< std::string > wordlist = _wordlist;

	std::sort(wordlist.begin(), wordlist.end());

	if (false) {
		//depluralizin'
		//ends up costing bytes because of s_bits storage
		std::vector< std::pair< Context, int32_t > > s_bits;
		for (uint32_t i = 0; i + 1 < wordlist.size(); /* later */) {
			if (wordlist[i] + "s" == wordlist[i+1]) {
				s_bits.emplace_back(Context(), 1);
				wordlist.erase(wordlist.begin() + i + 1);
			} else {
				s_bits.emplace_back(Context(), 0);
				++i;
			}
		}
		REPORT(s_bits);
	}

	if (false) {
		//qu elimination
		//saves ~ 120 bytes
		for (auto &word : wordlist) {
			for (uint32_t i = 0; i + 1 < word.size(); /* later */) {
				if (word.substr(i,2) == "qu") {
					word = word.substr(0,i) + "~" + word.substr(i+2);
				} else {
					++i;
				}
			}
		}
	}


	if (false) { //remove words ending in suffix:
		std::string suf = "ing";
		std::set< std::string > words;
		for (auto w : wordlist) {
			if (w.size() < suf.size() || w.substr(w.size()-suf.size()) != suf) {
				words.insert(w);
			}
		}
		std::set< std::string > stems;
		for (auto w : wordlist) {
			if (w.size() >= suf.size() && w.substr(w.size()-suf.size()) == suf) {
				if (words.count(w.substr(0, w.size()-suf.size()))) {
					stems.insert(w.substr(0, w.size()-suf.size()));
				} else {
					words.insert(w);
				}
			}
		}
		std::cout << "After removing '" << suf << "', have " << words.size() << " words, " << stems.size() << " of which are stems." << std::endl;

		wordlist = std::vector< std::string >(words.begin(), words.end());

		std::sort(wordlist.begin(), wordlist.end(), [](std::string const &a, std::string const &b){
			return a.substr(a.size()-1) < b.substr(b.size()-1);
		});

		std::vector< std::pair< Context, int32_t > > s_stem;

		for (auto w : wordlist) {
			s_stem.emplace_back(Context(), stems.count(w));
		}

		REPORT(s_stem);

		std::sort(wordlist.begin(), wordlist.end());

		s_stem.clear();

		for (auto w : wordlist) {
			s_stem.emplace_back(Context(), stems.count(w));
		}

		REPORT(s_stem);


	}

	if (params.reverse) {
		//reverse words:
		for (auto &w : wordlist) {
			std::reverse(w.begin(), w.end());
		}
	}

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

	std::vector< std::pair< Context, int32_t > > s_letters;
	std::vector< std::pair< Context, int32_t > > s_terminals;
	std::vector< std::pair< Context, int32_t > > s_child_counts;
	std::vector< std::pair< Context, int32_t > > s_first_letters;
	std::vector< std::pair< Context, int32_t > > s_letter_deltas;
	std::vector< std::pair< Context, int32_t > > s_id_counts;
	std::vector< std::pair< Context, int32_t > > s_first_ids;
	std::vector< std::pair< Context, int32_t > > s_id_deltas;


	{ //store tree in fragments by refs
		std::map< uint32_t, uint32_t > size_hist;
		std::map< uint32_t, uint32_t > refs_hist;
		std::map< uint32_t, uint32_t > ids_hist;

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
				s_letters.emplace_back(Context(), seed->src);

				continue; //TEST: actually, only store root fragment
			}

			uint32_t fragment_size = 0;
			std::set< uint32_t > fragment_refs;

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
						fragment_refs.insert(cn.second->id);
					}
				}
				//store ids in order:
				std::sort(ids.begin(), ids.end());
				//store children in order as well (should already be sorted -- map):
				std::sort(children.begin(), children.end());

				Context context;
				context("letter", n->src);
				s_terminals.emplace_back(context, n->terminal ? 1 : 0);
				context("terminal", n->terminal ? 1 : 0);

				s_child_counts.emplace_back(context, children.size());
				context("#children", children.size());
				s_id_counts.emplace_back(context, ids.size());
				context("#ids", ids.size());


				if (!children.empty()) { //children
					s_first_letters.emplace_back(context, children[0].first);
					for (uint32_t i = 1; i < children.size(); ++i) {
						s_letter_deltas.emplace_back(context, children[i].first - children[i-1].first);
					}
				}
				if (!ids.empty()) {
					s_first_ids.emplace_back(context, ids[0]);
					for (uint32_t i = 1; i < ids.size(); ++i) {
						s_id_deltas.emplace_back(context, ids[i] - ids[i-1]);
					}
				}

				ids_hist.insert(std::make_pair(ids.size(), 0)).first->second += 1;

				for (auto c : children) {
					todo.push_back(c.second);
				}
			}

			size_hist.insert(std::make_pair(fragment_size, 0)).first->second += 1;
			refs_hist.insert(std::make_pair(fragment_refs.size(), 0)).first->second += 1;
		}


		if (params.verbose) {
			for (auto h : size_hist) {
				std::cout << "|" << h.first << "| " << h.second << std::endl;
			}
			for (auto h : refs_hist) {
				std::cout << "refs(" << h.first << ") " << h.second << std::endl;
			}
			for (auto h : ids_hist) {
				std::cout << "ids(" << h.first << ") " << h.second << std::endl;
			}


		}

	}


/*
	//HACK: simulate "best possible delta compression":
	if (!s_first_ids.empty()) {
		std::sort(s_first_ids.begin(), s_first_ids.end(), [](std::pair< Context, int32_t > const &a, std::pair< Context, int32_t > const &b) {
			return a.second < b.second;
		});
		for (uint32_t i = s_first_ids.size() - 2; i < s_first_ids.size(); --i) {
			s_first_ids[i+1].second -= s_first_ids[i].second;
		}
		s_first_ids[0].second = 0;
	}
*/


	double bits = 
		REPORT(s_letters)
		+ REPORT(s_terminals)
		+ REPORT(s_child_counts)
		+ REPORT(s_first_letters)
		+ REPORT(s_letter_deltas)
		+ REPORT(s_id_counts)
		+ REPORT(s_first_ids)
		+ REPORT(s_id_deltas);
	std::cout << "[" << params.describe() << "] " << std::ceil(bits / 8.0) << " bytes." << std::endl;
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
	params.reverse = false;
	params.split = Params::SingleSplit;
	compress(wordlist, params);
#else
	params.verbose = false;
	params.split = Params::SingleSplit;

	for (int letter_map = 0; letter_map < Params::MapCount; ++letter_map)
	for (int re_id = 0; re_id < Params::IdCount; ++re_id)
	for (int inline_single = 0; inline_single < 2; ++inline_single)
	for (int reverse = 0; reverse < 2; ++reverse)
	{

		params.letter_map = (Params::LetterMap)letter_map;
		params.re_id = (Params::ReID)re_id;
		params.inline_single = inline_single;
		params.reverse = reverse;
		uint32_t iters = 1;
		/*if (params.re_id == Params::Random) {
			iters = 10;
		}*/
		for (uint32_t iter = 0; iter < iters; ++iter) {
			compress(wordlist, params);
		}
	}
#endif


	return 0;

}
