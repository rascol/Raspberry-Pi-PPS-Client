/**
 * @file pps-sntp.cpp
 * @brief This file contains functions and structures for accessing time updates via SNTP.
 */

/*
 * Copyright (C) 2016-2018  Raymond S. Connell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "../client/pps-client.h"
#define ADDR_LEN 17
extern struct G g;

/**
 * Local file-scope shared variables.
 */
static struct sntpLocalVars {
	char *ntp_server[MAX_SERVERS];
	int serverTimeDiff[MAX_SERVERS];
	bool threadIsBusy[MAX_SERVERS];
	pthread_t tid[MAX_SERVERS];
	int numServers;
	int timeCheckEnable;
	bool allServersQueried;
	unsigned int lastServerUpdate;
} f;

const char* srv0 = "0.debian.pool.ntp.org";
const char* srv1 = "1.debian.pool.ntp.org";
const char* srv2 = "2.debian.pool.ntp.org";
const char* srv3 = "3.debian.pool.ntp.org";

/**
 * Allocates memory, reads the list of NTP servers above and
 * partitions the memory into an array of server addresses that are
 * returned by tcp->ntp_server[].
 *
 * The memory MUST be released by the caller by calling "delete" on
 * the tcp->buf param. That automatically frees the tcp->ntp_server[]
 * array memory.
 *
 * @param[out] tcp Struct pointer for passing data to a thread.
 *
 * @returns The number of server addresses or -1 on error.
 */
int allocNTPServerList(timeCheckParams *tcp){

	if (tcp->buf != NULL){
		delete[] tcp->buf;
		tcp->buf = NULL;
	}

	int buflen = 0;
	buflen += strlen(srv0) + 1;
	buflen += strlen(srv1) + 1;
	buflen += strlen(srv2) + 1;
	buflen += strlen(srv3) + 1;

	tcp->ntp_server = f.ntp_server;

	tcp->buf = new char[buflen];

	char *bufptr = tcp->buf;
	strcpy(bufptr, srv0);
	f.ntp_server[0] = bufptr;

	bufptr += strlen(srv0) + 1;
	strcpy(bufptr, srv1);
	f.ntp_server[1] = bufptr;

	bufptr += strlen(srv1) + 1;
	strcpy(bufptr, srv2);
	f.ntp_server[2] = bufptr;

	bufptr += strlen(srv2) + 1;
	strcpy(bufptr, srv3);
	f.ntp_server[3] = bufptr;

	for (int i = 0; i < MAX_SERVERS; i++){
		f.serverTimeDiff[i] = 1000000;
		f.threadIsBusy[i] = false;
	}
	return 4;
}

void copyToLog(char *logbuf, const char* msg){
	char timestamp[100];
	time_t t = time(NULL);
	struct tm *tmp = localtime(&t);
	strftime(timestamp, STRBUF_SZ, "%F %H:%M:%S ", tmp);
	strcat(logbuf, timestamp);
	strcat(logbuf, msg);
}

/**
 * Gets the time correction in whole seconds determined by
 * an SNTP server.
 *
 * @param[in] server The server URL.
 * @param[in] id An identifer for constructing a filename for
 * server messages.
 * @param[in] strbuf A buffer to hold server messages.
 * @param[in] logbuf A buffer to hold messages for the error log.
 *
 * @returns The time correction to be made or -1 on error.
 */
