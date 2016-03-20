/*
 * pps-client.cpp - Client for a PPS source synchronized to
 * time of day
 *
 * Created on: Nov 17, 2015
 *
 * Copyright (C) 2015  Raymond S. Connell
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
 *
 * NOTES:
 *
 * This daemon synchronizes the system clock to a Pulse-Per-Second (PPS)
 * source to a resolution of one microsecond with an absolute accuracy
 * of a few microseconds. To obtain this level of performance pps-client
 * provides offset corrections every second and frequency corrections every
 * minute. This and removal of jitter in the reported PPS keeps the system
 * clock continuously synchronized to the PPS source.
 *
 * A wired GPIO connection is required from a PPS source. Synchronization
 * is provided by the rising edge of that PPS source which is connected to
 * GPIO 4.
 *
 * The executeable for this daemon is "/usr/sbin/pps-client"
 *
 * The daemon script is "/etc/init.d/pps-client"
 *
 * The configuration file is "/etc/pps-client.conf"
 *
 * The kernel driver is
 * "/lib/modules/`uname -r`/kernel/drivers/misc/pps-client.ko"
 *
 * The daemon is started with,
 *
 * $ sudo service pps-client start
 *
 * and stopped with,
 *
 * $ sudo service pps-client stop
 *
 * The pps-client does not become a permanent service that automatically
 * loads at boot until the chkconfig command is used:
 *
 * $ sudo chkconfig --add pps-client
 *
 * If chkconfig is not already installed:
 *
 * $ sudo apt-get install chkconfig
 *
 * By using the -v (verbose) option, pps-client will display a second-by-
 * second display of the tracking parameters captured at the rising edge
 * of the PPS signal. The purpose of the tracking display is to provide
 * the ability to monitor the controller in real time as it acquires.
 *
 * $ sudo service pps-client start -v
 *
 * This tracking display will continue until the daemon is stopped or the
 * terminal is closed.
 *
 * If the deamon is already running (e.g., it was started at boot or the
 * terminal displaying the tracking params was closed) the trackin display
 * can be started with:
 *
 * $ pps-client -v
 *
 * This command runs additional instances of pps-client that only display
 * what the daemon is generating but do nothing else. The tracking display
 * can be stopped with ctrl-c. This has no effect on the daemon.
 *
 * If the daemon was not already running when the above command was issued,
 * pps-client won't start and you will get a bunch of error messages
 * suggesting that you should have used sudo.
 *
 * By whatever methond it was successfully started, the daemon can always
 * be stopped from any connected terminal with
 *
 * $ sudo service pps-client stop
 *
 * The tracking params shown in the tracking display are listed in displayed
 * order below (hint: Set the terminal window wide!):
 *
 * timestamp - of the rollover of the second correponding to the time
 *     correction made to the system clock.
 * sequence number - the total number of PPS interrupts received
 *     since pps-client was started.
 * jitter - the current time deviation in microseconds recorded
 *     at the reception of the current PPS interrupt.
 * freqOffset - the current frequency offset of the system clock
 *     in parts per million of the system clock frequency.
 * avgCorrection - the time corrections (in microseconds) averaged over
 *     the last minute.
 * clamp - the hard limit (in microsecs) applied to the raw time error to
 *     convert it to a time correction.
 *
 * To determine the status of the time tracking pps-client can save
 * any combination of the following status files which are enabled
 * in /etc/pps-client.conf:
 *
 * /var/local/error-distrib - contains the cumulative distribution
 *     of time corrections applied to the system clock over the
 *     previous 24 hours.
 *
 * /var/local/error-distrib-forming - contains the currently forming
 *     distribution of time corrections to the system clock. When
 *     24 hours of corrections have been accumulated, these are
 *     transferred to "/var/local/error-distrib".
 *
 * /var/local/jitter-distrib - contains the cumulative distribution
 *     of all time (jitter) values recorded at reception of the PPS
 *     interrupt over the previous 24 hours.
 *
 * /var/local/jitter-distrib-forming - contains the currently forming
 *     distribution of jitter values. When 24 hours of corrections
 *     have been accumulated, these are transferred to
 *     "/var/local/jitter-distrib".
 *
 * /var/local/frequency-vars - contains a rolling record over the
 *     last 24 hours of the average system clock frequency offset
 *     in each 5 minute interval and the Allan deviation of one
 *     minute frequency samples over the five minute interval, both
 *     in parts per million of the system clock frequency.
 *
 * /var/local/pps-offsets - contains a second-by-second record
 *     over the last 10 minutes of time corrections made to the
 *     system clock and simultaneous system clock frequency offset.
 *
 * /var/local/burst-distrib - contains the cumulative distribution
 *     of lengths in seconds of jitter bursts that occurred over
 *     24 hours.
 *
 * /var/local/burst-distrib-forming - contains the currently forming
 *     distribution of jitter burst lengths. When 24 hours of these
 *     have been accumulated, they are transferred to
 *     /var/local/burst-distrib.
 *
 * /var/local/longbursts - contains a list of the jitter values in
 *     each jitter burst of length 6 seconds or longer.
 *
 * If "calibrate=enable" in /etc/pps-client.conf additional files
 * may be saved:
 *
 * /var/local/intrpt-distrib - contains a cumulative distribution
 *     of recorded calibration interrupt delays accumulated over six
 *     days to allow a numerically equivalent comparison with the
 *     jitter and error distributions accumulated over 24 hours.
 *
 * /var/local/intrpt-distrib-forming - contains the currently forming
 *     distribution of calibration interrupt delays. When six days of
 *     these have been accumulated they are transferred to
 *     /var/local/intrpt-distrib.
 *
 * /var/local/sysDelay-distrib - contains a cumulative distribution
 *     of sysDelay values that were applied to the pps-client
 *     controller in place of the static sysDelay value used when
 *     "calibrate" is not enabled. Six days of values are accumulated
 *     to allow a numerically equivalent comparison with the jitter
 *     and error distributions accumulated over 24 hours.
 *
 * /var/local/sysDelay-distrib-forming - contains the currently
 *     forming distribution of sysDelay values. When six days of
 *     these have been accumulated they are transferred to
 *     /var/local/sysDelay-distrib.
 *
 * Operation of the daemon can also be verified by reading the
 * saved timestamp of the last PPS rising edge a few times:
 *
 * $ cat /run/shm/pps-assert
 *
 * which also displays the sequence number of the PPS which is
 * the number of PPS interrupts received since pps-client started
 * but also provides the approximate number of seconds the daemon
 * has been running.
 *
 * The pps-client requires NTP to provide the date and local time
 * to the nearest second. SNTP is queried every 1024 seconds
 * (~17 minutes) to maintain the date and time including time
 * change for DST and leap second updates.
 *
 * Finally, pps-client can have a startup time as long as 20
 * minutes the first time it runs. Subsequent startups will usually
 * settle to 5 microsecond accuracy within about 10 minutes.
 */

