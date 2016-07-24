/*
 * pps-client.h
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

#ifndef PPS_CLIENT_OLD_PPS_CLIENT_H_
#define PPS_CLIENT_OLD_PPS_CLIENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/timex.h>
#include <math.h>
#include <sys/types.h>
#include <errno.h>
#include <poll.h>

#define USECS_PER_SEC 1000000
#define SECS_PER_MINUTE 60
#define SECS_PER_5_MIN 300
#define SECS_PER_10_MIN 600
#define SECS_PER_HOUR 3600
#define SECS_PER_DAY 86400
#define NUM_5_MIN_INTERVALS 288
#define FIVE_MINUTES 5
#define PER_MINUTE (1.0 / (double)SECS_PER_MINUTE)
#define SETTLE_TIME (2 * SECS_PER_MINUTE + SECS_PER_10_MIN)
#define INV_GAIN_1 1
#define INV_GAIN_0 4
#define INTEGRAL_GAIN 0.63212
#define SHOW_INTRPT_DATA_INTVL 6
#define CALIBRATE_PERIOD SECS_PER_MINUTE
#define INV_DELAY_SAMPLES_PER_MIN (1.0 / (double)SECS_PER_MINUTE)
#define FREQDIFF_INTRVL 5
#define INTRPT_MOST_DECAY_RATE 0.975

#define OFFSETFIFO_LEN 80
#define NUM_AVERAGES 10
#define PER_NUM_INTEGRALS (1.0 / (double)NUM_AVERAGES)

#define ADJTIMEX_SCALE 65536.0			// Frequency scaling required by adjtimex().

#define INTERRUPT_LATENCY 16			// Default interrupt latency in microseconds.

#define RAW_ERROR_ZERO  20				// Index corresponding to rawError == 0 in detectDelayPeak().
#define MIN_PEAK_RATIO 0.05				// Minimum ratio to detect a second peak in detectDelayPeak().
#define MAX_VALLEY_RATIO 0.99			// Maximum ratio to detect a valley before the second peak in detectDelayPeak().
#define RAW_ERROR_DECAY 0.98851			// Decay rate for rawError samples (1 hour half life)
#define MAX_PEAK_DELAY 15				// Look for a delay peak less than this value. If larger not a delay peak.
#define MAX_PEAK_TO_TAIL_DELAY 7		// Maximum extent of a tail from the peak

#define INTERRUPT_LOST 15				// Number of consequtive lost interrupts at which a warning starts

#define MAX_SERVERS 10					// Maximum number of SNTP time servers to use
#define CHECK_TIME 1024					// Interval between internet time checks (about 17 minutes)

#define MAX_SPIKES 30					// Maximum microseconds to suppress a burst of positive jitter

#define NOISE_FACTOR 0.354				// Adjusts g.noiseLevel to track g.sysDelay
#define NOISE_LEVEL_MIN 4				// The minimum level at which interrupt delays are delay spikes.
#define SLEW_LEN 10						// The slew accumulator (slewAccum) update interval
#define SLEW_MAX 65						// Jitter slew value below which the controller will begin to frequency lock.

#define FUDGE 1							// Accurate time calibration required PPS latency to be longer than
										// measured interrupt latency by this amount (micoseconds).
#define MAX_LINE_LEN 50
#define STRBUF_SZ 500
#define LOGBUF_SZ 500
#define MSGBUF_SZ 500
#define SNTP_MSG_SZ 110
#define CONFIG_FILE_SZ 10000

#define ERROR_DISTRIB_LEN 101
#define JITTER_DISTRIB_LEN 201
#define INTRPT_DISTRIB_LEN 51

#define HARD_LIMIT_NONE 32768
#define HARD_LIMIT_1024 1024
#define HARD_LIMIT_4 4
#define HARD_LIMIT_1 1
#define HARD_LIMIT_05 0.5

#define HIGH 1
#define LOW 0

#define MAX_CONFIGS 32

#define ERROR_DISTRIB 1
#define ALERT_PPS_LOST 2
#define JITTER_DISTRIB 4
#define CALIBRATE 8
#define INTERRUPT_DISTRIB 16
#define SYSDELAY_DISTRIB 32
#define EXIT_LOST_PPS 64
#define FIX_DELAY_PEAK 128
#define SHOW_REMOVE_NOISE 256

/**
 * Struct for passing arguments to and from threads
 * querying SNTP servers.
 */
struct timeCheckParams {
	pthread_t *tid;				// Thread id
	pthread_attr_t attr;		// Thread attribute object
	int serverIndex;			// Identifying index from the list of active SNTP servers
	int *serverTimeDiff;		// Time difference between local time and server time
	char **ntp_server;			// The active SNTP server list
	char *buf;					// Space for the active SNTP server list
	char *strbuf;				// Space for messages and query strings
	char *logbuf;				// Space for returned log messages
	bool *threadIsBusy;			// True while thread is waiting for or processing a server query
};

