/**
 * Copyright (C) 2020 ls4096 <ls4096@8bitbyte.ca>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <math.h>

#include "BoatWindResponse.h"


/**
 * For a given "true wind speed" (TWS) and "true wind angle" (TWA), the wind
 * response factor is determined using bilinear interpolation on the wind speed
 * and wind angle between adjacent values in the lookup table.
 *
 * Speed through water (STW) is calculated by multiplying the wind speed (WS)
 * by the interpolated wind response factor.
 *
 * The overall calculation is as follows:
 * 	STW = TWS * INTERPOLATED_RESPONSE(TWS, TWA);
 *
 * All angle units are in degrees.
 * All speed units are in metres/second.
 */


/**
 * Wind response factor lookup table for the "SailNavSim Classic" sailing vessel
 */
static const double SAILNAVSIM_CLASSIC_RESPONSE[] =
{
//	1	2	4	8	12	16	24	m/s

	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	// 0 deg
	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	// 10 deg
	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	// 20 deg
	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	// 30 deg
	0.45,	0.58,	0.55,	0.36,	0.25,	0.17,	0.10,	// 40 deg
	0.52,	0.63,	0.63,	0.42,	0.30,	0.21,	0.12,	// 50 deg
	0.60,	0.68,	0.68,	0.45,	0.32,	0.22,	0.13,	// 60 deg
	0.62,	0.75,	0.69,	0.46,	0.33,	0.22,	0.14,	// 70 deg
	0.61,	0.78,	0.70,	0.47,	0.34,	0.23,	0.14,	// 80 deg
	0.60,	0.76,	0.71,	0.48,	0.34,	0.23,	0.14,	// 90 deg
	0.58,	0.74,	0.72,	0.48,	0.35,	0.23,	0.14,	// 100 deg
	0.55,	0.71,	0.72,	0.49,	0.35,	0.23,	0.15,	// 110 deg
	0.53,	0.68,	0.70,	0.49,	0.35,	0.24,	0.15,	// 120 deg
	0.51,	0.65,	0.68,	0.48,	0.35,	0.24,	0.15,	// 130 deg
	0.48,	0.60,	0.61,	0.47,	0.35,	0.25,	0.15,	// 140 deg
	0.45,	0.57,	0.58,	0.45,	0.34,	0.25,	0.16,	// 150 deg
	0.43,	0.54,	0.54,	0.42,	0.33,	0.24,	0.16,	// 160 deg
	0.41,	0.52,	0.52,	0.40,	0.32,	0.23,	0.15,	// 170 deg
	0.39,	0.50,	0.50,	0.37,	0.30,	0.20,	0.13,	// 180 deg

	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	// Values on these two lines are never used, but we add them
	0.00							// here to prevent reading garbage in calculations below.
};

#define SAILNAVSIM_CLASSIC_COURSE_CHANGE_RATE (3.0)
#define SAILNAVSIM_CLASSIC_BOAT_INERTIA (20.0)


/**
 * Wind response factor lookup table for the "Seascape 18" sailing vessel
 * Derived and approximated from ORC data (sail number: NOR/NOR15672)
 */
