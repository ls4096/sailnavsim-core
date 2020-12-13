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
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#include "Command.h"

#include "ErrLog.h"


#define ERRLOG_ID "Command"


static const char* CMD_ACTION_STR_STOP = "stop";
static const char* CMD_ACTION_STR_START = "start";
static const char* CMD_ACTION_STR_COURSE = "course";

static const char* CMD_ACTION_STR_ADD_BOAT = "add";
static const char* CMD_ACTION_STR_REMOVE_BOAT = "remove";


#define CMD_VAL_NONE (0)
#define CMD_VAL_INT (1)
#define CMD_VAL_DOUBLE (2)

static const uint8_t CMD_ACTION_VALS_NONE[COMMAND_MAX_ARG_COUNT] = { CMD_VAL_NONE, CMD_VAL_NONE, CMD_VAL_NONE, CMD_VAL_NONE };

static const uint8_t CMD_ACTION_COURSE_VALS[COMMAND_MAX_ARG_COUNT] = { CMD_VAL_INT, CMD_VAL_NONE, CMD_VAL_NONE, CMD_VAL_NONE };
static const uint8_t CMD_ACTION_ADD_BOAT_VALS[COMMAND_MAX_ARG_COUNT] = { CMD_VAL_DOUBLE, CMD_VAL_DOUBLE, CMD_VAL_INT, CMD_VAL_INT };


#define BOAT_TYPE_MAX_VALUE (8)


static void* commandThreadMain(void* arg);
static void handleCmd(char* cmdStr);

static int getAction(const char* s);
static const uint8_t* getActionExpectedValueTypes(int action);
static bool areValuesValidForAction(int action, CommandValue values[COMMAND_MAX_ARG_COUNT]);
static void queueCmd(Command* cmd);


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

	pthread_mutex_init(&_cmdsLock, 0);

	if (pthread_create(&_commandThread, 0, &commandThreadMain, 0) != 0)
	{
		ERRLOG("Failed to start command processing thread!");
		return -1;
	}

	return 0;
}

Command* Command_next()
{
	Command* cmd = 0;
	pthread_mutex_lock(&_cmdsLock);

	if (_cmds != 0)
	{
		cmd = _cmds;
		_cmds = cmd->next;
		cmd->next = 0;
	}

	pthread_mutex_unlock(&_cmdsLock);
	return cmd;
}


#define COMMAND_BUF_SIZE (1024)

static void* commandThreadMain(void* arg)
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

		sleep(1);
	}

	fclose(f);
	return 0;
}

static void handleCmd(char* cmdStr)
{
	char* s;
	char* t;

	Command* cmd = (Command*) malloc(sizeof(Command));
	cmd->name = 0;
	cmd->next = 0;

	if ((s = strtok_r(cmdStr, ",", &t)) == 0)
	{
		goto fail;
	}
	cmd->name = strdup(s);

	if ((s = strtok_r(0, ",", &t)) == 0)
	{
		goto fail;
	}

	if ((cmd->action = getAction(s)) == COMMAND_ACTION_INVALID)
	{
		goto fail;
	}

	const uint8_t* vals = getActionExpectedValueTypes(cmd->action);

	for (int i = 0; i < COMMAND_MAX_ARG_COUNT; i++)
	{
		switch (vals[i])
		{
			case CMD_VAL_NONE:
				break;

			case CMD_VAL_INT:
			case CMD_VAL_DOUBLE:
				if ((s = strtok_r(0, ",", &t)) == 0)
				{
					goto fail;
				}

				if (vals[i] == CMD_VAL_INT)
				{
					cmd->values[i].i = strtol(s, 0, 10);
				}
				else
				{
					cmd->values[i].d = strtod(s, 0);
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

	queueCmd(cmd);
	return;

fail:
	if (cmd->name)
	{
		free(cmd->name);
	}
	free(cmd);
}

static int getAction(const char* s)
{
	if (strncmp(CMD_ACTION_STR_STOP, s, strlen(CMD_ACTION_STR_STOP)) == 0)
	{
		return COMMAND_ACTION_STOP;
	}
	else if (strncmp(CMD_ACTION_STR_START, s, strlen(CMD_ACTION_STR_START)) == 0)
	{
		return COMMAND_ACTION_START;
	}
	else if (strncmp(CMD_ACTION_STR_COURSE, s, strlen(CMD_ACTION_STR_COURSE)) == 0)
	{
		return COMMAND_ACTION_COURSE;
	}
	else if (strncmp(CMD_ACTION_STR_ADD_BOAT, s, strlen(CMD_ACTION_STR_ADD_BOAT)) == 0)
	{
		return COMMAND_ACTION_ADD_BOAT;
	}
	else if (strncmp(CMD_ACTION_STR_REMOVE_BOAT, s, strlen(CMD_ACTION_STR_REMOVE_BOAT)) == 0)
	{
		return COMMAND_ACTION_REMOVE_BOAT;
	}

	return COMMAND_ACTION_INVALID;
}

static const uint8_t* getActionExpectedValueTypes(int action)
{
	switch (action)
	{
		case COMMAND_ACTION_COURSE:
			return CMD_ACTION_COURSE_VALS;
		case COMMAND_ACTION_ADD_BOAT:
			return CMD_ACTION_ADD_BOAT_VALS;
	}

	return CMD_ACTION_VALS_NONE;
}

static bool areValuesValidForAction(int action, CommandValue values[COMMAND_MAX_ARG_COUNT])
{
	switch (action)
	{
		case COMMAND_ACTION_COURSE:
		{
			return (values[0].i >= 0 && values[0].i <= 360);
		}
		case COMMAND_ACTION_ADD_BOAT:
		{
			return (values[0].d > -90.0 && values[0].d < 90.0 &&
					values[1].d >= -180.0 && values[1].d <= 180.0 &&
					values[2].i >= 0 && values[2].i <= BOAT_TYPE_MAX_VALUE &&
					values[3].i >= 0 && values[3].i <= 0x0003);
		}
	}

	// All other actions do not use values and have no restrictions.
	return true;
}

static void queueCmd(Command* cmd)
{
	pthread_mutex_lock(&_cmdsLock);

	if (_cmds == 0)
	{
		_cmds = cmd;
	}
	else
	{
		_cmdsLast->next = cmd;
	}

	_cmdsLast = cmd;

	pthread_mutex_unlock(&_cmdsLock);
}
