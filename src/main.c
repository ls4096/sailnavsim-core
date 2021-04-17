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

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include <proteus/proteus.h>
#include <proteus/Compass.h>
#include <proteus/GeoInfo.h>
#include <proteus/Logging.h>
#include <proteus/Ocean.h>
#include <proteus/ScalarConv.h>
#include <proteus/Wave.h>
#include <proteus/Weather.h>

#include "Boat.h"
#include "BoatInitParser.h"
#include "BoatRegistry.h"
#include "CelestialSight.h"
#include "Command.h"
#include "ErrLog.h"
#include "Logger.h"
#include "NetServer.h"
#include "PerfUtils.h"


#define ERRLOG_ID "Main"

// How often to write boat logs
// One iteration always covers one second in the simulation.
// Minimum value: 2; a value less than 2 results in no boat logs being written
#define ITERATIONS_PER_LOG (60)

#define NETSERVER_THREAD_COUNT (5)


#define WX_DATA_DIR_PATH_F006 "wx_data_f006/"
#define WX_DATA_DIR_PATH_F009 "wx_data_f009/"

#define OCEAN_DATA_PATH_T030 "ocean_data/t030.csv"
#define OCEAN_DATA_PATH_T042 "ocean_data/t042.csv"

#define WAVE_DATA_PATH_F30 "wave_data/f30.csv"
#define WAVE_DATA_PATH_F42 "wave_data/f42.csv"

#define GEO_INFO_DATA_DIR_PATH "geo_water_data/"

#define COMPASS_DATA_PATH "compass_data/mag_dec.csv"


#define CMDS_INPUT_PATH "./cmds"

#define BOAT_INIT_DATA_FILENAME "./boatinit.txt"

#define CSV_LOGGER_DIR "./boatlogs/"
#define SQLITE_DB_FILENAME "./sailnavsim.sql"


#define PERF_TEST_ITERATIONS_WARMUP (10)
#define PERF_TEST_ITERATIONS_MEASURE (40)
#define PERF_TEST_MIN_BOAT_COUNT (100)
#define PERF_TEST_MAX_BOAT_COUNT (204800)


static const char* VERSION_STRING = "SailNavSim version 1.9.1 (" __DATE__ " " __TIME__ ")";


static int parseArgs(int argc, char** argv);
static void printVersionInfo();

static void handleCommand(Command* cmd);
static void handleBoatRegistryCommand(Command* cmd);

static bool isApproximatelyNearVisibleLand(const proteus_GeoPos* pos, float visibility);
static bool isLandFoundOnCircle(const proteus_GeoPos* pos, double r, int n);

static void perfAddAndStartRandomBoat();

