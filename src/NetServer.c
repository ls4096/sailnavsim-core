/**
 * Copyright (C) 2021 ls4096 <ls4096@8bitbyte.ca>
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

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <proteus/Ocean.h>
#include <proteus/Wave.h>
#include <proteus/Weather.h>

#include "NetServer.h"
#include "BoatRegistry.h"
#include "Command.h"
#include "ErrLog.h"


#define ERRLOG_ID "NetServer"
#define THREAD_NAME "NetServer"
#define WORKER_THREAD_NAME_PREFIX "NSWorker"


#define REQ_TYPE_INVALID				(0)
#define REQ_TYPE_GET_WIND				(1)
#define REQ_TYPE_GET_WIND_GUST				(2)
#define REQ_TYPE_GET_OCEAN_CURRENT			(3)
#define REQ_TYPE_GET_SEA_ICE				(4)
#define REQ_TYPE_GET_WAVE_HEIGHT			(5)
#define REQ_TYPE_GET_BOAT_DATA				(6)
#define REQ_TYPE_GET_BOAT_DATA_NO_CELESTIAL		(7)
#define REQ_TYPE_BOAT_CMD				(8)

static const char* REQ_STR_GET_WIND =			"wind";
static const char* REQ_STR_GET_WIND_GUST =		"wind_gust";
static const char* REQ_STR_GET_OCEAN_CURRENT =		"ocean_current";
static const char* REQ_STR_GET_SEA_ICE =		"sea_ice";
static const char* REQ_STR_GET_WAVE_HEIGHT =		"wave_height";
static const char* REQ_STR_GET_BOAT_DATA =		"bd";
static const char* REQ_STR_GET_BOAT_DATA_NO_CELESTIAL =	"bd_nc";
static const char* REQ_STR_BOAT_CMD =			"boatcmd";


#define REQ_MAX_ARG_COUNT (2)

#define REQ_VAL_NONE	(0)
#define REQ_VAL_INT	(1)
#define REQ_VAL_DOUBLE	(2)
#define REQ_VAL_STRING	(3)

static const uint8_t REQ_VALS_NONE[REQ_MAX_ARG_COUNT] = { REQ_VAL_NONE, REQ_VAL_NONE };

static const uint8_t REQ_VALS_LAT_LON[REQ_MAX_ARG_COUNT] = { REQ_VAL_DOUBLE, REQ_VAL_DOUBLE };
static const uint8_t REQ_VALS_BOAT_DATA[REQ_MAX_ARG_COUNT] = { REQ_VAL_STRING, REQ_VAL_NONE };

typedef union
{
	int i;
	double d;
	const char* s;
} ReqValue;


static int startListen(unsigned int port);

static void* netServerThreadMain(void* arg);
static int queueAcceptedFd(int fd);

static void* netServerWorkerThreadMain(void* arg);
static int getNextFd();
static void processConnection(unsigned int workerThreadId, int fd);
static int handleMessage(int fd, char* reqStr);

static void incCounter(int ctr);

static int getReqType(const char* s);
static const uint8_t* getReqExpectedValueTypes(int reqType);
static bool areValuesValidForReqType(int reqType, ReqValue values[REQ_MAX_ARG_COUNT]);

static void populateWindResponse(char* buf, size_t bufSize, proteus_GeoPos* pos, bool gust);
static void populateOceanResponse(char* buf, size_t bufSize, proteus_GeoPos* pos, bool seaIce);
static void populateWaveResponse(char* buf, size_t bufSize, proteus_GeoPos* pos);
static void populateBoatDataResponse(char* buf, size_t bufSize, const char* key, bool noCelestial);
static void populateBoatCmdResponse(char* buf, size_t bufSize, char** tok);


static pthread_t _netServerThread;
static int _listenFd = 0;


int NetServer_init(unsigned int port, unsigned int workerThreads)
{
	if (startListen(port) != 0)
	{
		ERRLOG1("Failed to start listening on localhost port %d!", port);
		return -2;
	}

	ERRLOG1("Listening on port %d", port);

	unsigned int* wt = malloc(sizeof(unsigned int));
	*wt = workerThreads;

	if (0 != pthread_create(&_netServerThread, 0, &netServerThreadMain, wt))
	{
		ERRLOG("Failed to start net server thread!");
		free(wt);
		return -1;
	}

#if defined(_GNU_SOURCE) && defined(__GLIBC__)
	if (0 != pthread_setname_np(_netServerThread, THREAD_NAME))
	{
		ERRLOG1("Couldn't set thread name to %s. Continuing anyway.", THREAD_NAME);
	}
#endif

	return 0;
}


static int startListen(unsigned int port)
{
	int rc = 0;

	struct sockaddr_in sa;

	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	_listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenFd < 0)
	{
		ERRLOG1("Failed to open socket! errno=%d", errno);
		return -1;
	}

	rc = bind(_listenFd, (struct sockaddr*) &sa, sizeof(struct sockaddr_in));
	if (rc != 0)
	{
		ERRLOG2("Failed to bind socket! rc=%d errno=%d", rc, errno);
		rc = -2;
		goto done;
	}

	rc = listen(_listenFd, 100);
	if (rc != 0)
	{
		ERRLOG2("Failed to listen on socket! rc=%d errno=%d", rc, errno);
		rc = -3;
		goto done;
	}

done:
	if (rc != 0)
	{
		close(_listenFd);
		_listenFd = 0;
	}

	return rc;
}


// Statistics counters
#define COUNTER_ACCEPT		(0)
#define COUNTER_ACCEPT_FAIL	(1)
#define COUNTER_READ		(2)
#define COUNTER_READ_FAIL	(3)
#define COUNTER_DATA_TOO_LONG	(4)
#define COUNTER_MESSAGE		(5)
#define COUNTER_MESSAGE_FAIL	(6)
#define COUNTERS_COUNT		(COUNTER_MESSAGE_FAIL + 1)
static unsigned int _counter[COUNTERS_COUNT] = { 0 };

static pthread_mutex_t _counterLock;


#define MAX_ACCEPTED_FDS (256)
// Circular buffer for accepted fds waiting for a worker thread to free up
static int _acceptedFds[MAX_ACCEPTED_FDS];
static int _acceptedFdsStart = 0;
static int _acceptedFdsNext = 0;
static bool _acceptedFdsHas = false;
static pthread_mutex_t _acceptedFdsLock;
static pthread_cond_t _acceptedFdsCond;


static void* netServerThreadMain(void* arg)
{
	const unsigned int workerThreadCount = *((unsigned int*)arg);
	free(arg);

	pthread_t workerThreads[workerThreadCount];

	if (0 != pthread_mutex_init(&_counterLock, 0))
	{
		ERRLOG("Failed to init counter mutex!");
		return 0;
	}

	if (0 != pthread_mutex_init(&_acceptedFdsLock, 0))
	{
		ERRLOG("Failed to init accepted fds mutex!");
		return 0;
	}

	if (0 != pthread_cond_init(&_acceptedFdsCond, 0))
	{
		ERRLOG("Failed to init accepted fds condvar!");
		return 0;
	}

	ERRLOG1("Starting up %u worker threads...", workerThreadCount);
	for (unsigned int i = 0; i < workerThreadCount; i++)
	{
		unsigned int* workerArg = malloc(sizeof(unsigned int));
		*workerArg = i;
		if (0 != pthread_create(workerThreads + i, 0, &netServerWorkerThreadMain, workerArg))
		{
			// TODO: Make this fatal?
			ERRLOG1("Failed to start worker thread %u!", i);
			free(workerArg);
			continue;
		}

#if defined(_GNU_SOURCE) && defined(__GLIBC__)
		char threadName[32];
		snprintf(threadName, 32, "%s%u", WORKER_THREAD_NAME_PREFIX, i);
		if (0 != pthread_setname_np(workerThreads[i], threadName))
		{
			ERRLOG1("Couldn't set thread name to %s. Continuing anyway.", threadName);
		}
#endif
	}

	ERRLOG("Server thread preparing to accept...");

	for (;;)
	{
		// Occasionally log statistics counters.
		// NOTE: We don't need to lock the mutex for COUNTER_ACCEPT and COUNTER_ACCEPT_FAIL,
		//       since no other thread ought to be reading/writing this counter.
		if ((_counter[COUNTER_ACCEPT] & 0x03ff) == 0)
		{
			unsigned int counters[COUNTERS_COUNT];
			if (0 != pthread_mutex_lock(&_counterLock))
			{
				ERRLOG("Failed to lock counter mutex!");
			}
			else
			{
				memcpy(counters, _counter, COUNTERS_COUNT * sizeof(unsigned int));
				if (0 != pthread_mutex_unlock(&_counterLock))
				{
					ERRLOG("Failed to unlock counter mutex!");
				}

				ERRLOG7("Stats: accept=%u, accept_fail=%u, read=%u, read_fail=%u, data_too_long=%u, message=%u, message_fail=%u", \
						counters[COUNTER_ACCEPT], \
						counters[COUNTER_ACCEPT_FAIL], \
						counters[COUNTER_READ], \
						counters[COUNTER_READ_FAIL], \
						counters[COUNTER_DATA_TOO_LONG], \
						counters[COUNTER_MESSAGE], \
						counters[COUNTER_MESSAGE_FAIL]);
			}
		}

		struct sockaddr_in peer;
		socklen_t sl = sizeof(struct sockaddr_in);

		int fd = accept(_listenFd, (struct sockaddr*) &peer, &sl);
		_counter[COUNTER_ACCEPT]++;

		if (fd < 0)
		{
			ERRLOG1("Failed accept! errno=%d", errno);
			_counter[COUNTER_ACCEPT_FAIL]++;

			continue;
		}

		if (queueAcceptedFd(fd) != 0)
		{
			ERRLOG1("Closing fd %d early due to error queueing accepted fd!", fd);
			close(fd);
		}
	}

	// FIXME: If any threads failed to start above, then we may need to take this into account below.
	for (unsigned int i = 0; i < workerThreadCount; i++)
	{
		// FIXME: Handle return value here (in case queueing failed).
		queueAcceptedFd(-1);
	}

	for (unsigned int i = 0; i < workerThreadCount; i++)
	{
		if (0 != pthread_join(workerThreads[i], 0))
		{
			ERRLOG1("Failed to join worker thread %d!", i);
		}
	}

	pthread_mutex_destroy(&_acceptedFdsLock);
	pthread_cond_destroy(&_acceptedFdsCond);

	pthread_mutex_destroy(&_counterLock);

	close(_listenFd);
	_listenFd = 0;

	return 0;
}

static int queueAcceptedFd(int fd)
{
	int rc = 0;

	if (0 != pthread_mutex_lock(&_acceptedFdsLock))
	{
		ERRLOG("queueAcceptedFd: Failed to lock accepted fds mutex!");
		return -2;
	}

	if (_acceptedFdsHas && (_acceptedFdsStart == _acceptedFdsNext))
	{
		// No more room for accepted fds!
		ERRLOG("Accepted fds queue is full!");
		rc = -1;
		goto done;
	}

	_acceptedFds[_acceptedFdsNext++] = fd;

	if (_acceptedFdsNext == MAX_ACCEPTED_FDS)
	{
		_acceptedFdsNext = 0;
	}

	_acceptedFdsHas = true;

	if (0 != pthread_cond_signal(&_acceptedFdsCond))
	{
		ERRLOG("Failed to signal condvar!");
	}

done:
	if (0 != pthread_mutex_unlock(&_acceptedFdsLock))
	{
		ERRLOG("queueAcceptedFd: Failed to unlock accepted fds mutex!");
	}

	return rc;
}


static void* netServerWorkerThreadMain(void* arg)
{
	const unsigned int workerThreadId = *((unsigned int*)arg);
	free(arg);

	for (;;)
	{
		const int fd = getNextFd();

		if (fd < 0)
		{
			break;
		}

		processConnection(workerThreadId, fd);
		close(fd);
	}

	return 0;
}

static int getNextFd()
{
	int fd = -1;

	if (0 != pthread_mutex_lock(&_acceptedFdsLock))
	{
		ERRLOG("getNextFd: Failed to lock accepted fds mutex!");
		return -1;
	}

	while (!_acceptedFdsHas)
	{
		if (0 != pthread_cond_wait(&_acceptedFdsCond, &_acceptedFdsLock))
		{
			ERRLOG("Failed to wait on condvar!");
			goto done;
		}
	}

	fd = _acceptedFds[_acceptedFdsStart++];

	if (_acceptedFdsStart == MAX_ACCEPTED_FDS)
	{
		_acceptedFdsStart = 0;
	}

	if (_acceptedFdsStart == _acceptedFdsNext)
	{
		_acceptedFdsHas = false;
	}

done:
	if (0 != pthread_mutex_unlock(&_acceptedFdsLock))
	{
		ERRLOG("getNextFd: Failed to unlock mutex!");
	}

	return fd;
}

#define MSG_BUF_SIZE (1024)

static void processConnection(unsigned int workerThreadId, int fd)
{
	char buf[MSG_BUF_SIZE];
	buf[0] = 0;

	// Number of bytes in read buffer ready for message parsing/processing
	int readyBytes = 0;

	// End of read stream indicator
	bool eos = false;

	// Request loop
	for (;;)
	{
		int rb = 0;

		if (readyBytes == MSG_BUF_SIZE)
		{
			// We've encountered a request message that doesn't fit inside the buffer.
			ERRLOG1("worker%u: Excessive message length!", workerThreadId);
			incCounter(COUNTER_DATA_TOO_LONG);

			break;
		}

		if (!eos)
		{
			// Not the end of the request stream yet, so possibly try to read more data...

			fd_set rfds;
			FD_ZERO(&rfds);
			FD_SET(fd, &rfds);
			struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };

			// Attempt to read from the fd if either of the following conditions is true:
			// 	1. select() tells us the fd has data ready for read.
			// 	2. We have no more request bytes to process for messages in our local buffer.
			if (select(fd + 1, &rfds, 0, 0, &tv) != 0 || readyBytes == 0)
			{
				// Read from the fd.
				rb = read(fd, buf + readyBytes, MSG_BUF_SIZE - readyBytes);
				incCounter(COUNTER_READ);

				if (rb < 0)
				{
					ERRLOG2("worker%u: Failed read! errno=%d", workerThreadId, errno);
					incCounter(COUNTER_READ_FAIL);

					break;
				}
				else if (rb == 0)
				{
					// End of stream.
					eos = true;
				}
			}
		}

		// Find the end of the next request message string.
		int i = 0;
		bool foundNewline = false;
		for (const char* s = buf; *s != 0 && i < rb + readyBytes; s++)
		{
			i++;
			if (*s == '\n')
			{
				foundNewline = true;
				break;
			}
		}

		readyBytes += rb;

		if (!foundNewline)
		{
			// Didn't find newline in buffer.
			if (eos)
			{
				// End of request stream, so break out of request loop.
				break;
			}
			else
			{
				// Not end of request stream, so keep trying to read.
				continue;
			}
		}
		foundNewline = false;

		// Replace newline in buffer with null terminator.
		buf[i - 1] = 0;

		// Handle the request message.
		incCounter(COUNTER_MESSAGE);
		if (handleMessage(fd, buf) != 0)
		{
			ERRLOG1("worker%u: Failed to handle request!", workerThreadId);
			incCounter(COUNTER_MESSAGE_FAIL);

			break;
		}

		// Move start of next message data to start of buffer.
		// NOTE: memmove() allows source and destination buffers to overlap.
		memmove(buf, buf + i, MSG_BUF_SIZE - i);
		readyBytes -= i;

		if (eos && readyBytes == 0)
		{
			// End of stream, and no more bytes to process.
			break;
		}
	}
}

static int handleMessage(int fd, char* reqStr)
{
	char* s;
	char* t;

	if ((s = strtok_r(reqStr, ",", &t)) == 0)
	{
		goto fail;
	}

	const int reqType = getReqType(s);
	if (reqType == REQ_TYPE_INVALID)
	{
		goto fail;
	}

	const uint8_t* vals = getReqExpectedValueTypes(reqType);

	ReqValue values[REQ_MAX_ARG_COUNT];

	for (int i = 0; i < REQ_MAX_ARG_COUNT; i++)
	{
		switch (vals[i])
		{
			case REQ_VAL_NONE:
				break;

			case REQ_VAL_INT:
			case REQ_VAL_DOUBLE:
			case REQ_VAL_STRING:
				if ((s = strtok_r(0, ",", &t)) == 0)
				{
					goto fail;
				}

				if (vals[i] == REQ_VAL_INT)
				{
					values[i].i = strtol(s, 0, 10);
				}
				else if (vals[i] == REQ_VAL_DOUBLE)
				{
					values[i].d = strtod(s, 0);
				}
				else
				{
					values[i].s = s;
				}

				break;

			default:
				goto fail;
		}
	}

	if (!areValuesValidForReqType(reqType, values))
	{
		goto fail;
	}


	char buf[MSG_BUF_SIZE];
	proteus_GeoPos pos = { values[0].d, values[1].d };

	switch (reqType)
	{
		case REQ_TYPE_GET_WIND:
			populateWindResponse(buf, MSG_BUF_SIZE, &pos, false);
			break;
		case REQ_TYPE_GET_WIND_GUST:
			populateWindResponse(buf, MSG_BUF_SIZE, &pos, true);
			break;
		case REQ_TYPE_GET_OCEAN_CURRENT:
			populateOceanResponse(buf, MSG_BUF_SIZE, &pos, false);
			break;
		case REQ_TYPE_GET_SEA_ICE:
			populateOceanResponse(buf, MSG_BUF_SIZE, &pos, true);
			break;
		case REQ_TYPE_GET_WAVE_HEIGHT:
			populateWaveResponse(buf, MSG_BUF_SIZE, &pos);
			break;
		case REQ_TYPE_GET_BOAT_DATA:
		case REQ_TYPE_GET_BOAT_DATA_NO_CELESTIAL:
			populateBoatDataResponse(buf, MSG_BUF_SIZE, values[0].s, (reqType == REQ_TYPE_GET_BOAT_DATA_NO_CELESTIAL));
			break;
		case REQ_TYPE_BOAT_CMD:
			populateBoatCmdResponse(buf, MSG_BUF_SIZE, &t);
			break;
		default:
			goto fail;
	}

	const int bl = strlen(buf);
	int wt = 0;

	for (;;)
	{
		int wb = write(fd, buf + wt, bl - wt);

		if (wb < 0)
		{
			return -1;
		}
		else
		{
			wt += wb;
		}

		if (wt == bl)
		{
			break;
		}
	}

	return 0;

fail:
	if (write(fd, "error\n", 6) != 6)
	{
		return -1;
	}

	return -1;
}

static void incCounter(int ctr)
{
	pthread_mutex_lock(&_counterLock);
	_counter[ctr]++;
	pthread_mutex_unlock(&_counterLock);
}

static int getReqType(const char* s)
{
	if (strcmp(REQ_STR_GET_WIND, s) == 0)
	{
		return REQ_TYPE_GET_WIND;
	}
	else if (strcmp(REQ_STR_GET_WIND_GUST, s) == 0)
	{
		return REQ_TYPE_GET_WIND_GUST;
	}
	else if (strcmp(REQ_STR_GET_OCEAN_CURRENT, s) == 0)
	{
		return REQ_TYPE_GET_OCEAN_CURRENT;
	}
	else if (strcmp(REQ_STR_GET_SEA_ICE, s) == 0)
	{
		return REQ_TYPE_GET_SEA_ICE;
	}
	else if (strcmp(REQ_STR_GET_WAVE_HEIGHT, s) == 0)
	{
		return REQ_TYPE_GET_WAVE_HEIGHT;
	}
	else if (strcmp(REQ_STR_GET_BOAT_DATA, s) == 0)
	{
		return REQ_TYPE_GET_BOAT_DATA;
	}
	else if (strcmp(REQ_STR_GET_BOAT_DATA_NO_CELESTIAL, s) == 0)
	{
		return REQ_TYPE_GET_BOAT_DATA_NO_CELESTIAL;
	}
	else if (strcmp(REQ_STR_BOAT_CMD, s) == 0)
	{
		return REQ_TYPE_BOAT_CMD;
	}

	return REQ_TYPE_INVALID;
}

static const uint8_t* getReqExpectedValueTypes(int reqType)
{
	switch (reqType)
	{
		case REQ_TYPE_GET_WIND:
		case REQ_TYPE_GET_WIND_GUST:
		case REQ_TYPE_GET_OCEAN_CURRENT:
		case REQ_TYPE_GET_SEA_ICE:
		case REQ_TYPE_GET_WAVE_HEIGHT:
			return REQ_VALS_LAT_LON;
		case REQ_TYPE_GET_BOAT_DATA:
		case REQ_TYPE_GET_BOAT_DATA_NO_CELESTIAL:
			return REQ_VALS_BOAT_DATA;
	}

	return REQ_VALS_NONE;
}

static bool areValuesValidForReqType(int reqType, ReqValue values[REQ_MAX_ARG_COUNT])
{
	switch (reqType)
	{
		case REQ_TYPE_GET_WIND:
		case REQ_TYPE_GET_WIND_GUST:
		case REQ_TYPE_GET_OCEAN_CURRENT:
		case REQ_TYPE_GET_SEA_ICE:
		case REQ_TYPE_GET_WAVE_HEIGHT:
		{
			return (values[0].d >= -90.0 && values[0].d <= 90.0 &&
					values[1].d >= -180.0 && values[1].d <= 180.0);
		}
	}

	// All other request types either do not use request values or have no particular restrictions.
	return true;
}

#define INVALID_INTEGER_VALUE (-999)
#define INVALID_DOUBLE_VALUE (-999.0)

static void populateWindResponse(char* buf, size_t bufSize, proteus_GeoPos* pos, bool gust)
{
	proteus_Weather wx;
	proteus_Weather_get(pos, &wx, true);

	snprintf(buf, bufSize, "%s,%f,%f,%f,%f\n",
			gust ? REQ_STR_GET_WIND_GUST : REQ_STR_GET_WIND,
			pos->lat,
			pos->lon,
			wx.wind.angle,
			gust ? wx.windGust : wx.wind.mag);
}

static void populateOceanResponse(char* buf, size_t bufSize, proteus_GeoPos* pos, bool seaIce)
{
	proteus_OceanData od;
	const bool valid = proteus_Ocean_get(pos, &od);

	if (seaIce)
	{
		snprintf(buf, bufSize, "%s,%f,%f,%f\n",
				REQ_STR_GET_SEA_ICE,
				pos->lat,
				pos->lon,
				valid ? od.ice : INVALID_DOUBLE_VALUE);
	}
	else
	{
		snprintf(buf, bufSize, "%s,%f,%f,%f,%f\n",
				REQ_STR_GET_OCEAN_CURRENT,
				pos->lat,
				pos->lon,
				valid ? od.current.angle : INVALID_DOUBLE_VALUE,
				valid ? od.current.mag : INVALID_DOUBLE_VALUE);
	}
}

static void populateWaveResponse(char* buf, size_t bufSize, proteus_GeoPos* pos)
{
	proteus_WaveData wd;
	const bool valid = proteus_Wave_get(pos, &wd);

	snprintf(buf, bufSize, "%s,%f,%f,%f\n",
			REQ_STR_GET_WAVE_HEIGHT,
			pos->lat,
			pos->lon,
			valid ? wd.waveHeight : INVALID_DOUBLE_VALUE);
}

static void populateBoatDataResponse(char* buf, size_t bufSize, const char* key, bool noCelestial)
{
	if (BoatRegistry_OK != BoatRegistry_rdlock())
	{
		ERRLOG("Failed to read-lock BoatRegistry lock for boat data response!");
		snprintf(buf, bufSize, "%s,%s,failed\n", REQ_STR_GET_BOAT_DATA, key);
		return;
	}

	const Boat* boat = BoatRegistry_get(key);

	proteus_GeoPos pos;
	proteus_GeoVec v;
	proteus_GeoVec vGround;

	if (boat)
	{
		if (noCelestial && (boat->boatFlags & BOAT_FLAG_CELESTIAL))
		{
			boat = 0;
		}
		else
		{
			pos = boat->pos;
			v = boat->v;
			vGround = boat->vGround;
		}
	}

	if (BoatRegistry_OK != BoatRegistry_unlock())
	{
		ERRLOG("Failed to unlock BoatRegistry lock for boat data response!");
	}

	if (boat)
	{
		snprintf(buf, bufSize, "%s,%s,ok,%.6f,%.6f,%.1f,%.2f,%.1f,%.2f\n",
				noCelestial ? REQ_STR_GET_BOAT_DATA_NO_CELESTIAL : REQ_STR_GET_BOAT_DATA,
				key,
				pos.lat,
				pos.lon,
				v.angle,
				v.mag,
				vGround.angle,
				vGround.mag);
	}
	else
	{
		snprintf(buf, bufSize, "%s,%s,noboat\n", (noCelestial ? REQ_STR_GET_BOAT_DATA_NO_CELESTIAL : REQ_STR_GET_BOAT_DATA), key);
	}
}

static void populateBoatCmdResponse(char* buf, size_t bufSize, char** tok)
{
	int rc = -1;
	char* s;

	if ((s = strtok_r(0, "\n", tok)) == 0)
	{
		goto fail;
	}

	rc = Command_add(s);

fail:
	snprintf(buf, bufSize, "%s,%s\n", REQ_STR_BOAT_CMD, (rc == 0) ? "ok" : "fail");
}