time_t getServerTime(const char *server, int id, char *strbuf, char *logbuf){
	struct timeval startTime, returnTime;
	struct stat stat_buf;
	struct tm tm;
	int rv;
	char *end;
	int fracSec;

	char num[2];
	char buf[500];

	const char *filename = "/run/shm/sntp_out";		// Construct a filename string:
	sprintf(num, "%d", id);							//    "/run/shm/sntp_outn" with n the id val.

	char *cmd = buf;									// Construct a command string:
	strcpy(cmd, "sntp ");							//    "sntp [server_name] > /run/shm/sntp_outn"
	strcat(cmd, server);
	strcat(cmd, " > ");
	strcat(cmd, filename);
	strcat(cmd, num);

	gettimeofday(&startTime, NULL);
	rv = sysCommand(cmd);								// Issue the command:
	if (rv == -1){
		return -1;
	}
	gettimeofday(&returnTime, NULL);					// system() will block until sntp returns or times out.

	if (returnTime.tv_sec - startTime.tv_sec > 0){	// Took more than 1 second
												// Reusing buf for constructing error message on exit.
		sprintf(buf, "Skipped server %d. Took more than 1 second to respond.\n", id);
		copyToLog(logbuf, buf);
		return -1;
	}

	char *fname = buf;
	strcpy(fname, filename);
	strcat(fname, num);
													// Open the file: "/run/shm/sntp_out[n]"
	int fd = open((const char *)fname, O_RDONLY);
	if (fd == -1){
		strcpy(buf, "ERROR: could not open \"");		// Reusing buf for constructing error message on exit.
		strcat(buf, fname);
		strcat(buf, "\"\n");
		copyToLog(logbuf, buf);
		return -1;
	}

	fstat(fd, &stat_buf);
	int sz = stat_buf.st_size;						// Get the size of file.

	if (sz < SNTP_MSG_SZ){
		rv = read(fd, strbuf, sz);					// Read the file.
		if (rv == -1){
			strcpy(buf, "ERROR: reading \"");		// Reusing buf for constructing error message on exit.
			strcat(buf, filename);
			strcat(buf, "\" was interrupted.\n");
			copyToLog(logbuf, buf);
			return -1;
		}
		strbuf[sz] = '\0';
		close(fd);
		remove(fname);
	}
	else {
		writeFileMsgToLogbuf(fname, logbuf);
		close(fd);
		return -1;
	}

	int i;
	for (i = 0; i < sz; i++){						// Locate the first
		if (strbuf[i] == '\n'){						// line feed.
			i += 1;
			break;
		}
	}
	char *pLine = strbuf + i;

	if (pLine[4] != '-' || pLine[7] != '-'){
													// Reusing buf for constructing error message on exit.
		sprintf(buf, "SNTP server %d returned an error message:\n", id);
		strcat(buf, strbuf);
		strcat(buf, "\n");
		copyToLog(logbuf, buf);
		return -1;
	}

	end = strstr(pLine, "+/-");						// Chop string here if this is returned
	if (end != NULL){								// by a server.
		*end = '\0';
	}

	int tz;
	float delta;										// The floor of this in integer seconds will be the
													// whole second time correction returned by SNTP.
	memset(&tm, 0, sizeof(struct tm));
													// Start decoding.
	// For example: 2016-02-01 16:28:54.146050 (+0500) -0.01507
	sscanf(pLine, "%4d-%2d-%2d %2d:%2d:%2d.%6d (%5d) %f",
			&tm.tm_year, &tm.tm_mon, &tm.tm_mday,
			&tm.tm_hour, &tm.tm_min, &tm.tm_sec,
			&fracSec, &tz, &delta);

	return (int)floor(delta);						// Time correction to be made in whole seconds
}

/**
 * Requests a date/time from an SNTP time server in a detached thread
 * that exits after filling the timeCheckParams struct, tcp, with the
 * requested information and any error info.
 *
 * @param[in,out] tcp struct pointer for passing data.
 */
void doTimeCheck(timeCheckParams *tcp){

	int i = tcp->serverIndex;
	char *strbuf = tcp->strbuf + i * STRBUF_SZ;
	char *logbuf = tcp->logbuf + i * LOGBUF_SZ;

	logbuf[0] = '\0';								// Clear the logbuf.

	tcp->threadIsBusy[i] = true;

	char *ntp_server = tcp->ntp_server[i];

	time_t timeDiff = getServerTime(ntp_server, i, strbuf, logbuf);
	if (timeDiff == -1){
		tcp->serverTimeDiff[i] = 1000000;			// Marker for no time returned
	}
	else {
		tcp->serverTimeDiff[i] = timeDiff;
	}

	tcp->threadIsBusy[i] = false;
}

/**
 * Takes a consensus of the time error between local time and
 * the time reported by SNTP servers and reports the error as
 * g.consensusTimeError.
 *
 * @returns The number of SNTP servers reporting.
 */
int getTimeConsensusAndCount(void){
	int diff[MAX_SERVERS];
	int count[MAX_SERVERS];

	int nServersReporting = 0;

	memset(diff, 0, MAX_SERVERS * sizeof(int));
	memset(count, 0, MAX_SERVERS * sizeof(int));

	for (int j = 0; j < f.numServers; j++){				// Construct a distribution of diffs
		if (f.serverTimeDiff[j] != 1000000){				// Skip a server not returning a time
			int k;
			for (k = 0; k < f.numServers; k++){
				if (f.serverTimeDiff[j] == diff[k]){		// Matched a value
					count[k] += 1;
					break;
				}
			}
			if (k == f.numServers){						// No value match
				for (int m = 0; m < f.numServers; m++){
					if (count[m] == 0){
						diff[m] = f.serverTimeDiff[j];	// Create a new diff value
						count[m] = 1;					// Set its count to 1
						break;
					}
				}
			}
			nServersReporting += 1;
		}
	}

	int maxHits = 0;
	int maxHitsIndex = 0;
														// Get the diff having the max number of hits
	for (int j = 0; j < f.numServers; j++){
		if (count[j] > maxHits){
			maxHits = count[j];
			maxHitsIndex = j;
		}
	}
	g.consensusTimeError = diff[maxHitsIndex];

	sprintf(g.msgbuf, "Number of servers responding: %d\n", nServersReporting);
	bufferStatusMsg(g.msgbuf);

	for (int i = 0; i < MAX_SERVERS; i++){
		f.serverTimeDiff[i] = 1000000;
	}
	return nServersReporting;
}

