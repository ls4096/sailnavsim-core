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


#define ERRLOG_ID "Main"

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


static const char* VERSION_STRING = "SailNavSim version 1.2.0-dev (" __DATE__ " " __TIME__ ")";


static int parseArgs(int argc, char** argv);
static void printVersionInfo();

static void handleCommand(Command* cmd, BoatEntry* boats);
static void handleBoatRegistryCommand(Command* cmd);


int main(int argc, char** argv)
{
	int argsRc;
	if ((argsRc = parseArgs(argc, argv)) != 0)
	{
		return ((argsRc > 0) ? 0 : argsRc);
	}

	ERRLOG(VERSION_STRING);
	ERRLOG1("Using libProteus version %s", proteus_getVersionString());

	proteus_Logging_setOutputFd(2); // Direct libproteus logging output to stderr.

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


	long iter = 0;

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

		if (boatCount > 0)
		{
			// Log boat data once every ITERATIONS_PER_LOG iterations.
			const bool doLog = (iter % ITERATIONS_PER_LOG == 0);

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
				Boat_advance(e->boat, 1.0);

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

		// Handle pending commands.
		unsigned int cmdCount = 0;
		Command* cmd;
		while ((cmd = Command_next()))
		{
			handleCommand(cmd, boats);
			cmdCount++;
		}

		iter++;
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
	else if (argc == 2)
	{
		if (0 == strcmp("-v", argv[1]))
		{
			printVersionInfo();
			return 1;
		}
		else if (0 == strcmp("--version", argv[1]))
		{
			printVersionInfo();
			return 1;
		}
		else
		{
			printf("Invalid args.\n");
			return -1;
		}
	}
	else
	{
		printf("Invalid args.\n");
		return -1;
	}
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
			goto end;
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
		goto end;
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

end:
	free(cmd->name);
	free(cmd);
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
