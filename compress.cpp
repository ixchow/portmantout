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
#include <unordered_set>
#include <list>

#include <Eigen/Dense>

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
	Params() : verbose(false), verbose_split(false), only_root(false), letter_map(NoMap), re_id(None), split(NoSplit), reverse(false)  { }
	bool verbose;
	bool verbose_split;
	bool only_root;
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

		if (reverse) {
			desc += "~";
		} else {
			desc += "-";
		}
		if (only_root) {
			desc += "!root!";
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

class PlyData {
public:
	PlyData(PlyData *_previous, Node *_node) : previous(_previous), node(_node) {
		//compute everything we might care to store about this node:

		child_count = 0;
		letter = node->src;
		terminal = node->terminal;

		for (auto cn : *node) {
			assert(cn.second->refs >= 1);
			//store refs == 1 nodes or nodes with no children inline:
			if (cn.second->refs == 1) {
				++child_count;
			} else {
				ids.push_back(cn.second->id);
			}
		}

		std::sort(ids.begin(), ids.end());

		//what if we delta these a bit... (?)
		for (uint32_t i = ids.size() - 1; i - 1 < ids.size(); --i) {
			ids[i] = ids[i] - ids[i-1];
		}
	}

	PlyData *previous;
	Node *node; //for sanity checking and child-finding; all relevant data is extracted from node during construction.

	std::vector< uint32_t > ids; //node: id_count is also a feature.
	uint8_t child_count;
	char letter;
	bool terminal;

};

class Ply {
public:
	Ply(Node *root) : previous(NULL) {
		first_feature = 0;
		data.emplace_back((PlyData *)NULL, root);

		setup_from_data();
	}
	Ply(Ply *_previous) : previous(_previous) {
		assert(previous);
		first_feature = previous->first_feature + previous->feature_count;

		//add a data entry for every child implied by previous ply:
		for (auto &d : previous->data) {
			for (auto cn : *d.node) {
				assert(cn.second->refs >= 1);
				//recurse to nodes with refs == 1:
				if (cn.second->refs == 1) {
					assert(cn.first == cn.second->src);
					data.emplace_back(&d, cn.second);
				}
			}
		}

		setup_from_data();
	}

	void setup_from_data() {

		feature_count = 0;
		for (auto &d : data) {
			feature_count = std::max< uint32_t >(feature_count,
				  1 //letter
				+ 1 //child count
				+ 1 //ids count
				+ 1 //terminal
				+ d.ids.size() //ids themselves
			);
		}
	}

	enum : uint32_t {
		Letter = 0,
		ChildCount = 1,
		IdCount = 2,
		Terminal = 3,
		FirstId = 4,
	};

	Ply *previous;

	std::vector< PlyData > data;
	uint32_t first_feature;
	uint32_t feature_count;

	int32_t get_feature(uint32_t index, uint32_t feature) {
		assert(feature < first_feature + feature_count);
		assert(index < data.size());
		PlyData &d = data[index];
		if (feature < first_feature) {
			assert(previous);
			return previous->get_feature(d.previous - &(previous->data[0]), feature);
		} else {
			if (feature - first_feature == Letter) {
				return d.letter;
			} else if (feature - first_feature == ChildCount) {
				return d.child_count;
			} else if (feature - first_feature == IdCount) {
				return d.ids.size();
			} else if (feature - first_feature == Terminal) {
				return d.terminal;
			} else { assert(feature - first_feature >= FirstId);
				uint32_t i = feature - first_feature - FirstId;
				if (i < d.ids.size()) {
					return d.ids[i];
				} else {
					return 0;
				}
			}
		}
	}
};

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

	//let's re-id by reference frequency (not sure how useful, but worth a shot):
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
	std::vector< std::pair< Context, int32_t > > s_all_letters;
	std::vector< std::pair< Context, int32_t > > s_first_letters;
	std::vector< std::pair< Context, int32_t > > s_letter_deltas;
	std::vector< std::pair< Context, int32_t > > s_id_counts;
	std::vector< std::pair< Context, int32_t > > s_first_ids;
	std::vector< std::pair< Context, int32_t > > s_second_ids;
	std::vector< std::pair< Context, int32_t > > s_third_ids;
	std::vector< std::pair< Context, int32_t > > s_id_deltas;
	std::vector< std::pair< Context, int32_t > > s_ids;

	{ //everything that gets referenced more than once gets stored as a separate blob:

		//store by (descending) id:
		std::vector< std::pair< uint32_t, Node * > > by_id;
		for (auto n : unique.store) {
			by_id.emplace_back(n->id, n);
		}
		std::sort(by_id.rbegin(), by_id.rend());

		std::unordered_set< Node * > stored; //keep track as a sanity check

		double total_bits = 0.0;

		for (auto seed_ : by_id) {
			Node *seed = seed_.second;

			if (seed->refs != 0) {
				//skip everything except root for the moment.
				continue;
			}

			if (seed->refs == 1) {
				//This fails, which does tend to worry one:
				//assert(stored.count(seed)); //should have already stored
				continue;
			}


			//peel fragment into plys:
			std::list< Ply > plys;
			plys.emplace_back(seed);

			while (!plys.back().data.empty()) {
				plys.emplace_back(&plys.back());
			}
			plys.pop_back();

			std::cout << "Peeled into " << plys.size() << " plys:" << std::endl;
			for (auto &ply : plys) {
				std::cout << "    " << ply.data.size() << " nodes, " << ply.feature_count << " features." << std::endl;
				for (auto &d : ply.data) {
					assert(!stored.count(d.node));
					stored.insert(d.node);
				}
			}

			double fragment_bits = 0.0;

			//Okay, figure out a good order in which to compress each ply (?)
			std::vector< uint32_t > stored;
			for (auto &ply : plys) {
				std::vector< uint32_t > inds;
				inds.reserve(ply.data.size());
				for (uint32_t i = 0; i < ply.data.size(); ++i) {
					inds.emplace_back(i);
				}

				std::vector< uint32_t > to_store;
				to_store.reserve(ply.feature_count);
				for (uint32_t f = ply.first_feature; f < ply.first_feature + ply.feature_count; ++f) {
					to_store.emplace_back(f);
				}

				std::vector< std::vector< int32_t > > feats(ply.first_feature + ply.feature_count, std::vector< int32_t >(ply.data.size()));
				for (uint32_t f = 0; f < feats.size(); ++f) {
					for (uint32_t d = 0; d < feats[f].size(); ++d) {
						feats[f][d] = ply.get_feature(d, f);
					}
				}

				//sort re-sorts the indices based on the current feature priority:
				auto sort = [&stored, &to_store, &inds, &feats]() {
					assert(to_store.size() + stored.size() == feats.size());
					std::sort(inds.begin(), inds.end(), [&](uint32_t a, uint32_t b){
						for (auto const f : stored) {
							int32_t fa = feats[f][a];
							int32_t fb = feats[f][b];
							if (fa != fb) return fa < fb;
						}
						for (auto const f : to_store) {
							int32_t fa = feats[f][a];
							int32_t fb = feats[f][b];
							if (fa != fb) return fa < fb;
						}
						return false;
					});
				};

				static std::mt19937 mt(0xfeedbeef);
				
				double ply_bits = 0.0;

				std::cout << " " << ply.data.size() << " nodes x " << ply.feature_count << " features:" << std::endl;
				while (!to_store.empty()) {
					assert(to_store.size() + stored.size() == ply.first_feature + ply.feature_count);

					//TODO: some sort of more reasonable search for the best order
					std::sort(to_store.begin(), to_store.end());
					//std::swap(to_store[0], to_store[mt() % to_store.size()]);

					std::cout << "   " << ply.first_feature << "+" << to_store[0] - ply.first_feature << ":"; std::cout.flush();

					double best_bits = std::numeric_limits< double >::infinity();
					uint32_t best_first = -1U;
					uint32_t first = 0;
					for (uint32_t iter = 0; iter < 10; ++iter) {
						//try all sorts of features as "most important":
						first = first + 1;
						if (first > stored.size()) first = 0;

						for (uint32_t i = 0; i < stored.size(); ++i) {
							if (stored[i] == first) {
								std::swap(stored[i], stored[0]);
								break;
							}
						}
						//randomize importance of the other ones:
						for (uint32_t i = 1; i < stored.size(); ++i) {
							std::swap(stored[i], stored[i + mt() % (stored.size() - i)]);
						}

						//always sort preserving the selected value:
						for (uint32_t i = 1; i < to_store.size(); ++i) {
							std::swap(to_store[i], to_store[i + mt() % (to_store.size() - i)]);
						}
						sort();


						std::vector< int32_t > data;

						if (to_store[0] - ply.first_feature > Ply::FirstId + 1) {
							//we glom 'em, so no data here.
							best_bits = 0.0;
							best_first = 0;
							break;
						} else if (to_store[0] - ply.first_feature == Ply::FirstId + 1) {
							//just glom the deltas; only need one iter
							std::vector< int32_t > nodelta;
							for (auto i : inds) {
								PlyData &d = ply.data[i];
								bool first = true;
								for (auto id : d.ids) {
									if (first) {
										first = false;
									} else {
										nodelta.emplace_back(id);
									}
								}
							}
							best_bits = est_bits_helper(nodelta);
							best_first = 0;
							break;
						} else if (to_store[0] - ply.first_feature == Ply::FirstId) {
							for (auto i : inds) {
								PlyData &d = ply.data[i];
								if (!d.ids.empty()) {
									data.emplace_back(d.ids[0]);
								}
							}
						} else {
							for (auto i : inds) {
								data.emplace_back(feats[to_store[0]][i]);
							}
						}

						//trim zeros:
						while (!data.empty() && data.back() == 0) {
							data.pop_back();
						}
						while (!data.empty() && data[0] == 0) {
							data.erase(data.begin());
						}

						double test_bits = 0.0;

						if (data.size() > 1) {
							for (uint32_t i = 0; i + 1 < data.size(); ++i) {
								data[i] = data[i+1] - data[i];
							}
							data.pop_back();

							test_bits += est_bits_helper(data);
						}

						if (test_bits < best_bits) {
							best_bits = test_bits;
							best_first = first;
						}
						if (test_bits == 0) break;
					}


					std::cout << " " << std::ceil(best_bits / 8.0) << " [" << best_first << "]";
					//------------------------------------------
					if (!stored.empty() && inds.size() > 2 && to_store[0] - ply.first_feature <= Ply::FirstId) { //Try a linear regression solve:
						std::sort(stored.begin(), stored.end());

						Eigen::MatrixXf A;
						Eigen::VectorXf b;
						std::vector< uint32_t > ind2row(ply.data.size(), -1U);

						if (to_store[0] - ply.first_feature == Ply::FirstId) {
							uint32_t rows = 0;
							for (uint32_t i = 0; i < ply.data.size(); ++i) {
								if (!ply.data[i].ids.empty()) {
									ind2row[i] = rows++;
								}
							}
							A = Eigen::MatrixXf(rows, stored.size());
							b = Eigen::VectorXf(rows);
							uint32_t row = 0;
							for (uint32_t i = 0; i < ply.data.size(); ++i) {
								if (!ply.data[i].ids.empty()) {
									for (uint32_t col = 0; col < stored.size(); ++col) {
										A(row, col) = feats[stored[col]][row];
									}
									b(row) = feats[to_store[0]][row];
									++row;
								}
							}
						} else {
							A = Eigen::MatrixXf(ply.data.size(), stored.size());
							b = Eigen::VectorXf(ply.data.size());
							for (uint32_t row = 0; row < ply.data.size(); ++row) {
								for (uint32_t col = 0; col < stored.size(); ++col) {
									A(row, col) = feats[stored[col]][row];
								}
								b(row) = feats[to_store[0]][row];
							}

							for (uint32_t i = 0; i < ply.data.size(); ++i) {
								ind2row[i] = i;
							}
						}

						//best (least-squares) predictor of the feature:
						Eigen::VectorXf x = A.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(b);


						Eigen::VectorXf score = A * x;

						std::sort(inds.begin(), inds.end(), [&](uint32_t a, uint32_t b) {
							float sa = (ind2row[a] < score.rows() ? score[ind2row[a]] : std::numeric_limits< float >::infinity());
							float sb = (ind2row[b] < score.rows() ? score[ind2row[b]] : std::numeric_limits< float >::infinity());
							if (sa != sb) return sa < sb;
							for (auto const f : stored) {
								int32_t fa = feats[f][a];
								int32_t fb = feats[f][b];
								if (fa != fb) return fa < fb;
							}
							for (auto const f : to_store) {
								int32_t fa = feats[f][a];
								int32_t fb = feats[f][b];
								if (fa != fb) return fa < fb;
							}
							return false;
						});

						std::vector< int32_t > data;
						std::vector< int32_t > residual;

						/*} else if (to_store[0] - ply.first_feature == Ply::FirstId) {
							for (auto i : inds) {
								PlyData &d = ply.data[i];
								bool first = true;
								for (auto id : d.ids) {
									if (first) {
										data.emplace_back(id);
										first = false;
									} else {
										data2.emplace_back(id);
									}
								}
							}
						} else*/ {
							for (auto i : inds) {
								data.emplace_back(feats[to_store[0]][i]);
								if (ind2row[i] < score.rows()) {
									residual.emplace_back(feats[to_store[0]][i] - std::round(score[ind2row[i]]));
								} else {
									residual.emplace_back(feats[to_store[0]][i]);
								}
							}
						}

						//trim zeros:
						while (!data.empty() && data.back() == 0) {
							data.pop_back();
						}
						while (!data.empty() && data[0] == 0) {
							data.erase(data.begin());
						}

						double test_bits = 0.0;
						if (!data.empty()) {
							for (uint32_t i = 0; i + 1 < data.size(); ++i) {
								data[i] = data[i+1] - data[i];
							}
							data.pop_back();

							test_bits += est_bits_helper(data);
						}

						std::cout << "   reg " << std::ceil(test_bits / 8.0);
						if (test_bits < best_bits) {
							std::cout << " [!!]";
							best_bits = test_bits;
						}

						double residual_bits = est_bits_helper(residual);
						std::cout << "  res " << std::ceil(residual_bits / 8.0);
						if (residual_bits < best_bits) {
							std::cout << " [!!]";
							best_bits = residual_bits;
						}

					}
					std::cout << std::endl;
					//------------------------------------------

					ply_bits += best_bits;

					stored.push_back(to_store[0]);
					to_store.erase(to_store.begin());
				}
				std::cout << " ----> " << std::ceil(ply_bits / 8.0) << " bytes" << std::endl;
				fragment_bits += ply_bits;

			} //for (plys)

			total_bits += fragment_bits;


		} //for (seeds)

		std::cout << "Total bytes: " << std::ceil(total_bits / 8.0) << std::endl;

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
	params.reverse = false;
	params.split = Params::SingleSplit;
	params.only_root = false;
	compress(wordlist, params);
#else
	params.verbose = false;
	params.split = Params::SingleSplit;

	for (int letter_map = 0; letter_map < Params::MapCount; ++letter_map)
	for (int re_id = 0; re_id < Params::IdCount; ++re_id)
	for (int reverse = 0; reverse < 2; ++reverse)
	{

		params.letter_map = (Params::LetterMap)letter_map;
		params.re_id = (Params::ReID)re_id;
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
