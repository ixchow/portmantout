#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <cassert>
#include <vector>
#include <deque>
#include <time.h>

class Level : public std::map< char, Level * > {
public:
	Level() : terminal(0) { }
	uint32_t terminal;
};

class Counts {
public:
	Counts() : nodes(0), terminals(0) {
	}
	uint32_t nodes;
	std::vector< uint32_t > terminals;
	std::vector< uint32_t > depth;
};

void count_tree(Counts &counts, Level *level, uint32_t depth) {
	while (counts.depth.size() <= depth) {
		counts.depth.push_back(0);
	}
	counts.depth[depth] += 1;

	while (counts.terminals.size() <= level->terminal) {
		counts.terminals.push_back(0);
	}
	counts.terminals[level->terminal] += 1;

	counts.nodes += 1;
	for (auto c : *level) {
		count_tree(counts, c.second, depth + 1);
	}
}

void stopwatch(const char *name) {
	struct timespec ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	double now = double(ts.tv_sec) + 1.0e-9 * double(ts.tv_nsec);
	static double prev = now;

	std::cout << name << ": " << (now - prev) * 1000.0 << "ms" << std::endl;

	prev = now;
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
			at->terminal += 1;
		}
		std::cout << "Have " << symbols.size() << " symbols." << std::endl;
		//unroll tree for some stats:
		Counts counts;
		count_tree(counts, &root, 0);
		std::cout << "Depths:" << std::endl;
		for (uint32_t i = 0; i < counts.depth.size(); ++i) {
			std::cout << i << ": " << counts.depth[i] << std::endl;
		}
		std::cout << "Terminal counts: " << std::endl;
		for (uint32_t i = 0; i < counts.terminals.size(); ++i) {
			std::cout << i << ": " << counts.terminals[i] << std::endl;
		}
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

	uint32_t found_words = 0;

	//TODO: check coverage by keeping track of letters since last terminal
	std::deque< char > queue;
	Level *at = &root;
	for (auto c : portmantout) {
		queue.push_back(c);
		auto f = at->find(c);
		while (f == at->end()) {
			queue.pop_front();
			at = &root;
			for (auto q : queue) {
				f = at->find(q);
				if (f == at->end()) break;
				else at = f->second;
				if (at->terminal > 0) {
					at->terminal -= 1;
					++found_words;
				}
			}
			assert(at != &root);
		}
		at = f->second;
		if (at->terminal > 0) {
			at->terminal -= 1;
			++found_words;
		}
	}

	stopwatch("test");

	std::cout << "Found " << found_words << " words." << std::endl;

	Counts counts;
	count_tree(counts, &root, 0);

	if (counts.terminals.size() > 1) {
		std::cout << "Missing at least " << counts.terminals[1] << " words." << std::endl;
		return 1;
	}

	return 0;

}
