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

#ifndef _Command_h_
#define _Command_h_


#define COMMAND_ACTION_INVALID (-1)

#define COMMAND_ACTION_STOP (0)
#define COMMAND_ACTION_START (1)
#define COMMAND_ACTION_COURSE_TRUE (2)
#define COMMAND_ACTION_COURSE_MAG (3)

#define COMMAND_ACTION_ADD_BOAT (4)
#define COMMAND_ACTION_REMOVE_BOAT (5)


#define COMMAND_MAX_ARG_COUNT (4)


typedef struct Command Command;

typedef union
{
	int i;
	double d;
} CommandValue;

struct Command
{
	char* name;
	int action;
	CommandValue values[COMMAND_MAX_ARG_COUNT];

	Command* next;
};


int Command_init(const char* cmdsInputPath);
Command* Command_next();
int Command_add(char* cmdStr);


#endif // _Command_h_
