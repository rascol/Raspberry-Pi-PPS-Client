/*
 * normalDistribParams.cpp
 *
 * Created on: Oct 20, 2016
 * Modified: Jan 1, 2018
 * Copyright (C) 2018 Raymond S. Connell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
unsigned int idnum = 1839762001;		// 1839762001
double pi = 3.14159265359;

const char *version = "1.1";

/**
 * Generates pseudo-random number in range
 * [low,high).
 */
double randomVar(double low, double high)
{
	double val;

	val = (double)rand() / 2147483648.0;

	if (low == 0.0 && high == 1.0)
		return (double)val;

	return (double)(val * (high - low) + low);
}

/**
 * Calculates the mean and standard deviation from three
 * consecutive values of a sample distribution binned
 * at unit intervals using Monte Carlo simulation and
 * the error function approximation to the cumulative
 * normal distribution. Random values of of mean and
 * stddev are tried until (n1,y1), (n2,y2) and (n3,y3)
 * fit the normal distribution passing through the points
 * as closely as possible within the number of trials.
 *
 * Each bin extends over range [n - 0.5, n + 0.5).
 *
 * @param[in] y1 Number of samples in the first bin.
 * @param[in] n1 Bin index of the first bin.
 * @param[in] y2 Number of samples in the second bin.
 * @param[in] n2 Bin index of the second bin.
 * @param[in] y3 Number of samples in the third bin.
 * @param[in] n3 Bin index of the third bin.
 * @param[out] mean Calculated mean.
 * @param[out] stddev Calculated standard deviation.
 * @param[in] Y_total Total number of distribution samples.
 * @returns The simulation error as the average error
 * between the three sample points and the best fit normal
 * distribution.
 */
double getNormalParams1(double y1, double n1, double y2, double n2, double y3, double n3, double *mean, double *stddev, double Y_total){
	double m, sd, error1, error2, error3;
	double s11, s12, s21, s22, s31, s32;
	double denom, d;

	double root2 = sqrt(2.0);

	double best_mean = 0.0, best_sd = 0.0;

	double min_d = 1e6;
	double rng = 1.5;

	double r1 = 2.0 * y1 / Y_total;					// Relative bin weights pre-scaled by 2 to match erf()
	double r2 = 2.0 * y2 / Y_total;
	double r3 = 2.0 * y3 / Y_total;

	for (int i = 0; i < 1000000; i++){

		m = best_mean + randomVar(-rng, rng);		// Trial mean in range 2 * rng
		sd = best_sd + randomVar(-rng, rng);			// Trial SD in range 2 * rng

		denom = 1.0 / (root2 * sd);

		s11 = (n1 - 0.5 - m) * denom;				// Upper and lower limits of first bin
		s12 = (n1 + 0.5 - m) * denom;

		error1 = (erf(s12) - erf(s11)) - r1;			// 2 * difference between ideal and measured for bin1

		s21 = (n2 - 0.5 - m) * denom;				// Upper and lower limits of second bin
		s22 = (n2 + 0.5 - m) * denom;

		error2 = (erf(s22) - erf(s21)) - r2;			// 2 * difference between ideal and measured for bin2

		s31 = (n3 - 0.5 - m) * denom;				// Upper and lower limits of third bin
		s32 = (n3 + 0.5 - m) * denom;

		error3 = (erf(s32) - erf(s31)) - r3;			// 2 * difference between ideal and measured for bin3

		d = sqrt((error1 * error1 + error2 * error2 + error3 * error3) / 3.0);

		if (d < min_d){
			min_d = d;
			best_mean = m;
			best_sd = sd;
		}

		rng *= 0.999995;
	}

	*mean = best_mean;
	*stddev = best_sd;

	return min_d / 2.0;								// Fractional area difference between the ideal Gaussian area
}													// and the measured area over a range of n1 - 0.5 to n3 + 0.5.

int main(int argc, char *argv[]){

	if (!(argc == 7 || argc == 8)){
		printf("Requires three successive sample pairs, y1 x1 y2 x2 y3 x3, with\n");
		printf("unit separation that wrap the peak of the distribution near zero.\n");
		printf("Also accepts a seventh arg that specifies the sample size. Otherwise\n");
		printf("the y values are normalized to the default sample size of 86,400.\n\n");
		printf("Prints the mean relative to the sample point with x = 0 of an ideal\n");
		printf("normal distribution that best fits the three points, then standard\n");
		printf("deviation of the best fit ideal distribution, then the relative sample\n");
		printf("fit to that ideal distribution.\n\n");
		return 0;
	}

	double Y1, n1, Y2, n2, Y3, n3;
	sscanf(argv[1], "%lf", &Y1);
	sscanf(argv[2], "%lf", &n1);
	sscanf(argv[3], "%lf", &Y2);
	sscanf(argv[4], "%lf", &n2);
	sscanf(argv[5], "%lf", &Y3);
	sscanf(argv[6], "%lf", &n3);

	double mean, sd;

	double Y_total = 86400.0;
	if (argc == 8){
		sscanf(argv[7], "%lf", &Y_total);
	}

	double relative_error = getNormalParams1(Y1, n1, Y2, n2, Y3, n3, &mean, &sd, Y_total);

	printf("Relative to the best fit normal distribution:\n");
	printf("mean:  %lf\n", mean);
	printf("stddev: %lf\n", sd);
	printf("Relative fit of samples: %lf\n", 1.0 - relative_error);

	return 0;
}
