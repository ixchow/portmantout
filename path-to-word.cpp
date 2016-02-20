#include <algorithm>
#include "graph.hpp"
#include "stopwatch.hpp"

int main(int argc, char **argv) {

	if (argc != 2) {
		std::cerr << "Usage:\n\t./path-to-word <path>" << std::endl;
		return 1;
	}

	stopwatch("start");
	std::vector< uint32_t > path;

	{
		std::ifstream in(argv[1]);
		in.seekg(0, std::ios_base::end);
		if (in.tellg() % 4) {
			std::cout << "Expected multiple of four bytes path." << std::endl;
		}
		path.resize(in.tellg() / 4);
		in.seekg(0);
		if (!in.read(reinterpret_cast< char * >(&path[0]), path.size() * 4)) {
			std::cout << "Failed to read path." << std::endl;
			return 1;
		}
	}
	std::cout << "Path of length " << path.size() << "." << std::endl;
	if (path.empty()) {
		std::cout << "Empty path -> nothing to do." << std::endl;
		return 1;
	}

	stopwatch("read path");

	Graph graph;
	if (!graph.read("wordlist.graph")) {
		std::cerr << "Failed to read graph." << std::endl;
		exit(1);
	}
	stopwatch("read graph");

	std::string so_far;

	uint32_t at = 0;
	for (auto const &next : path) {
		std::vector< uint32_t > from(graph.nodes, -1U);
		std::vector< uint32_t > ply;
		ply.emplace_back(at);
		from[at] = at;
		bool done = false;
		while (!ply.empty()) {
			std::vector< uint32_t > next_ply;
			for (auto i : ply) {
				for (uint32_t a = graph.adj_start[i]; a < graph.adj_start[i+1]; ++a) {
					uint32_t n = graph.adj[a];
					if (from[n] == -1U) {
						from[n] = i;
						next_ply.emplace_back(n);
						if (n == next) {
							done = true;
							break;
						}
					}
				}
				if (done) break;
			}
			ply = std::move(next_ply);
			if (done) break;
		}

		{
			std::string chars;

			uint32_t pt = next;
			assert(pt < from.size());
			while (from[pt] != pt) {
				uint32_t f = from[pt];
				assert(f < graph.nodes);
				bool found = false;
				for (uint32_t a = graph.adj_start[f]; a < graph.adj_start[f+1]; ++a) {
					if (graph.adj[a] == pt) {
						found = true;
						chars += graph.adj_char[a];
						break;
					}
				}
				assert(found);

				pt = f;
			}
			assert(pt == at);

			std::reverse(chars.begin(), chars.end());
			so_far += chars;
		}

		if ((&next - &path[0] + 1) % 1000 == 0) {
			stopwatch("step * 1000");
			std::cout << "( " << (&next - &path[0] + 1) << " / " << path.size() << " ) -> At " << so_far.size() << " characters." << std::endl;
		}

		at = next;
	}

	stopwatch("trace");

	std::string filename = "ix-" + std::to_string(so_far.size()) + ".txt";
	std::ofstream out(filename);
	out << so_far;
	std::cout << "Wrote " << filename << "." << std::endl;

	stopwatch("write");



	return 0;
}
