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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <sailnavsim_rustlib.h>

#include "BoatRegistry.h"
#include "ErrLog.h"


#define ERRLOG_ID "BoatRegistry"


static BoatEntry* _first = 0;
static BoatEntry* _last = 0;
static unsigned int _boatCount = 0;

static pthread_rwlock_t _lock = PTHREAD_RWLOCK_INITIALIZER;

static void* _rustlibRegistry = 0;


static int hashName(const char* name);
static BoatEntry* findBoatEntry(const char* name);


typedef struct BoatEntryEntry BoatEntryEntry;

struct BoatEntryEntry
{
	BoatEntry* e;
	BoatEntryEntry* next;
};

#define BUCKET_COUNT (4096)
#define BUCKET_MASK (0x0fff)

static BoatEntryEntry* _buckets[BUCKET_COUNT] = { 0 };


int BoatRegistry_init()
{
	_rustlibRegistry = sailnavsim_rustlib_boatregistry_new();
	return (_rustlibRegistry ? 0 : -1);
}

void BoatRegistry_destroy()
{
	if (_rustlibRegistry)
	{
		sailnavsim_rustlib_boatregistry_free(_rustlibRegistry);
		_rustlibRegistry = 0;
	}
}

int BoatRegistry_add(Boat* boat, const char* name, const char* group, const char* boatAltName)
{
	if (findBoatEntry(name) != 0)
	{
		return BoatRegistry_EXISTS;
	}

	BoatEntry* newEntry = malloc(sizeof(BoatEntry));
	if (!newEntry)
	{
		ERRLOG("Failed to alloc BoatEntry!");
		return BoatRegistry_FAILED;
	}

	newEntry->name = strdup(name);
	if (!newEntry->name)
	{
		ERRLOG("Failed to alloc newEntry->name!");
		free(newEntry);
		return BoatRegistry_FAILED;
	}

	if (group)
	{
		newEntry->group = strdup(group);
		if (!newEntry->group)
		{
			ERRLOG("Failed to alloc newEntry->group!");
			free(newEntry->name);
			free(newEntry);
			return BoatRegistry_FAILED;
		}
	}
	else
	{
		newEntry->group = 0;
	}

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

	BoatEntryEntry* newEntryEntry = malloc(sizeof(BoatEntryEntry));
	if (!newEntryEntry)
	{
		ERRLOG("Failed to alloc BoatEntryEntry!");
		free(newEntry);
		free(newEntry->name);
		if (newEntry->group)
		{
			free(newEntry->group);
		}
		return BoatRegistry_FAILED;
	}

	int rc;
	if (group && (0 != (rc = sailnavsim_rustlib_boatregistry_group_add_boat(_rustlibRegistry, group, name, boatAltName))))
	{
		ERRLOG1("Failed to add boat to group! rc=", rc);
		free(newEntry);
		free(newEntry->name);
		if (newEntry->group)
		{
			free(newEntry->group);
		}
		free(newEntryEntry);
		return BoatRegistry_FAILED;
	}

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

const BoatEntry* BoatRegistry_getBoatEntry(const char* name)
{
	return findBoatEntry(name);
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
			if (bee->e->group)
			{
				sailnavsim_rustlib_boatregistry_group_remove_boat(_rustlibRegistry, bee->e->group, name);
			}

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
			if (e->group)
			{
				free(e->group);
			}
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

const char* BoatRegistry_getBoatsInGroupResponse(const char* group)
{
	return sailnavsim_rustlib_boatregistry_produce_group_membership_response(_rustlibRegistry, group);
}

void BoatRegistry_freeBoatsInGroupResponse(const char* resp)
{
	sailnavsim_rustlib_boatregistry_free_group_membership_response((char*)resp);
}

int BoatRegistry_rdlock()
{
	if (0 != pthread_rwlock_rdlock(&_lock))
	{
		ERRLOG("Failed to lock for read!");
		return BoatRegistry_FAILED;
	}

	return BoatRegistry_OK;
}

int BoatRegistry_wrlock()
{
	if (0 != pthread_rwlock_wrlock(&_lock))
	{
		ERRLOG("Failed to lock for write!");
		return BoatRegistry_FAILED;
	}

	return BoatRegistry_OK;
}

int BoatRegistry_unlock()
{
	if (0 != pthread_rwlock_unlock(&_lock))
	{
		ERRLOG("Failed to unlock!");
		return BoatRegistry_FAILED;
	}

	return BoatRegistry_OK;
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

	return (n & BUCKET_MASK);
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
