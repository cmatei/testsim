
#include <string>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <map>

#include "keyer.h"

std::map<char, std::string> Keyer::morse = {
	{ 'a', ".-" },
	{ 'b', "-..." },
	{ 'c', "-.-." },
	{ 'd', "-.." },
	{ 'e', "." },
	{ 'f', "..-." },
	{ 'g', "--." },
	{ 'h', "...." },
	{ 'i', ".." },
	{ 'j', ".---" },
	{ 'k', "-.-" },
	{ 'l', ".-.." },
	{ 'm', "--" },
	{ 'n', "-." },
	{ 'o', "---" },
	{ 'p', ".--." },
	{ 'q', "--.-" },
	{ 'r', ".-." },
	{ 's', "..." },
	{ 't', "-" },
	{ 'u', "..-" },
	{ 'v', "...-" },
	{ 'w', ".--" },
	{ 'x', "-..-" },
	{ 'y', "-.--" },
	{ 'z', "--.." },
	{ '0', "-----" },
	{ '1', ".----" },
	{ '2', "..---" },
	{ '3', "...--" },
	{ '4', "....-" },
	{ '5', "....." },
	{ '6', "-...." },
	{ '7', "--..." },
	{ '8', "---.." },
	{ '9', "----." },
	{ '.', ".-.-.-" },
	{ '-', "-....-" },
	{ ',', "--..--" },
	{ '?', "..--.." },
	{ '/', "-..-." },
	{ ';', "-.-.-." },
	{ '(', "-.--." },
	{ '[', "-.--." },
	{ ')', "-.--.-" },
	{ ']', "-.--.-" },
	{ '@', ".--.-." },
	{ '*', "...-.-" },
	{ '+', ".-.-." },
	{ '%', ".-..." },
	{ ':', "---..." },
	{ '=', "-...-" },
	{ '"', ".-..-." },
	{ '\'', ".----." },
	{ '!', "---." },
	{ '$', "...-..-" },
	{ ' ', "" },
	{ '_', "" },
};


Keyer::Keyer(size_t samplerate_, size_t bufsize_, float risetime)
{
	samplerate = samplerate_;
	bufsize = bufsize_;

	setRisetime(risetime);
}

void Keyer::setRisetime(float risetime_)
{
	risetime = risetime_;

	float step = 1.0 / (2.7 * risetime * samplerate);

	risevec.clear();
	int steps = static_cast<int>(std::round(1.0f / step));
	for (int j = 0; j <= steps; j++) {
		float i = j * step;
		risevec.push_back(0.5f * (1.0f + std::erf(5.0f * (i - 0.5f))));
	}
}


std::string Keyer::Encode(std::string txt)
{
	std::string s;

	for (auto c : txt) {
		s += morse[c];
		s += ' ';
	}

	// replace last space with tilde
	if (s.size()) {
		s.pop_back();
		s += '~';
	}

	return s;
}

std::vector<float> Keyer::getEnvelope(const std::string& msg, float wpm)
{
	std::vector<float> env;

	size_t ndits = 0;
	size_t nspac = 0;
	size_t ndahs = 0;
	size_t ntild = 0;

	for (const char c : msg) {
		switch (c) {
		case '.':
			ndits++;
			break;
		case '-':
			ndahs++;
			break;
		case ' ':
			nspac++;
			break;
		case '~':
			ntild++;
			break;
		}
	}

	size_t nr = risevec.size();
	size_t count = 2 * (ndits + nspac + 2 * ndahs) + ntild;
	size_t samples = (size_t) std::round(1.2 * samplerate / wpm);

	size_t n = (size_t) bufsize * std::ceil((count * samples + nr) / bufsize);

	env.resize(n);
	std::fill(env.begin(), env.end(), 0.0);

	std::vector<float> dit(nr + samples, 1.0);
	std::copy(risevec.begin(), risevec.end(), dit.begin());
	std::copy(risevec.begin(), risevec.end(), dit.rbegin());

	std::vector<float> dah(nr + 3 * samples, 1.0);
	std::copy(risevec.begin(), risevec.end(), dah.begin());
	std::copy(risevec.begin(), risevec.end(), dah.rbegin());

	size_t k = 0;
	for (const char c : msg) {
		switch (c) {
		case '.':
			std::copy(dit.begin(), dit.end(), env.begin() + k);
			k += 2 * samples;
			break;
		case '-':
			std::copy(dah.begin(), dah.end(), env.begin() + k);
			k += 4 * samples;
			break;
		case ' ':
			k += 2 * samples - nr;
			break;
		case '~':
			k += samples - nr;
			break;
		}
	}

       return env;
}