/**
 * Updates the PPS-Client log with any errors reported by threads
 * querying SNTP time servers.
 *
 * @param[out] buf The message buffer shared by the threads.
 * @param[in] numServers The number of SNTP servers.
 */
void updateLog(char *buf, int numServers){

	char *logbuf;

	for (int i = 0; i < numServers; i++){
		logbuf = buf + i * LOGBUF_SZ;

		if (strlen(logbuf) > 0){
			writeToLogNoTimestamp(logbuf);
		}
	}
}

/**
 * At an interval defined by CHECK_TIME, queries a list of SNTP servers
 * for date/time using detached threads so that delays in server responses
 * do not affect the operation of the waitForPPS() loop.
 *
 * @param[in,out] tcp Struct pointer for passing data.
 */
void makeSNTPTimeQuery(timeCheckParams *tcp){
	int rv;

	if (f.allServersQueried){
		if (g.queryCount == 0){
			f.allServersQueried = false;

			getTimeConsensusAndCount();
			updateLog(tcp->logbuf, f.numServers);
		}
		if (g.queryCount > 0){
			g.queryCount -= 1;
		}
	}

	if (g.seq_num >= 600							// was g.seq_num >= 0
			&& g.seq_num % 60 == 0){		// Start a time check against the list of SNTP servers
		g.blockDetectClockChange = BLOCK_FOR_10;
												// Refresh the server list once per day
		if ((g.seq_num - f.lastServerUpdate) > SECS_PER_DAY
				|| g.seq_num == CHECK_TIME){		// and at the first CHECK_TIME

			f.numServers = allocNTPServerList(tcp);

			if (f.numServers == -1){
				sprintf(g.logbuf, "Unable to allocate the SNTP servers!\n");
				writeToLog(g.logbuf);
				return;
			}
			f.lastServerUpdate = g.seq_num;
		}

		f.timeCheckEnable = f.numServers;

		bufferStatusMsg("Starting a time check.\n");
	}

	if (f.timeCheckEnable > 0){

		tcp->serverIndex = f.timeCheckEnable - 1;

		f.timeCheckEnable -= 1;

		int idx = tcp->serverIndex;

		if (idx == 0){
			f.allServersQueried = true;
			g.queryCount = 1;
		}

		if (f.threadIsBusy[idx]){
			sprintf(g.msgbuf, "Server %d is busy.\n", idx);
			bufferStatusMsg(g.msgbuf);
		}
		else {
			sprintf(g.msgbuf, "Requesting time from Server %d\n", idx);
			bufferStatusMsg(g.msgbuf);

			rv = pthread_create(&((tcp->tid)[idx]), &(tcp->attr), (void* (*)(void*))&doTimeCheck, tcp);
			if (rv != 0){
				sprintf(g.logbuf, "Can't create thread : %s\n", strerror(errno));
				writeToLog(g.logbuf);
			}
		}
	}
}

/**
 * Allocates memory and initializes threads that will be used by
 * makeSNTPTimeQuery() to query SNTP time servers. Thread must be
 * released and memory deleted by calling freeSNTPThreads().
 *
 * @param[out] tcp Struct pointer for passing data.
 *
 * @returns 0 on success or -1 on error.
 */
int allocInitializeSNTPThreads(timeCheckParams *tcp){
	memset(&f, 0, sizeof(struct sntpLocalVars));

	tcp->tid = f.tid;
	tcp->serverIndex = 0;
	tcp->serverTimeDiff = f.serverTimeDiff;
	tcp->strbuf = new char[STRBUF_SZ * MAX_SERVERS];
	tcp->logbuf = new char[LOGBUF_SZ * MAX_SERVERS];
	tcp->threadIsBusy = f.threadIsBusy;
	tcp->buf = NULL;

	int rv = pthread_attr_init(&(tcp->attr));
	if (rv != 0) {
		sprintf(g.logbuf, "Can't init pthread_attr_t object: %s\n", strerror(errno));
		writeToLog(g.logbuf);
		return -1;
	}

	rv = pthread_attr_setstacksize(&(tcp->attr), PTHREAD_STACK_REQUIRED);
	if (rv != 0){
		sprintf(g.logbuf, "Can't set pthread_attr_setstacksize(): %s\n", strerror(errno));
		writeToLog(g.logbuf);
		return -1;
	}

	rv = pthread_attr_setdetachstate(&(tcp->attr), PTHREAD_CREATE_DETACHED);
	if (rv != 0){
		sprintf(g.logbuf, "Can't set pthread_attr_t object state: %s\n", strerror(errno));
		writeToLog(g.logbuf);
		return -1;
	}

	return 0;
}

/**
 * Releases threads and deletes memory used by makeSNTPTimeQuery();
 *
 * @param[in] tcp The struct pointer that was used for passing data.
 */
void freeSNTPThreads(timeCheckParams *tcp){
	pthread_attr_destroy(&(tcp->attr));
	delete[] tcp->strbuf;
	delete[] tcp->logbuf;
	if (tcp->buf != NULL){
		delete[] tcp->buf;
	}
}
