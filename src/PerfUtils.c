/**
 * Copyright (C) 2020-2021 ls4096 <ls4096@8bitbyte.ca>
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

#include <stdlib.h>

#include "PerfUtils.h"


static int getRandInt(int max);
static int getRandInt2(int max);
static int getRandInt3(int max);


#define RANDOM_NAME_LEN (32)
static const char* RANDOM_NAME_CHARS = "0123456789abcdef";

char* PerfUtils_getRandomName()
{
	char* name = malloc(RANDOM_NAME_LEN + 1);

	for (int i = 0; i < RANDOM_NAME_LEN; i++)
	{
		if (i % 6 == 0)
		{
			name[i] = RANDOM_NAME_CHARS[getRandInt3(15)];
		}
		else if (i % 4 == 0)
		{
			name[i] = RANDOM_NAME_CHARS[getRandInt2(15)];
		}
		else
		{
			name[i] = RANDOM_NAME_CHARS[getRandInt(15)];
		}
	}

	name[RANDOM_NAME_LEN] = 0;

	return name;
}

double PerfUtils_getRandomLat()
{
	return (((double) getRandInt(159000)) / 1000.0) - 79.0;
}

double PerfUtils_getRandomLon()
{
	return (((double) getRandInt(360000)) / 1000.0) - 180.0;
}

int PerfUtils_getRandomBoatType()
{
	return getRandInt(11);
}

int PerfUtils_getRandomBoatFlags()
{
	return getRandInt(0x001f);
}

int PerfUtils_getRandomCourse()
{
	return getRandInt(360);
}

bool PerfUtils_getRandomBool()
{
	return (getRandInt(1) == 1);
}

const char* PerfUtils_getRandomBoatGroupName()
{
	static const char* PERF_BOAT_GROUPS[] = {
		"G0",
		"G1",
		"G2",
		"G3",
		"G4",
		"G5",
		"G6",
		"G7",
		"G8",
		"G9",
		"G10",
		"G11",
	};

	return PERF_BOAT_GROUPS[getRandInt(11)];
}



static int getRandInt(int max)
{
	static unsigned int _randSeed = 314159265;
	return (rand_r(&_randSeed) % (max + 1));
}

static int getRandInt2(int max)
{
	static unsigned int _randSeed2 = 271828183;
	return (rand_r(&_randSeed2) % (max + 1));
}

static int getRandInt3(int max)
{
	static unsigned int _randSeed3 = 141421356;
	return (rand_r(&_randSeed3) % (max + 1));
}
