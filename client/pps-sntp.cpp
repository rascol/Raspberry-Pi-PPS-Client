/**
 * @file pps-sntp.cpp
 * @brief The pps-sntp.cpp file contains functions and structures for accessing time updates via SNTP.
 *
 * Copyright (C) 2016  Raymond S. Connell
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

/**
 * Allocates memory, reads a list of NTP servers from "ntpq -pn" and
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
	struct stat stat_buf;

	char **server = tcp->ntp_server;

	const char *filename = "/run/shm/server-list";

	int rv = system("ntpq -pn > /run/shm/server-list");
	if (rv == -1){
		sprintf(g.logbuf, "ntpq -pn failed!\n");
		writeToLog(g.logbuf);
		return rv;
	}

	int fd = open_logerr(filename, O_RDONLY);
	if (fd == -1){
		return -1;
	}
	fstat(fd, &stat_buf);
	int sz = stat_buf.st_size;

	if (tcp->buf != NULL){
		delete tcp->buf;
		tcp->buf = NULL;
	}
	tcp->buf = new char[sz+ADDR_LEN];

	rv = read_logerr(fd, tcp->buf, sz, filename);
	if (rv == -1){
		return rv;
	}

	tcp->buf[sz] = '\0';

	close(fd);
	remove(filename);

	char addrbuf[ADDR_LEN];

	int i = 0;
	char *line = strtok(tcp->buf, "\n");
	while (line != NULL && i < MAX_SERVERS){
		char *addr = strpbrk(line, "0123456789");
		if (addr != NULL){
			strncpy(addrbuf, addr, ADDR_LEN);

			addrbuf[ADDR_LEN-1] = '\0';
												// Disqualified addresses
			char *badAddr = strstr(addrbuf, "255");
			char *badAddr2 = strstr(addrbuf, "127.127.1.0");

			if (addr != NULL && badAddr == NULL && badAddr2 == NULL){
				server[i] = addr;
				i++;
			}
		}

		line = strtok(NULL, "\n");
	}

	sprintf(g.msgbuf, "Number of server addresses: %d\n", i);
	bufferStatusMsg(g.msgbuf);

	for (int j = 0; j < i; j++){				// Zero terminate the individual server address strings.
		int sz = strcspn(server[j], " ");
		server[j][sz] = '\0';
	}

	for (int i = 0; i < MAX_SERVERS; i++){
		f.serverTimeDiff[i] = 1000000;
		f.threadIsBusy[i] = false;
	}
	return i;
}

/**
 * Check for NTP servers in a wait loop that waits for up to
 * one minute for ntpq to respond with a server list. This
 * requires an active internet connection over ethernet or wifi.
 *
 * @returns The number of SNTP servers or 0 if none found.
 */
int waitForNTPServers(void){
	int nServers = 0;
	timeCheckParams tcp;

	memset(&tcp, 0, sizeof(timeCheckParams));
	tcp.ntp_server = f.ntp_server;

	for (int i = 0; i < 6; i++){
		nServers = allocNTPServerList(&tcp);
		if (nServers != 0){
			break;
		}
		sprintf(g.logbuf, "Waiting 10 seconds for NTP servers...\n");
		printf(g.logbuf);
		writeToLog(g.logbuf);

		sleep(10);
	}
	if (tcp.buf != NULL){
		delete tcp.buf;
		tcp.buf = NULL;
	}
	if (nServers <= 0){
		sprintf(g.logbuf, "Could not get NTP servers. Exiting.\n");
		writeToLog(g.logbuf);
	}
	else {
		sprintf(g.logbuf, "Have NTP servers. Continuing.\n");
		writeToLog(g.logbuf);
	}
	return nServers;
}

/**
 * Reads the time in seconds since the epoch Jan 00, 1900 from
 * an SNTP server.
 *
 * @param[in] server The server URL.
 * @param[in] id An identifer for constructing a filename for
 * server messages.
 * @param[in] strbuf A buffer to hold server messages.
 * @param[in] logbuf A buffer to hold messages for the error log.
 *
 * @returns The time or -1 on error.
 */
