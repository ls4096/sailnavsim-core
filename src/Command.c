/**
 * Copyright (C) 2020-2024 ls4096 <ls4096@8bitbyte.ca>
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
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#include "Command.h"

#include "BoatWindResponse.h"
#include "ErrLog.h"


#define ERRLOG_ID "Command"
#define THREAD_NAME "Command"


static const char* CMD_ACTION_STR_STOP = "stop";
static const char* CMD_ACTION_STR_START = "start";
static const char* CMD_ACTION_STR_COURSE_TRUE = "course";
static const char* CMD_ACTION_STR_COURSE_MAG = "course_m";
static const char* CMD_ACTION_STR_SAIL_AREA = "sail_area";

static const char* CMD_ACTION_STR_ADD_BOAT = "add";
static const char* CMD_ACTION_STR_ADD_BOAT_WITH_GROUP = "add_g";
static const char* CMD_ACTION_STR_REMOVE_BOAT = "remove";


#define CMD_VAL_NONE (0)
#define CMD_VAL_INT (1)
#define CMD_VAL_DOUBLE (2)
#define CMD_VAL_STRING (3)

static const uint8_t CMD_ACTION_VALS_NONE[COMMAND_MAX_ARG_COUNT] = { CMD_VAL_NONE, CMD_VAL_NONE, CMD_VAL_NONE, CMD_VAL_NONE, CMD_VAL_NONE, CMD_VAL_NONE };

static const uint8_t CMD_ACTION_SINGLE_INT_VALS[COMMAND_MAX_ARG_COUNT] = { CMD_VAL_INT, CMD_VAL_NONE, CMD_VAL_NONE, CMD_VAL_NONE, CMD_VAL_NONE, CMD_VAL_NONE };
static const uint8_t CMD_ACTION_ADD_BOAT_VALS[COMMAND_MAX_ARG_COUNT] = { CMD_VAL_DOUBLE, CMD_VAL_DOUBLE, CMD_VAL_INT, CMD_VAL_INT, CMD_VAL_NONE, CMD_VAL_NONE };
static const uint8_t CMD_ACTION_ADD_BOAT_WITH_GROUP_VALS[COMMAND_MAX_ARG_COUNT] = { CMD_VAL_DOUBLE, CMD_VAL_DOUBLE, CMD_VAL_INT, CMD_VAL_INT, CMD_VAL_STRING, CMD_VAL_STRING };


#define BOAT_TYPE_MAX_VALUE (11)
#define BOAT_FLAGS_MAX_VALUE (0x003f)


static void* commandThreadMain();
static int handleCmd(char* cmdStr);

static int getAction(const char* s);
static const uint8_t* getActionExpectedValueTypes(int action);
static bool areValuesValidForAction(int action, CommandValue values[COMMAND_MAX_ARG_COUNT]);
static bool isBoatTypeValid(int boatType);
static int queueCmd(Command* cmd);


static const char* _cmdsInputPath = 0;

static pthread_t _commandThread;
static Command* _cmds = 0;
static Command* _cmdsLast = 0;
static pthread_mutex_t _cmdsLock;


int Command_init(const char* cmdsInputPath)
{
	if (!cmdsInputPath)
	{
		return -3;
	}

	_cmdsInputPath = strdup(cmdsInputPath);

	if (0 != pthread_mutex_init(&_cmdsLock, 0))
	{
		ERRLOG("Failed to init cmds mutex!");
		return -4;
	}

	if (0 != pthread_create(&_commandThread, 0, &commandThreadMain, 0))
	{
		ERRLOG("Failed to start command processing thread!");
		return -1;
	}

#if defined(_GNU_SOURCE) && defined(__GLIBC__)
	if (0 != pthread_setname_np(_commandThread, THREAD_NAME))
	{
		ERRLOG1("Couldn't set thread name to %s. Continuing anyway.", THREAD_NAME);
	}
#endif

	return 0;
}

Command* Command_next()
{
	Command* cmd = 0;

	if (0 != pthread_mutex_lock(&_cmdsLock))
	{
		ERRLOG("next: Failed to lock cmds mutex!");
		return 0;
	}

	if (_cmds != 0)
	{
		cmd = _cmds;
		_cmds = cmd->next;
		cmd->next = 0;
	}

	if (0 != pthread_mutex_unlock(&_cmdsLock))
	{
		ERRLOG("next: Failed to unlock cmds mutex!");
	}

	return cmd;
}

int Command_add(char* cmdStr)
{
	return handleCmd(cmdStr);
}

void Command_free(Command* cmd)
{
	if (cmd->name)
	{
		free(cmd->name);
	}

	const uint8_t* valueTypes = getActionExpectedValueTypes(cmd->action);
	for (int i = 0; i < COMMAND_MAX_ARG_COUNT; i++)
	{
		if (valueTypes[i] == CMD_VAL_STRING && cmd->values[i].s)
		{
			free(cmd->values[i].s);
		}
	}

	free(cmd);
}


#define COMMAND_BUF_SIZE (1024)

static void* commandThreadMain()
{
	char buf[COMMAND_BUF_SIZE];

	FILE* f = fopen(_cmdsInputPath, "r");
	if (f == 0)
	{
		ERRLOG("Failed to open command input path!");
		return 0;
	}

	for (;;)
	{
		while (fgets(buf, COMMAND_BUF_SIZE, f) != 0)
		{
			handleCmd(buf);
		}

		clearerr(f);
		sleep(1);
	}

	fclose(f);
	return 0;
}

static int handleCmd(char* cmdStr)
{
	char* s;
	char* t;

	// Remove trailing newline, if present.
	const size_t slen = strlen(cmdStr);
	if (slen > 0 && cmdStr[slen - 1] == '\n')
	{
		cmdStr[slen - 1] = 0;
	}

	Command* cmd = malloc(sizeof(Command));
	if (!cmd)
	{
		ERRLOG("Failed to alloc cmd!");
		goto fail;
	}

	cmd->name = 0;
	cmd->next = 0;

	if ((s = strtok_r(cmdStr, ",", &t)) == 0)
	{
		goto fail;
	}

	cmd->name = strdup(s);
	if (!cmd->name)
	{
		ERRLOG("Failed to alloc cmd->name!");
		goto fail;
	}

	if ((s = strtok_r(0, ",", &t)) == 0)
	{
		goto fail;
	}

	if ((cmd->action = getAction(s)) == COMMAND_ACTION_INVALID)
	{
		goto fail;
	}

	const uint8_t* vt = getActionExpectedValueTypes(cmd->action);

	for (int i = 0; i < COMMAND_MAX_ARG_COUNT; i++)
	{
		switch (vt[i])
		{
			case CMD_VAL_NONE:
				break;

			case CMD_VAL_INT:
			case CMD_VAL_DOUBLE:
				if ((s = strtok_r(0, ",", &t)) == 0)
				{
					goto fail;
				}

				if (vt[i] == CMD_VAL_INT)
				{
					cmd->values[i].i = strtol(s, 0, 10);
				}
				else
				{
					cmd->values[i].d = strtod(s, 0);
				}

				break;

			case CMD_VAL_STRING:
				if ((s = strtok_r(0, ",", &t)) == 0)
				{
					goto fail;
				}
				else
				{
					cmd->values[i].s = strdup(s);
				}

				break;

			default:
				goto fail;
		}
	}

	if (!areValuesValidForAction(cmd->action, cmd->values))
	{
		goto fail;
	}

	return queueCmd(cmd);

fail:
	if (cmd)
	{
		Command_free(cmd);
	}

	return -1;
}

static int getAction(const char* s)
{
	if (strcmp(CMD_ACTION_STR_STOP, s) == 0)
	{
		return COMMAND_ACTION_STOP;
	}
	else if (strcmp(CMD_ACTION_STR_START, s) == 0)
	{
		return COMMAND_ACTION_START;
	}
	else if (strcmp(CMD_ACTION_STR_COURSE_TRUE, s) == 0)
	{
		return COMMAND_ACTION_COURSE_TRUE;
	}
	else if (strcmp(CMD_ACTION_STR_COURSE_MAG, s) == 0)
	{
		return COMMAND_ACTION_COURSE_MAG;
	}
	else if (strcmp(CMD_ACTION_STR_SAIL_AREA, s) == 0)
	{
		return COMMAND_ACTION_SAIL_AREA;
	}
	else if (strcmp(CMD_ACTION_STR_ADD_BOAT, s) == 0)
	{
		return COMMAND_ACTION_ADD_BOAT;
	}
	else if (strcmp(CMD_ACTION_STR_ADD_BOAT_WITH_GROUP, s) == 0)
	{
		return COMMAND_ACTION_ADD_BOAT_WITH_GROUP;
	}
	else if (strcmp(CMD_ACTION_STR_REMOVE_BOAT, s) == 0)
	{
		return COMMAND_ACTION_REMOVE_BOAT;
	}

	return COMMAND_ACTION_INVALID;
}

static const uint8_t* getActionExpectedValueTypes(int action)
{
	switch (action)
	{
		case COMMAND_ACTION_COURSE_TRUE:
		case COMMAND_ACTION_COURSE_MAG:
		case COMMAND_ACTION_SAIL_AREA:
			return CMD_ACTION_SINGLE_INT_VALS;
		case COMMAND_ACTION_ADD_BOAT:
			return CMD_ACTION_ADD_BOAT_VALS;
		case COMMAND_ACTION_ADD_BOAT_WITH_GROUP:
			return CMD_ACTION_ADD_BOAT_WITH_GROUP_VALS;
	}

	return CMD_ACTION_VALS_NONE;
}

static bool areValuesValidForAction(int action, CommandValue values[COMMAND_MAX_ARG_COUNT])
{
	switch (action)
	{
		case COMMAND_ACTION_COURSE_TRUE:
		case COMMAND_ACTION_COURSE_MAG:
		{
			return (values[0].i >= 0 && values[0].i <= 360);
		}
		case COMMAND_ACTION_SAIL_AREA:
		{
			return (values[0].i >= 0 && values[0].i <= 100);
		}
		case COMMAND_ACTION_ADD_BOAT:
		case COMMAND_ACTION_ADD_BOAT_WITH_GROUP:
		{
			if (action == COMMAND_ACTION_ADD_BOAT_WITH_GROUP)
			{
				if (!values[4].s || strlen(values[4].s) == 0)
				{
					return false;
				}
			}

			return (values[0].d > -90.0 && values[0].d < 90.0 &&
					values[1].d >= -180.0 && values[1].d <= 180.0 &&
					isBoatTypeValid(values[2].i) &&
					values[3].i >= 0 && values[3].i <= BOAT_FLAGS_MAX_VALUE);
		}
	}

	// All other actions do not use values and have no restrictions.
	return true;
}

static bool isBoatTypeValid(int boatType)
{
	return (BoatWindResponse_isBoatTypeBasic(boatType) || BoatWindResponse_isBoatTypeAdvanced(boatType));
}

static int queueCmd(Command* cmd)
{
	if (0 != pthread_mutex_lock(&_cmdsLock))
	{
		ERRLOG("queueCmd: Failed to lock cmds mutex!");
		return -1;
	}

	if (_cmds == 0)
	{
		_cmds = cmd;
	}
	else
	{
		_cmdsLast->next = cmd;
	}

	_cmdsLast = cmd;

	if (0 != pthread_mutex_unlock(&_cmdsLock))
	{
		ERRLOG("queueCmd: Failed to unlock cmds mutex!");
	}

	return 0;
}
