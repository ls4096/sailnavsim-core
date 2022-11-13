/**
 * Copyright (C) 2020-2022 ls4096 <ls4096@8bitbyte.ca>
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
#include <string.h>
#include <time.h>

#include <sailnavsim_rustlib.h>

#include "tests.h"
#include "tests_assert.h"

#include "BoatRegistry.h"


int test_BoatRegistry_runBasic()
{
	unsigned int boatCount = -1;
	BoatEntry* entry;
	void* iterator;
	int rc;

	rc = BoatRegistry_init();
	EQUALS(rc, 0);


	// No boats
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_TRUE(entry == 0);
	EQUALS(boatCount, 0);

	// Add boat
	Boat* b = Boat_new(0.0, 0.0, 0, 0);
	rc = BoatRegistry_add(b, "TestBoat0", 0, 0);
	EQUALS(BoatRegistry_OK, rc);

	// 1 boat
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_FALSE(entry == 0);
	EQUALS(boatCount, 1);

	// Get boat
	b = BoatRegistry_get("TestBoat0");
	EQUALS_DBL(0.0, b->pos.lat);
	EQUALS_DBL(0.0, b->pos.lon);
	EQUALS(0, b->boatType);
	EQUALS(0, b->boatFlags);

	// Remove boat
	// 0 boats remain
	b = BoatRegistry_remove("TestBoat0");
	EQUALS_DBL(0.0, b->pos.lat);
	EQUALS_DBL(0.0, b->pos.lon);
	EQUALS(0, b->boatType);
	EQUALS(0, b->boatFlags);
	free(b);

	// Get boat that doesn't exist
	b = BoatRegistry_get("TestBoat0");
	IS_TRUE(b == 0);

	// Get boat that doesn't exist
	b = BoatRegistry_get("TestBoat1");
	IS_TRUE(b == 0);

	// No boats
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_TRUE(entry == 0);
	EQUALS(boatCount, 0);


	// Add boat
	b = Boat_new(0.1, 0.1, 0, 0);
	rc = BoatRegistry_add(b, "TestBoat0", 0, 0);
	EQUALS(BoatRegistry_OK, rc);

	// 1 boat
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_FALSE(entry == 0);
	EQUALS(boatCount, 1);

	// Try to add boat with same name
	b = Boat_new(0.9, 0.9, 0, 0);
	rc = BoatRegistry_add(b, "TestBoat0", 0, 0);
	EQUALS(BoatRegistry_EXISTS, rc);
	free(b);

	// Still 1 boat
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_FALSE(entry == 0);
	EQUALS(boatCount, 1);

	// Get boat
	b = BoatRegistry_get("TestBoat0");
	EQUALS_DBL(0.1, b->pos.lat);
	EQUALS_DBL(0.1, b->pos.lon);
	EQUALS(0, b->boatType);
	EQUALS(0, b->boatFlags);

	// Add new boat
	b = Boat_new(1.0, 1.0, 0, 0);
	rc = BoatRegistry_add(b, "TestBoat1", 0, 0);
	EQUALS(BoatRegistry_OK, rc);

	// 2 boats now
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_FALSE(entry == 0);
	EQUALS(boatCount, 2);

	// Remove boat
	// 1 boat remains
	b = BoatRegistry_remove("TestBoat0");
	EQUALS_DBL(0.1, b->pos.lat);
	EQUALS_DBL(0.1, b->pos.lon);
	EQUALS(0, b->boatType);
	EQUALS(0, b->boatFlags);
	free(b);

	// Get boat that doesn't exist
	b = BoatRegistry_get("TestBoat0");
	IS_TRUE(b == 0);

	// Still 1 boat
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_FALSE(entry == 0);
	EQUALS(boatCount, 1);

	// Remove boat that doesn't exist
	// Still 1 boat remains
	b = BoatRegistry_remove("TestBoat0");
	IS_TRUE(b == 0);

	// Remove boat
	// 0 boats remain
	b = BoatRegistry_remove("TestBoat1");
	EQUALS_DBL(1.0, b->pos.lat);
	EQUALS_DBL(1.0, b->pos.lon);
	EQUALS(0, b->boatType);
	EQUALS(0, b->boatFlags);
	free(b);

	// Get boat that doesn't exist
	b = BoatRegistry_get("TestBoat0");
	IS_TRUE(b == 0);

	// Get boat that doesn't exist
	b = BoatRegistry_get("TestBoat1");
	IS_TRUE(b == 0);

	// No boats
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_TRUE(entry == 0);
	EQUALS(boatCount, 0);


	BoatRegistry_destroy();


	return 0;
}


int test_BoatRegistry_runBasicWithGroups()
{
	unsigned int boatCount = -1;
	BoatEntry* entry;
	void* iterator;
	int rc;

	rc = BoatRegistry_init();
	EQUALS(rc, 0);


	// No boats
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_TRUE(entry == 0);
	EQUALS(boatCount, 0);

	// Add boat
	Boat* b = Boat_new(0.0, 0.0, 0, 0);
	rc = BoatRegistry_add(b, "TestBoat0", "TestGroup0", 0);
	EQUALS(BoatRegistry_OK, rc);

	// 1 boat
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_FALSE(entry == 0);
	EQUALS(boatCount, 1);

	// Get boat
	b = BoatRegistry_get("TestBoat0");
	EQUALS_DBL(0.0, b->pos.lat);
	EQUALS_DBL(0.0, b->pos.lon);
	EQUALS(0, b->boatType);
	EQUALS(0, b->boatFlags);

	// Remove boat
	// 0 boats remain
	b = BoatRegistry_remove("TestBoat0");
	EQUALS_DBL(0.0, b->pos.lat);
	EQUALS_DBL(0.0, b->pos.lon);
	EQUALS(0, b->boatType);
	EQUALS(0, b->boatFlags);
	free(b);

	// Get boat that doesn't exist
	b = BoatRegistry_get("TestBoat0");
	IS_TRUE(b == 0);

	// Get boat that doesn't exist
	b = BoatRegistry_get("TestBoat1");
	IS_TRUE(b == 0);

	// No boats
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_TRUE(entry == 0);
	EQUALS(boatCount, 0);


	// Add boat
	b = Boat_new(0.1, 0.1, 0, 0);
	rc = BoatRegistry_add(b, "TestBoat0", "TestGroup1", "TestBoatAlt0");
	EQUALS(BoatRegistry_OK, rc);

	// 1 boat
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_FALSE(entry == 0);
	EQUALS(boatCount, 1);

	// Try to add boat with same name
	b = Boat_new(0.9, 0.9, 0, 0);
	rc = BoatRegistry_add(b, "TestBoat0", "TestGroup2", 0);
	EQUALS(BoatRegistry_EXISTS, rc);
	free(b);

	// Still 1 boat
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_FALSE(entry == 0);
	EQUALS(boatCount, 1);

	// Get boat
	b = BoatRegistry_get("TestBoat0");
	EQUALS_DBL(0.1, b->pos.lat);
	EQUALS_DBL(0.1, b->pos.lon);
	EQUALS(0, b->boatType);
	EQUALS(0, b->boatFlags);

	// Add new boat
	b = Boat_new(1.0, 1.0, 0, 0);
	rc = BoatRegistry_add(b, "TestBoat1", "TestGroup3", 0);
	EQUALS(BoatRegistry_OK, rc);

	// 2 boats now
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_FALSE(entry == 0);
	EQUALS(boatCount, 2);

	// Remove boat
	// 1 boat remains
	b = BoatRegistry_remove("TestBoat0");
	EQUALS_DBL(0.1, b->pos.lat);
	EQUALS_DBL(0.1, b->pos.lon);
	EQUALS(0, b->boatType);
	EQUALS(0, b->boatFlags);
	free(b);

	// Get boat that doesn't exist
	b = BoatRegistry_get("TestBoat0");
	IS_TRUE(b == 0);

	// Still 1 boat
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_FALSE(entry == 0);
	EQUALS(boatCount, 1);

	// Remove boat that doesn't exist
	// Still 1 boat remains
	b = BoatRegistry_remove("TestBoat0");
	IS_TRUE(b == 0);

	// Remove boat
	// 0 boats remain
	b = BoatRegistry_remove("TestBoat1");
	EQUALS_DBL(1.0, b->pos.lat);
	EQUALS_DBL(1.0, b->pos.lon);
	EQUALS(0, b->boatType);
	EQUALS(0, b->boatFlags);
	free(b);

	// Get boat that doesn't exist
	b = BoatRegistry_get("TestBoat0");
	IS_TRUE(b == 0);

	// Get boat that doesn't exist
	b = BoatRegistry_get("TestBoat1");
	IS_TRUE(b == 0);

	// No boats
	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	IS_TRUE(entry == 0);
	EQUALS(boatCount, 0);


	BoatRegistry_destroy();


	return 0;
}


#define LOAD_BOAT_COUNT_MAX (2500)
#define LOAD_ITERATIONS (2500)

static int verifyLoadBoatRegistry(const bool* boatList, bool withGroups);
static int verifyBoatsInGroupResponse(const bool* boatList, const char* groupName, const char* resp);

static int getRandInt(int max);
static unsigned int _initRandSeed = 0;
static unsigned int _randSeed = 0;

static double getBoatLatForR(int r);
static double getBoatLonForR(int r);

int test_BoatRegistry_runLoad()
{
	bool boatList[LOAD_BOAT_COUNT_MAX];
	memset(boatList, 0, LOAD_BOAT_COUNT_MAX * sizeof(bool));

	char boatName[32];
	Boat* b;
	Boat* b2;
	int rc;
	unsigned int boatCount;

	int addOk = 0;
	int addExists = 0;
	int removeOk = 0;
	int removeNotExists = 0;

	rc = BoatRegistry_init();
	EQUALS(rc, 0);


	// Load up the BoatRegistry, adding and removing boats at random.
	for (unsigned int i = 0; i < LOAD_ITERATIONS; i++)
	{
		const int r = getRandInt(LOAD_BOAT_COUNT_MAX - 1);

		if (getRandInt(10) < 8)
		{
			// Add random boat.

			b = Boat_new(getBoatLatForR(r), getBoatLonForR(r), 0, 0);

			sprintf(boatName, "Boat%d", r);
			rc = BoatRegistry_add(b, boatName, 0, 0);
			if (boatList[r])
			{
				// Boat already exists.
				EQUALS(BoatRegistry_EXISTS, rc);
				free(b);

				addExists++;
			}
			else
			{
				// Boat was added.
				EQUALS(BoatRegistry_OK, rc);
				boatList[r] = true;

				addOk++;
			}
		}
		else
		{
			// Remove random boat.

			sprintf(boatName, "Boat%d", r);
			b = BoatRegistry_get(boatName);
			b2 = BoatRegistry_remove(boatName);

			EQUALS(b, b2);

			if (boatList[r])
			{
				// Boat existed and was removed.
				IS_TRUE(b != 0);
				boatList[r] = false;
				free(b);

				removeOk++;
			}
			else
			{
				// Boat does not exist.
				IS_TRUE(b == 0);

				removeNotExists++;
			}
		}

		if (0 != verifyLoadBoatRegistry(boatList, false))
		{
			return 1;
		}

		// Boat count must equal successful adds minus successful removes.
		void* iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
		sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
		EQUALS(addOk - removeOk, (int)boatCount);
	}

	printf("\t\t_initRandSeed:\t\t%d\n", _initRandSeed);
	printf("\t\tfinalBoatCount:\t\t%d\n", boatCount);
	printf("\t\taddOk:\t\t\t%d\n", addOk);
	printf("\t\taddExists:\t\t%d\n", addExists);
	printf("\t\tremoveOk:\t\t%d\n", removeOk);
	printf("\t\tremoveNotExists:\t%d\n", removeNotExists);


	// Remove remaining boats.
	for (unsigned int i = 0; i < LOAD_BOAT_COUNT_MAX; i++)
	{
		if (boatList[i])
		{
			sprintf(boatName, "Boat%u", i);

			b = BoatRegistry_remove(boatName);

			IS_TRUE(b != 0);
			boatList[i] = false;
			free(b);
		}
	}

	void* iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	BoatEntry* entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	EQUALS(0, boatCount);
	IS_TRUE(entry == 0);


	BoatRegistry_destroy();


	_initRandSeed = 0;
	return 0;
}

static int verifyLoadBoatRegistry(const bool* boatList, bool withGroups)
{
	bool localBoatList[LOAD_BOAT_COUNT_MAX];
	memset(localBoatList, 0, LOAD_BOAT_COUNT_MAX * sizeof(bool));

	char boatName[32];
	Boat* b;
	unsigned int count = 0;

	// Build our local list of boats that exist.
	for (unsigned int i = 0; i < LOAD_BOAT_COUNT_MAX; i++)
	{
		sprintf(boatName, "Boat%u", i);
		if ((b = BoatRegistry_get(boatName)))
		{
			EQUALS_DBL(b->pos.lat, getBoatLatForR(i));
			EQUALS_DBL(b->pos.lon, getBoatLonForR(i));

			localBoatList[i] = true;
			count++;
		}
	}

	// Ensure that our local list matches list from caller.
	for (unsigned int i = 0; i < LOAD_BOAT_COUNT_MAX; i++)
	{
		EQUALS(boatList[i], localBoatList[i]);
	}


	// Get entries, and check boat count.
	unsigned int boatCount;
	void* iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	BoatEntry* entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	if (count == 0)
	{
		IS_TRUE(entry == 0);
	}
	else
	{
		IS_FALSE(entry == 0);
	}

	EQUALS(boatCount, count);


	// Iterate through entries and mark which boats exist.
	memset(localBoatList, 0, LOAD_BOAT_COUNT_MAX * sizeof(bool));
	while (entry != 0)
	{
		int boatNum = atoi(entry->name + strlen("Boat"));
		localBoatList[boatNum] = true;

		EQUALS_DBL(entry->boat->pos.lat, getBoatLatForR(boatNum));
		EQUALS_DBL(entry->boat->pos.lon, getBoatLonForR(boatNum));

		entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	}

	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);

	// Ensure that the list of boats from entry iteration matches list from caller.
	for (unsigned int i = 0; i < LOAD_BOAT_COUNT_MAX; i++)
	{
		EQUALS(boatList[i], localBoatList[i]);
	}

	if (withGroups)
	{
		// Also check boat group membership for a random group.

		char groupName[16];
		sprintf(groupName, "Group%d", getRandInt(99));

		const char* groupResp = BoatRegistry_getBoatsInGroupResponse(groupName);

		if (0 != verifyBoatsInGroupResponse(boatList, groupName, groupResp))
		{
			return 1;
		}

		BoatRegistry_freeBoatsInGroupResponse(groupResp);
	}

	return 0;
}

static int getRandInt(int max)
{
	if (_initRandSeed == 0)
	{
		_initRandSeed = time(0);
		_randSeed = _initRandSeed;
	}

	return (rand_r(&_randSeed) % (max + 1));
}

static double getBoatLatForR(int r)
{
	return (((double)r / (double)LOAD_BOAT_COUNT_MAX) * 170.0 - 85.0);
}

static double getBoatLonForR(int r)
{
	return (((double)r / (double)LOAD_BOAT_COUNT_MAX) * 340.0 - 170.0);
}


int test_BoatRegistry_runLoadWithBigGroups()
{
	bool boatList[LOAD_BOAT_COUNT_MAX];
	memset(boatList, 0, LOAD_BOAT_COUNT_MAX * sizeof(bool));

	char boatName[32];
	char groupName[32];
	char boatAltName[32];
	Boat* b;
	Boat* b2;
	int rc;
	unsigned int boatCount;

	int addOk = 0;
	int addExists = 0;
	int removeOk = 0;
	int removeNotExists = 0;

	rc = BoatRegistry_init();
	EQUALS(rc, 0);


	// Load up the BoatRegistry, adding and removing boats at random.
	for (unsigned int i = 0; i < LOAD_ITERATIONS; i++)
	{
		const int r = getRandInt(LOAD_BOAT_COUNT_MAX - 1);

		if (getRandInt(10) < 8)
		{
			// Add random boat.

			b = Boat_new(getBoatLatForR(r), getBoatLonForR(r), 0, 0);

			sprintf(boatName, "Boat%d", r);
			sprintf(groupName, "Group%d", r % 100);
			sprintf(boatAltName, "BoatAltName%d", r % 127);
			rc = BoatRegistry_add(b, boatName, groupName, ((r % 3 == 0) ? 0 : boatAltName));
			if (boatList[r])
			{
				// Boat already exists.
				EQUALS(BoatRegistry_EXISTS, rc);
				free(b);

				addExists++;
			}
			else
			{
				// Boat was added.
				EQUALS(BoatRegistry_OK, rc);
				boatList[r] = true;

				addOk++;
			}
		}
		else
		{
			// Remove random boat.

			sprintf(boatName, "Boat%d", r);
			b = BoatRegistry_get(boatName);
			b2 = BoatRegistry_remove(boatName);

			EQUALS(b, b2);

			if (boatList[r])
			{
				// Boat existed and was removed.
				IS_TRUE(b != 0);
				boatList[r] = false;
				free(b);

				removeOk++;
			}
			else
			{
				// Boat does not exist.
				IS_TRUE(b == 0);

				removeNotExists++;
			}
		}

		if (0 != verifyLoadBoatRegistry(boatList, true))
		{
			return 1;
		}

		// Boat count must equal successful adds minus successful removes.
		void* iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
		sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
		EQUALS(addOk - removeOk, (int)boatCount);
	}

	printf("\t\t_initRandSeed:\t\t%d\n", _initRandSeed);
	printf("\t\tfinalBoatCount:\t\t%d\n", boatCount);
	printf("\t\taddOk:\t\t\t%d\n", addOk);
	printf("\t\taddExists:\t\t%d\n", addExists);
	printf("\t\tremoveOk:\t\t%d\n", removeOk);
	printf("\t\tremoveNotExists:\t%d\n", removeNotExists);


	// Remove remaining boats.
	for (unsigned int i = 0; i < LOAD_BOAT_COUNT_MAX; i++)
	{
		if (boatList[i])
		{
			sprintf(boatName, "Boat%u", i);

			b = BoatRegistry_remove(boatName);

			IS_TRUE(b != 0);
			boatList[i] = false;
			free(b);
		}
	}

	void* iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	BoatEntry* entry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);
	EQUALS(0, boatCount);
	IS_TRUE(entry == 0);


	BoatRegistry_destroy();


	_initRandSeed = 0;
	return 0;
}

static int verifyBoatsInGroupResponse(const bool* boatList, const char* groupName, const char* resp)
{
	bool localBoatList[LOAD_BOAT_COUNT_MAX / 100];
	memset(localBoatList, 0, (LOAD_BOAT_COUNT_MAX / 100) * sizeof(bool));

	const int groupNum = atoi(groupName + 5);

	char* s = strdup(resp);
	char* t;
	char* u;

	char boatAltName[32];

	u = strtok_r(s, ",", &t);
	while (u)
	{
		int boatNum = atoi(u + 4);
		EQUALS(boatNum % 100, groupNum);
		localBoatList[boatNum / 100] = true;

		u = strtok_r(0, "\n", &t);
		IS_TRUE(u != 0);
		if ((boatNum % 3) != 0)
		{
			sprintf(boatAltName, "BoatAltName%d", boatNum % 127);
			IS_TRUE(0 == strcmp(u, boatAltName));
		}
		else
		{
			IS_TRUE((u[0] == '!') && (u[1] == 0));
		}

		u = strtok_r(0, ",", &t);
	}

	for (int i = 0; i < LOAD_BOAT_COUNT_MAX / 100; i++)
	{
		EQUALS(boatList[i * 100 + groupNum], localBoatList[i]);
	}

	free(s);
	return 0;
}
