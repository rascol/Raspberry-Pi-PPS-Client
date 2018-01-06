/**
 * @file pps-client.h
 * @brief This file contains includes, defines and structures for PPS-Client.
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

#define PTHREAD_STACK_REQUIRED 16384		//!< Stack space requirements for threads
#define USECS_PER_SEC 1000000
#define SECS_PER_MINUTE 60
#define SECS_PER_5_MIN 300
#define SECS_PER_10_MIN 600
#define SECS_PER_HOUR 3600
#define SECS_PER_DAY 86400
#define NUM_5_MIN_INTERVALS 288			//!< Number of five minute intervals in 24 hours
#define FIVE_MINUTES 5
#define PER_MINUTE (1.0 / (double)SECS_PER_MINUTE)	//!< Inverse seconds per minute
#define SETTLE_TIME (2 * SECS_PER_10_MIN)	//!< The PPS-Client up time required before saving performance data
#define INV_GAIN_1 1						//!< Controller inverse proportional gain constant during active controller operation
#define INV_GAIN_0 4						//!< Controller inverse proportional gain constant at startup
#define INTEGRAL_GAIN 0.63212			//!< Controller integral gain constant in active controller operation
#define SHOW_INTRPT_DATA_INTVL 6			//!< The number of seconds between displays of interrupt delay in the PPS-Client status line
#define INV_DELAY_SAMPLES_PER_MIN (1.0 / (double)SECS_PER_MINUTE) //!< Constant for calculating \b G.sysDelay
#define FREQDIFF_INTRVL 5				//!< The number of minutes between Allan deviation samples of system clock frequency correction

#define OFFSETFIFO_LEN 80				//!< Length of \b G.correctionFifo which contains the data used to generate \b G.avgCorrection.
#define NUM_INTEGRALS 10					//!< Number of integrals used by \b makeAverageIntegral() to calculate the one minute clock frequency correction
#define PER_NUM_INTEGRALS (1.0 / (double)NUM_INTEGRALS)	//!< Inverse of NUM_INTEGRALS

#define ADJTIMEX_SCALE 65536.0			//!< Frequency scaling required by \b adjtimex().

#define INTERRUPT_LATENCY 6				//!< Default interrupt latency assigned to sysDelay (microseconds).

#define RAW_ERROR_ZERO  20				//!< Index corresponding to rawError == 0 in \b detectDelayPeak().
#define MIN_PEAK_RATIO 0.05				//!< Minimum ratio to detect a second peak in \b detectDelayPeak().
#define MAX_VALLEY_RATIO 0.99			//!< Maximum ratio to detect a valley before the second peak in \b detectDelayPeak().
#define RAW_ERROR_DECAY 0.98851			//!< Decay rate for \b G.rawError samples (1 hour half life)

#define INTERRUPT_LOST 15				//!< Number of consecutive lost interrupts at which a warning starts

#define MAX_SERVERS 4					//!< Maximum number of SNTP time servers to use
#define CHECK_TIME 1024					//!< Interval between Internet time checks (about 17 minutes)
#define BLOCK_FOR_10 10					//!< Blocks detection of external system clock changes for 10 seconds
#define BLOCK_FOR_3 3					//!< Blocks detection of external system clock changes for 3 seconds
#define CHECK_TIME_SERIAL 600			//!< Interval between serial port time checks (about 10 minutes)

#define MAX_SPIKES 30					//!< Maximum microseconds to suppress a burst of continuous positive jitter

#define NOISE_FACTOR 0.354				//!< Adjusts \b G.noiseLevel to track \b G.sysDelay
#define NOISE_LEVEL_MIN 4				//!< The minimum level at which interrupt delays are delay spikes.
#define SLEW_LEN 10						//!< The slew accumulator (slewAccum) update interval
#define SLEW_MAX 65						//!< Jitter slew value below which the controller will begin to frequency lock.

#define MAX_LINE_LEN 50
#define STRBUF_SZ 500
#define LOGBUF_SZ 500
#define MSGBUF_SZ 500
#define SNTP_MSG_SZ 200
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

#define ERROR_DISTRIB 1				// Configuration file Keys
#define ALERT_PPS_LOST 2
#define JITTER_DISTRIB 4
#define CALIBRATE 8
#define INTERRUPT_DISTRIB 16
#define SYSDELAY_DISTRIB 32
#define EXIT_LOST_PPS 64
#define PPS_GPIO 128
#define OUTPUT_GPIO 256
#define INTRPT_GPIO 512
#define SNTP 1024
#define SERIAL 2048
#define SERIAL_PORT 4096

/*
 * Struct for passing arguments to and from threads
 * querying time servers.
 */