static const double SEASCAPE_18_RESPONSE[] =
{
//	1	2	4	8	12	16	24	m/s

	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	// 0 deg
	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	// 10 deg
	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	// 20 deg
	0.400,	0.400,	0.250,	0.200,	0.180,	0.139,	0.092,	// 30 deg
	0.620,	0.620,	0.595,	0.350,	0.290,	0.226,	0.149,	// 40 deg
	0.755,	0.755,	0.668,	0.394,	0.317,	0.246,	0.162,	// 50 deg
	0.792,	0.792,	0.688,	0.417,	0.337,	0.261,	0.172,	// 60 deg
	0.811,	0.811,	0.698,	0.444,	0.359,	0.278,	0.183,	// 70 deg
	0.826,	0.826,	0.712,	0.469,	0.386,	0.300,	0.198,	// 80 deg
	0.837,	0.837,	0.730,	0.490,	0.420,	0.325,	0.214,	// 90 deg
	0.841,	0.841,	0.733,	0.515,	0.451,	0.350,	0.231,	// 100 deg
	0.845,	0.845,	0.736,	0.540,	0.483,	0.374,	0.247,	// 110 deg
	0.818,	0.818,	0.721,	0.575,	0.546,	0.423,	0.279,	// 120 deg
	0.767,	0.767,	0.692,	0.540,	0.602,	0.467,	0.308,	// 130 deg
	0.706,	0.706,	0.652,	0.497,	0.594,	0.461,	0.304,	// 140 deg
	0.635,	0.635,	0.602,	0.447,	0.523,	0.405,	0.267,	// 150 deg
	0.555,	0.555,	0.525,	0.385,	0.465,	0.360,	0.249,	// 160 deg
	0.525,	0.525,	0.475,	0.355,	0.440,	0.341,	0.237,	// 170 deg
	0.475,	0.475,	0.445,	0.338,	0.425,	0.329,	0.228,	// 180 deg

	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	// Values on these two lines are never used, but we add them
	0.00							// here to prevent reading garbage in calculations below.
};

#define SEASCAPE_18_COURSE_CHANGE_RATE (6.0)
#define SEASCAPE_18_BOAT_INERTIA (12.0)


/**
 * Wind response factor lookup table for the "Contessa 25" sailing vessel
 * Derived and approximated from ORC data (sail number: GRE/GRE1417)
 */
static const double CONTESSA_25_RESPONSE[] =
{
//	1	2	4	8	12	16	24	m/s

	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	// 0 deg
	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	// 10 deg
	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	// 20 deg
	0.100,	0.100,	0.080,	0.050,	0.040,	0.032,	0.022,	// 30 deg
	0.580,	0.580,	0.530,	0.350,	0.280,	0.223,	0.152,	// 40 deg
	0.693,	0.693,	0.618,	0.382,	0.301,	0.241,	0.164,	// 50 deg
	0.727,	0.727,	0.651,	0.391,	0.310,	0.248,	0.169,	// 60 deg
	0.743,	0.743,	0.665,	0.398,	0.320,	0.256,	0.175,	// 70 deg
	0.753,	0.753,	0.678,	0.404,	0.327,	0.262,	0.179,	// 80 deg
	0.757,	0.757,	0.689,	0.409,	0.331,	0.265,	0.181,	// 90 deg
	0.760,	0.760,	0.691,	0.418,	0.341,	0.273,	0.186,	// 100 deg
	0.763,	0.763,	0.694,	0.428,	0.351,	0.280,	0.192,	// 110 deg
	0.735,	0.735,	0.675,	0.425,	0.357,	0.285,	0.195,	// 120 deg
	0.692,	0.692,	0.635,	0.416,	0.350,	0.280,	0.192,	// 130 deg
	0.639,	0.639,	0.590,	0.403,	0.338,	0.271,	0.184,	// 140 deg
	0.578,	0.578,	0.538,	0.383,	0.320,	0.256,	0.175,	// 150 deg
	0.490,	0.490,	0.465,	0.363,	0.315,	0.252,	0.173,	// 160 deg
	0.440,	0.440,	0.417,	0.348,	0.305,	0.244,	0.167,	// 170 deg
	0.400,	0.400,	0.386,	0.353,	0.305,	0.244,	0.167,	// 180 deg

	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	// Values on these two lines are never used, but we add them
	0.00							// here to prevent reading garbage in calculations below.
};

#define CONTESSA_25_COURSE_CHANGE_RATE (3.0)
#define CONTESSA_25_BOAT_INERTIA (20.0)


/**
 * Wind response factor lookup table for the "Hanse 385" sailing vessel
 * Derived and approximated from ORC data (sail number: NOR/NOR14873)
 */