#include "../build/pps-client.h"

const char *version = "0.2.1";

/**
 * Declares the global variables.
 */
struct ppsClientGlobalVars g;

/**
 * Sets global variables to initial values at
 * startup or restart and set frequency offset
 * to zero.
 */
void initialize(bool verbose){
	memset(&g, 0, sizeof(struct ppsClientGlobalVars));

	g.isVerbose = verbose;
	g.sysDelay = INTERRUPT_LATENCY;
	g.delayMedian = (double)INTERRUPT_LATENCY;
	g.integralGain = INTEGRAL_GAIN;
	g.invProportionalGain = INV_GAIN_0;
	g.maxInterval = FIVE_MINUTES;
	g.hardLimit = HARD_LIMIT_NONE;
	g.delayPeriod = CALIBRATE_PERIOD;
	g.doCalibration = true;
	g.exitOnLostPPS = true;

	g.t3.modes = ADJ_FREQUENCY;			// Initialize system clock
	g.t3.freq = 0;						// frequency offset to zero.
	adjtimex(&g.t3);
}

/**
 * Returns true when the control loop can begin to
 * control the system clock frequency. At program
 * start only the time slew is adjusted because the
 * drift can be too large for it to be practical to
 * adjust the system clock frequency to correct for
 * it. SLEW_MAX sets a reasonable limit below which
 * frequency offset can also be adjusted to correct
 * system time.
 *
 * Consequently, once the drift is within SLEW_MAX
 * microseconds of zero and the controller has been
 * running for at least 60 seconds (time selected for
 * convenience), this function returns "true" causing
 * the controller to begin to also adjust the system
 * clock frequency offset.
 */
bool getAcquireState(void){

	if (! g.slewIsLow && g.slew_idx == 0
			&& fabs(g.avgSlew) < SLEW_MAX){					// SLEW_MAX only needs to be low enough
		g.slewIsLow = true;									// that the controller can begin locking
	}														// at limitValue == HARD_LIMIT_NONE

	return (g.slewIsLow && g.seq_num >= SECS_PER_MINUTE);	// The g.seq_num reqirement sets a limit on the
}															// length of time to run the Type 1 controller
															// that initially pushes avgSlew below SLEW_MAX.

/**
 * Uses g.avgSlew or avgCorrection and the curent
 * hard limit, g.hardLimit, to determine the global
 * g.hardLimit to set on zero error to convert error
 * values to time corrections.
 *
 * Because it is much more effective and does not
 * introduce additional time delay, hard limiting
 * is used instead of filtering to remove noise
 * (jitter) from the reported time of PPS capture.
 */