struct saveFileData {
	const char *label;			// Command line identifier
	void *array;				// Array hold data to be saved
	const char *filename;		// Filename to save data
	int arrayLen;				// Length of the array in array units
	int arrayType;				// Array type: 1 - int, 2 - double
	int arrayZero;				// Array offset of zero.
};

/**
 * Struct definition for program-wide global variables.
 */
struct ppsClientGlobalVars {
	bool isVerbose;

	int intrptDelay;
	int	sysDelay;

	int sysDelayShift;
	int delayShift;
	int delayMinIdx;

	int sysDelayDistrib[INTRPT_DISTRIB_LEN];

	double delayMedian;

	int delayCount;
	int delayPeriod;
	int delay_idx;

	struct timeval t;
	struct timeval t1;
	struct timeval t2;
	struct timex t3;

	int tm[6];

	time_t pps_t_sec;
	int pps_t_usec;

	int invProportionalGain;
	double integralGain;
	int hardLimit;

	int jitter;
	int jitterCount;

	int jitterDistrib[JITTER_DISTRIB_LEN];

	double rawErrorDistrib[ERROR_DISTRIB_LEN];

	int noiseLevel;

	bool isDelaySpike;
	int nDelaySpikes;

	unsigned int seq_num;

	int seq_numRec[SECS_PER_10_MIN];

	unsigned int activeCount;
	unsigned int lastActiveCount;

	int seconds;
	int days;
	int intervalCount;

	__time_t timestampRec[NUM_5_MIN_INTERVALS];

	double avgIntegral;
	int integralCount;

	double integral[NUM_AVERAGES];

	int correctionAccum;
	int correctionFifoCount;

	int correctionFifo[OFFSETFIFO_LEN];

	int correctionFifo_idx;
	double avgCorrection;

	int errorDistrib[ERROR_DISTRIB_LEN];

	double avgSlew;
	double slewAccum;
	int slew_idx;
	bool slewIsLow;

	double freqOffset;
	double lastFreqOffset;
	double freqOffsetSum;

	double freqOffsetDiff[FREQDIFF_INTRVL];

	double freqAllanDev[NUM_5_MIN_INTERVALS];

	double freqOffsetRec[NUM_5_MIN_INTERVALS];

	int offsetRec[SECS_PER_10_MIN];

	int recIndex;

	double freqOffsetRec2[SECS_PER_10_MIN];

	int recIndex2;

	int interruptLossCount;
	bool interruptReceived;

	int interruptDistrib[INTRPT_DISTRIB_LEN];

	int intrptDelayShift;
	int intrptDelayMinIdx;

	unsigned int intrptCount;

	double intrptErrorDistrib[ERROR_DISTRIB_LEN];

	int consensisTimeError;

	bool isAcquiring;
	bool exit_loop;
	bool exit_requested;
	bool lostIntr;
	bool doCalibration;
	bool exitOnLostPPS;

	unsigned int config_select;

	char logbuf[LOGBUF_SZ];

	char msgbuf[MSGBUF_SZ];

	char savebuf[MSGBUF_SZ];

	char strbuf[STRBUF_SZ];

	int nIntrptDelaySpikes;
	bool medianIsSet;
	unsigned int ppsCount;

	bool fixDelayPeak;
	bool showRemoveNoise;
	int delayPeakLen;
	bool disableDelayShift;
};

void initialize(void);
void bufferStatusMsg(const char *params);
int writeStatusStrings(void);
bool ppsIsRunning(void);
int forceNTPupdate(void);
int writeFileMsgToLog(const char *);
int writeFileMsgToLogbuf(const char *, char *);
void writeToLog(char *);
pid_t getChildPID(void);
int createPIDfile(void);
int readConfigFile(char *[], char *, int);
void writeOffsets(void);
void writeTimestamp(double);
void writeSysDelay(void);
void bufferStateParams(void);
int allocNTPServerList(timeCheckParams *);
int disableNTP(void);
int enableNTP(void);
unsigned int ntpdate(const char *);
time_t getServerTime(const char *, int, char *, char *);
int open_logerr(const char*, int);
int read_logerr(int fd, char *, int, const char *);
int allocInitializeSNTPThreads(timeCheckParams *);
void freeSNTPThreads(timeCheckParams *);
void makeSNTPTimeQuery(timeCheckParams *);
int waitForNTPServers(void);
void writeInterruptDelayFile(void);
void writeInterruptDistribFile(void);
void processFiles(char *[], char *, int);
bool isEnabled(int, char *[]);
bool isDisabled(int, char *[]);
void saveDelay(int);
void writeSysdelayDistribFile(void);
void showStatusEachSecond(void);
struct timespec setSyncDelay(int, int);
int accessDaemon(int argc, char *argv[]);
int driver_load(void);
void driver_unload(void);

#endif /* PPS_CLIENT_OLD_PPS_CLIENT_H_ */