static const double HANSE_385_RESPONSE[] =
{
//	1	2	4	8	12	16	24	m/s

	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	// 0 deg
	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	// 10 deg
	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	// 20 deg
	0.200,	0.200,	0.180,	0.150,	0.120,	0.097,	0.067,	// 30 deg
	0.660,	0.660,	0.620,	0.400,	0.320,	0.256,	0.175,	// 40 deg
	0.835,	0.835,	0.758,	0.472,	0.369,	0.295,	0.201,	// 50 deg
	0.910,	0.910,	0.819,	0.489,	0.383,	0.307,	0.209,	// 60 deg
	0.960,	0.960,	0.855,	0.503,	0.396,	0.317,	0.217,	// 70 deg
	0.985,	0.985,	0.873,	0.515,	0.411,	0.329,	0.224,	// 80 deg
	0.985,	0.985,	0.872,	0.523,	0.427,	0.341,	0.234,	// 90 deg
	0.945,	0.945,	0.853,	0.531,	0.438,	0.351,	0.239,	// 100 deg
	0.905,	0.905,	0.834,	0.539,	0.450,	0.360,	0.245,	// 110 deg
	0.873,	0.873,	0.806,	0.534,	0.458,	0.367,	0.250,	// 120 deg
	0.812,	0.812,	0.755,	0.521,	0.447,	0.357,	0.244,	// 130 deg
	0.741,	0.741,	0.698,	0.503,	0.428,	0.342,	0.234,	// 140 deg
	0.660,	0.660,	0.632,	0.478,	0.402,	0.321,	0.219,	// 150 deg
	0.575,	0.575,	0.545,	0.450,	0.391,	0.311,	0.213,	// 160 deg
	0.500,	0.500,	0.488,	0.428,	0.383,	0.302,	0.206,	// 170 deg
	0.440,	0.440,	0.450,	0.425,	0.380,	0.300,	0.204,	// 180 deg

	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	// Values on these two lines are never used, but we add them
	0.00							// here to prevent reading garbage in calculations below.
};

#define HANSE_385_COURSE_CHANGE_RATE (2.75)
#define HANSE_385_BOAT_INERTIA (22.5)


/**
 * Wind response factor lookup table for the "Volvo 70" sailing vessel
 * Derived and approximated from ORC data (sail number: AUS/ITA70)
 */
static const double VOLVO_70_RESPONSE[] =
{
//	1	2	4	8	12	16	24	m/s

	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	// 0 deg
	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	// 10 deg
	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	// 20 deg
	0.300,	0.300,	0.333,	0.400,	0.280,	0.217,	0.141,	// 30 deg
	1.240,	1.240,	1.100,	0.780,	0.512,	0.396,	0.258,	// 40 deg
	1.442,	1.442,	1.330,	0.868,	0.595,	0.461,	0.300,	// 50 deg
	1.562,	1.562,	1.396,	0.931,	0.647,	0.500,	0.326,	// 60 deg
	1.634,	1.634,	1.459,	1.022,	0.706,	0.547,	0.356,	// 70 deg
	1.697,	1.697,	1.520,	1.098,	0.752,	0.581,	0.378,	// 80 deg
	1.750,	1.750,	1.580,	1.159,	0.783,	0.605,	0.394,	// 90 deg
	1.737,	1.737,	1.570,	1.179,	0.826,	0.639,	0.416,	// 100 deg
	1.723,	1.723,	1.560,	1.199,	0.870,	0.673,	0.438,	// 110 deg
	1.642,	1.642,	1.474,	1.220,	0.886,	0.685,	0.446,	// 120 deg
	1.446,	1.446,	1.338,	1.129,	0.887,	0.686,	0.447,	// 130 deg
	1.266,	1.266,	1.192,	1.020,	0.836,	0.647,	0.421,	// 140 deg
	1.102,	1.102,	1.037,	0.892,	0.730,	0.565,	0.368,	// 150 deg
	0.920,	0.920,	0.927,	0.795,	0.651,	0.504,	0.328,	// 160 deg
	0.860,	0.860,	0.880,	0.757,	0.615,	0.476,	0.309,	// 170 deg
	0.833,	0.833,	0.862,	0.742,	0.600,	0.464,	0.302,	// 180 deg

	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	// Values on these two lines are never used, but we add them
	0.00							// here to prevent reading garbage in calculations below.
};

