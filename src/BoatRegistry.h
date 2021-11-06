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

#ifndef _BoatRegistry_h_
#define _BoatRegistry_h_

#include "Boat.h"


#define BoatRegistry_OK		(0)
#define BoatRegistry_EXISTS	(-1)
#define BoatRegistry_NOTEXISTS	(-2)
#define BoatRegistry_FAILED	(-3)


typedef struct BoatEntry BoatEntry;

struct BoatEntry
{
	char* name;
	char* group;
	Boat* boat;

	BoatEntry* next;
	BoatEntry* prev;
};

int BoatRegistry_init();
void BoatRegistry_destroy();

int BoatRegistry_add(Boat* boat, const char* name, const char* group);
Boat* BoatRegistry_get(const char* name);
const BoatEntry* BoatRegistry_getBoatEntry(const char* name);
Boat* BoatRegistry_remove(const char* name);
BoatEntry* BoatRegistry_getAllBoats(unsigned int* boatCount);

const char* BoatRegistry_getBoatsInGroupResponse(const char* group);
void BoatRegistry_freeBoatsInGroupResponse(const char* resp);

int BoatRegistry_rdlock();
int BoatRegistry_wrlock();
int BoatRegistry_unlock();


#endif // _BoatRegistry_h_