void setHardLimit(double avgCorrection){

	double avgErrorMag = fabs(avgCorrection);

	if (g.activeCount < SECS_PER_MINUTE){
		g.hardLimit = HARD_LIMIT_NONE;
		return;
	}

	if (abs(g.avgSlew) > SLEW_MAX){					// As long as average time slew is
		int d_4 = abs(g.avgSlew) * 4;				// ouside of range SLEW_MAX this keeps
		while (g.hardLimit < d_4					// g.hardLimit above 4 * g.avgSlew
				&& g.hardLimit < HARD_LIMIT_NONE){	// which is high enough to allow the
			g.hardLimit = g.hardLimit << 1;			// controller to pull avgSlew within
		}											// SLEW_MAX.
		return;
	}

	if (g.hardLimit == HARD_LIMIT_1){
		if (avgErrorMag > HARD_LIMIT_05){
			g.hardLimit = g.hardLimit << 1;
		}
	}
	else if (avgErrorMag < HARD_LIMIT_05){
		g.hardLimit = HARD_LIMIT_1;
	}
	else if (avgErrorMag < (g.hardLimit >> 2)){		// If avgCorrection is below 1/4 of limitValue
		g.hardLimit = g.hardLimit >> 1;				// then halve limitValue.
	}
	else if (avgErrorMag > (g.hardLimit >> 1)){		// If avgCorrection is above 1/2 of limitValue
		g.hardLimit = g.hardLimit << 1;				// then double limitValue.

		if (g.hardLimit > HARD_LIMIT_NONE){
			g.hardLimit = HARD_LIMIT_NONE;
		}
	}
}

/**
 * Removes jitter burst noise by returning "true"
 * as long as the jitter value remains beyond a
 * threshold (BURST_LEVEL). Is not active unless
 * g.hardLimit is at HARD_LIMIT_4 or below.
 *
 * Although pps-client also uses hard limiting to
 * remove noise, that alone is not sufficient to
 * remove the positive bias on the control point
 * that would otherwise be introduced by persistent
 * and relatively long burst intervals. Because
 * intervals of burst noise are readily distinguished
 * from the random backgound by relative time duration,
 * this function is able to remove the effect of
 * jitter burst noise almost completely.
 *
 * If the jitter burst persists for longer than
 * BURST_MAX seconds it is assumed that a shift in
 * the system clock operating point has occurred
 * and the controller resumes control at the new
 * operating point.
 */
bool detectBurstNoise(int zeroError){
	bool isBurstNoise = false;

	if (g.hardLimit <= HARD_LIMIT_4 && zeroError >= BURST_LEVEL){
		if (g.burstLen < BURST_MAX) {
			g.longburstArray[g.burstLen] = zeroError;  // Record jitter values for longbursts
			g.burstLen += 1;

			isBurstNoise = true;
		}
		else {										// If burstLen == BURST_MAX stop the
			isBurstNoise = false;					// suspend even if burst continues.
		}
	}
	else {
		isBurstNoise = false;

		if (g.burstLen > 0){

			if (g.burstLen >= LONGBURST){			// Report on burst >= LONGBURST
				g.longBurst = g.burstLen;
			}

			g.burstLength[g.burstLen-1] += 1;		// Update burst length distrib
			g.burstLen = 0;
		}
	}
	return isBurstNoise;
}

/**
 * Gets the average time offset from zero over the interval
 * SLEW_LEN and updates avgSlew with this value every SLEW_LEN
 * seconds.
 */
void getTimeSlew(int zeroError){

	g.slewAccum += (double)zeroError;

	g.slew_idx += 1;
	if (g.slew_idx >= SLEW_LEN){
		g.slew_idx = 0;

		g.avgSlew = g.slewAccum / (double)SLEW_LEN;

		g.slewAccum = 0.0;
	}
}

/**
 * Clamps rawError to an adaptive value determined at the current
 * hardLimit value from the current value of avgCorrection.
 *
 * Once the timeError values have been limited to values of +/- 1
 * microsecond and the control loop has settled, this clamping causes
 * the controller to make the average number of positive and negative
 * jitter values equal rather than making the sum of the positive and
 * negative jitter values zero. This removes the bias that would
 * otherwise be introduced by the jitter values which are largely
 * random and consequently would introduce a constantly changing
 * random offset. The result is also to move the average PPS interrupt
 * delay to its median value.
 */
int clampJitter(int rawError){

	setHardLimit(g.avgCorrection);

	if (rawError > g.hardLimit){
		rawError = g.hardLimit;
	}
	else if (rawError < -g.hardLimit){
		rawError = -g.hardLimit;
	}

	return rawError;
}

/**
 * Each second, records the time correction that was applied to
 * the system clock and also records the last clock frequency
 * offset (in parts per million) that was applied to the system
 * clock.
 *
 * These values are recorded so that they may be saved to disk
 * for future analysis.
 */
void recordOffsets(int timeCorrection){
	g.seq_numRec[g.recIndex2] = g.seq_num;
	g.offsetRec[g.recIndex2] = timeCorrection;
	g.freqOffsetRec2[g.recIndex2] = g.freqOffset;

	g.recIndex2 += 1;
	if (g.recIndex2 == SECS_PER_10_MIN){
		g.recIndex2 = 0;
	}
}

/**
 * Accumulates the clock frequency offset over the last 5 minutes
 * and records offset difference each minute over the previous 5
 * minute interval. This function is called once each minute.
 *
 * The values of offset difference, g.freqOffsetDiff[], are used
 * to calculate the Allan deviation of the clock frequency offset
 * which is determined so that it can be saved to disk for analysis.
 * g.freqOffsetSum is used to calculate the average clock frequency
 * offset in each 5 minute interval so that value can also be saved
 * to disk for analysis.
 */
