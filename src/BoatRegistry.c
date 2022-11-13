/**
 * Copyright (C) 2020-2022 ls4096 <ls4096@8bitbyte.ca>
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

static pthread_rwlock_t _lock = PTHREAD_RWLOCK_INITIALIZER;

static void* _rustlibRegistry = 0;


static BoatEntry* findBoatEntry(const char* name);


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

void* BoatRegistry_registry()
{
	return _rustlibRegistry;
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

	int rc;

	if (0 != (rc = sailnavsim_rustlib_boatregistry_add_boat_entry(_rustlibRegistry, newEntry, name)))
	{
		ERRLOG1("Failed to add boat to rustlib registry! rc=%d", rc);

		free(newEntry->name);
		if (newEntry->group)
		{
			free(newEntry->group);
		}
		free(newEntry);

		return BoatRegistry_FAILED;
	}

	if (group && (0 != (rc = sailnavsim_rustlib_boatregistry_group_add_boat(_rustlibRegistry, group, name, boatAltName))))
	{
		ERRLOG1("Failed to add boat to group! rc=%d", rc);

		BoatEntry* removedEntry = sailnavsim_rustlib_boatregistry_remove_boat_entry(_rustlibRegistry, name);
		if (removedEntry != newEntry)
		{
			ERRLOG("Unexpected unequal removed BoatEntry compared to local BoatEntry!");
		}

		free(newEntry->name);
		if (newEntry->group)
		{
			free(newEntry->group);
		}
		free(newEntry);

		return BoatRegistry_FAILED;
	}

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
	BoatEntry* e = sailnavsim_rustlib_boatregistry_remove_boat_entry(_rustlibRegistry, name);
	if (!e)
	{
		return 0;
	}

	Boat* boat = e->boat;

	free(e->name);
	if (e->group)
	{
		sailnavsim_rustlib_boatregistry_group_remove_boat(_rustlibRegistry, e->group, name);
		free(e->group);
	}
	free(e);

	return boat;
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


static BoatEntry* findBoatEntry(const char* name)
{
	return sailnavsim_rustlib_boatregistry_get_boat_entry(_rustlibRegistry, name);
}
