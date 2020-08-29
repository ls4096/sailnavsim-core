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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include <proteus/proteus.h>
#include <proteus/GeoInfo.h>
#include <proteus/Logging.h>
#include <proteus/Ocean.h>
#include <proteus/Weather.h>

#include "Boat.h"
#include "BoatInitParser.h"
#include "BoatRegistry.h"
#include "Command.h"
#include "ErrLog.h"
#include "Logger.h"
#include "PerfUtils.h"


#define ERRLOG_ID "Main"

// How often to write boat logs
// Minimum value: 2; a value less than 2 results in no boat logs being written
#define ITERATIONS_PER_LOG (60)


#define WX_DATA_DIR_PATH_F006 "wx_data_f006/"
#define WX_DATA_DIR_PATH_F009 "wx_data_f009/"

#define OCEAN_DATA_PATH_T030 "ocean_data/t030.csv"
#define OCEAN_DATA_PATH_T042 "ocean_data/t042.csv"

#define GEO_INFO_DATA_DIR_PATH "geo_water_data/"


#define CMDS_INPUT_PATH "./cmds"

#define BOAT_INIT_DATA_FILENAME "./boatinit.txt"

#define CSV_LOGGER_DIR "./boatlogs/"
#define SQLITE_DB_FILENAME "./sailnavsim.sql"


#define PERF_TEST_ITERATIONS_WARMUP (20)
#define PERF_TEST_ITERATIONS_MEASURE (80)
#define PERF_TEST_MIN_BOAT_COUNT (100)
#define PERF_TEST_MAX_BOAT_COUNT (51200)


static const char* VERSION_STRING = "SailNavSim version 1.3.0-dev (" __DATE__ " " __TIME__ ")";


static int parseArgs(int argc, char** argv);
static void printVersionInfo();

static void handleCommand(Command* cmd, BoatEntry* boats);
static void handleBoatRegistryCommand(Command* cmd);

static void perfAddAndStartRandomBoat();


