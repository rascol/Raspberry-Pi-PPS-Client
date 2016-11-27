/**
 * @file pps-client.h
 * @brief The pps-client.h file contains includes, defines and structures for pps-client.
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
#include <sys/mman.h>

#define PTHREAD_STACK_REQUIRED 16384
#define USECS_PER_SEC 1000000
#define SECS_PER_MINUTE 60
#define SECS_PER_5_MIN 300
#define SECS_PER_10_MIN 600
#define SECS_PER_HOUR 3600
#define SECS_PER_DAY 86400
#define NUM_5_MIN_INTERVALS 288
#define FIVE_MINUTES 5
#define PER_MINUTE (1.0 / (double)SECS_PER_MINUTE)
#define SETTLE_TIME (2 * SECS_PER_10_MIN)
#define INV_GAIN_1 1
#define INV_GAIN_0 4
#define INTEGRAL_GAIN 0.63212
#define SHOW_INTRPT_DATA_INTVL 6
#define INV_DELAY_SAMPLES_PER_MIN (1.0 / (double)SECS_PER_MINUTE)
#define FREQDIFF_INTRVL 5
#define INTRPT_MOST_DECAY_RATE 0.975

#define OFFSETFIFO_LEN 80
#define NUM_INTEGRALS 10
#define PER_NUM_INTEGRALS (1.0 / (double)NUM_INTEGRALS)

#define ADJTIMEX_SCALE 65536.0			//!< Frequency scaling required by \b adjtimex().

#define INTERRUPT_LATENCY 6				//!< Default interrupt latency assigned to sysDelay (microseconds).

#define RAW_ERROR_ZERO  20				//!< Index corresponding to rawError == 0 in detectDelayPeak().
#define MIN_PEAK_RATIO 0.05				//!< Minimum ratio to detect a second peak in detectDelayPeak().
#define MAX_VALLEY_RATIO 0.99			//!< Maximum ratio to detect a valley before the second peak in detectDelayPeak().
#define RAW_ERROR_DECAY 0.98851			//!< Decay rate for rawError samples (1 hour half life)

#define INTERRUPT_LOST 15				//!< Number of consequtive lost interrupts at which a warning starts

#define MAX_SERVERS 10					//!< Maximum number of SNTP time servers to use
#define CHECK_TIME 1024					//!< Interval between internet time checks (about 17 minutes)

#define MAX_SPIKES 30					//!< Maximum microseconds to suppress a burst of positive jitter

#define NOISE_FACTOR 0.354				//!< Adjusts g.noiseLevel to track g.sysDelay
#define NOISE_LEVEL_MIN 4				//!< The minimum level at which interrupt delays are delay spikes.
#define SLEW_LEN 10						//!< The slew accumulator (slewAccum) update interval
#define SLEW_MAX 65						//!< Jitter slew value below which the controller will begin to frequency lock.

#define MAX_LINE_LEN 50
#define STRBUF_SZ 500
#define LOGBUF_SZ 500
#define MSGBUF_SZ 500
#define SNTP_MSG_SZ 110
#define CONFIG_FILE_SZ 10000

#define NUM_PARAMS 5
#define ERROR_DISTRIB_LEN 121
#define JITTER_DISTRIB_LEN 121
#define INTRPT_DISTRIB_LEN 121

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

/*
 * Struct for passing arguments to and from threads
 * querying SNTP servers.
 */
struct timeCheckParams {
	pthread_t *tid;				//!< Thread id
	pthread_attr_t attr;		//!< Thread attribute object
	int serverIndex;			//!< Identifying index from the list of active SNTP servers
	int *serverTimeDiff;		//!< Time difference between local time and server time
	char **ntp_server;			//!< The active SNTP server list
	char *buf;					//!< Space for the active SNTP server list
	char *strbuf;				//!< Space for messages and query strings
	char *logbuf;				//!< Space for returned log messages
	bool *threadIsBusy;			//!< True while thread is waiting for or processing a server query
};								//!< Struct for passing arguments to and from threads querying SNTP servers.

/*
 * Struct for program-wide global variables.
 */
struct G {
	bool isVerbose;									//!< Enables continuous printing of state status params when "true".

	unsigned int seq_num;							//!< Advancing count of the number of PPS interrupt timings that have been received.

	bool isAcquiring;								//!< Set "true" by getAcquireState() when the control loop can begin to control the system clock frequency.
	unsigned int activeCount;						//!< Advancing count of controller cycles once G.isAcquiring is "true".

	bool interruptReceived;							//!< Set "true" when makeTimeCorrection() processes an interrupt time from the pps-client device driver.
	bool interruptLost;								//!< Set "true" when a PPS interrupt time fails to be received.
	int interruptLossCount;							//!< Records the number of consequtive lost PPS interrupt times.

