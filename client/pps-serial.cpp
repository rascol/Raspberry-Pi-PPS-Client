/**
 * @file pps-serial.cpp
 * @brief The pps-serial.cpp file contains functions and structures for accessing time updates via the serial port.
 *
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
extern struct G g;

/**
 * Local file-scope shared variables.
 */
static struct serialLocalVars {
	int serverTimeDiff[1];
	bool threadIsBusy[1];
	pthread_t tid[1];
	int timeCheckEnable;
	bool allServersQueried;
	unsigned int lastServerUpdate;


	char *serialPort;
	int lostGPSCount;
	bool doReadSerial;
	int lastSerialTimeDif;
} f;

/**
 * Processes a block of GPS messages to find a GPRMC
 * message and extract the UTC time in seconds.
 *
 * @param[in] msgbuf The block of GPS messages to
 * process.
 *
 * @param[in/out] tcp A struct pointer used to pass
 * thread data.
 *
 * @param[out] gmtSeconds The UTC time in seconds.
 *
 * @returns true if a complete GPRMC message is found
 * and the time was extracted. Else false.
 */
bool getUTCfromGPSmessages(const char *msgbuf, timeCheckParams *tcp, time_t *gmtSeconds){

	char scnbuf[10];
	memset(scnbuf, 0, 10);

	struct tm gmt;
	memset(&gmt, 0, sizeof(struct tm));

	char *active, *ctmp2, *ctmp4;

	active = scnbuf;
	ctmp2 = active + 2;
	ctmp4 = ctmp2 + 2;

	float ftmp1, ftmp3, ftmp5, ftmp6;
	int frac;

	char *pstr = strstr((char *)msgbuf, "$GPRMC");	// $GPRMC,205950.000,A,3614.5277,N,08051.3851,W,0.02,288.47,051217, ,,D*75
	if (pstr != NULL){								// If a GPRMC message was received, continue.
		char *end = pstr + 64;
		char *pNext = strstr(pstr + 10, "$");		// Verify this is a complete message by looking for next message start symbol
		if (pNext != NULL){							// If the message is complete, continue.
			*end = '\0';
			sscanf(pstr, "$GPRMC,%2d%2d%2d.%d,%1c,%f,%1c,%f,%1c,%f,%f,%2d%2d%2d,", &gmt.tm_hour, &gmt.tm_min, &gmt.tm_sec,
					&frac, active, &ftmp1, ctmp2, &ftmp3, ctmp4, &ftmp5, &ftmp6, &gmt.tm_mday, &gmt.tm_mon, &gmt.tm_year);

			if (active[0] == 'A'){					// Verify this is an active message
				gmt.tm_mon -= 1;						// Convert to tm struct format with months: 0 to 11
				gmt.tm_year += 100;					// Convert to tm struct format with year since 1900
				*gmtSeconds =  mktime(&gmt);			// True UTC seconds

				f.lostGPSCount = 0;
			}
			else {
				sprintf(tcp->strbuf, "getUTCfromGPSmessages() A GPS message was received but it is not active.\n");
				writeToLog(tcp->strbuf);

				if (f.lostGPSCount == 0) {
					f.lostGPSCount = 1;
				}
				else {
					f.lostGPSCount += 1;
					if (f.lostGPSCount == 5){
						sprintf(tcp->strbuf, "getUTCfromGPSmessages() Unable to connect to GPS. Will retry in %d minutes.\n", CHECK_TIME_SERIAL / 60);
						writeToLog(tcp->strbuf);

						f.lostGPSCount = 0;
						tcp->doReadSerial = false;
					}
				}
			}
		}
		else {										// Did not get a usable GPRMC message
			return false;
		}
	}
	else {											// Did not get GPRMC message
		return false;
	}
	return true;
}