struct timeCheckParams {
	pthread_t *tid;									//!< Thread id
	pthread_attr_t attr;								//!< Thread attribute object
	int serverIndex;									//!< Identifying index from the list of active SNTP servers
	int *serverTimeDiff;								//!< Time difference between local time and server time
	char **ntp_server;								//!< The active SNTP server list when SNTP is used
	char *serialPort;								//!< The serial port filename when serial time is used
	char *buf;										//!< Space for the active SNTP server list
	bool doReadSerial;								//!< Flag to read serial messages from serial port
	char *strbuf;									//!< Space for messages and query strings
	char *logbuf;									//!< Space for returned log messages
	bool *threadIsBusy;								//!< True while thread is waiting for or processing a time query
	int rv;											//!< Return value of thread
};													//!< Struct for passing arguments to and from threads querying SNTP time servers or GPS receivers.

/*
 * Struct for program-wide global variables.
 */
struct G {
	int ppsGPIO;										//!< The PPS GPIO interrupt number read from pps-client.conf and passed to the driver.
	int outputGPIO;									//!< The calibrate GPIO output number read from pps-client.conf and passed to the driver.
	int intrptGPIO;									//!< The calibrate GPIO interrupt number read from pps-client.conf and passed to the driver.

	bool isVerbose;									//!< Enables continuous printing of PPS-Client status params when "true".

	bool configWasRead;								//!< True if pps-client.conf was read at least once.

	unsigned int seq_num;							//!< Advancing count of the number of PPS interrupt timings that have been received.

	bool isControlling;								//!< Set "true" by \b getAcquireState() when the control loop can begin to control the system clock frequency.
	unsigned int activeCount;						//!< Advancing count of controller cycles once \b G.isControlling is "true".

	bool interruptReceived;							//!< Set "true" when \b makeTimeCorrection() processes an interrupt time from the PPS-Client device driver.
	bool interruptLost;								//!< Set "true" when a PPS interrupt time fails to be received.
	int interruptLossCount;							//!< Records the number of consecutive lost PPS interrupt times.

	struct timeval t;								//!< Time of system response to the PPS interrupt. Received from the PPS-Client device driver.
	int interruptTime;								//!< Fractional second part of \b G.t received from PPS-Client device driver.

	int tm[6];										//!< Returns the interrupt calibration reception and response times from the PPS-Client device driver.

	int t_now;										//!< Whole seconds of current time reported by \b gettimeofday().
	int t_count;										//!< Whole seconds counted at the time of \b G.t_now.
	double t_mono_now;								//!< Current monotonic count of passing seconds
	double t_mono_last;								//!< Last recorded monotonic count used to determine a lost PPS update
	double zeroAccum;								//!< Accumulator to test nearness to zero in \b isNearZero()

	int intrptDelay;									//!< Value of the interrupt delay calibration measurement received from the PPS-Client device driver.
	int intrptError;									//!< Set equal to "intrptDelay - sysDelay" in \b getInterruptDelay().
	unsigned int intrptCount;						//!< Advancing count of intrptErrorDistrib[] entries made by \b detectDelayPeak().
	double delayMedian;								//!< Median of \b G.intrptDelay values calculated in \b getInterruptDelay().
	int	sysDelay;									//!< System time delay between reception and response to an external interrupt.
													//!< Calculated as the one-minute median of \b G.intrptDelay values in \b getInterruptDelay().