#define VOLVO_70_COURSE_CHANGE_RATE (2.25)
#define VOLVO_70_BOAT_INERTIA (30.0)


/**
 * Wind response factor lookup table for the "Super Maxi - Scallywag" sailing vessel
 * Derived and approximated from ORC data (sail number: AUS/HKG2276)
 */
static const double SUPER_MAXI_SCALLYWAG_RESPONSE[] =
{
//	1	2	4	8	12	16	24	m/s

	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	// 0 deg
	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	// 10 deg
	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	// 20 deg
	0.400,	0.400,	0.450,	0.550,	0.400,	0.310,	0.196,	// 30 deg
	1.510,	1.510,	1.400,	0.950,	0.580,	0.449,	0.284,	// 40 deg
	1.867,	1.867,	1.628,	1.012,	0.674,	0.521,	0.330,	// 50 deg
	2.020,	2.020,	1.712,	1.079,	0.728,	0.563,	0.356,	// 60 deg
	2.131,	2.131,	1.812,	1.174,	0.801,	0.620,	0.392,	// 70 deg
	2.193,	2.193,	1.884,	1.245,	0.859,	0.665,	0.420,	// 80 deg
	2.205,	2.205,	1.929,	1.292,	0.902,	0.698,	0.441,	// 90 deg
	2.152,	2.152,	1.884,	1.325,	0.915,	0.708,	0.447,	// 100 deg
	2.098,	2.098,	1.839,	1.358,	0.928,	0.718,	0.454,	// 110 deg
	2.028,	2.028,	1.822,	1.356,	0.959,	0.742,	0.469,	// 120 deg
	1.873,	1.873,	1.709,	1.331,	0.954,	0.738,	0.466,	// 130 deg
	1.682,	1.682,	1.563,	1.257,	0.924,	0.715,	0.452,	// 140 deg
	1.457,	1.457,	1.384,	1.134,	0.866,	0.670,	0.424,	// 150 deg
	1.135,	1.135,	1.130,	0.986,	0.777,	0.617,	0.390,	// 160 deg
	0.997,	0.997,	0.990,	0.862,	0.699,	0.555,	0.360,	// 170 deg
	0.928,	0.928,	0.900,	0.778,	0.634,	0.518,	0.335,	// 180 deg

	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	// Values on these two lines are never used, but we add them
	0.00							// here to prevent reading garbage in calculations below.
};

#define SUPER_MAXI_SCALLYWAG_COURSE_CHANGE_RATE (2.25)
#define SUPER_MAXI_SCALLYWAG_BOAT_INERTIA (32.0)


/**
 * Wind response factor lookup table for the "140-foot Brigantine" sailing vessel
 * Approximated from a polar plot for the STS Young Endeavour.
 */
