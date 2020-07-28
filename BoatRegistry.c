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

#include <stdlib.h>
#include <string.h>

#include "BoatRegistry.h"


/**
 * BoatRegistry maintains the collection of boats in the simulation.
 *
 * TODO: Reimplement this as a hash table (for example) instead of current linked list.
 *       Boat add/get/remove all currently run in O(n) time for n boats.
 *       Right now, performance can suffer greatly when simulating a very large number
 *       of boats, particularly when many commands are sent to boats at the same time.
 */


static BoatEntry* _first = 0;
static unsigned int _boatCount = 0;


int BoatRegistry_add(Boat* boat, const char* name)
{
	BoatEntry* e = _first;
	BoatEntry* last = _first;

	while (e != 0)
	{
		if (0 == strcmp(name, e->name))
		{
			return BoatRegistry_EXISTS;
		}

		last = e;
		e = e->next;
	}

	BoatEntry* newEntry = (BoatEntry*) malloc(sizeof(BoatEntry));
	newEntry->name = strdup(name);
	newEntry->boat = boat;
	newEntry->next = 0;

	if (last == 0)
	{
		_first = newEntry;
	}
	else
	{
		last->next = newEntry;
	}

	_boatCount++;
	return BoatRegistry_OK;
}

Boat* BoatRegistry_get(const char* name)
{
	BoatEntry* e = _first;
	while (e != 0)
	{
		if (0 == strcmp(name, e->name))
		{
			return e->boat;
		}

		e = e->next;
	}

	return 0;
}

Boat* BoatRegistry_remove(const char* name)
{
	BoatEntry* e = _first;
	BoatEntry* last = _first;

	while (e != 0)
	{
		if (0 == strcmp(name, e->name))
		{
			if (e == _first)
			{
				_first = e->next;
			}
			else
			{
				last->next = e->next;
			}

			Boat* boat = e->boat;

			free(e->name);
			free(e);

			_boatCount--;
			return boat;
		}

		last = e;
		e = e->next;
	}

	return 0;
}

BoatEntry* BoatRegistry_getAllBoats(unsigned int* boatCount)
{
	if (boatCount)
	{
		*boatCount = _boatCount;
	}

	return _first;
}