	int rawError;									//!< Set equal to \b G.interruptTime - \b G.sysDelay in \b makeTimeCorrection().

	int delayShift;									//!< Interval of a delay shift when one is detected by \b detectDelayPeak().
	int sysDelayShift;								//!< Assigned from \b G.delayShift and subtracted from \b G.rawError in \b correctDelayPeak() when a delay shift occurs.
	int delayPeakLen;								//!< Counts the length of a delay peak that is being corrected in \b correctDelayPeak().
	bool disableDelayShift;							//!< Suspends delay shift correction in \b correctDelayPeak() when \b G.delayPeakLen exceeds \b MAX_SPIKES.

	int disableDelayCount;							//!< Delay shift correction is suspended if a continuous sequence of delay spikes is longer
													//!< than \b MAX_SPIKES because that indicates that a shift in the control point is required.

	double rawErrorDistrib[ERROR_DISTRIB_LEN];		//!< The distribution used to detect a delay shift in \b detectDelayPeak().
	int delayMinIdx;									//!< If a delay shift occurs, the minimum value preceding the delay peak in \b rawErrorDistrib[].
	unsigned int ppsCount;							//!< Advancing count of \b G.rawErrorDistrib[] entries made by \b detectDelayPeak().

	int nIntrptDelaySpikes;

	int noiseLevel;									//!< PPS time delay value beyond which a delay is defined to be a delay spike.
	int nDelaySpikes;								//!< Current count of continuous delay spikes made by \b detectDelaySpike().
	bool isDelaySpike;								//!< Set "true" by \b detectDelaySpike() when \b G.rawError exceeds \b G.noiseLevel.

	double slewAccum;								//!< Accumulates \b G.rawError in \b getTimeSlew() and is used to determine \b G.avgSlew.
	int slewAccum_cnt;								//!< Count of the number of times \b G.rawError has been summed into \b G.slewAccum.
	double avgSlew;									//!< Average slew value determined by \b getTimeSlew() from the average of \b G.slewAccum each time \b G.slewAccum_cnt reaches \b SLEW_LEN.
	bool slewIsLow;									//!< Set to "true" in \b getAcquireState() when \b G.avgSlew is less than \b SLEW_MAX. This is a precondition for \b getAcquireState() to set \b G.isControlling to "true".

	int zeroError;									//!< The controller error resulting from removing jitter noise from \b G.rawError in \b removeNoise().
	int hardLimit;									//!< An adaptive limit value determined by \b setHardLimit() and applied to \b G.rawError by \b clampJitter() as the final noise reduction step to generate \b G.zeroError.
	int invProportionalGain;							//!< Controller proportional gain configured inversely to use as an int divisor.
	int timeCorrection;								//!< Time correction value constructed in \b makeTimeCorrection() by dividing \b G.zeroError by \b G.invProportionalGain.
	struct timex t3;									//!< Passes \b G.timeCorrection to the system function \b adjtimex() in \b makeTimeCorrection().

	double avgCorrection;							//!< A one-minute rolling average of \b G.timeCorrection values generated by \b getAverageCorrection().
	int correctionFifo[OFFSETFIFO_LEN];				//!< Contains the \b G.timeCorrection values from over the previous 60 seconds.
	int correctionFifoCount;							//!< Signals that \b G.correctionFifo contains a full count of \b G.timeCorrection values.
	int correctionAccum;								//!< Accumulates \b G.timeCorrection values from \b G.correctionFifo in \b getAverageCorrection() in order to generate \b G.avgCorrection.

