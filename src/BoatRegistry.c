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


static BoatEntry* _first = 0;
static BoatEntry* _last = 0;
static unsigned int _boatCount = 0;


static int hashName(const char* name);
static BoatEntry* findBoatEntry(const char* name);


typedef struct BoatEntryEntry BoatEntryEntry;

struct BoatEntryEntry
{
	BoatEntry* e;
	BoatEntryEntry* next;
};

static BoatEntryEntry* _buckets[4096] = { 0 };


int BoatRegistry_add(Boat* boat, const char* name)
{
	if (findBoatEntry(name) != 0)
	{
		return BoatRegistry_EXISTS;
	}

	BoatEntry* newEntry = (BoatEntry*) malloc(sizeof(BoatEntry));
	newEntry->name = strdup(name);
	newEntry->boat = boat;

	newEntry->next = 0;
	newEntry->prev = _last;

	if (!_first)
	{
		_first = newEntry;
	}
	else
	{
		_last->next = newEntry;
	}

	_last = newEntry;


	const int bucket = hashName(name);

	BoatEntryEntry* newEntryEntry = (BoatEntryEntry*) malloc(sizeof(BoatEntryEntry));
	newEntryEntry->e = newEntry;
	newEntryEntry->next = 0;

	if (!_buckets[bucket])
	{
		_buckets[bucket] = newEntryEntry;
	}
	else
	{
		newEntryEntry->next = _buckets[bucket];
		_buckets[bucket] = newEntryEntry;
	}


	_boatCount++;
	return BoatRegistry_OK;
}

Boat* BoatRegistry_get(const char* name)
{
	const BoatEntry* e = findBoatEntry(name);

	return (e != 0) ? e->boat : 0;
}

Boat* BoatRegistry_remove(const char* name)
{
	const int bucket = hashName(name);
	BoatEntryEntry* bee = _buckets[bucket];

	const BoatEntryEntry* const first = bee;
	BoatEntryEntry* last = 0;

	while (bee)
	{
		if (0 == strcmp(name, bee->e->name))
		{
			if (bee == first)
			{
				_buckets[bucket] = bee->next;
			}
			else
			{
				last->next = bee->next;
			}

			BoatEntry* e = bee->e;

			if (e->prev)
			{
				e->prev->next = e->next;
			}
			else
			{
				_first = e->next;
			}

			if (e->next)
			{
				e->next->prev = e->prev;
			}
			else
			{
				_last = e->prev;
			}

			Boat* boat = e->boat;

			free(bee);

			free(e->name);
			free(e);

			_boatCount--;
			return boat;
		}

		last = bee;
		bee = bee->next;
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


// A fast and good enough hash function for typical boat "names" for our purposes
static int hashName(const char* name)
{
	static const int PRIMES[] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53 };

	unsigned int n = 1;

	for (const char* c = name; *c != 0; c++)
	{
		n *= PRIMES[((unsigned int)*c) & 0x0f];
		n >>= 1;
	}

	return (n & 0x0fff);
}

static BoatEntry* findBoatEntry(const char* name)
{
	unsigned int bucket = hashName(name);
	const BoatEntryEntry* bee = _buckets[bucket];
	while (bee)
	{
		if (strcmp(name, bee->e->name) == 0)
		{
			return bee->e;
		}

		bee = bee->next;
	}

	return 0;
}