int main(int argc, char** argv)
{
	int argsRc;
	if ((argsRc = parseArgs(argc, argv)) < 0)
	{
		return argsRc;
	}

	bool perfTest = false;

	switch (argsRc)
	{
		case 1:
			printVersionInfo();
			return 0;
		case 2:
			perfTest = true;
			break;
		default:
			break;
	}

	ERRLOG(VERSION_STRING);
	ERRLOG1("Using libProteus version %s", proteus_getVersionString());

	if (perfTest)
	{
		// Perf test run, so direct libproteus logging output to nowhere.
		proteus_Logging_setOutputFd(-1);
	}
	else
	{
		// Normal run, so direct libproteus logging output to stderr.
		proteus_Logging_setOutputFd(2);
	}


	int initRc;
	if ((initRc = BoatInitParser_start(BOAT_INIT_DATA_FILENAME, SQLITE_DB_FILENAME)) == 0)
	{
		BoatInitEntry* be;
		while ((be = BoatInitParser_getNext()) != 0)
		{
			if (BoatRegistry_OK != BoatRegistry_add(be->boat, be->name))
			{
				ERRLOG("Failed to add boat to registry!");
				return -1;
			}

			free(be->name);
			free(be);
		}
	}
	else if (initRc == 1)
	{
		ERRLOG("Boat init found nothing. Continuing with no boats.");
	}
	else
	{
		ERRLOG("Failed to read boats for init!");
		return -1;
	}

	if (proteus_Weather_init(PROTEUS_WEATHER_SOURCE_DATA_GRID_1P00, WX_DATA_DIR_PATH_F006, WX_DATA_DIR_PATH_F009) != 0)
	{
		ERRLOG("Failed to init weather!");
		return -1;
	}

	if (proteus_Ocean_init(OCEAN_DATA_PATH_T030, OCEAN_DATA_PATH_T042) != 0)
	{
		ERRLOG("Failed to init ocean data!");
		return -1;
	}

	if (proteus_GeoInfo_init(GEO_INFO_DATA_DIR_PATH) != 0)
	{
		ERRLOG("Failed to init geographic info!");
		return -1;
	}

	if (Command_init(CMDS_INPUT_PATH) != 0)
	{
		ERRLOG("Failed to init command processor!");
		return -1;
	}

	if (Logger_init(CSV_LOGGER_DIR, SQLITE_DB_FILENAME) != 0)
	{
		ERRLOG("Failed to init boat logger!");
		return -1;
	}

	if (Boat_init() != 0)
	{
		ERRLOG("Failed to init boat engine!");
		return -1;
	}


	int lastIter = 1;

	int perfIter = 0;
	long perfTotalNs = 0;
	bool perfFirst = true;

	struct timespec nextT;
	if (0 != clock_gettime(CLOCK_MONOTONIC, &nextT))
	{
		ERRLOG1("clock_gettime failed! errno=%d", errno);
		return -1;
	}

	for (;;)
	{
		time_t curTime = time(0);

		unsigned int boatCount;
		BoatEntry* boats = BoatRegistry_getAllBoats(&boatCount);

		// Process all boats.
		if (boatCount > 0)
		{
			// Log boat data once every ITERATIONS_PER_LOG iterations.
			const int iter = (ITERATIONS_PER_LOG >= 2) ? (curTime % ITERATIONS_PER_LOG) : 1;
			const bool doLog = perfTest ? false : ((ITERATIONS_PER_LOG >= 2) ? (iter < lastIter) : false);

			lastIter = iter;

			LogEntry* logEntries = 0;
			if (doLog)
			{
				logEntries = (LogEntry*) malloc(boatCount * sizeof(LogEntry));
			}

			int ilog = 0;

			// Advance boats.
			BoatEntry* e = boats;
			while (e)
			{
				Boat_advance(e->boat, 1.0 /* seconds per iteration */);

				if (doLog)
				{
					Logger_fillLogEntry(e->boat, e->name, curTime, logEntries + ilog);
					ilog++;
				}

				e = e->next;
			}

			if (doLog)
			{
				Logger_writeLogs(logEntries, boatCount);
			}
		}


		// If this is a performance test run, then handle things a bit differently, take some measurements, and loop back early.
		if (perfTest)
		{
			unsigned int currentBoatCount;

			if (perfIter == 0)
			{
				BoatRegistry_getAllBoats(&currentBoatCount);

				if (perfFirst)
				{
					// First time running performance iterations, so start with the minimum
					// necessary boat count for the first set of measurements.
					for (int i = currentBoatCount; i < PERF_TEST_MIN_BOAT_COUNT; i++)
					{
						perfAddAndStartRandomBoat();
					}

					perfFirst = false;
				}
				else
				{
					if (currentBoatCount * 2 > PERF_TEST_MAX_BOAT_COUNT)
					{
						// We're done all performance measurement sets, so exit the loop.
						break;
					}

					// Double the number of boats for the next set of measurements.
					for (int i = currentBoatCount; i < currentBoatCount * 2; i++)
					{
						perfAddAndStartRandomBoat();
					}
				}
			}

			// Only measure after warm-up period.
			if (perfIter >= PERF_TEST_ITERATIONS_WARMUP)
			{
				if (perfIter >= PERF_TEST_ITERATIONS_WARMUP + PERF_TEST_ITERATIONS_MEASURE)
				{
					// We're done this set, so print result and proceed to next set of iterations.
					BoatRegistry_getAllBoats(&currentBoatCount);

					long bips = PERF_TEST_ITERATIONS_MEASURE * currentBoatCount * 1000000000L / perfTotalNs;

					printf("Boat count %d...Boat iterations per second: %ld\n", currentBoatCount, bips);

					perfIter = -1;
					perfTotalNs = 0;
				}
				else
				{
					// Perform time measurement and add to running time total.
					struct timespec t2;
					if (0 != clock_gettime(CLOCK_MONOTONIC, &t2))
					{
						ERRLOG1("clock_gettime failed! errno=%d", errno);
						return -1;
					}

					const long nsTaken = (t2.tv_nsec - nextT.tv_nsec) + 1000000000L * (t2.tv_sec - nextT.tv_sec);
					perfTotalNs += nsTaken;
				}
			}


			if (0 != clock_gettime(CLOCK_MONOTONIC, &nextT))
			{
				ERRLOG1("clock_gettime failed! errno=%d", errno);
				return -1;
			}

			perfIter++;
			continue;
		} // End of performance testing control block.


		// Handle pending commands.
		unsigned int cmdCount = 0;
		Command* cmd;
		while ((cmd = Command_next()))
		{
			handleCommand(cmd, boats);

			free(cmd->name);
			free(cmd);

			cmdCount++;
		}


		// Next iteration 1 second later
		nextT.tv_sec++;

		struct timespec tp;
		if (0 != clock_gettime(CLOCK_MONOTONIC, &tp))
		{
			ERRLOG1("clock_gettime failed! errno=%d", errno);
			return -1;
		}

		long sleepTime = (nextT.tv_sec * 1000000000 + nextT.tv_nsec) - (tp.tv_sec * 1000000000 + tp.tv_nsec);
		if (sleepTime < 1)
		{
			ERRLOG2("Iteration (b=%u, c=%u) took longer than 1 second. Starting next right away!", boatCount, cmdCount);
			continue;
		}

		struct timespec sleepT = { sleepTime / 1000000000, sleepTime % 1000000000 };
		struct timespec remT;

		ERRLOG3("Iter (b=%u, c=%u). Next in %ld us.", boatCount, cmdCount, sleepTime / 1000);

		int rc;
		while (0 != (rc = nanosleep(&sleepT, &remT)))
		{
			if (errno == EINTR)
			{
				sleepT = remT;
			}
			else
			{
				ERRLOG1("nanosleep failed! errno=%d", errno);
				return -1;
			}
		}
	}

	return 0;
}