void recordFrequencyVars(void){
	g.freqOffsetSum += g.freqOffset;

	g.freqOffsetDiff[g.intervalCount] = g.freqOffset - g.lastFreqOffset;

	g.lastFreqOffset = g.freqOffset;
	g.intervalCount += 1;
}

/**
 * Constructs a distribution of time correction values with
 * zero offset at middle index so that this distribution can
 * be saved to disk for future analysis.
 *
 * A time correction is the raw time error passed through a hard
 * limitter to remove jitter and then scaled by the proportional
 * gain constant.
 */
void buildDistrib(int timeCorrection){
	int len = ERROR_DISTRIB_LEN - 1;
	int idx = timeCorrection + len / 2;

	if (idx < 0){
		idx = 0;
	}
	else if (idx > len){
		idx = len;
	}
	g.errorDistrib[idx] += 1;
}

/**
 * Constructs a distribution of jitter with zero offset
 * at middle index so that this distribution may be
 * saved to disk for future analysis.
 *
 * All jitter is collected including jitter bursts.
 */
void buildJitterDistrib(int zeroError){
	int len = JITTER_DISTRIB_LEN - 1;
	int idx = zeroError + len / 2;

	if (idx < 0){
		idx = 0;
	}
	else if (idx > len){
		idx = len;
	}
	g.jitterDistrib[idx] += 1;

	g.jitterCount += 1;
	if (g.jitterCount == SECS_PER_MINUTE){
		g.jitterCount = 0;
	}
}

/**
 * Constructs, at each second over the last 10 seconds
 * in each minute, 10 integrals of the average time
 * correction over the last minute.
 *
 * These integrals are then averaged to g.avgIntegral
 * just before the minute rolls over. The use of this
 * average of the last 10 integrals to correct the
 * frequency offset of thesystem clock provides a modest
 * improvement over using only the single last integral.
 */
void makeAverageIntegral(double avgCorrection){

	int indexOffset = SECS_PER_MINUTE - NUM_AVERAGES;

	if (g.correctionFifo_idx >= indexOffset){

		int i = g.correctionFifo_idx - indexOffset;
		if (i == 0){
			g.avgIntegral = 0.0;
			g.integralCount = 0;
		}

		g.integral[i] = g.integral[i] + avgCorrection;			// avgCorrection sums into g.integral[i] once each
																// minute forming the ith integral over the last minute.
		if (g.hardLimit == HARD_LIMIT_1){
			g.avgIntegral += g.integral[i];						// Accumulate each integral that is being formed
			g.integralCount += 1;								// into g.avgIntegral for averaging.
		}
	}

	if (g.correctionFifo_idx == SECS_PER_MINUTE - 1				// just before the minute rolls over.
			&& g.integralCount == NUM_AVERAGES){

		g.avgIntegral *= PER_NUM_INTEGRALS;						// Normaloze g.avgIntegral.
	}
}

/**
 * Advances the g.correctionFifo index each second and
 * returns "true" when SECS_PER_MINUTE new time correction
 * values have been accumulated in g.correctionAccum.
 *
 * When a value of "true" is returned, new average
 * time correction integrals have been generated by
 * makeAverageIntegral() and are ready for use.
 */
bool integralIsReady(void){
	bool isReady = false;

	if (g.correctionFifo_idx == 0){
		isReady = true;
	}

	g.correctionFifo_idx += 1;
	if (g.correctionFifo_idx >= SECS_PER_MINUTE){
		g.correctionFifo_idx = 0;
	}

	return isReady;
}

/**
 * Maintains g.correctionFifo which contains second-by-second
 * values of time corrections over the last minute, accumulates
 * a rolling sum of these and returns the average correction
 * up to the current second.
 */
double getAverageCorrection(int timeCorrection){

	double avgCorrection;

	if (g.seq_num > SETTLE_TIME){

		if (g.config_select & ERROR_DISTRIB){
			buildDistrib(timeCorrection);
		}
	}

	g.correctionAccum += timeCorrection;			// Add the new timeCorrection into the error accumulator.

	if (g.correctionFifoCount == SECS_PER_MINUTE){	// Once the fifo is full, maintain the continuous
													// rolling sum accumulator by subtracting the
		int oldError = g.correctionFifo[g.correctionFifo_idx];
		g.correctionAccum -= oldError;				// old timeCorrection value at the current correctionFifo_idx.
	}

	g.correctionFifo[g.correctionFifo_idx] = timeCorrection;	// and replacing the old value in the fifo with the new.

	if (g.correctionFifoCount < SECS_PER_MINUTE){	// When correctionFifoCount == SECS_PER_MINUTE
		g.correctionFifoCount += 1;					// the fifo is full and ready to use.
	}

	avgCorrection = g.correctionAccum * PER_MINUTE;
	return avgCorrection;
}

/**
 * Sets the system time whenever there is an error
 * relative to the whole seconds obtained from
 * internet SNTP servers.
 *
 * Errors are infrequent: DST change twice a year or
 * leap seconds less frequently. The whole seconds
 * of system clock are set immedately to the correct
 * time.
 */
