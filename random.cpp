
#include <cmath>
#include <random>

#include "random.h"

static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_real_distribution urd(0.0, 1.0);

double RNG::random()
{
	return urd(gen);
}

int RNG::integers(int low, int high)
{
	int result = low + static_cast<int>(std::floor((high - low) * random()));
	return std::min(result, high - 1);
}

double RNG::uniform()
{
	return random();
}

double RNG::normal(double mean, double stddev)
{
	std::normal_distribution<double> dist(mean, stddev);
	return dist(gen);
}

double RNG::rayleigh(double scale)
{
	// Rayleigh distribution: f(x) = (x/σ²)exp(-x²/2σ²)
	// Can be generated from: σ * sqrt(-2 * ln(U)) where U ~ Uniform(0,1)
	double u = random();
	return scale * std::sqrt(-2.0 * std::log(u));
}

int RNG::poisson(double lambda)
{
	std::poisson_distribution<int> dist(lambda);
	return dist(gen);
}