/**
 * Waits for the serial port to buffer enough messages
 * that a GPRMC can usually be found among those. Because
 * the message stream is collected at a relatively slow
 * 9600 baud some steams will take too long to be fully
 * read and processed within one second. For that reason
 * messages are read for only about 250 ms. This insures
 * the messages that are read occur in the second for which
 * the time is required.
 *
 * @param[out] msgbuf The buffer to read the GPS messages.
 *
 * @param[in/out] tcp A struct pointer used to pass
 * thread data.
 *
 * @param[out] t_gmt The time when the serial port starts
 * buffering messages.
 *
 * @param[out] gmtSeconds The time in UTC seconds on the
 * local clock when the serial port starts buffering messages.
 *
 * @returns 1 on success, 0 if no GPS messages or -1 on a system
 * error.
 */
int waitForGPSmessages(char *msgbuf, timeCheckParams *tcp, struct timeval *t_gmt, time_t *gmtSeconds){

	int rfd = open(tcp->serialPort, O_RDONLY);
	if (rfd == -1){
		sprintf(tcp->strbuf, "waitForGPSmessages() Unable to open %s\n", tcp->serialPort);
		writeToLog(tcp->strbuf);
		close(rfd);
		return -1;
	}
	fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(rfd, &rfds);

	struct timeval wait;
    wait.tv_sec = 1;
    wait.tv_usec = 0;
    							// select blocks until the serial port starts receiving GPS messages.

	int rv = select(rfd + 1, &rfds, NULL, NULL, &wait);
	if (rv == -1){
		sprintf(tcp->strbuf,"waitForGPSmessages() select failed with error %s.\n", strerror(errno));
		writeToLog(tcp->strbuf);
		close(rfd);
		return -1;
	}
	if (rv == 0){
		sprintf(tcp->strbuf,"waitForGPSmessages() No messages were available within one second.\n");
		writeToLog(tcp->strbuf);
		close(rfd);
		return 0;
	}
							// Serial port starts buffering GPS messages here

    time_t clkSeconds = time(0);						// Get local time in seconds
	struct tm *gmt = gmtime(&clkSeconds);			// Get a tm struct for UTC according to system clock
	*gmtSeconds = mktime(gmt);						// Get UTC seconds according to system clock

	gettimeofday(t_gmt, NULL);						// Time at start of buffering GPS messages.

	struct timespec slp;
	slp.tv_sec = 0;
	slp.tv_nsec = 250000000;							// ~250 chars

	nanosleep(&slp, NULL);							// Sleep long enough to get ~250 chars at 9600 baud ~ 1 char per msec.

							// Read the buffered serial port messages here

	int nchars = read(rfd, msgbuf, 250);
	if (nchars == -1){
		sprintf(tcp->strbuf,"waitForGPSmessages() read on serial port received empty buffer.\n");
		writeToLog(tcp->strbuf);
		close(rfd);
		return 0;
	}

	close(rfd);
	return 1;
}

//void writeTimeToLog(char *strbuf, struct timeval t_gmt, struct timeval t_gmt0, struct timeval t_rdy){
//	writeToLog(strbuf);
//	sprintf(strbuf, "  t_gmt.tv_sec: %d t_gmt.tv_usec: %d\n", (int)t_gmt.tv_sec, (int)t_gmt.tv_usec);
//	writeToLog(strbuf);
//	sprintf(strbuf, "  t_gmt0.tv_sec: %d t_gmt0.tv_usec: %d\n", (int)t_gmt0.tv_sec, (int)t_gmt0.tv_usec);
//	writeToLog(strbuf);
//	sprintf(strbuf, "  t_rdy.tv_sec: %d t_rdy.tv_usec: %d\n", (int)t_rdy.tv_sec, (int)t_rdy.tv_usec);
//	writeToLog(strbuf);
//}

/**
 * Reads the UTC time from the $GPRMC message of a connected GPS receiver
 * through the serial port, converts the time to local time and compares
 * it with local time read from the system clock.
 *
 * @param[out] timeDif Time difference in seconds from the system time to
 * the GPS time.
 *
 * @param[in/out] tcp A struct pointer used to pass thread data.
 *
 * @returns 1 if a time difference was generated, 0 if not or -1 on a
 * system error.
 */