void setClockToNTPtime(int pps_fd){

	sprintf(g.logbuf, "seq_num: %d consensisTimeError: %d\n", g.seq_num, g.consensisTimeError);
	writeToLog(g.logbuf);

	int msg[2];
	msg[0] = 3;
	msg[1] = g.consensisTimeError;
	write(pps_fd, msg, 2 * sizeof(int));

	g.consensisTimeError = 0;
}

/**
 * Removes burst noise and jitter from rawError and
 * returns the resulting zeroError.
 */
int removeNoise(int rawError){

	int zeroError;

	g.jitter = rawError;

	if ((g.config_select & JITTER_DISTRIB) && g.seq_num > SETTLE_TIME){
		buildJitterDistrib(rawError);
	}

	g.isBurstNoise = detectBurstNoise(rawError);	// g.isBurstNoise == true will prevent time and
													// frequency updates during a jitter burst.
	if (g.isBurstNoise){
		return 0;
	}

	getTimeSlew(rawError);

	zeroError = clampJitter(rawError);			// Recover the time error by
													// limiting away the jitter.
	if (g.isAcquiring){
		g.invProportionalGain = INV_GAIN_1;
	}
	return zeroError;
}

/**
 * if g.hardLimit == HARD_LIMIT_1, gets an integral time
 * correction as a 10 second average of integrals of average
 * time corrections over one minute. Otherwise gets the
 * integral time correction as the single last integral
 * of average time corrections over one minute.
 */
double getIntegral(void){
	double integral;

	if (g.hardLimit == HARD_LIMIT_1
			&& g.integralCount == NUM_AVERAGES){
		integral = g.avgIntegral;					// Use average of last 10 integrals
	}												// in the last minute.
	else {
		integral = g.integral[9];					// Use only the last integral from
													// the last minute
	}

	if (g.config_select & FREQUENCY_VARS){
		recordFrequencyVars();
	}

	return integral;
}

/**
 * Gets the time of the PPS rising edge from the
 * timeCorrection value and sets the corresponding
 * timestamp.
 */
void getPPStime(timeval t, int timeCorrection){
	g.pps_t_sec = t.tv_sec;
	g.pps_t_usec = -timeCorrection;
	if (timeCorrection > 0){
		g.pps_t_sec -= 1;
		g.pps_t_usec = 1000000 - timeCorrection;
	}

	double timestamp = t.tv_sec - 1e-6 * (double)timeCorrection;
	writeTimestamp(timestamp);
}

/**
 * Gets the fractional seconds part of interrupt time
 * and if the value should be interpreted as negative
 * then translates the value.
 */
int getFractionalSeconds(timeval pps_t){
	int interruptTime = pps_t.tv_usec;

	if (interruptTime > 500000){
		interruptTime -= USECS_PER_SEC;
	}
	return interruptTime;
}

/**
 * Makes time corrections each second, frequency
 * corrections each minute and removes jitter
 * from the PPS time reported by pps_t.
 *
 * This function is called by readPPS_SetTime()
 * from within the one-second delay loop in the
 * waitForPPS() function.
 */
void makeTimeCorrection(timeval pps_t, int pps_fd){
	g.interruptReceived = true;

	if (g.consensisTimeError != 0){						// When an NTP time correction is needed
		setClockToNTPtime(pps_fd);						// apply it here.
	}

	g.seq_num += 1;

	int interruptTime = getFractionalSeconds(pps_t);

	int rawError = interruptTime - g.sysDelay;			// References the controller to g.sysDelay which sets the time
														// of the PPS rising edge to zero at the start of the second.
	int zeroError = removeNoise(rawError);

	if (g.isBurstNoise){								// Skip a jitter burst.
		getPPStime(pps_t, 0);
		return;
	}

	int timeCorrection = -zeroError
			/ g.invProportionalGain;					// Apply controller proportional gain factor.

	g.t3.modes = ADJ_OFFSET_SINGLESHOT;					// Adjust the time slew. adjtimex() limits the maximum
	g.t3.offset = timeCorrection;						// correction to about 500 microseconds each second so
	adjtimex(&g.t3);									// it can take up to 20 minutes to start pps-client.

	g.isAcquiring = getAcquireState();					// Allows time to reduce time slew on startup.
	if (g.isAcquiring){

		g.avgCorrection = getAverageCorrection(timeCorrection);

		makeAverageIntegral(g.avgCorrection);			// Constructs an average of integrals of one
														// minute rolling averages of time corrections.
		if (integralIsReady()){							// Get a new frequency offset.

			g.freqOffset = getIntegral()
					* g.integralGain;					// Apply controller integral gain factor.

			long freqCorrection = (long)round(g.freqOffset * ADJTIMEX_SCALE);

			g.t3.modes = ADJ_FREQUENCY;
			g.t3.freq = freqCorrection;
			adjtimex(&g.t3);							// Adjust the system clock frequency.
		}

		if (g.config_select & PPS_OFFSETS){
			recordOffsets(timeCorrection);
		}

		g.activeCount += 1;
	}

	getPPStime(pps_t, timeCorrection);
	return;
}

/**
 * Responds to the SIGTERM signal by starting the exit
 * sequence in the daemon.
 */
void TERMhandler(int sig){
	signal(SIGTERM, SIG_IGN);
	g.exit_requested = true;
	signal(SIGTERM, TERMhandler);
}

