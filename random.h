#ifndef __RANDOM_H
#define __RANDOM_H


class RNG {
public:
	RNG() {}

	double random();
	int integers(int low, int high);

	// Additional distribution methods
	double uniform();  // Alias for random(), returns [0, 1)
	double normal(double mean, double stddev);
	double rayleigh(double scale);
	int poisson(double lambda);  // Poisson distribution
};

#endif