static const double BRIGANTINE_140_RESPONSE[] =
{
//	1	2	4	8	12	16	24	m/s

	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	// 0 deg
	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	// 10 deg
	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	// 20 deg
	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	// 30 deg
	0.122,	0.122,	0.092,	0.073,	0.056,	0.042,	0.030,	// 40 deg
	0.533,	0.533,	0.401,	0.321,	0.273,	0.247,	0.176,	// 50 deg
	0.704,	0.704,	0.530,	0.424,	0.367,	0.319,	0.228,	// 60 deg
	0.782,	0.782,	0.588,	0.471,	0.394,	0.331,	0.236,	// 70 deg
	0.882,	0.882,	0.663,	0.531,	0.433,	0.350,	0.249,	// 80 deg
	0.910,	0.910,	0.684,	0.547,	0.442,	0.356,	0.253,	// 90 deg
	0.943,	0.943,	0.709,	0.567,	0.448,	0.360,	0.256,	// 100 deg
	0.977,	0.977,	0.734,	0.588,	0.468,	0.372,	0.265,	// 110 deg
	0.999,	0.999,	0.751,	0.601,	0.477,	0.378,	0.269,	// 120 deg
	1.016,	1.016,	0.764,	0.611,	0.485,	0.389,	0.277,	// 130 deg
	1.010,	1.010,	0.760,	0.608,	0.491,	0.417,	0.297,	// 140 deg
	0.977,	0.977,	0.735,	0.588,	0.474,	0.406,	0.289,	// 150 deg
	0.916,	0.916,	0.689,	0.551,	0.444,	0.381,	0.271,	// 160 deg
	0.850,	0.850,	0.639,	0.511,	0.403,	0.336,	0.239,	// 170 deg
	0.833,	0.833,	0.626,	0.501,	0.390,	0.322,	0.230,	// 180 deg

	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	// Values on these two lines are never used, but we add them
	0.00							// here to prevent reading garbage in calculations below.
};

#define BRIGANTINE_140_COURSE_CHANGE_RATE (1.25)
#define BRIGANTINE_140_BOAT_INERTIA (45.0)

/**
 * Wind response factor lookup table for the "Maxi Trimaran" sailing vessel
 * Approximated from a polar an approximate polar plot
 */
static const double MAXI_TRIMARAN_RESPONSE[] =
{
//	1	2	4	8	12	16	24	m/s

	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,	-0.10,
	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,	-0.08,
	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,	-0.05,
	1.37,	1.33,	1.12,	0.67,	0.50,	0.38,	0.22,
	2.01,	2.02,	1.66,	1.00,	0.76,	0.58,	0.33,
	2.38,	2.41,	1.76,	1.10,	0.84,	0.65,	0.38,
	2.66,	2.70,	1.87,	1.18,	0.91,	0.73,	0.43,
	2.92,	2.85,	1.96,	1.25,	1.01,	0.83,	0.51,
	3.06,	2.96,	2.14,	1.38,	1.14,	0.95,	0.56,
	3.06,	2.96,	2.19,	1.45,	1.26,	1.05,	0.61,
	2.92,	2.85,	2.14,	1.55,	1.34,	1.07,	0.60,
	2.64,	2.67,	2.17,	1.59,	1.35,	1.11,	0.65,
	2.59,	2.59,	2.14,	1.59,	1.37,	1.17,	0.69,
	2.38,	2.34,	2.01,	1.61,	1.39,	1.21,	0.72,
	2.01,	1.98,	1.80,	1.53,	1.40,	1.23,	0.78,
	1.58,	1.58,	1.53,	1.31,	1.31,	1.30,	0.77,
	1.30,	1.26,	1.26,	1.16,	1.11,	1.15,	0.74,
	1.10,	1.13,	1.13,	0.97,	0.92,	0.95,	0.62,
	0.92,	0.98,	0.96,	0.85,	0.81,	0.84,	0.51,
	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	0.00,	// Values on these two lines are never used, but we add them
	0.00							// here to prevent reading garbage in calculations below.
};

#define MAXI_TRIMARAN_COURSE_CHANGE_RATE (3.10)
#define MAXI_TRIMARAN_BOAT_INERTIA (25.0)

static const double* WIND_RESPONSES[] = {
	SAILNAVSIM_CLASSIC_RESPONSE, // 0
	SEASCAPE_18_RESPONSE, // 1
	CONTESSA_25_RESPONSE, // 2
	HANSE_385_RESPONSE, // 3
	VOLVO_70_RESPONSE, // 4
	SUPER_MAXI_SCALLYWAG_RESPONSE, // 5
	BRIGANTINE_140_RESPONSE, // 6
	MAXI_TRIMARAN_RESPONSE // 7
};