/**
 * Catches the SIGHUP signal, causing it to be ignored.
 */
void HUPhandler(int sig){
	signal(SIGHUP, SIG_IGN);
}

/**
 * Logs loss and resumption of the PPS interrupt and if so
 * configured can set a hardware pin HIGH and LOW respectively.
 * Can force exit if the interrupt is lost for more than one
 * hour when "exit-lost-pps=enable" is set in the config file.
 */
int checkPPSInterrupt(int pps_fd){
	int output;

	if (g.seq_num > 0 && g.exit_requested == false){
		if (g.interruptReceived == false){
			g.interruptLossCount += 1;

			if (g.interruptLossCount == INTERRUPT_LOST){
				sprintf(g.logbuf, "WARNING: PPS interrupt lost\n");
				writeToLog(g.logbuf);

				if (g.config_select & ALERT_PPS_LOST){
					output = HIGH;
					write(pps_fd, &output, sizeof(int));
				}
			}
			if (g.exitOnLostPPS &&  g.interruptLossCount >= SECS_PER_HOUR){
				sprintf(g.logbuf, "ERROR: Lost PPS for one hour.");
				writeToLog(g.logbuf);
				return -1;
			}
		}
		else {
			if (g.interruptLossCount >= INTERRUPT_LOST){
				sprintf(g.logbuf, "PPS interrupt resumed\n");
				writeToLog(g.logbuf);

				if (g.config_select & ALERT_PPS_LOST){
					output = LOW;
					write(pps_fd, &output, sizeof(int));
				}
			}
			g.interruptLossCount = 0;
		}
	}

	g.interruptReceived = false;

	return 0;
}

/**
 * Accumulates a distribution of interrupt delay.
 */
void buildInterruptDistrib(int intrptDelay){
	int len = INTRPT_DISTRIB_LEN - 1;
	int idx = intrptDelay;

	if (idx > len){
		idx = len;
	}
	g.interruptDistrib[idx] += 1;
}

/**
 * Accumulates a decaying distribution of interrupt
 * delay that is used to estimate the most probable
 * value of interrupt delay for the purpose of
 * removing burst noise from the interrupt measurement.
 */
void buildInterruptMostDistrib(int intrptDelay){
	int len = INTRPT_DISTRIB_LEN - 1;
	int idx = intrptDelay;

	if (idx > len){
		idx = len;
	}
	g.interruptMostDistrib[idx] += 1.0;

	if (g.interruptCount > 0){
		if (g.interruptCount % 60 == 0){	// Every minute
			int idxOfMost = 0;
			double mostVal = 0.0;

			for (int i = 0; i < len; i++){
				if (g.interruptMostDistrib[i] > mostVal){
					mostVal = g.interruptMostDistrib[i];
					idxOfMost = i;
				}
			}
			g.interruptMost = idxOfMost;

			for (int i = 0; i < len; i++){
				g.interruptMostDistrib[i] *= INTRPT_MOST_DECAY_RATE;
			}
		}
	}
	else {
		g.interruptMost = g.sysDelay;
	}

	g.interruptCount += 1;
}

/**
 * Accumulates a distribution of sysDelay that can
 * be saved to disk for analysis.
 */
void buildSysDelayDistrib(int sysDelay){
	int len = INTRPT_DISTRIB_LEN - 1;
	int idx = sysDelay;

	if (idx > len){
		idx = len;
	}
	g.sysDelayDistrib[idx] += 1;
}

/**
 * When CALIBRATE is enabled, computes the median,
 * g.delayMedian, of the calibration interrupt delay,
 * intrptDelay ignoring jitter bursts.
 *
 * g.delayMedian is rounded to determine the current
 * value of g.sysDelay which is the control point for
 * the controller. By servoing to that value, pps-client
 * sets the time of the PPS rising edge.
 */
void getDelayMedian(int intrptDelay){

	buildInterruptMostDistrib(intrptDelay);

	int diff = 0;

	if ((intrptDelay - g.interruptMost)
			>= INTRPT_BURST_LEVEL){ 			// If intrptDelay is a jitter burst
		return;									// discard the intrptDelay value.
	}

	diff = intrptDelay - g.sysDelay;

	if (diff > 0){
		g.delayMedian += INV_DELAY_SAMPLES_PER_MIN;
	}
	else if (diff < 0){
		g.delayMedian -= INV_DELAY_SAMPLES_PER_MIN;
	}

	g.delay_idx += 1;
	if (g.delay_idx >= g.delayPeriod){
		g.delay_idx = 0;
	}

	if (g.delayCount < g.delayPeriod){
		g.delayCount += 1;
	}

	return;
}

/**
 * Sets a nanosleep() time delay equal to the time remaining
 * in the second from the time recorded as fracSec plus an
 * adjustment value of timeAt in microseconds.
 */
struct timespec setSyncDelay(int timeAt, int fracSec){

	struct timespec ts2;

	int timerVal = USECS_PER_SEC + timeAt - fracSec;

