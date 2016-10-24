/*
 * normalDistribParams.cpp
 *
 * Created on: Oct 20, 2016
 * Copyright (C) 2016  Raymond S. Connell
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
 * Calculates the mean and stdandard deviation from two
 * consequtive values of a sample distribution binned
 * at unit intervals using monte carlo simulation and
 * the error function approximation to the cumulative
 * normal distribution. Random values of of mean and
 * stddev are tried until (n1,y1) and (n2,y2) fit the
 * normal distribution passing through the points.
 *
 * @param[in] y1 Number of samples in the first bin.
 * @param[in] n1 Bin index of the first bin.
 * @param[in] y2 Number of samples in the second bin.
 * @param[in] n2 Bin index of the second bin.
 * @param[out] mean Calculated mean.
 * @param[out] stddev Calculated standard deviation.
 * @param[in] Y_total Total number of distributin samples.
 * @returns The simulation error as the average error
 * between the two sample points and the best fit normal
 * distribution.
 */
double getNormalParams(double y1, double n1, double y2, double n2, double *mean, double *stddev, double Y_total){
	double m, sd, x_error, y_error;
	double s11, s12, s21, s22;
	double denom, d;

	double root2 = sqrt(2.0);

	double best_mean = 0.0, best_sd = 0.0;

	double min_d = 1e6;
	double rng = 1.0;

	double r1 = 2.0 * y1 / Y_total;
	double r2 = 2.0 * y2 / Y_total;

	for (int i = 0; i < 1000000; i++){

		m = best_mean + randomVar(-rng, rng);
		sd = best_sd + randomVar(-rng, rng);

		denom = 1.0 / (root2 * sd);

		s11 = (n1 - 0.5 - m) * denom;
		s12 = (n1 + 0.5 - m) * denom;

		x_error = (erf(s12) - erf(s11)) - r1;

		s21 = (n2 - 0.5 - m) * denom;
		s22 = (n2 + 0.5 - m) * denom;

		y_error = (erf(s22) - erf(s21)) - r2;

		d = (x_error * x_error + y_error * y_error) / 2.0;
		d = sqrt(d);

		if (d < min_d){
			min_d = d;
			best_mean = m;
			best_sd = sd;
		}

		rng *= 0.999995;
	}

	*mean = best_mean;
	*stddev = best_sd;

	return min_d;
}

int main(int argc, char *argv[]){

	if (!(argc == 7 || argc == 8)){
		printf("Requires three successive sample pairs, y1 x1 y2 x2 y3 x3, with\n");
		printf("unit separation that wrap the peak of the distribution near zero.\n");
		printf("Also accepts a seventh arg that specifies the sample size. Otherwise\n");
		printf("y values are normalized to the default sample size of 86,400.\n\n");
		printf("Prints the mean and error relative to the ideal normal distribution\n");
		printf("that fits the three points, then standard deviation and error relative\n");
		printf("to the ideal distrbution, then the simulation error.\n\n");
		return 0;
	}

	double Y1, n1, Y2, n2, Y3, n3;
	sscanf(argv[1], "%lf", &Y1);
	sscanf(argv[2], "%lf", &n1);
	sscanf(argv[3], "%lf", &Y2);
	sscanf(argv[4], "%lf", &n2);
	sscanf(argv[5], "%lf", &Y3);
	sscanf(argv[6], "%lf", &n3);

	double mean, sd, mean1, mean2, sd1, sd2;

	double Y_total = 86400.0;
	if (argc == 8){
		sscanf(argv[7], "%lf", &Y_total);
	}

	double min_d1 = getNormalParams(Y1, n1, Y2, n2, &mean1, &sd1, Y_total);

	double min_d2 = getNormalParams(Y2, n2, Y3, n3, &mean2, &sd2, Y_total);

	double sim_error = 0.5 * sqrt(min_d1 * min_d1 + min_d2 * min_d2);

	mean = 0.5 * (mean1 + mean2);
	double mean_error = 0.5 * fabs(mean2 - mean1);

	sd = 0.5 * (sd1 + sd2);
	double sd_error = 0.5 * fabs(sd1 - sd2);

	printf("Relative to an ideal normal distribution:\n");
	printf("mean:  %lf error: %lf\n", mean, mean_error);
	printf("stddev: %lf error: %lf\n", sd, sd_error);
	printf("Simulation error: %lf\n", sim_error);

	return 0;
}