int getTimeOffsetOverSerial(int *timeDif, timeCheckParams *tcp){

	time_t difSeconds = 0, gmt0Seconds = 0, gmtSeconds;

	char msgbuf[300];
	memset(msgbuf, 0, 300 * sizeof(char));

	struct timeval t_gmt, t_rdy;

	int rv = waitForGPSmessages(msgbuf, tcp, &t_gmt, &gmtSeconds);
	if (rv == 0 || rv == -1){
		return rv;
	}

	if (!getUTCfromGPSmessages(msgbuf, tcp, &gmt0Seconds)){
		return 0;
	}

	gettimeofday(&t_rdy, NULL);					// Time at completion of UTC time extraction

	bool finishedInSameSecond = (t_gmt.tv_sec == t_rdy.tv_sec);
	if (finishedInSameSecond){
		difSeconds = gmt0Seconds - gmtSeconds;
		*timeDif = (int)difSeconds;
		return 1;
	}
	else {
		sprintf(tcp->strbuf, "getTimeOffsetOverSerial() Discarded the data. Took over 1 second. OS latency!\n");
		writeToLog(tcp->strbuf);
		return 0;
	}
}

/**
 * Requests the time difference between system and GPS
 * from getTimeOffsetOverSerial().
 *
 * If the time difference is zero, returns the zero result
 * in tcp->serverTimeDiff[0] and ends the request.
 *
 * Otherwise a second request is made to verify the first
 * non-zero result. If the two results are identical then
 * the time difference is returned in tcp->serverTimeDiff[0].
 * If not, zero is returned and an error message is written
 * to pps-client.log.
 *
 * If a system error occurs while reading the serial port,
 * zero is returned in tcp->serverTimeDiff[0] and an error
 * message is written to pps-client.log.
 *
 * @param[in/out] tcp A struct pointer used to pass thread
 * data.
 */
void doSerialTimeCheck(timeCheckParams *tcp){

	tcp->threadIsBusy[0] = true;
	int timeDif = 0;

	int isValidDif = getTimeOffsetOverSerial(&timeDif, tcp);
	if (isValidDif > 0){
		if (timeDif == 0){								// No time difference
			if (f.lastSerialTimeDif != 0){
				sprintf(tcp->strbuf, "doSerialTimeCheck() No timeDif on second read. First read was GPS error.\n");
				writeToLog(tcp->strbuf);
			}
			tcp->doReadSerial = false;

			tcp->serverTimeDiff[0] = 0;
			f.lastSerialTimeDif = 0;

			tcp->rv = 1;
			goto end;
		}
		if (timeDif != 0 && f.lastSerialTimeDif == 0){	// No previous read.
			sprintf(tcp->strbuf, "doSerialTimeCheck() timeDif detected on first read: %d\n", timeDif);
			writeToLog(tcp->strbuf);

			tcp->serverTimeDiff[0] = 0;
			f.lastSerialTimeDif = timeDif;				// So flag timeDif for verification and continue read serial.

			tcp->rv = 0;
			goto end;
		}
		if (timeDif != 0 && f.lastSerialTimeDif != 0){	// Got a timeDif on second read
			if (timeDif == f.lastSerialTimeDif ){		// timeDif is valid.
				sprintf(tcp->strbuf, "doSerialTimeCheck() Verified timeDif on second read: %d\n", timeDif);
				writeToLog(tcp->strbuf);

				tcp->doReadSerial = false;				// So stop read serial

				tcp->serverTimeDiff[0] = timeDif;		// and send back the timeDif.
				f.lastSerialTimeDif = 0;

				tcp->rv = 1;
				goto end;
			}
			else {										// timeDifs don't match
				sprintf(tcp->strbuf, "doSerialTimeCheck() Second timeDif read: %d does not match the first: %d. Not valid.\n", timeDif, f.lastSerialTimeDif);
				writeToLog(tcp->strbuf);

				tcp->doReadSerial = false;

				tcp->serverTimeDiff[0] = 0;				// No valid timeDif so send back zero.
				f.lastSerialTimeDif = 0;

				tcp->rv = 0;
				goto end;
			}
		}
	}
	else if (isValidDif == 0){
		sprintf(tcp->strbuf, "doSerialTimeCheck() Did not see a GPRMC message. Retrying.\n");
		writeToLog(tcp->strbuf);
		tcp->doReadSerial = true;
	}
	else if (isValidDif == -1){
		tcp->rv = -1;
		goto end;
	}
	tcp->rv = 0;
end:
	tcp->threadIsBusy[0] = false;
	return;
}