	if (timerVal >= USECS_PER_SEC){
		ts2.tv_sec = 1;
		ts2.tv_nsec = (timerVal - USECS_PER_SEC) * 1000;
	}
	else if (timerVal < 0){
		ts2.tv_sec = 0;
		ts2.tv_nsec = (USECS_PER_SEC + timerVal) * 1000;
	}
	else {
		ts2.tv_sec = 0;
		ts2.tv_nsec = timerVal * 1000;
	}

	return ts2;
}

/**
 * When CALIBRATE is enabled, calculates the time
 * interval between a write to an I/O pin that
 * generates a hardware interrupt and the recognition
 * of that interrupt by the system.
 *
 * Both the write time and the recognition time are
 * read from the pps-client kernel driver approximately
 * each second. The time interval is then calculated
 * along with its median value. The median value,
 * generated with one-minute weighting, is the approximate
 * interrupt delay and is assigned to g.sysDelay.
 */
void getInterruptDelay(int pps_fd){
	ssize_t rv;
	struct timespec rec, rem;

	rec.tv_sec = 0;
	rec.tv_nsec = 100000;						// Wait until the PPS interrupt has occurred to
	nanosleep(&rec, &rem);						// avoid collision with the calibrate interrupt

	int out = 1;
	write(pps_fd, &out, sizeof(int));			// Set the output pin and disable PPS interrupt reads.

	rv = read(pps_fd, (void *)g.tm, 6 * sizeof(int));
	if (rv > 0){

		g.intrptDelay = g.tm[5] - g.tm[3];

		buildInterruptDistrib(g.intrptDelay);

		getDelayMedian(g.intrptDelay);
		g.sysDelay = (int)round(g.delayMedian);

		buildSysDelayDistrib(g.sysDelay);

		if (g.activeCount % CALIBRATE_DISPLAY == 0){
			sprintf(g.msgbuf, "Interrupt delay: %d usec, Delay median: %lf usec  sysDelay: %d usec\n",
					g.intrptDelay, g.delayMedian, g.sysDelay);
			bufferStatusMsg(g.msgbuf);
		}
	}
	else if (rv < 0){
		sprintf(g.logbuf, "pps-client device read() returned: %d Error: %s\n", rv, strerror(errno));
		writeToLog(g.logbuf);
	}

	out = 0;
	write(pps_fd, &out, sizeof(int));		// Reset the output pin and resume PPS interrupt reads.
}

/**
 * Requests a read of the reception time of the PPS hardware
 * interrupt by the pps-client driver and passes the value
 * read to makeTimeCorrection(). The driver is accessed with
 * pps_fd.
 *
 * When read(pps_fd) is called, the driver puts pps-client to
 * sleep until a PPS hardware interrupt has occurred. When it
 * occurs, the driver wakes up pps-client, read(pps_fd) copies
 * the time at which the interrupt was caught by the driver and
 * the value is passed to makeTimeCorrection().
 *
 * The first time pps-client runs, the time slew can be as
 * large as hundreds of milliseconds. When this is the case,
 * limits imposed by adjtimex() prevent changes in offset of
 * more than about 500 microseconds each second. As a result,
 * pps-client will make a partial correction each minute and
 * will restart several times before the slew is small enough
 * that getAcquireState() will set g.isAcquiring to "true".
 * This looping will eventually converge  but can take as
 * long as 20 minutes.
 */
bool readPPS_SetTime(bool verbose, int pps_fd){

	bool restart = false;

	ssize_t rv = read(pps_fd, (void *)g.tm, 2 * sizeof(int));

	g.lostIntr = false;
	if (rv <= 0){
		if (rv == 0 && ! g.exit_requested){
			bufferStatusMsg("Read PPS interrupt failed\n");
		}
		else {
			sprintf(g.logbuf, "pps-client PPS read() returned: %d Error: %s\n", rv, strerror(errno));
			writeToLog(g.logbuf);
		}
		g.lostIntr = true;
	}
	else {
		g.t.tv_sec = g.tm[0];
		g.t.tv_usec = g.tm[1];

		makeTimeCorrection(g.t, pps_fd);

		if ((! g.isAcquiring && g.seq_num > SECS_PER_MINUTE)		// If time slew on startup is too large
				|| (g.isAcquiring && g.hardLimit > HARD_LIMIT_1024	// or if g.avgSlew becomes too large
				&& abs(g.avgSlew) > SLEW_MAX)){						// after acquiring

			sprintf(g.logbuf, "pps-client is restarting...\n");
			writeToLog(g.logbuf);

			initialize(verbose);							// then restart the controller.

			restart = true;
		}
	}

	return restart;
}

/**
 * Runs the one-second wait loop that waits for
 * the PPS hardware interrupt that returns the
 * timestamp of the interrupt which is passed to
 * makeTimeCorrection().
 */