	double integral[NUM_INTEGRALS];					//!< Array of integrals constructed by \b makeAverageIntegral().
	double avgIntegral;								//!< One-minute average of the integrals in \b G.integral[].
	int integralCount;								//!< Counts the integrals formed over the last 10 controller cycles and signals when all integrals in \b G.integral have been constructed.

	int correctionFifo_idx;							//!< Advances \b G.correctionFifo on each controller cycle in \b integralIsReady() which returns "true" every 60 controller cycles.

	double integralGain;								//!< Current controller integral gain.
	double integralTimeCorrection;					//!< Integral or average integral of \b G.timeCorrection returned by \b getIntegral();
	double freqOffset;								//!< System clock frequency correction calculated as \b G.integralTimeCorrection * \b G.integralGain.

	int consensusTimeError;							//!< Consensus value of whole-second time corrections for DST or leap seconds from Internet SNTP servers.

	char linuxVersion[20];							//!< Array for recording the Linux version.
	/**
	 * @cond FILES
	 */
	char logbuf[LOGBUF_SZ];
	char msgbuf[MSGBUF_SZ];
	char savebuf[MSGBUF_SZ];
	char strbuf[STRBUF_SZ];
	char *configVals[MAX_CONFIGS];

	bool exit_requested;
	bool exitOnLostPPS;
	bool exit_loop;

	bool doCalibration;
	bool doNTPsettime;

	bool doSerialsettime;
	int blockDetectClockChange;

	int serialTimeError;

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

	double intrptErrorDistrib[ERROR_DISTRIB_LEN];		//!< The intrptError distribution calculated in \b detectDelayPeak().

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
	int queryCount;

	double freqAllanDev[NUM_5_MIN_INTERVALS];
	double freqOffsetRec[NUM_5_MIN_INTERVALS];
	double freqOffsetRec2[SECS_PER_10_MIN];
	__time_t timestampRec[NUM_5_MIN_INTERVALS];
	int offsetRec[SECS_PER_10_MIN];
	char serialPort[50];
	char configBuf[CONFIG_FILE_SZ];
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

int allocInitializeSerialThread(timeCheckParams *tcp);
void freeSerialThread(timeCheckParams *tcp);
int makeSerialTimeQuery(timeCheckParams *tcp);

/**
 * Struct to hold associated data for PPS-Client command line
 * save data requests with the -s flag.
 */
struct saveFileData {
	const char *label;			//!< Command line identifier
	void *array;					//!< Array to hold data to be saved
	const char *filename;		//!< Filename to save data
	int arrayLen;				//!< Length of the array in array units
	int arrayType;				//!< Array type: 1 - int, 2 - double
	int arrayZero;				//!< Array index of data zero.
};

int sysCommand(const char *);
void initFileLocalData(void);
void initSerialLocalData(void);
void bufferStatusMsg(const char *);
int writeStatusStrings(void);
bool ppsIsRunning(void);
int writeFileMsgToLog(const char *);
int writeFileMsgToLogbuf(const char *, char *);
void writeToLog(char *);
pid_t getChildPID(void);
int createPIDfile(void);
int readConfigFile(void);
void writeOffsets(void);
void writeTimestamp(double);
void writeSysDelay(void);
int bufferStateParams(void);
int disableNTP(void);
int enableNTP(void);
int open_logerr(const char*, int);
int read_logerr(int fd, char *, int, const char *);
void writeInterruptDistribFile(void);
int processFiles(void);
bool isEnabled(int);
bool isDisabled(int);
void writeSysdelayDistribFile(void);
void showStatusEachSecond(void);
struct timespec setSyncDelay(int, int);
int accessDaemon(int argc, char *argv[]);
int driver_load(int, int, int);
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
bool configHasValue(int, char *[], void *);
int getDriverGPIOvals(void);
void writeToLogNoTimestamp(char *);
int getTimeErrorOverSerial(int *);
/**
 * @endcond
 */
#endif /* PPS_CLIENT_OLD_PPS_CLIENT_H_ */
