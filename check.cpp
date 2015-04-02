#include <string>
#include <iostream>
#include <fstream>

int main(int argc, char **argv) {
	std::string portmantout;
	if (!std::getline(std::cin, portmantout) || portmantout.size() == 0) {
		std::cerr << "Please pass a portmantout on stdin." << std::endl;
		return 1;
	}

	std::cout << "Testing portmantout of " << portmantout.size() << " letters." << std::endl;


	uint32_t missing_words = 0;
	uint32_t found_words = 0;
	std::ifstream wordlist("wordlist.asc");
	std::string word;
	while (std::getline(wordlist, word)) {
		if (portmantout.find(word) == std::string::npos) {
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

	if (missing_words) {
		std::cerr << "Have " << missing_words << " missing words." << std::endl;
		return 1;
	}

	return 0;

}
