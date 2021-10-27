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

static unsigned int _randSeed = 314159265;


#define RANDOM_NAME_LEN (32)
static const char* RANDOM_NAME_CHARS = "0123456789abcdef";

char* PerfUtils_getRandomName()
{
	char* name = malloc(RANDOM_NAME_LEN + 1);

	for (int i = 0; i < RANDOM_NAME_LEN; i++)
	{
		name[i] = RANDOM_NAME_CHARS[getRandInt(15)];
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


static int getRandInt(int max)
{
	return (rand_r(&_randSeed) % (max + 1));
}