/**
 * Gets the time from a serial port connected to a GPS
 * receiver or equivalent and returns the difference in
 * seconds from the local time to the true GPS time.
 *
 * If a difference is detected, the error is verified
 * by looking for a second identical time difference.
 * If the time difference repeats, the result is valid
 * and is returned in g.serialTimeError. Otherwise the
 * first difference was an error and g.serialTimeError
 * returns 0.
 *
 * @returns 0 or -1 on a system error.
 */
int makeSerialTimeQuery(timeCheckParams *tcp){

	int rv = 0;

	if (tcp->threadIsBusy[0] == false){
		rv = tcp->rv;
		if (rv == -1){
			sprintf(tcp->strbuf, "Time check failed with an error. See the pps-client.log\n");
			bufferStatusMsg(tcp->strbuf);
			return rv;
		}
		if (rv == 1){
			sprintf(tcp->strbuf, "GPS Reported clock offset: %d\n", tcp->serverTimeDiff[0]);
			bufferStatusMsg(tcp->strbuf);
			tcp->rv = 0;
			rv = 0;
		}
		g.serialTimeError = tcp->serverTimeDiff[0];
		tcp->serverTimeDiff[0] = 0;

		f.doReadSerial = tcp->doReadSerial;
	}
	else {
		sprintf(g.msgbuf, "Thread is busy.\n");
		bufferStatusMsg(g.msgbuf);
		return rv;
	}

	if (g.seq_num == 1 ||
			g.seq_num % CHECK_TIME_SERIAL == 0){				// Start a time check every CHECK_TIME_SERIAL
		f.doReadSerial = true;								// Read continues until f.doReadSerial is set false.

		sprintf(g.logbuf, "Requesting a GPS time check.\n");
		bufferStatusMsg(g.logbuf);
	}

	if (f.doReadSerial){
		g.blockDetectClockChange = BLOCK_FOR_3;

		int rv = pthread_create(&((tcp->tid)[0]), &(tcp->attr), (void* (*)(void*))&doSerialTimeCheck, tcp);
		if (rv != 0){
			sprintf(g.logbuf, "Can't create thread : %s\n", strerror(errno));
			writeToLog(g.logbuf);
			return -1;
		}
	}
	return rv;
}

/**
 * Allocates memory and initializes a thread that will be used by
 * makeSerialimeQuery() to query the serial port. Thread must be
 * released and memory deleted by calling freeSerialThread().
 *
 * @param[out] tcp Struct pointer for passing data.
 *
 * @returns 0 on success or -1 on error.
 */
int allocInitializeSerialThread(timeCheckParams *tcp){
	memset(&f, 0, sizeof(struct serialLocalVars));

	int buflen = strlen(g.serialPort);
	f.serialPort = new char[buflen + 1];
	strcpy(f.serialPort, g.serialPort);

	f.serverTimeDiff[0] = 0;
	f.threadIsBusy[0] = false;

	tcp->tid = f.tid;
	tcp->serverTimeDiff = f.serverTimeDiff;
	tcp->strbuf = new char[STRBUF_SZ];
	tcp->threadIsBusy = f.threadIsBusy;
	tcp->serialPort = f.serialPort;

	printf("allocInitializeSerialThread() tcp->serialPort: %s\n", tcp->serialPort);

	tcp->rv = 0;
	tcp->doReadSerial = false;

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
 * Releases thread and deletes memory used by makeSerialTimeQuery();
 *
 * @param[in] tcp The struct pointer that was used for passing data.
 */
void freeSerialThread(timeCheckParams *tcp){
	pthread_attr_destroy(&(tcp->attr));
	delete[] tcp->strbuf;
	if (tcp->serialPort != NULL){
		delete[] tcp->serialPort;
		tcp->serialPort = NULL;
	}
}