static int parseArgs(int argc, char** argv)
{
	if (argc == 1)
	{
		return 0;
	}

	bool doPerf = false;

	for (int i = 1; i < argc; i++)
	{
		if (0 == strcmp("-v", argv[i]) || 0 == strcmp("--version", argv[i]))
		{
			return 1;
		}
		else if (0 == strcmp("--perf", argv[i]))
		{
			doPerf = true;
		}
		else
		{
			printf("Invalid argument: %s\n", argv[i]);
			return -1;
		}
	}

	if (doPerf)
	{
		return 2;
	}

	return 0;
}

static void printVersionInfo()
{
	printf("%s, using libProteus version %s\n", VERSION_STRING, proteus_getVersionString());
}

static void handleCommand(Command* cmd, BoatEntry* boats)
{
	// First check if it's a boat registry action, and handle those actions separately.
	switch (cmd->action)
	{
		case COMMAND_ACTION_ADD_BOAT:
		case COMMAND_ACTION_REMOVE_BOAT:
			handleBoatRegistryCommand(cmd);
			return;
	}

	Boat* foundBoat = 0;

	while (boats)
	{
		if (strcmp(boats->name, cmd->name) == 0)
		{
			foundBoat = boats->boat;
			break;
		}

		boats = boats->next;
	}

	if (!foundBoat)
	{
		return;
	}

	switch (cmd->action)
	{
		case COMMAND_ACTION_STOP:
			foundBoat->sailsDown = true;
			break;
		case COMMAND_ACTION_START:
			if (Boat_isHeadingTowardWater(foundBoat))
			{
				foundBoat->stop = false;
				foundBoat->sailsDown = false;
				foundBoat->movingToSea = true;
			}
			break;
		case COMMAND_ACTION_COURSE:
			foundBoat->desiredCourse = cmd->values[0].i;
			break;
	}
}

static void handleBoatRegistryCommand(Command* cmd)
{
	switch (cmd->action)
	{
		case COMMAND_ACTION_ADD_BOAT:
		{
			Boat* boat = Boat_new(cmd->values[0].d, cmd->values[1].d, cmd->values[2].i);
			if (BoatRegistry_OK != BoatRegistry_add(boat, cmd->name))
			{
				free(boat);
			}

			break;
		}
		case COMMAND_ACTION_REMOVE_BOAT:
		{
			Boat* boat;
			if ((boat = BoatRegistry_remove(cmd->name)))
			{
				free(boat);
			}

			break;
		}
	}
}

static void perfAddAndStartRandomBoat()
{
	Command cmd;

	cmd.name = PerfUtils_getRandomName();
	cmd.next = 0;


	// Add boat.
	cmd.action = COMMAND_ACTION_ADD_BOAT;
	cmd.values[0].d = PerfUtils_getRandomLat();
	cmd.values[1].d = PerfUtils_getRandomLon();
	cmd.values[2].i = PerfUtils_getRandomBoatType();

	handleCommand(&cmd, 0);


	// Get up-to-date BoatEntry to use.
	BoatEntry* boats = BoatRegistry_getAllBoats(0);


	// Set course.
	cmd.action = COMMAND_ACTION_COURSE;
	cmd.values[0].i = PerfUtils_getRandomCourse();

	handleCommand(&cmd, boats);


	// Start boat.
	cmd.action = COMMAND_ACTION_START;

	handleCommand(&cmd, boats);


	free(cmd.name);
}
