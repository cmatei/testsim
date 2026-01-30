#ifndef __CALLLIST_H
#define __CALLLIST_H

#include <string>
#include <vector>

#include "random.h"

class CallList {
public:
	CallList(RNG *rng_, std::string callfile = "MASTER.SCP");

	const std::string &pick();

private:
	RNG *rng;
	std::vector<std::string> calls;
};

#endif