time_t getServerTime(const char *server, int id, char *strbuf, char *logbuf){
	time_t t = 0;
	struct stat stat_buf;
	struct tm tm;
	int rv;
	char *end;
	int fracSec;

	char num[2];
	char buf[100];

	const char *filename = "/run/shm/sntp_out";	// Constuct a filename string:
	sprintf(num, "%d", id);						//    "/run/shm/sntp_outn" with n the id val.

	char *cmd = buf;							// Construct a command string:
	strcpy(cmd, "sntp ");						//    "sntp <server_name> > /run/shm/sntp_outn"
	strcat(cmd, server);
	strcat(cmd, " > ");
	strcat(cmd, filename);
	strcat(cmd, num);

	system(cmd);								// Issue the command:
												// system may block until the server returns.
	char *fname = buf;
	strcpy(fname, filename);
	strcat(fname, num);
												// Open the file: "/run/shm/sntp_out[n]"
	int fd = open((const char *)fname, O_RDONLY);
	if (fd == -1){
		strcpy(logbuf, "ERROR: could not open \"");
		strcat(logbuf, fname);
		strcat(logbuf, "\"\n");
		return -1;
	}

	fstat(fd, &stat_buf);
	int sz = stat_buf.st_size;					// Get the size of file.

	if (sz < SNTP_MSG_SZ){						// SNTP_MSG_SZ is slighty larger than the expected message size.
		rv = read(fd, strbuf, sz);				// Attempt to read the file.
		if (rv == -1){
			strcpy(logbuf, "ERROR: reading \"");
			strcat(logbuf, filename);
			strcat(logbuf, "\" was interrupted.\n");
			return rv;
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

	const char *substr = "Started sntp";		// Get a pointer to the beginning of the time line
	char *pLine = strstr(strbuf, substr) + strlen(substr);
	if (pLine == NULL || strlen(pLine) < 4){
		sprintf(logbuf, "SNTP server %d did not return the time:\n", id);
		strcat(logbuf, strbuf);
		strcat(logbuf, "\n");
		return -1;
	}
	if (*pLine == '\n'){
		++pLine;
	}

	if (pLine[4] != '-' || pLine[7] != '-'){
		sprintf(logbuf, "SNTP server %d returned an unexpected message:\n", id);
		strcat(logbuf, strbuf);
		strcat(logbuf, "\n");
		return -1;
	}

	end = strstr(pLine, "+/-");					// Chop string here if this is returned
	if (end != NULL){							// by a server.
		*end = '\0';
	}

	int tz;
	float delta;
	double dtime;
									// If the message format was as expected start decoding.
	memset(&tm, 0, sizeof(struct tm));

	// For example: 2016-02-01 16:28:54.146050 (+0500) -0.01507
	sscanf(pLine, "%4d-%2d-%2d %2d:%2d:%2d.%6d (%5d) %f",
			&tm.tm_year, &tm.tm_mon, &tm.tm_mday,
			&tm.tm_hour, &tm.tm_min, &tm.tm_sec,
			&fracSec, &tz, &delta);

	if (delta >= 500000){						// This round the seconds by
		tm.tm_sec += 1;							// accounting for fractional seconds.
	}

	tm.tm_isdst = -1;							// Let mktime() supply DST info.
	tm.tm_year = tm.tm_year - 1900;				// Years since 1900
	tm.tm_mon = tm.tm_mon - 1;					// Months: 0 to 11

	t = mktime(&tm);
	dtime = (double)t +(double)delta;

	return (int)round(dtime);
}

/**
 * Requests a date/time from an SNTP time server in a detached thread
 * that exits after filling the timeCheckParams struct, tcp, with the
 * requested information and any error info.
 *
 * @param[in,out] tcp Struct pointer for passing data.
 */
void doTimeCheck(timeCheckParams *tcp){

	int i = tcp->serverIndex;
	struct timeval tv0, tv1;
	char *strbuf = tcp->strbuf + i * STRBUF_SZ;
	char *logbuf = tcp->logbuf + i * LOGBUF_SZ;

	logbuf[0] = '\0';								// Clear the logbuf.

	tcp->threadIsBusy[i] = true;

	gettimeofday(&tv0, NULL);

	char *ntp_server = tcp->ntp_server[i];

	time_t serverTime = getServerTime(ntp_server, i, strbuf, logbuf);
	if (serverTime == -1){
		tcp->serverTimeDiff[i] = 1000000;			// Marker for no time returned
	}
	else {
		gettimeofday(&tv1, NULL);
		if (tv1.tv_sec - tv0.tv_sec != 0){			// Server took too long (more than one second).
			tcp->serverTimeDiff[i] = 1000000;		// Mark invalid.
		}
		else {
			tcp->serverTimeDiff[i] = serverTime - tv1.tv_sec;
		}
	}

	tcp->threadIsBusy[i] = false;
}

/**
 * Takes a consensis of the time error between local time and
 * the time reported by SNTP servers and reports the error as
 * g.consensisTimeError.
 *
 * @returns The number of SNTP servers reporting.
 */
int getTimeConsensisAndCount(void){
	int diff[MAX_SERVERS];
	int count[MAX_SERVERS];

	int nServersReporting = 0;

	memset(diff, 0, MAX_SERVERS * sizeof(int));
	memset(count, 0, MAX_SERVERS * sizeof(int));

	for (int j = 0; j < f.numServers; j++){				// Construct a distribution of diffs
		if (f.serverTimeDiff[j] != 1000000){			// Skip a server not returning a time
			int k;
			for (k = 0; k < f.numServers; k++){
				if (f.serverTimeDiff[j] == diff[k]){	// Matched a value
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
	g.consensisTimeError = diff[maxHitsIndex];

	sprintf(g.msgbuf, "Number of servers responding: %d\n", nServersReporting);
	bufferStatusMsg(g.msgbuf);

	for (int i = 0; i < MAX_SERVERS; i++){
		f.serverTimeDiff[i] = 1000000;
	}
	return nServersReporting;
}

/**
 * Updates the pps-client log with any errors reported by threads
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
			writeToLog(logbuf);
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
		f.allServersQueried = false;

		getTimeConsensisAndCount();
		updateLog(tcp->logbuf, f.numServers);
	}

	if (g.seq_num > 0 							// Start a time check against the list of SNTP servers
			&& g.seq_num % CHECK_TIME == 0){
												// Refresh the server list once per day
		if ((g.seq_num - f.lastServerUpdate) > SECS_PER_DAY
				|| g.seq_num == CHECK_TIME){	// and at the first CHECK_TIME

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
		}

		if (f.threadIsBusy[idx]){
			sprintf(g.msgbuf, "Server %d is busy.\n", idx);
			bufferStatusMsg(g.msgbuf);
		}

		if ( ! f.threadIsBusy[idx]){

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
	tcp->ntp_server = f.ntp_server;
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
	else {
		rv = pthread_attr_setdetachstate(&(tcp->attr), PTHREAD_CREATE_DETACHED);
	}

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
	delete tcp->strbuf;
	delete tcp->logbuf;
	if (tcp->buf != NULL){
		delete tcp->buf;
	}
}
