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
			for (uint32_t i = 0; i + 1 < word.size(); ++i) {
				covering[pos + i] += 1;
			}
			pos = portmantout.find(word, pos + 1);
		}

		if (!found) {
			if (missing_words <= 10) {
				std::cerr << "'" << word << "' MISSING!" << std::endl;
			}
			++missing_words;
		} else {
			++found_words;
		}
		if ((missing_words + found_words) % 1000 == 0) {
			std::cout << missing_words << "/" << found_words << std::endl;
		}
	}
	std::cout << "Found " << found_words << " words." << std::endl;

	bool covered = true;

	for (auto c : covering) {
		if (c == 0) {
			covered = false;
			std::cout << "Missing a covering!" << std::endl;
			break;
		}
	}
	if (!covered) return false;
	std::cout << "Fully covered." << std::endl;

	if (missing_words) {
		std::cerr << "Have " << missing_words << " missing words." << std::endl;
		return 1;
	}

	return 0;

}
