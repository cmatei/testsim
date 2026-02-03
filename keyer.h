#ifndef __KEYER_H
#define __KEYER_H

#include <string>
#include <vector>
#include <map>


class Keyer {
public:
	Keyer(size_t samplerate_ = 11025, size_t bufsize_ = 512, double risetime_ = 0.005);

	void setRisetime(double risetime);
	std::string Encode(std::string txt);

	std::vector<double> getEnvelope(const std::string& msg, double wpm);

private:
	static std::map<char, std::string> morse;

	size_t samplerate, bufsize;
	double risetime;

	std::vector<double> risevec;
};


#endif
