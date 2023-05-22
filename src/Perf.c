/**
 * Copyright (C) 2020-2023 ls4096 <ls4096@8bitbyte.ca>
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

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <proteus/Weather.h>
#include <proteus/Ocean.h>
#include <proteus/Wave.h>

#include <sailnavsim_rustlib.h>

#include "Perf.h"

#include "BoatRegistry.h"
#include "CelestialSight.h"
#include "ErrLog.h"
#include "GeoUtils.h"
#include "NetServer.h"


#define ERRLOG_ID "Perf"


#define PERF_CLOCK_INIT() struct timespec _perf_clock_t0; struct timespec _perf_clock_t1; long PERF_CLOCK_NS_TAKEN

#define PERF_CLOCK_RESET() do { \
	if (0 != clock_gettime(CLOCK_MONOTONIC, &_perf_clock_t0)) \
	{ \
		ERRLOG1("clock_gettime failed! errno=%d", errno); \
		return -1; \
	} \
} while (0)

#define PERF_CLOCK_MEASURE() do { \
	if (0 != clock_gettime(CLOCK_MONOTONIC, &_perf_clock_t1)) \
	{ \
		ERRLOG1("clock_gettime failed! errno=%d", errno); \
		return -1; \
	} \
	PERF_CLOCK_NS_TAKEN = (_perf_clock_t1.tv_nsec - _perf_clock_t0.tv_nsec) + 1000000000L * (_perf_clock_t1.tv_sec - _perf_clock_t0.tv_sec); \
} while (0)

// Kilo iterations per second
#define PERF_CLOCK_KIPS (((double) ITERATIONS) / (((double) PERF_CLOCK_NS_TAKEN) / 1000000.0))


static int runAddBoats(unsigned int boatCount);
static int runRemoveAllBoats(bool expectNullBoats);
static int runNetServerRequests(int netServerWriteFd, Perf_CommandHandlerFunc commandHandler);
static int runDataGets();

static char* getRandomName(unsigned int len);
static double getRandomLat();
static double getRandomLon();
static int getRandomBoatType();
static int getRandomBoatFlags();
static int getRandomCourse();
static bool getRandomBool();
static char* getRandomBoatGroupName();

static int getRandInt(int max);
static int getRandInt2(int max);
static int getRandInt3(int max);


#define PERF_RANDOM_BOAT_NAME_LEN (32)
#define PERF_RANDOM_BOAT_ALT_NAME_LEN (15)

void Perf_addAndStartRandomBoat(int groupNameLen, Perf_CommandHandlerFunc commandHandler)
{
	Command cmd;

	cmd.name = getRandomName(PERF_RANDOM_BOAT_NAME_LEN);
	cmd.next = 0;


	// Add boat.
	const bool withGroup = getRandomBool();
	cmd.action = (withGroup ? COMMAND_ACTION_ADD_BOAT_WITH_GROUP : COMMAND_ACTION_ADD_BOAT);
	cmd.values[0].d = getRandomLat();
	cmd.values[1].d = getRandomLon();
	cmd.values[2].i = getRandomBoatType();
	cmd.values[3].i = getRandomBoatFlags();
	if (withGroup)
	{
		if (groupNameLen <= 0)
		{
			cmd.values[4].s = getRandomBoatGroupName();
		}
		else
		{
			cmd.values[4].s = getRandomName(groupNameLen);
		}
		cmd.values[5].s = getRandomName(PERF_RANDOM_BOAT_ALT_NAME_LEN); // Boat alt name
	}

	commandHandler(&cmd);

	if (withGroup)
	{
		free(cmd.values[4].s);
		cmd.values[4].s = 0;

		free(cmd.values[5].s);
		cmd.values[5].s = 0;
	}


	// Set course.
	cmd.action = (getRandomBool() ? COMMAND_ACTION_COURSE_TRUE : COMMAND_ACTION_COURSE_MAG);
	cmd.values[0].i = getRandomCourse();

	commandHandler(&cmd);


	// Start boat.
	cmd.action = COMMAND_ACTION_START;

	commandHandler(&cmd);


	free(cmd.name);
}

int Perf_runAdditional(Perf_CommandHandlerFunc commandHandler)
{
	int rc;


	// Test "boat registry adding and removing" performance.
	rc = runRemoveAllBoats(false);
	if (rc != 0)
	{
		return rc;
	}

	const unsigned int BOAT_COUNTS[] = {
		10000,
		20000,
		50000,
		100000,
		200000
	};

	for (size_t i = 0; i < (sizeof(BOAT_COUNTS) / sizeof(unsigned int)); i++)
	{
		rc = runAddBoats(BOAT_COUNTS[i]);
		if (rc != 0)
		{
			return rc;
		}
		rc = runRemoveAllBoats(true);
		if (rc != 0)
		{
			return rc;
		}
	}

	int writeFd = open("/dev/null", O_WRONLY);
	if (writeFd < 0)
	{
		ERRLOG1("Failed to open /dev/null for NetServer write fd! errno=%d", errno);
		return -2;
	}
	rc = runNetServerRequests(writeFd, commandHandler);
	close(writeFd);
	if (rc != 0)
	{
		return rc;
	}

	rc = runDataGets();
	if (rc != 0)
	{
		return rc;
	}


	PERF_CLOCK_INIT();


	const size_t POSITION_COUNT = 4096;
	unsigned int ITERATIONS;


	proteus_GeoPos* positions = malloc(POSITION_COUNT * sizeof(proteus_GeoPos));
	for (size_t i = 0; i < POSITION_COUNT; i++)
	{
		positions[i].lat = getRandomLat();
		positions[i].lon = getRandomLon();
	}


	// Test "near visible land" performance.
	PERF_CLOCK_RESET();
	ITERATIONS = 100000;
	unsigned int landCount = 0;
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		if (GeoUtils_isApproximatelyNearVisibleLand(positions + (i % POSITION_COUNT), 24000.0))
		{
			landCount++;
		}
	}
	PERF_CLOCK_MEASURE();
	printf("Land visibility checks per second (total visible: %u/%u): %.1fk\n", landCount, ITERATIONS, PERF_CLOCK_KIPS);


	// Test "celestial sight shooting" performance.
	PERF_CLOCK_RESET();
	ITERATIONS = 1000000;
	double azs = 0.0;
	double alts = 0.0;
	unsigned int sightCount = 0;
	const time_t shotTime = time(0);
	CelestialSight sight;
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		CelestialSight_shoot(
				shotTime,
				positions + (i % POSITION_COUNT),
				0,
				1013.25,
				15.0,
				&sight);
		if (sight.obj != -1)
		{
			sightCount++;

			azs += sight.coord.az;
			alts += sight.coord.alt;
		}
	}

	PERF_CLOCK_MEASURE();

	const double az_avg = azs / ((double) sightCount);
	const double alt_avg = alts / ((double) sightCount);

	printf("Celestial sight attempts per second (total shot: %u/%u, az_avg: %.3f, alt_avg: %.3f): %.1fk\n", sightCount, ITERATIONS, az_avg, alt_avg, PERF_CLOCK_KIPS);


	free(positions);
	positions = 0;


	return 0;
}


static int runAddBoats(unsigned int boatCount)
{
	PERF_CLOCK_INIT();

	typedef struct
	{
		char* name;
		char* group;
		char* altName;
	} BoatInfoToAdd;

	BoatInfoToAdd* boatInfos = malloc(boatCount * sizeof(BoatInfoToAdd));
	for (unsigned int i = 0; i < boatCount; i++)
	{
		BoatInfoToAdd* b = boatInfos + i;
		b->name = getRandomName(PERF_RANDOM_BOAT_NAME_LEN);
		b->group = getRandomName(3);
		b->altName = getRandomName(PERF_RANDOM_BOAT_ALT_NAME_LEN);
	}

	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < boatCount; i++)
	{
		BoatInfoToAdd* b = boatInfos + i;
		int rc = BoatRegistry_add(0, b->name, b->group, b->altName);
		if (rc != BoatRegistry_OK)
		{
			ERRLOG1("BoatRegistry_add() failed! rc=%d", rc);
			return -1;
		}
	}
	PERF_CLOCK_MEASURE();
	printf("BoatRegistry boats added (count=%u): %.3fs\n", boatCount, ((double) PERF_CLOCK_NS_TAKEN) / 1000000000.0);

	for (unsigned int i = 0; i < boatCount; i++)
	{
		BoatInfoToAdd* b = boatInfos + i;
		free(b->name);
		free(b->group);
		free(b->altName);
	}
	free(boatInfos);

	return 0;
}

static int runRemoveAllBoats(bool expectNullBoats)
{
	PERF_CLOCK_INIT();

	unsigned int boatCount;
	void* iterator;
	BoatEntry* boatEntry;

	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCount);
	boatEntry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);

	char** boatNames = malloc(boatCount * sizeof(char*));
	for (unsigned int i = 0; i < boatCount; i++)
	{
		if (0 == boatEntry || 0 == boatEntry->name)
		{
			ERRLOG("Null boat entry or boat entry name!");
			return -1;
		}

		boatNames[i] = strdup(boatEntry->name);
		boatEntry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	}
	if (boatEntry != 0)
	{
		ERRLOG("Unexpected non-null boat entry item!");
		return -1;
	}
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);

	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < boatCount; i++)
	{
		Boat* b = BoatRegistry_remove(boatNames[i]);
		if (b)
		{
			free(b);
		}

		if ((0 == b) != expectNullBoats)
		{
			ERRLOG("Unexpected null/non-null boat removed!");
			return -1;
		}
	}
	PERF_CLOCK_MEASURE();

	unsigned int boatCountAfter;

	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), &boatCountAfter);
	boatEntry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);

	if (boatEntry != 0 || boatCountAfter != 0)
	{
		ERRLOG("Boat entry returned or count after removing all entries is non-zero!");
		return -1;
	}
	printf("BoatRegistry boats removed (count=%u): %.3fs\n", boatCount, ((double) PERF_CLOCK_NS_TAKEN) / 1000000000.0);
	for (unsigned int i = 0; i < boatCount; i++)
	{
		free(boatNames[i]);
	}
	free(boatNames);
	boatNames = 0;

	return 0;
}

static int runNetServerRequests(int netServerWriteFd, Perf_CommandHandlerFunc commandHandler)
{
	const unsigned int ITERATIONS = 100000;
	const size_t BOAT_COUNT = 100000;
	const size_t POSITION_COUNT = 100000;
	const size_t REQ_STR_BUF_SIZE = 256;


	proteus_GeoPos* positions = malloc(POSITION_COUNT * sizeof(proteus_GeoPos));
	for (size_t i = 0; i < POSITION_COUNT; i++)
	{
		positions[i].lat = getRandomLat();
		positions[i].lon = getRandomLon();
	}

	PERF_CLOCK_INIT();

	for (unsigned int i = 0; i < BOAT_COUNT; i++)
	{
		Perf_addAndStartRandomBoat(3, commandHandler);
	}


	// "Get wind" performance
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		char reqStr[REQ_STR_BUF_SIZE];
		const proteus_GeoPos* pos = positions + (i % POSITION_COUNT);
		snprintf(reqStr, REQ_STR_BUF_SIZE, "wind,%f,%f", pos->lat, pos->lon);
		NetServer_handleRequest(netServerWriteFd, reqStr);
	}
	PERF_CLOCK_MEASURE();
	printf("NetServer \"get wind\" requests per second: %.1fk\n", PERF_CLOCK_KIPS);


	// "Get wind current adjusted" performance
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		char reqStr[REQ_STR_BUF_SIZE];
		const proteus_GeoPos* pos = positions + (i % POSITION_COUNT);
		snprintf(reqStr, REQ_STR_BUF_SIZE, "wind_c,%f,%f", pos->lat, pos->lon);
		NetServer_handleRequest(netServerWriteFd, reqStr);
	}
	PERF_CLOCK_MEASURE();
	printf("NetServer \"get wind current adjusted\" requests per second: %.1fk\n", PERF_CLOCK_KIPS);


	// "Get wind gust" performance
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		char reqStr[REQ_STR_BUF_SIZE];
		const proteus_GeoPos* pos = positions + (i % POSITION_COUNT);
		snprintf(reqStr, REQ_STR_BUF_SIZE, "wind_gust,%f,%f", pos->lat, pos->lon);
		NetServer_handleRequest(netServerWriteFd, reqStr);
	}
	PERF_CLOCK_MEASURE();
	printf("NetServer \"get wind gust\" requests per second: %.1fk\n", PERF_CLOCK_KIPS);


	// "Get wind gust current adjusted" performance
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		char reqStr[REQ_STR_BUF_SIZE];
		const proteus_GeoPos* pos = positions + (i % POSITION_COUNT);
		snprintf(reqStr, REQ_STR_BUF_SIZE, "wind_gust_c,%f,%f", pos->lat, pos->lon);
		NetServer_handleRequest(netServerWriteFd, reqStr);
	}
	PERF_CLOCK_MEASURE();
	printf("NetServer \"get wind gust current adjusted\" requests per second: %.1fk\n", PERF_CLOCK_KIPS);


	// "Get ocean current" performance
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		char reqStr[REQ_STR_BUF_SIZE];
		const proteus_GeoPos* pos = positions + (i % POSITION_COUNT);
		snprintf(reqStr, REQ_STR_BUF_SIZE, "ocean_current,%f,%f", pos->lat, pos->lon);
		NetServer_handleRequest(netServerWriteFd, reqStr);
	}
	PERF_CLOCK_MEASURE();
	printf("NetServer \"get ocean current\" requests per second: %.1fk\n", PERF_CLOCK_KIPS);


	// "Get sea ice" performance
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		char reqStr[REQ_STR_BUF_SIZE];
		const proteus_GeoPos* pos = positions + (i % POSITION_COUNT);
		snprintf(reqStr, REQ_STR_BUF_SIZE, "sea_ice,%f,%f", pos->lat, pos->lon);
		NetServer_handleRequest(netServerWriteFd, reqStr);
	}
	PERF_CLOCK_MEASURE();
	printf("NetServer \"get sea ice\" requests per second: %.1fk\n", PERF_CLOCK_KIPS);


	// "Get wave height" performance
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		char reqStr[REQ_STR_BUF_SIZE];
		const proteus_GeoPos* pos = positions + (i % POSITION_COUNT);
		snprintf(reqStr, REQ_STR_BUF_SIZE, "wave_height,%f,%f", pos->lat, pos->lon);
		NetServer_handleRequest(netServerWriteFd, reqStr);
	}
	PERF_CLOCK_MEASURE();
	printf("NetServer \"get wave height\" requests per second: %.1fk\n", PERF_CLOCK_KIPS);


	void* iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), 0);
	const BoatEntry* firstBoatEntry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
	const BoatEntry* boatEntry;


	// "Get boat data" performance
	boatEntry = firstBoatEntry;
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		char reqStr[REQ_STR_BUF_SIZE];
		snprintf(reqStr, REQ_STR_BUF_SIZE, "bd,%s", boatEntry->name);
		NetServer_handleRequest(netServerWriteFd, reqStr);
		if (1 == sailnavsim_rustlib_boatregistry_boats_iterator_has_next(iterator))
		{
			boatEntry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
		}
		else
		{
			boatEntry = firstBoatEntry;
		}
	}
	PERF_CLOCK_MEASURE();
	printf("NetServer \"get boat data\" requests per second: %.1fk\n", PERF_CLOCK_KIPS);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);


	iterator = sailnavsim_rustlib_boatregistry_get_boats_iterator(BoatRegistry_registry(), 0);
	firstBoatEntry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);


	// "Get boat group members" performance
	boatEntry = firstBoatEntry;
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		char reqStr[REQ_STR_BUF_SIZE];
		snprintf(reqStr, REQ_STR_BUF_SIZE, "boatgroupmembers,%s", boatEntry->name);
		NetServer_handleRequest(netServerWriteFd, reqStr);
		if (1 == sailnavsim_rustlib_boatregistry_boats_iterator_has_next(iterator))
		{
			boatEntry = sailnavsim_rustlib_boatregistry_boats_iterator_get_next(iterator);
		}
		else
		{
			boatEntry = firstBoatEntry;
		}
	}
	PERF_CLOCK_MEASURE();
	printf("NetServer \"get boat group members\" requests per second: %.1fk\n", PERF_CLOCK_KIPS);
	sailnavsim_rustlib_boatregistry_free_boats_iterator(iterator);


	// "System request counts" performance
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		char reqStr[REQ_STR_BUF_SIZE];
		snprintf(reqStr, REQ_STR_BUF_SIZE, "sys_req_counts,");
		NetServer_handleRequest(netServerWriteFd, reqStr);
	}
	PERF_CLOCK_MEASURE();
	printf("NetServer \"system request counts\" requests per second: %.1fk\n", PERF_CLOCK_KIPS);


	if (0 != runRemoveAllBoats(false))
	{
		ERRLOG("Failed to remove all boats!");
		return -1;
	}

	free(positions);
	positions = 0;

	return 0;
}

static int runDataGets()
{
	const unsigned int ITERATIONS = 1000000;
	const size_t POSITION_COUNT = 1000000;


	proteus_GeoPos* positions = malloc(POSITION_COUNT * sizeof(proteus_GeoPos));
	for (size_t i = 0; i < POSITION_COUNT; i++)
	{
		positions[i].lat = getRandomLat();
		positions[i].lon = getRandomLon();
	}

	PERF_CLOCK_INIT();


	// Weather_get(windOnly=true) performance
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		const proteus_GeoPos* pos = positions + (i % POSITION_COUNT);
		proteus_Weather wx;
		proteus_Weather_get(pos, &wx, true);
	}
	PERF_CLOCK_MEASURE();
	printf("Weather_get(windOnly=true) calls per second: %.1fk\n", PERF_CLOCK_KIPS);


	// Weather_get(windOnly=false) performance
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		const proteus_GeoPos* pos = positions + (i % POSITION_COUNT);
		proteus_Weather wx;
		proteus_Weather_get(pos, &wx, false);
	}
	PERF_CLOCK_MEASURE();
	printf("Weather_get(windOnly=false) calls per second: %.1fk\n", PERF_CLOCK_KIPS);


	// Ocean_get performance
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		const proteus_GeoPos* pos = positions + (i % POSITION_COUNT);
		proteus_OceanData od;
		proteus_Ocean_get(pos, &od);
	}
	PERF_CLOCK_MEASURE();
	printf("Ocean_get calls per second: %.1fk\n", PERF_CLOCK_KIPS);


	// Wave_get performance
	PERF_CLOCK_RESET();
	for (unsigned int i = 0; i < ITERATIONS; i++)
	{
		const proteus_GeoPos* pos = positions + (i % POSITION_COUNT);
		proteus_WaveData wd;
		proteus_Wave_get(pos, &wd);
	}
	PERF_CLOCK_MEASURE();
	printf("Wave_get calls per second: %.1fk\n", PERF_CLOCK_KIPS);


	free(positions);
	positions = 0;

	return 0;
}

static char* getRandomName(unsigned int len)
{
	static const char* RANDOM_NAME_CHARS = "0123456789abcdef";

	char* name = malloc(len + 1);

	for (size_t i = 0; i < len; i++)
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

	name[len] = 0;

	return name;
}

static double getRandomLat()
{
	return (((double) getRandInt(159000)) / 1000.0) - 79.0;
}

static double getRandomLon()
{
	return (((double) getRandInt(360000)) / 1000.0) - 180.0;
}

static int getRandomBoatType()
{
	return getRandInt(11);
}

static int getRandomBoatFlags()
{
	return getRandInt(0x001f);
}

static int getRandomCourse()
{
	return getRandInt(360);
}

static bool getRandomBool()
{
	return (getRandInt(1) == 1);
}

static char* getRandomBoatGroupName()
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

	return strdup(PERF_BOAT_GROUPS[getRandInt(11)]);
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
