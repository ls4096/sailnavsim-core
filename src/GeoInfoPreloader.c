/**
 * Copyright (C) 2026 ls4096 <ls4096@8bitbyte.ca>
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
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <proteus/GeoPos.h>
#include <proteus/GeoInfo.h>

#include "GeoInfoPreloader.h"

#include "ErrLog.h"


#define ERRLOG_ID "GeoInfoPreloader"
#define THREAD_NAME "GeoTilePreload"


typedef struct Position Position;

struct Position
{
	proteus_GeoPos pos;
	Position* next;
};


static void* preloaderThreadMain();


static pthread_t _preloaderThread;
static Position* _positions = 0;
static Position* _positionsLast = 0;
static pthread_mutex_t _positionsLock;
static pthread_cond_t _positionsCond;

static bool _init = false;


int GeoInfoPreloader_init()
{
	if (0 != pthread_mutex_init(&_positionsLock, 0))
	{
		ERRLOG("Failed to init positions mutex!");
		return -2;
	}

	if (0 != pthread_create(&_preloaderThread, 0, &preloaderThreadMain, 0))
	{
		ERRLOG("Failed to start geo tile preloader thread!");
		return -1;
	}

#if defined(_GNU_SOURCE) && defined(__GLIBC__)
	if (0 != pthread_setname_np(_preloaderThread, THREAD_NAME))
	{
		ERRLOG1("Couldn't set thread name to %s. Continuing anyway.", THREAD_NAME);
	}
#endif

	_init = true;

	return 0;
}

void GeoInfoPreloader_addPosition(const proteus_GeoPos* pos)
{
	if (!_init)
	{
		return;
	}

	Position* p = malloc(sizeof(Position));
	if (!p)
	{
		ERRLOG("addPosition: Failed to malloc Position!");
		return;
	}

	p->pos = *pos;
	p->next = 0;

	if (0 != pthread_mutex_lock(&_positionsLock))
	{
		ERRLOG("addPosition: Failed to lock positions mutex!");
		free(p);
		return;
	}

	if (_positions == 0)
	{
		_positions = p;
	}
	else
	{
		_positionsLast->next = p;
	}

	_positionsLast = p;

	if (0 != pthread_cond_signal(&_positionsCond))
	{
		ERRLOG("addPosition: Failed to signal condvar!");
	}

	if (0 != pthread_mutex_unlock(&_positionsLock))
	{
		ERRLOG("addPosition: Failed to unlock positions mutex!");
	}
}


static void* preloaderThreadMain()
{
	for (;;)
	{
		if (0 != pthread_mutex_lock(&_positionsLock))
		{
			ERRLOG("preloaderThreadMain: Failed to lock positions mutex!");
			sleep(1);
			continue;
		}

		while (_positions == 0)
		{
			if (0 != pthread_cond_wait(&_positionsCond, &_positionsLock))
			{
				ERRLOG("preloaderThreadMain: Failed to wait on condvar!");
			}
		}

		while (_positions != 0)
		{
			Position* p = _positions;
			_positions = p->next;

			if (0 != pthread_mutex_unlock(&_positionsLock))
			{
				ERRLOG("preloaderThreadMain: Failed to unlock positions mutex!");
			}

			// Calling proteus_GeoInfo_isWater() will load the tile if needed.
			proteus_GeoInfo_isWater(&p->pos);
			free(p);

			if (0 != pthread_mutex_lock(&_positionsLock))
			{
				ERRLOG("preloaderThreadMain: Failed to lock positions mutex!");
			}
		}

		if (0 != pthread_mutex_unlock(&_positionsLock))
		{
			ERRLOG("preloaderThreadMain: Failed to unlock positions mutex!");
		}
	}

	return 0;
}