	struct timeval t;								//!< Time of system response to the PPS interrupt. Receieved from the pps-client device driver.
	int interruptTime;								//!< Fractional second part of G.t received from pps-client device driver.

	int tm[6];										//!< Returns the interrupt calibration reception and response times from the pps-client device driver.
	int intrptDelay;								//!< Value of the interrupt delay calibration measurement received from the pps-client device driver.
	int intrptError;								//!< Set equal to "intrptDelay - sysDelay" in getInterruptDelay().
	unsigned int intrptCount;						//!< Advancing count of intrptErrorDistrib[] entries made by detectDelayPeak().
	double delayMedian;								//!< Median of G.intrptDelay values calculated in getInterruptDelay().
	int	sysDelay;									//!< System time delay between reception and response to an external interrupt.
													//!< Calculated as the one-minute median of G.intrptDelay values in getInterruptDelay().

	int rawError;									//!< Set equal to G.interruptTime - G.sysDelay in makeTimeCorrection().

	int delayShift;									//!< Interval of a delay shift when one is detected by detectDelayPeak().
	int sysDelayShift;								//!< Assigned from G.delayShift and subtracted from G.rawError in correctDelayPeak() when a delay shift occurs.
	int delayPeakLen;								//!< Counts the length of a delay peak that is being corrected in correctDelayPeak().
	bool disableDelayShift;							//!< Suspends delay shift correction in correctDelayPeak() when G.delayPeakLen exceeds \b MAX_SPIKES.
	int disableDelayCount;
													//!< Delay shift correction is suspended if a continuous sequence of delay spikes is longer
													//!< than \b MAX_SPIKES because that indicates that a shift in the control point is required.

	double rawErrorDistrib[ERROR_DISTRIB_LEN];		//!< The G.rawError distribution used to detect a delay shift in detectDelayPeak().
	int delayMinIdx;								//!< If a delay shift occurs, the minimum value preceding the delay peak in rawErrorDistrib[].
	unsigned int ppsCount;							//!< Advancing count of G.rawErrorDistrib[] entries made by detectDelayPeak().

	int nIntrptDelaySpikes;

	int noiseLevel;									//!< PPS time delay value beyond which a delay is defined to be a delay spike.
	int nDelaySpikes;								//!< Current count of continuous delay spikes made by detectDelaySpike().
	bool isDelaySpike;								//!< Set "true" by detectDelaySpike() when G.rawError exceeds G.noiseLevel.

	double slewAccum;								//!< Accumulates G.rawError in getTimeSlew() and is used to determine G.avgSlew.
	int slewAccum_cnt;								//!< Count of the number of times G.rawError has been summed into G.slewAccum.
	double avgSlew;									//!< Average slew value detemined by getTimeSlew() from the average of G.slewAccum each time G.slewAccum_cnt reaches \b SLEW_LEN.
	bool slewIsLow;									//!< Set to "true" in getAcquireState() when G.avgSlew is less than \b SLEW_MAX. This is a precondition for getAcquireState() to set G.isAcquiring to "true".

	int zeroError;									//!< The controller error resulting from removing jitter noise from G.rawError in removeNoise().
	int hardLimit;									//!< An adaptive limit value determined by setHardLimit() and applied to G.rawError by clampJitter() as the final noise reduction step to generate G.zeroError.
	int invProportionalGain;						//!< Controller proportional gain configured inversely to use as an int divisor.
	int timeCorrection;								//!< Time correction value constructed in makeTimeCorrection() by dividing G.zeroError by G.invProportionalGain.
	struct timex t3;								//!< Passes G.timeCorrection to the system function \b adjtimex() in makeTimeCorrection().

	double avgCorrection;							//!< A one-minute rolling average of G.timeCorrection values generated by getAverageCorrection().
	int correctionFifo[OFFSETFIFO_LEN];				//!< Contains the G.timeCorrection values from over the previous 60 seconds.
	int correctionFifoCount;						//!< Signals that G.correctionFifo contains a full count of G.timeCorrection values.
	int correctionAccum;							//!< Accumulates G.timeCorrection values from G.correctionFifo in getAverageCorrection() in order to generate G.avgCorrection.

	double integral[NUM_INTEGRALS];					//!< Array of integrals constructed by makeAverageIntegral().
	double avgIntegral;								//!< One-minute average of the integrals in G.integral[].
	int integralCount;								//!< Counts the integrals formed over the last 10 controller cycles and signals when all integrals in G.integral have been constructed.

