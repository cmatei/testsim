#include <fstream>

#include "calllist.h"


CallList::CallList(RNG *rng_, std::string callfile)
{
	rng = rng_;

	std::ifstream input(callfile);

	if (input.is_open()) {
		for (std::string line; std::getline(input, line);) {
			if (line.find("#", 0) == 0)
				continue;

			auto cr = line.find_first_of('\r');
			if (cr != std::string::npos)
				line = line.substr(0, cr);

			if (!line.size())
				continue;

			calls.push_back(line);
		}
		input.close();
	} else {
		exit(1);
	}
}

const std::string & CallList::pick()
{
	return calls[rng->integers(0, calls.size())];
}