static int _netPort = 0;


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

	if (proteus_Wave_init(WAVE_DATA_PATH_F30, WAVE_DATA_PATH_F42) != 0)
	{
		ERRLOG("Failed to init wave data!");
		return -1;
	}

	if (proteus_GeoInfo_init(GEO_INFO_DATA_DIR_PATH) != 0)
	{
		ERRLOG("Failed to init geographic info!");
		return -1;
	}

	if (proteus_Compass_init(COMPASS_DATA_PATH) != 0)
	{
		ERRLOG("Failed to init compass data!");
		return -1;
	}

	if (CelestialSight_init() != 0)
	{
		ERRLOG("Failed to init celestial sight system!");
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

	if (_netPort > 0)
	{
		signal(SIGPIPE, SIG_IGN);

		if (NetServer_init(_netPort, NETSERVER_THREAD_COUNT) != 0)
		{
			ERRLOG("Failed to init net server!");
			return -1;
		}
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
			CelestialSight* sights = 0;
			if (doLog)
			{
				// One log entry and (maximum) one celestial sight per boat on this iteration.
				logEntries = (LogEntry*) malloc(boatCount * sizeof(LogEntry));
				sights = (CelestialSight*) malloc(boatCount * sizeof(CelestialSight));
			}

			int ilog = 0;
			int totalSights = 0;

			if (BoatRegistry_OK != BoatRegistry_wrlock())
			{
				ERRLOG("Failed to write-lock BoatRegistry lock for boat advance!");
			}

			// Advance boats.
			BoatEntry* e = boats;
			while (e)
			{
				Boat_advance(e->boat, curTime);

				if (doLog)
				{
					bool isReportVisible = true;

					if ((e->boat->boatFlags & BOAT_FLAG_CELESTIAL))
					{
						proteus_Weather wx;
						proteus_Weather_get(&e->boat->pos, &wx, false);

						CelestialSight_shoot(curTime, &e->boat->pos, (int) roundf(wx.cloud), (double) wx.pressure, (double) wx.temp, sights + ilog);
						if (sights[ilog].obj >= 0)
						{
							totalSights++;
						}

						isReportVisible = isApproximatelyNearVisibleLand(&e->boat->pos, wx.visibility);
					}
					else
					{
						sights[ilog].obj = -1;
					}

					Logger_fillLogEntry(e->boat, e->name, curTime, isReportVisible, logEntries + ilog);

					ilog++;
				}

				e = e->next;
			}

			if (BoatRegistry_OK != BoatRegistry_unlock())
			{
				ERRLOG("Failed to unlock BoatRegistry lock after boat advance!");
			}

			if (doLog)
			{
				CelestialSightEntry* csEntries = malloc(totalSights * sizeof(CelestialSightEntry));
				CelestialSightEntry* nextEntry = csEntries;
				for (int i = 0; i < boatCount; i++)
				{
					if (sights[i].obj >= 0)
					{
						nextEntry->time = curTime;
						nextEntry->boatName = logEntries[i].boatName; // Shallow copy of string suffices here, since csEntries has same lifetime as logEntries.
						nextEntry->obj = sights[i].obj;
						nextEntry->az = sights[i].coord.az;
						nextEntry->alt = sights[i].coord.alt;
						nextEntry->compassMagDec = proteus_Compass_magdec(&logEntries[i].boatPos, curTime);

						nextEntry++;
					}
				}

				free(sights);

				Logger_writeLogs(logEntries, boatCount, csEntries, totalSights);
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


		if (BoatRegistry_OK != BoatRegistry_wrlock())
		{
			ERRLOG("Failed to write-lock BoatRegistry lock for commands!");
		}

		// Handle pending commands.
		unsigned int cmdCount = 0;
		Command* cmd;
		while ((cmd = Command_next()))
		{
			handleCommand(cmd);

			free(cmd->name);
			free(cmd);

			cmdCount++;
		}

		if (BoatRegistry_OK != BoatRegistry_unlock())
		{
			ERRLOG("Failed to unlock BoatRegistry lock after commands!");
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
			ERRLOG2("Iteration (b=%u, c=%u) fell behind. Starting next right away!", boatCount, cmdCount);
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
		else if (0 == strcmp("--netport", argv[i]))
		{
			if (argv[i + 1])
			{
				_netPort = atoi(argv[i + 1]);

				if (_netPort <= 0 || _netPort > 65535)
				{
					printf("Invalid netport argument: %s\n", argv[i + 1]);
					return -1;
				}

				i++;
			}
			else
			{
				printf("No netport argument provided!\n");
				return -1;
			}
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

static void handleCommand(Command* cmd)
{
	// First check if it's a boat registry action, and handle those actions separately.
	switch (cmd->action)
	{
		case COMMAND_ACTION_ADD_BOAT:
		case COMMAND_ACTION_REMOVE_BOAT:
			handleBoatRegistryCommand(cmd);
			return;
	}

	Boat* foundBoat = BoatRegistry_get(cmd->name);
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
			if (Boat_isHeadingTowardWater(foundBoat, time(0)))
			{
				foundBoat->stop = false;
				foundBoat->sailsDown = false;
				foundBoat->movingToSea = true;
			}
			break;
		case COMMAND_ACTION_COURSE_TRUE:
		case COMMAND_ACTION_COURSE_MAG:
			foundBoat->desiredCourse = cmd->values[0].i;
			foundBoat->courseMagnetic = (cmd->action == COMMAND_ACTION_COURSE_MAG);
			break;
	}
}

static void handleBoatRegistryCommand(Command* cmd)
{
	switch (cmd->action)
	{
		case COMMAND_ACTION_ADD_BOAT:
		{
			Boat* boat = Boat_new(cmd->values[0].d, cmd->values[1].d, cmd->values[2].i, cmd->values[3].i);
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

// Samples points inside a circle of the visibility radius for detecting nearby land.
#define MIN_RADIUS (30.0)
#define MAX_RADIUS (31000.0)
#define MAX_SAMPLE_POINTS_ON_CIRCLE (32)
static bool isApproximatelyNearVisibleLand(const proteus_GeoPos* pos, float visibility)
{
	if (!proteus_GeoInfo_isWater(pos))
	{
		return true;
	}

	int n = 4;
	for (double r = MIN_RADIUS; r <= visibility && r <= MAX_RADIUS; r *= 2.0)
	{
		if (isLandFoundOnCircle(pos, r, n))
		{
			return true;
		}

		if (n < MAX_SAMPLE_POINTS_ON_CIRCLE)
		{
			n *= 2;
		}
	}

	if (visibility > MIN_RADIUS)
	{
		// Check one last circle at the outer limits of visibility.
		if (isLandFoundOnCircle(pos, visibility, n))
		{
			return true;
		}
	}

	return false;
}

// Calculations to "look around" approximately uniformly (at "n" points) around an approximate circle (of somewhat-radius "r" metres) from the given position ("pos").
static bool isLandFoundOnCircle(const proteus_GeoPos* pos, double r, int n)
{
	proteus_GeoPos p;

	const double cosLat = cos(proteus_ScalarConv_deg2rad(pos->lat));

	for (int i = 0; i < n; i++)
	{
		// We could make this more exact, but a close-enough approximation suffices here and will run faster...

		p.lat = pos->lat +
			(r * cos(i * 2.0 * M_PI / n) / 111120.0);

		p.lon = pos->lon +
			(r * sin(i * 2.0 * M_PI / n) / (111120.0 * cosLat));

		if (p.lat > 90.0)
		{
			p.lat = 90.0;
		}
		else if (p.lat < -90.0)
		{
			p.lat = -90.0;
		}

		if (p.lon > 180.0)
		{
			p.lon -= 360.0;
		}
		else if (p.lon < -180.0)
		{
			p.lat += 360.0;
		}

		if (!proteus_GeoInfo_isWater(&p))
		{
			return true;
		}
	}

	return false;
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
	cmd.values[3].i = PerfUtils_getRandomBoatFlags();

	handleCommand(&cmd);


	// Set course.
	cmd.action = (PerfUtils_getRandomBool() ? COMMAND_ACTION_COURSE_TRUE : COMMAND_ACTION_COURSE_MAG);
	cmd.values[0].i = PerfUtils_getRandomCourse();

	handleCommand(&cmd);


	// Start boat.
	cmd.action = COMMAND_ACTION_START;

	handleCommand(&cmd);


	free(cmd.name);
}