	int correctionFifo_idx;							//!< Advances G.correctionFifo on each controller cycle in integralIsReady() which returns "true" every 60 controller cycles.

	double integralGain;							//!< Controller integral gain.
	double integralTimeCorrection;					//!< Integral or average integral of G.timeCorrection returned by getIntegral();
	double freqOffset;								//!< System clock frequency correction calculated as G.integralTimeCorrection * G.integralGain.

	int consensisTimeError;							//!< Consensis value of whole-second time corrections for DST or leap seconds from internet SNTP servers.

	char linuxVersion[20];							//!< Space for recording the Linux version.
	/**
	 * @cond FILES
	 */
	char logbuf[LOGBUF_SZ];
	char msgbuf[MSGBUF_SZ];
	char savebuf[MSGBUF_SZ];
	char strbuf[STRBUF_SZ];

	bool exit_requested;
	bool exitOnLostPPS;
	bool exit_loop;

	bool doCalibration;

	int recIndex;
	int recIndex2;

	time_t pps_t_sec;
	int pps_t_usec;

	unsigned int config_select;

	int intervalCount;

	int jitter;

	int seq_numRec[SECS_PER_10_MIN];

	double lastFreqOffset;
	double freqOffsetSum;
	double freqOffsetDiff[FREQDIFF_INTRVL];

	unsigned int lastActiveCount;

	double intrptErrorDistrib[ERROR_DISTRIB_LEN];	//!< The intrptError distribution calculated in detectDelayPeak().

	int intrptDistrib[NUM_PARAMS][INTRPT_DISTRIB_LEN];
	int delayLabel[NUM_PARAMS];

	int interruptDistrib[INTRPT_DISTRIB_LEN];
	int interruptCount;

	int sysDelayDistrib[INTRPT_DISTRIB_LEN];
	int sysDelayCount;

	int jitterDistrib[JITTER_DISTRIB_LEN];
	int jitterCount;

	int errorDistrib[ERROR_DISTRIB_LEN];
	int errorCount;

	double freqAllanDev[NUM_5_MIN_INTERVALS];
	double freqOffsetRec[NUM_5_MIN_INTERVALS];
	double freqOffsetRec2[SECS_PER_10_MIN];
	__time_t timestampRec[NUM_5_MIN_INTERVALS];
	int offsetRec[SECS_PER_10_MIN];
	/**
	 * @endcond
	 */
};													//!< Struct for program-wide global variables showing those important to the controller.

/**
 * @cond FILES
 */

int allocNTPServerList(timeCheckParams *);
time_t getServerTime(const char *, int, char *, char *);
int allocInitializeSNTPThreads(timeCheckParams *);
void freeSNTPThreads(timeCheckParams *);
void makeSNTPTimeQuery(timeCheckParams *);
int waitForNTPServers(void);

/**
 * Struct to hold associated data for pps-client command line
 * save data requests with the -s flag.
 */
struct saveFileData {
	const char *label;			//!< Command line identifier
	void *array;				//!< Array to hold data to be saved
	const char *filename;		//!< Filename to save data
	int arrayLen;				//!< Length of the array in array units
	int arrayType;				//!< Array type: 1 - int, 2 - double
	int arrayZero;				//!< Array index of data zero.
};

void bufferStatusMsg(const char *params);
int writeStatusStrings(void);
bool ppsIsRunning(void);
int writeFileMsgToLog(const char *);
int writeFileMsgToLogbuf(const char *, char *);
void writeToLog(char *);
pid_t getChildPID(void);
int createPIDfile(void);
int readConfigFile(char *[], char *, int);
void writeOffsets(void);
void writeTimestamp(double);
void writeSysDelay(void);
int bufferStateParams(void);
int disableNTP(void);
int enableNTP(void);
int open_logerr(const char*, int);
int read_logerr(int fd, char *, int, const char *);
void writeInterruptDistribFile(void);
void processFiles(char *[], char *, int);
bool isEnabled(int, char *[]);
bool isDisabled(int, char *[]);
void writeSysdelayDistribFile(void);
void showStatusEachSecond(void);
struct timespec setSyncDelay(int, int);
int accessDaemon(int argc, char *argv[]);
int driver_load(void);
void driver_unload(void);
void buildErrorDistrib(int);
void buildJitterDistrib(int);
void TERMhandler(int);
void HUPhandler(int);
void buildInterruptDistrib(int);
void buildInterruptJitterDistrib(int);
void buildSysDelayDistrib(int);
void recordFrequencyVars(void);
void recordOffsets(int timeCorrection);
/**
 * @endcond
 */
#endif /* PPS_CLIENT_OLD_PPS_CLIENT_H_ */
