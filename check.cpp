#include <string>
#include <iostream>
#include <fstream>
#include <vector>

int main(int argc, char **argv) {
	std::string portmantout;
	if (!std::getline(std::cin, portmantout) || portmantout.size() == 0) {
		std::cerr << "Please pass a portmantout on stdin." << std::endl;
		return 1;
	}

	std::cout << "Testing portmantout of " << portmantout.size() << " letters." << std::endl;

	//how many words cover each gap between letters?
	std::vector< uint8_t > covering(portmantout.size()-1, 0);

	uint32_t missing_words = 0;
	uint32_t found_words = 0;
	std::ifstream wordlist("wordlist.asc");
	std::string word;
	while (std::getline(wordlist, word)) {
		size_t pos = portmantout.find(word);
		bool found = (pos != std::string::npos);
		while (pos != std::string::npos) {
			/*for (uint32_t i = 0; i < pos; ++i) {
				std::cout << '.';
			}
			std::cout << word << "\n";*/
			for (uint32_t i = 0; i + 1 < word.size(); ++i) {
				covering[pos + i] += 1;
			}
			pos = portmantout.find(word, pos + 1);
		}

		if (!found) {
			/*if (missing_words <= 10) {
				std::cerr << "'" << word << "' MISSING!" << std::endl;
			}*/
			++missing_words;
		} else {
			++found_words;
		}
		if ((missing_words + found_words) % 1000 == 0) {
			std::cout << '.'; std::cout.flush();
			//missing_words << "/" << found_words << std::endl;
		}
	}
	std::cout << "Found " << found_words << " words." << std::endl;

	uint32_t uncovered = 0;
	for (auto c : covering) {
		if (c == 0) {
			++uncovered;
		}
	}
	std::cout << "Have " << uncovered << " uncovered transitions." << std::endl;
	std::cerr << "Have " << missing_words << " missing words." << std::endl;

	if (uncovered || missing_words) {
		return 1;
	}

	return 0;

}