static const double COURSE_CHANGE_RATES[] = {
	SAILNAVSIM_CLASSIC_COURSE_CHANGE_RATE, // 0
	SEASCAPE_18_COURSE_CHANGE_RATE, // 1
	CONTESSA_25_COURSE_CHANGE_RATE, // 2
	HANSE_385_COURSE_CHANGE_RATE, // 3
	VOLVO_70_COURSE_CHANGE_RATE, // 4
	SUPER_MAXI_SCALLYWAG_COURSE_CHANGE_RATE, // 5
	BRIGANTINE_140_COURSE_CHANGE_RATE, // 6
	MAXI_TRIMARAN_COURSE_CHANGE_RATE // 7
};

static const double BOAT_INERTIAS[] = {
	SAILNAVSIM_CLASSIC_BOAT_INERTIA, // 0
	SEASCAPE_18_BOAT_INERTIA, // 1
	CONTESSA_25_BOAT_INERTIA, // 2
	HANSE_385_BOAT_INERTIA, // 3
	VOLVO_70_BOAT_INERTIA, // 4
	SUPER_MAXI_SCALLYWAG_BOAT_INERTIA, // 5
	BRIGANTINE_140_BOAT_INERTIA, // 6
	MAXI_TRIMARAN_BOAT_INERTIA // 7
};

static const int BOAT_TYPE_MAX = 7;


double BoatWindResponse_getBoatSpeed(double windSpd, double angleFromWind, int boatType)
{
	if (boatType > BOAT_TYPE_MAX)
	{
		// Any boat type that isn't modeled always just gets a zero speed.
		return 0.0;
	}

	while (angleFromWind > 180.0)
	{
		angleFromWind -= 180.0;
	}

	const double angle = fabs(angleFromWind);
	const int iAngle = ((int) angle) / 10;

	const double angleFrac = (angle - (iAngle * 10)) / 10.0;

	const int iWindSpd = (int) windSpd;

	int iSpd;
	double spdFrac;
	if (iWindSpd >= 24)
	{
		iSpd = 6;
		spdFrac = 0;
	}
	else if (iWindSpd >= 16)
	{
		iSpd = 5;
		spdFrac = (windSpd - 16.0) / 8.0;
	}
	else if (iWindSpd >= 12)
	{
		iSpd = 4;
		spdFrac = (windSpd - 12.0) / 4.0;
	}
	else if (iWindSpd >= 8)
	{
		iSpd = 3;
		spdFrac = (windSpd - 8.0) / 4.0;
	}
	else if (iWindSpd >= 4)
	{
		iSpd = 2;
		spdFrac = (windSpd - 4.0) / 4.0;
	}
	else if (iWindSpd >= 2)
	{
		iSpd = 1;
		spdFrac = (windSpd - 2.0) / 2.0;
	}
	else if (iWindSpd >= 1)
	{
		iSpd = 0;
		spdFrac = windSpd - 1.0;
	}
	else
	{
		iSpd = 0;
		spdFrac = 0;
	}

	const int base = iAngle * 7 + iSpd;

	const double* response = WIND_RESPONSES[boatType];

	const double r0 = response[base] * (1.0 - spdFrac) + response[base + 1] * spdFrac;
	const double r1 = response[base + 7] * (1.0 - spdFrac) + response[base + 8] * spdFrac;

	return windSpd * ((r0 * (1.0 - angleFrac)) + (r1 * angleFrac));
}

double BoatWindResponse_getCourseChangeRate(int boatType)
{
	if (boatType > BOAT_TYPE_MAX)
	{
		// Any boat type that isn't modeled always just gets a zero rate.
		return 0.0;
	}

	return COURSE_CHANGE_RATES[boatType];
}

double BoatWindResponse_getSpeedChangeResponse(int boatType)
{
	if (boatType > BOAT_TYPE_MAX)
	{
		// Any boat type that isn't modeled just has very large inertia.
		return 1.0e30;
	}

	return BOAT_INERTIAS[boatType];
}
