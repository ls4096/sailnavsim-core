/**
 * Copyright (C) 2021-2024 ls4096 <ls4096@8bitbyte.ca>
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
#include <netdb.h>
#include <pthread.h>
#include <stdatomic.h>
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
#include "Boat.h"
#include "BoatRegistry.h"
#include "Command.h"
#include "ErrLog.h"
#include "WxUtils.h"


#define ERRLOG_ID "NetServer"
#define THREAD_NAME "NetServer"
#define WORKER_THREAD_NAME_PREFIX "NSWorker"


#define REQ_TYPE_INVALID				(0)
#define REQ_TYPE_GET_WIND				(1)
#define REQ_TYPE_GET_WIND_ADJCUR			(2)
#define REQ_TYPE_GET_WIND_GUST				(3)
#define REQ_TYPE_GET_WIND_GUST_ADJCUR			(4)
#define REQ_TYPE_GET_OCEAN_CURRENT			(5)
#define REQ_TYPE_GET_SEA_ICE				(6)
#define REQ_TYPE_GET_WAVE_HEIGHT			(7)
#define REQ_TYPE_GET_BOAT_DATA				(8)
#define REQ_TYPE_GET_BOAT_DATA_NO_CELESTIAL		(9)
#define REQ_TYPE_BOAT_CMD				(10)
#define REQ_TYPE_BOAT_GROUP_MEMBERSHIP			(11)
#define REQ_TYPE_SYS_REQUEST_COUNTS			(12)
#define COUNTERS_REQ_TYPE_COUNT				(REQ_TYPE_SYS_REQUEST_COUNTS + 1)

static const char* REQ_STR_GET_WIND =			"wind";
static const char* REQ_STR_GET_WIND_ADJCUR =		"wind_c";
static const char* REQ_STR_GET_WIND_GUST =		"wind_gust";
static const char* REQ_STR_GET_WIND_GUST_ADJCUR =	"wind_gust_c";
static const char* REQ_STR_GET_OCEAN_CURRENT =		"ocean_current";
static const char* REQ_STR_GET_SEA_ICE =		"sea_ice";
static const char* REQ_STR_GET_WAVE_HEIGHT =		"wave_height";
static const char* REQ_STR_GET_BOAT_DATA =		"bd";
static const char* REQ_STR_GET_BOAT_DATA_NO_CELESTIAL =	"bd_nc";
static const char* REQ_STR_BOAT_CMD =			"boatcmd";
static const char* REQ_STR_BOAT_GROUP_MEMBERSHIP =	"boatgroupmembers";
static const char* REQ_STR_SYS_REQUEST_COUNTS =		"sys_req_counts";


#define REQ_MAX_ARG_COUNT (2)

#define REQ_VAL_NONE	(0)
#define REQ_VAL_INT	(1)
#define REQ_VAL_DOUBLE	(2)
#define REQ_VAL_STRING	(3)

static const uint8_t REQ_VALS_NONE[REQ_MAX_ARG_COUNT] = { REQ_VAL_NONE, REQ_VAL_NONE };

static const uint8_t REQ_VALS_LAT_LON[REQ_MAX_ARG_COUNT] = { REQ_VAL_DOUBLE, REQ_VAL_DOUBLE };
static const uint8_t REQ_VALS_BOAT_DATA[REQ_MAX_ARG_COUNT] = { REQ_VAL_STRING, REQ_VAL_NONE };

static const uint8_t REQ_VALS_BOAT_GROUP_MEMBERSHIP[REQ_MAX_ARG_COUNT] = { REQ_VAL_STRING, REQ_VAL_NONE };

typedef union
{
	int i;
	double d;
	const char* s;
} ReqValue;


#define RECV_MSG_BUF_SIZE (1024)
#define SEND_MSG_BUF_SIZE (64 * 1024)


#define CACHE_LINE_SIZE (64)
typedef union {
	atomic_int_fast64_t i64;
	atomic_uint_fast64_t u64;
} __attribute__((aligned(CACHE_LINE_SIZE))) CacheLineAlignedAtomic;

// Statistics counters
#define COUNTER_ACCEPT		(0)
#define COUNTER_ACCEPT_FAIL	(1)
#define COUNTER_READ		(2)
#define COUNTER_READ_FAIL	(3)
#define COUNTER_DATA_TOO_LONG	(4)
#define COUNTER_MESSAGE		(5)
#define COUNTER_MESSAGE_FAIL	(6)
#define COUNTERS_COUNT		(COUNTER_MESSAGE_FAIL + 1)
static CacheLineAlignedAtomic _counter[COUNTERS_COUNT] = { 0 };

// Request type statistics counters
static CacheLineAlignedAtomic _counterReqType[COUNTERS_REQ_TYPE_COUNT] = { 0 };


static int startListen(const char* host, unsigned int port);

static void* netServerThreadMain(void* arg);
static int queueAcceptedFd(int fd);

static void* netServerWorkerThreadMain(void* arg);
static int getNextFd();
static void processConnection(unsigned int workerThreadId, int fd);

static void incCounter(int ctr);
static void incReqTypeCounter(int ctr);

static int getReqType(const char* s);
static const uint8_t* getReqExpectedValueTypes(int reqType);
static bool areValuesValidForReqType(int reqType, ReqValue values[REQ_MAX_ARG_COUNT]);

static void populateWindResponse(char* buf, size_t bufSize, const proteus_GeoPos* pos, bool gust, bool adjustForCurrent);
static void populateOceanResponse(char* buf, size_t bufSize, const proteus_GeoPos* pos, bool seaIce);
static void populateWaveResponse(char* buf, size_t bufSize, const proteus_GeoPos* pos);
static void populateBoatDataResponse(char* buf, size_t bufSize, const char* key, bool noCelestial);
static void populateBoatCmdResponse(char* buf, size_t bufSize, char** tok);
static void populateBoatGroupMembershipResponse(char* buf, size_t bufSize, const char* key);
static void populateSysRequestCountsResponse(char* buf, size_t bufSize);


static pthread_t _netServerThread;
static int _listenFd = 0;


int NetServer_init(const char* host, unsigned int port, unsigned int workerThreads)
{
	if (startListen(host, port) != 0)
	{
		ERRLOG1("Failed to start listening on localhost port %d!", port);
		return -2;
	}

	ERRLOG1("Listening on port %d", port);

	unsigned int* wt = malloc(sizeof(unsigned int));
	if (!wt)
	{
		ERRLOG("Failed to alloc arg wt for thread!");
		return -1;
	}

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

int NetServer_handleRequest(int writeFd, char* reqStr)
{
	char* s;
	char* t;

	if ((s = strtok_r(reqStr, ",", &t)) == 0)
	{
		goto fail;
	}

	const int reqType = getReqType(s);
	incReqTypeCounter(reqType);

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


	char buf[SEND_MSG_BUF_SIZE];
	proteus_GeoPos pos = { values[0].d, values[1].d };

	switch (reqType)
	{
		case REQ_TYPE_GET_WIND:
		case REQ_TYPE_GET_WIND_ADJCUR:
			populateWindResponse(buf, SEND_MSG_BUF_SIZE, &pos, false, (reqType == REQ_TYPE_GET_WIND_ADJCUR));
			break;
		case REQ_TYPE_GET_WIND_GUST:
		case REQ_TYPE_GET_WIND_GUST_ADJCUR:
			populateWindResponse(buf, SEND_MSG_BUF_SIZE, &pos, true, (reqType == REQ_TYPE_GET_WIND_GUST_ADJCUR));
			break;
		case REQ_TYPE_GET_OCEAN_CURRENT:
			populateOceanResponse(buf, SEND_MSG_BUF_SIZE, &pos, false);
			break;
		case REQ_TYPE_GET_SEA_ICE:
			populateOceanResponse(buf, SEND_MSG_BUF_SIZE, &pos, true);
			break;
		case REQ_TYPE_GET_WAVE_HEIGHT:
			populateWaveResponse(buf, SEND_MSG_BUF_SIZE, &pos);
			break;
		case REQ_TYPE_GET_BOAT_DATA:
		case REQ_TYPE_GET_BOAT_DATA_NO_CELESTIAL:
			populateBoatDataResponse(buf, SEND_MSG_BUF_SIZE, values[0].s, (reqType == REQ_TYPE_GET_BOAT_DATA_NO_CELESTIAL));
			break;
		case REQ_TYPE_BOAT_CMD:
			populateBoatCmdResponse(buf, SEND_MSG_BUF_SIZE, &t);
			break;
		case REQ_TYPE_BOAT_GROUP_MEMBERSHIP:
			populateBoatGroupMembershipResponse(buf, SEND_MSG_BUF_SIZE, values[0].s);
			break;
		case REQ_TYPE_SYS_REQUEST_COUNTS:
			populateSysRequestCountsResponse(buf, SEND_MSG_BUF_SIZE);
			break;
		default:
			goto fail;
	}

	const int bl = strlen(buf);
	int wt = 0;

	for (;;)
	{
		int wb = write(writeFd, buf + wt, bl - wt);

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
	if (write(writeFd, "error\n", 6) != 6)
	{
		return -1;
	}

	return -1;
}


static int startListen(const char* host, unsigned int port)
{
	int rc = 0;

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);

	if (!host)
	{
		sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	}
	else
	{
		struct addrinfo* ai;
		rc = getaddrinfo(host, 0, 0, &ai);
		if (rc != 0)
		{
			ERRLOG1("Failed to getaddrinfo()! rc=%d", rc);
			return -4;
		}

		if (ai->ai_family != AF_INET)
		{
			ERRLOG("Unsupported address family!");
			return -5;
		}

		sa.sin_family = ai->ai_family;
		sa.sin_addr = (((struct sockaddr_in*)ai->ai_addr)->sin_addr);

		freeaddrinfo(ai);
	}

	_listenFd = socket(sa.sin_family, SOCK_STREAM, 0);
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
		if (!workerArg)
		{
			// TODO: Make this fatal?
			ERRLOG1("Failed to alloc workerArg for thread %u!", i);
			continue;
		}

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
		if ((_counter[COUNTER_ACCEPT].u64 & 0x03ff) == 0)
		{
			ERRLOG7("Stats: accept=%lu, accept_fail=%lu, read=%lu, read_fail=%lu, data_too_long=%lu, message=%lu, message_fail=%lu", \
					_counter[COUNTER_ACCEPT].u64, \
					_counter[COUNTER_ACCEPT_FAIL].u64, \
					_counter[COUNTER_READ].u64, \
					_counter[COUNTER_READ_FAIL].u64, \
					_counter[COUNTER_DATA_TOO_LONG].u64, \
					_counter[COUNTER_MESSAGE].u64, \
					_counter[COUNTER_MESSAGE_FAIL].u64);
		}

		struct sockaddr_in peer;
		socklen_t sl = sizeof(struct sockaddr_in);

		int fd = accept(_listenFd, (struct sockaddr*) &peer, &sl);
		_counter[COUNTER_ACCEPT].u64++;

		if (fd < 0)
		{
			ERRLOG1("Failed accept! errno=%d", errno);
			_counter[COUNTER_ACCEPT_FAIL].u64++;

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

static void processConnection(unsigned int workerThreadId, int fd)
{
	char buf[RECV_MSG_BUF_SIZE];
	buf[0] = 0;

	// Number of bytes in read buffer ready for message parsing/processing
	int readyBytes = 0;

	// End of read stream indicator
	bool eos = false;

	// Request loop
	for (;;)
	{
		int rb = 0;

		if (readyBytes == RECV_MSG_BUF_SIZE)
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
				rb = read(fd, buf + readyBytes, RECV_MSG_BUF_SIZE - readyBytes);
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
		if (NetServer_handleRequest(fd, buf) != 0)
		{
			ERRLOG1("worker%u: Failed to handle request!", workerThreadId);
			incCounter(COUNTER_MESSAGE_FAIL);

			break;
		}

		// Move start of next message data to start of buffer.
		// NOTE: memmove() allows source and destination buffers to overlap.
		memmove(buf, buf + i, RECV_MSG_BUF_SIZE - i);
		readyBytes -= i;

		if (eos && readyBytes == 0)
		{
			// End of stream, and no more bytes to process.
			break;
		}
	}
}

static void incCounter(int ctr)
{
	_counter[ctr].u64++;
}

static void incReqTypeCounter(int ctr)
{
	_counterReqType[ctr].u64++;
}

static int getReqType(const char* s)
{
	if (strcmp(REQ_STR_GET_BOAT_DATA_NO_CELESTIAL, s) == 0)
	{
		// This is generally expected to be the most common request type,
		// so check for it first.
		return REQ_TYPE_GET_BOAT_DATA_NO_CELESTIAL;
	}
	else if (strcmp(REQ_STR_GET_WIND, s) == 0)
	{
		return REQ_TYPE_GET_WIND;
	}
	else if (strcmp(REQ_STR_GET_WIND_ADJCUR, s) == 0)
	{
		return REQ_TYPE_GET_WIND_ADJCUR;
	}
	else if (strcmp(REQ_STR_GET_WIND_GUST, s) == 0)
	{
		return REQ_TYPE_GET_WIND_GUST;
	}
	else if (strcmp(REQ_STR_GET_WIND_GUST_ADJCUR, s) == 0)
	{
		return REQ_TYPE_GET_WIND_GUST_ADJCUR;
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
	else if (strcmp(REQ_STR_BOAT_CMD, s) == 0)
	{
		return REQ_TYPE_BOAT_CMD;
	}
	else if (strcmp(REQ_STR_BOAT_GROUP_MEMBERSHIP, s) == 0)
	{
		return REQ_TYPE_BOAT_GROUP_MEMBERSHIP;
	}
	else if (strcmp(REQ_STR_SYS_REQUEST_COUNTS, s) == 0)
	{
		return REQ_TYPE_SYS_REQUEST_COUNTS;
	}

	return REQ_TYPE_INVALID;
}

static const uint8_t* getReqExpectedValueTypes(int reqType)
{
	switch (reqType)
	{
		case REQ_TYPE_GET_WIND:
		case REQ_TYPE_GET_WIND_ADJCUR:
		case REQ_TYPE_GET_WIND_GUST:
		case REQ_TYPE_GET_WIND_GUST_ADJCUR:
		case REQ_TYPE_GET_OCEAN_CURRENT:
		case REQ_TYPE_GET_SEA_ICE:
		case REQ_TYPE_GET_WAVE_HEIGHT:
			return REQ_VALS_LAT_LON;
		case REQ_TYPE_GET_BOAT_DATA:
		case REQ_TYPE_GET_BOAT_DATA_NO_CELESTIAL:
			return REQ_VALS_BOAT_DATA;
		case REQ_TYPE_BOAT_GROUP_MEMBERSHIP:
			return REQ_VALS_BOAT_GROUP_MEMBERSHIP;
	}

	return REQ_VALS_NONE;
}

static bool areValuesValidForReqType(int reqType, ReqValue values[REQ_MAX_ARG_COUNT])
{
	switch (reqType)
	{
		case REQ_TYPE_GET_WIND:
		case REQ_TYPE_GET_WIND_ADJCUR:
		case REQ_TYPE_GET_WIND_GUST:
		case REQ_TYPE_GET_WIND_GUST_ADJCUR:
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

static void populateWindResponse(char* buf, size_t bufSize, const proteus_GeoPos* pos, bool gust, bool adjustForCurrent)
{
	proteus_Weather wx;
	proteus_Weather_get(pos, &wx, true);

	double gustAngle = wx.wind.angle;

	if (adjustForCurrent)
	{
		proteus_OceanData od;
		if (proteus_Ocean_get(pos, &od))
		{
			gustAngle = WxUtils_adjustWindForCurrent(&wx, &od.current);
		}
	}

	if (gust)
	{
		snprintf(buf, bufSize, "%s,%f,%f,%f,%f\n",
				adjustForCurrent ? REQ_STR_GET_WIND_GUST_ADJCUR : REQ_STR_GET_WIND_GUST,
				pos->lat,
				pos->lon,
				gustAngle,
				wx.windGust);
	}
	else
	{
		snprintf(buf, bufSize, "%s,%f,%f,%f,%f\n",
				adjustForCurrent ? REQ_STR_GET_WIND_ADJCUR : REQ_STR_GET_WIND,
				pos->lat,
				pos->lon,
				wx.wind.angle,
				wx.wind.mag);
	}
}

static void populateOceanResponse(char* buf, size_t bufSize, const proteus_GeoPos* pos, bool seaIce)
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

static void populateWaveResponse(char* buf, size_t bufSize, const proteus_GeoPos* pos)
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
	double leewaySpeed;
	double heelingAngle;

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
			leewaySpeed = boat->leewaySpeed;
			heelingAngle = boat->heelingAngle;
		}
	}

	if (BoatRegistry_OK != BoatRegistry_unlock())
	{
		ERRLOG("Failed to unlock BoatRegistry lock for boat data response!");
	}

	if (boat)
	{
		snprintf(buf, bufSize, "%s,%s,ok,%.6f,%.6f,%.1f,%.2f,%.1f,%.2f,%.2f,%.1f\n",
				noCelestial ? REQ_STR_GET_BOAT_DATA_NO_CELESTIAL : REQ_STR_GET_BOAT_DATA,
				key,
				pos.lat,
				pos.lon,
				v.angle,
				v.mag,
				vGround.angle,
				vGround.mag,
				leewaySpeed,
				heelingAngle);
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

static void populateBoatGroupMembershipResponse(char* buf, size_t bufSize, const char* key)
{
	if (BoatRegistry_OK != BoatRegistry_rdlock())
	{
		ERRLOG("Failed to read-lock BoatRegistry lock for boat group membership response!");
		snprintf(buf, bufSize, "%s,%s,failed\n", REQ_STR_BOAT_GROUP_MEMBERSHIP, key);
		return;
	}

	const BoatEntry* entry = BoatRegistry_getBoatEntry(key);
	if (!entry)
	{
		snprintf(buf, bufSize, "%s,%s,%s\n", REQ_STR_BOAT_GROUP_MEMBERSHIP, key, "noboat");
	}
	else if (!entry->group)
	{
		snprintf(buf, bufSize, "%s,%s,%s\n", REQ_STR_BOAT_GROUP_MEMBERSHIP, key, "nogroup");
	}
	else if ((entry->boat->boatFlags & BOAT_FLAG_LIVE_SHARING_HIDDEN) != 0)
	{
		snprintf(buf, bufSize, "%s,%s,%s\n%s,?\n\n", REQ_STR_BOAT_GROUP_MEMBERSHIP, key, "ok", key);
	}
	else
	{
		const char* resp = BoatRegistry_getBoatsInGroupResponse(entry->group);
		if (!resp)
		{
			snprintf(buf, bufSize, "%s,%s,%s\n", REQ_STR_BOAT_GROUP_MEMBERSHIP, key, "fail");
		}
		else
		{
			snprintf(buf, bufSize, "%s,%s,%s\n%s\n", REQ_STR_BOAT_GROUP_MEMBERSHIP, key, "ok", resp);
			BoatRegistry_freeBoatsInGroupResponse(resp);
		}
	}

	if (BoatRegistry_OK != BoatRegistry_unlock())
	{
		ERRLOG("Failed to unlock BoatRegistry lock for boat group membership response!");
	}
}

static void populateSysRequestCountsResponse(char* buf, size_t bufSize)
{
	if ((COUNTERS_COUNT + COUNTERS_REQ_TYPE_COUNT) * 22 >= bufSize)
	{
		ERRLOG("Failed to write request counts response due to too many counters and/or not enough space in buffer!");
		goto fail;
	}


	int src = snprintf(buf, bufSize, "%s,", REQ_STR_SYS_REQUEST_COUNTS);
	if (src < 2) // < 2 because there must be at least two bytes written here (and similarly below)
	{
		ERRLOG1("snprintf failed with return = %d", src);
		goto fail;
	}

	int pos = src;

	for (int i = 0; i < COUNTERS_COUNT; i++)
	{
		if ((src = snprintf(buf + pos, bufSize - pos, "%lu,", _counter[i].u64)) < 2)
		{
			ERRLOG1("snprintf failed with return = %d", src);
			goto fail;
		}
		pos += src;
	}

	for (int i = 0; i < COUNTERS_REQ_TYPE_COUNT; i++)
	{
		if ((src = snprintf(buf + pos, bufSize - pos, "%lu,", _counterReqType[i].u64)) < 2)
		{
			ERRLOG1("snprintf failed with return = %d", src);
			goto fail;
		}
		pos += src;
	}

	buf[pos - 1] = '\n';

	return;

fail:
	snprintf(buf, bufSize, "%s,%s", REQ_STR_SYS_REQUEST_COUNTS, "fail");
}
