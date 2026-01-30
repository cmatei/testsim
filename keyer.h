#ifndef __KEYER_H
#define __KEYER_H

#include <string>
#include <vector>
#include <map>


class Keyer {
public:
	Keyer(size_t samplerate_ = 11025, size_t bufsize_ = 512, float risetime_ = 0.005);

	void setRisetime(float risetime);
	std::string Encode(std::string txt);

	std::vector<float> getEnvelope(const std::string& msg, float wpm);

private:
	static std::map<char, std::string> morse;

	size_t samplerate, bufsize;
	float risetime;

	std::vector<float> risevec;
};


#endif