void waitForPPS(bool verbose, int pps_fd){
	struct timeval tv1;
	struct timespec ts2;
	char *configVals[MAX_CONFIGS];
	int timePPS;
	int rv;
	char *pbuf = NULL;
	timeCheckParams tcp;
	int restart = 0;

	pbuf = new char[CONFIG_FILE_SZ];
	initialize(verbose);
	readConfigFile(configVals, pbuf, CONFIG_FILE_SZ);

	rv = allocInitializeSNTPThreads(&tcp);
	if (rv == -1){
		goto end;
	}

	signal(SIGHUP, HUPhandler);			// Handler used to ignore SIGHUP.
	signal(SIGTERM, TERMhandler);		// Handler for the termination signal.

	sprintf(g.logbuf, "pps-client v%s is starting ...\n", version);
	writeToLog(g.logbuf);
										// Set up a one-second delay loop that stays in synch by
	timePPS = 0;		    			// continuously re-timing to the roll-over of the second
	gettimeofday(&tv1, NULL);
	ts2 = setSyncDelay(timePPS, tv1.tv_usec);

	writeStatusStrings();

	for (;;){							// Delay loop
		if (g.exit_requested){
			sprintf(g.logbuf, "pps-client stopped.\n");
			writeToLog(g.logbuf);
			break;
		}

		nanosleep(&ts2, NULL);			// Sleep until ready to look for PPS interrupt

		restart = readPPS_SetTime(verbose, pps_fd);

		if (restart == true) {
			readConfigFile(configVals, pbuf, CONFIG_FILE_SZ);
		}
		else{
			if (checkPPSInterrupt(pps_fd) != 0){
				sprintf(g.logbuf, "Lost PPS. pps-client is exiting.\n");
				writeToLog(g.logbuf);
				break;
			}

			bufferStateParams();

			makeSNTPTimeQuery(&tcp);

			if (! g.lostIntr && ! g.isBurstNoise){
				processFiles(configVals, pbuf, CONFIG_FILE_SZ);
			}
		}

		if ( ! g.isBurstNoise && g.doCalibration
				&& g.hardLimit == HARD_LIMIT_1){

			getInterruptDelay(pps_fd);
		}

		writeStatusStrings();

		g.seconds += 1;
		if (g.seconds == SECS_PER_DAY){
			g.days += 1;
			g.seconds = 0;
		}

		gettimeofday(&tv1, NULL);

		ts2 = setSyncDelay(timePPS, tv1.tv_usec);
	}

end:
	freeSNTPThreads(&tcp);
	if (pbuf != NULL){
		delete pbuf;
	}
	return;
}

/**
 * Creates a detached process that will run as a
 * daemon. Accepts one command line arg: -v that
 * causes the daemon to run in verbose mode which
 * writes a status string and event messages to the
 * terminal once per second.
 *
 * Alternatively, if the daemon is already running,
 * displays a statement to that effect. If the -v
 * command line arg is also used, begins writing a
 * status string that will continue second-by-second
 * until ended by ctrl-c.
 */
int main(int argc, char *argv[])
{
	int rv = 0;
	int ppid, pps_fd;
	bool verbose = false;

	if (argc > 1){
		if (strcmp(argv[1], "-v") == 0){
			verbose = true;
		}
	}

	if (programIsRunning(verbose)){
		return rv;
	}

	pid_t pid = fork();								// Fork a duplicate child of this process.

	if (pid > 0){									// This is the parent process.
		bufferStatusMsg("Spawning pps-client daemon.\n");
		return rv;									// Return from the parent process and leave
	}												// the child running.

	if (pid == -1){									// Error: unable to fork a child from parent,
		sprintf(g.logbuf, "Fork in main() failed: %s\n", strerror(errno));
		writeToLog(g.logbuf);
		return pid;
	}
						// pid == 0 for the child process which now will run this code as a daemon.

	struct sched_param param;						// Process must be run as root
	param.sched_priority = 99;						// to get real-time priority.
	sched_setscheduler(0, SCHED_FIFO, &param);		// Otherwise, this has no effect.

	if (waitForNTPServers() <= 0){					// Make sure NTP has accessible servers
		goto end0;									// If not, must exit.
	}

	if (driver_load() == -1){
		sprintf(g.logbuf, "Could not load pps-client driver. Exiting.\n");
		writeToLog(g.logbuf);
		rv = -1;
		goto end0;
	}

	rv = disableNTP();
	if (rv != 0){
		goto end1;
	}

	ppid = createPIDfile();							// Create the PID file for this process.
	if (ppid == -1){								// Either already running or could not
		rv = -1;									// create a pid file. In either case, exit.
		goto end1;
	}

	pps_fd = open_logerr("/dev/pps-client", O_RDWR);// Open the pps-client device driver.
	if (pps_fd == -1){
		rv = -1;
		goto end2;
	}

	sprintf(g.msgbuf, "Process PID: %d\n", ppid);	// PPS client is starting.
	bufferStatusMsg(g.msgbuf);

	waitForPPS(verbose, pps_fd);					// Synchronize to the PPS.

	close(pps_fd);									// Close the interrupt device driver.

	sprintf(g.logbuf, "pps-client exiting.\n");
	writeToLog(g.logbuf);
end2:
	system("rm /var/run/pps-client.pid");			// Remove PID file with system() which blocks until
end1:												// rm completes keeping shutdown correcty sequenced.
	enableNTP();

	sleep(5);										// Wait for the driver to close.
	driver_unload();								// Driver is unloaded last to avoid system
end0:												// inability to unload it because the driver
	return rv;										// is still active.
}


