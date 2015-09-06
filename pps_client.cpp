/*
 * pps_client.cpp - Client for a PPS source synchronized to
 * time of day
 *
 * Created on: Sept 1, 2015
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
 * This daemon synchronizes the system clock to a Pulse-Per-
 * Second (PPS) source. Athough the Linux pps-gpio module can
 * provide an ATOM PPS clock from a PPS source to the ntpd
 * daemon which will synchronize the system clock to the ATOM
 * PPS clock, NTP synchronization is slow and does not take
 * adequate advantage of the sample per second rate available
 * from a PPS source. To get better performance this client,
 * which does not require ATOM support, synchronizes the system
 * clock to a PPS signal by providing offset corrections
 * every second and frequency corrections every minute. This
 * keeps the system clock continuously synchronized to within
 * microseconds of the PPS source.
 *
 * A wired GPIO connection is required from a PPS source.
 * Synchronization is provided by the rising edge of that
 * PPS source which is connected to GPIO 18.
 *
 * The executeable for this daemon is in /usr/sbin/pps-client.
 * The daemon script is /etc/init.d/pps-client.
 *
 * The daemon can start, stop or restart pps-client using the
 * service command.  Also, by using the -v option, the service
 * command can be used to display second by second the clock
 * time that was captured at the rising edge of the PPS signal
 * (with processing delay subtracted out):
 *
 * ~$ sudo service pps-client stop
 * ~$ sudo service pps-client start -v
 *
 * Also shown is the sequence number, the time offset recorded
 * each second, the clock frequency offset in parts per million
 * over the last minute and the minimum and maximum time
 * offsets in microseconds that have occurred since the client
 * started, respectively. Although the clock frequency error
 * is typically less than one part per million in any one minute
 * interval which could potentially result in 60 microseconds drift
 * over one minute, the time drift that does occur is continuously
 * corrected second by second to within a few microseconds.
 *
 * To assess the quality of the time tracking this client saves its
 * own statistics. The offset-distrib file in the /var/local directory
 * contains the cumulative distribution of time offset corrections
 * sent to the kernel. The frequency-vars file contains a record over
 * the last 24 hours of the average clock frequency offset that was
 * reported by the kernal in each 5 minute interval and the maximum
 * clock frequency change that occurred in that interval, both in
 * parts per million. These values are labeled with the timestamp
 * (in seconds since epoch) of the second when they were recorded.
 *
 * To stop the second-by-second display:
 *
 * ~$ sudo service pps-client restart
 *
 * After a restart it is also possible to get the second by
 * second display with:
 *
 * ~$ ps -C pps-client
 *
 * which returns the PID value pid_val and then
 *
 * ~$ sudo kill -s SIGINT pid_val
 *
 * The restart is reqired in order to get the process attached
 * to the local terminal so it can receive terminal commands.
 * The restart is only necessary one time after the system has
 * been restarted. To stop the continuous display just repeat
 * the last command. (WARNING: If the terminal is remote as,
 * for example, over an ssh connection and that connection is
 * lost before it can be exited, the control of the pps-client
 * using the kill command is also permanently lost. There are
 * ways around this problem but special programs are required.)
 *
 * Operation of the daemon can also be verified less intrusively
 * by reading the saved timestamp of the last PPS rising edge:
 *
 * ~$ cat /var/local/pps-assert
 *
 * which also displays the sequence number of the PPS. This
 * is also the number of seconds the daemon has been running.
 *
 * It is important that time server(s) for NTP that are listed
 * in /etc/ntp.conf MUST be up and running before pps-client is
 * started because NTP will set the local time to those servers
 * only at boot. Thereafter the NTP correction loop is disabled
 * and time correction is maintained entirely against the PPS
 * signal. Synchronization with the NTP time servers can be
 * verified only while ntpd is running. The ntpd daemon is NOT
 * normally running if pps-client is enabled. In that case force
 * an update with
 *
 * ~$ sudo service pps-client stop
 * ~$ sudo service ntp stop
 * ~$ sudo ntpd -gq
 * ~$ sudo service ntp start
 *
 * and then verfy NTP with
 *
 * ~$ ntpq -p
 *
 * That will produce a display that looks something like:
 *
 *      remote           refid      st t when poll reach   delay   offset  jitter
 * ==============================================================================
 * *vimo.dorui.net  209.51.161.238   2 u  332 1024  377   52.079    2.245   3.498
 * -dosaku.ctipc.co 216.218.254.202  2 u  720 1024  377   72.866   -5.779   1.381
 * +jtsage.com      200.98.196.212   2 u  585 1024  377   67.993   -1.507   3.741
 * -mx-a.smith.is   198.55.111.5     3 u  396 1024  377   91.188   -4.016  58.571
 * +golem.canonical 193.79.237.14    2 u  411 1024  377  145.732    0.522   1.161
 *
 * Note that the offset of the active server (marked with '*')
 * should be on the order of no more than several millisconds to
 * possibly 10 or 20 milliseconds as shown above. A very large
 * offset indicates that time synchronization has been lost.This
 * situation rarely occurs because the forced update described
 * above is automatically perfomed by pps-client when it starts
 * up. In the exceptional case, the solution is to stop pps-client
 * and force an NTP update as above. Ater verifying that sync has
 * been restored with
 *
 * ~$ ntpq -p
 *
 * then start pps-client again:

 * ~$ sudo service pps-client start
 *
 * In general no special settings are required in the /etc/ntp.conf
 * file.
 */

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
#include "/usr/local/include/wiringPi.h"

#define GPIO1 1										// wiringPi GPIO definition
#define HALF_SECOND 500000
#define ONE_SECOND 1000000
#define NORM (1.0 / 60.0)
#define INV_FREQ_SCALE (1.0 / 65536.0)				// The kernal reports frequency scaled by 65536. This removes the scaling.

#define WRITE_DATE 1
#define NO_DATE 0

#define INV_GAIN 4
#define GAIN (1.0 / (double)INV_GAIN)				// System clock proportional-integral (PI) controller gain factor
#define SCALED_GAIN (GAIN * 65536.0)

struct timeval t;
struct timeval t0;
struct timeval t1;
struct timex t3;
int usecOffset = 0;
int offsetCorrection = 0;

int seq_num = 0;
int sysDelay = 50;									// Estimated interrupt average processing delay in microseconds.
													// Minimum negative offset ever recorded : -26
int offsetAccum = 0;
int fifoCount = 0;

int fifo_buf[60];
int fifo_idx = 0;

double lastFreq = 0.0;
double avgOffset = 0.0;
int minOffset = 0;
int maxOffset = 0;

double clockFrequencyOffset = 0.0;

double freqOffsetSum = 0.0;
double minFrequency = 1e6;
double maxFrequency = -1e6;
int maxInterval = 5;
int intervalCount = 0;
double maxfreqChange[288];
double freqOffsetRec[288];
__time_t timestampRec[288];
int recIndex = 0;

bool skipCorrection = false;
bool isInitialized = false;

int showGlitch = 0;

unsigned int offsetDistrib[101];

char pps_buf[60];
char log_buf[100];
char time_buf[100];

useconds_t sampleIntvl = 300000000;							// Initializing sample interval.

bool verbose = false;
bool exit_requested = false;
const char *assert_file = "/var/local/pps-assert";			// File storing current PPS timestamp
const char *distrib_file = "/var/local/offset-distrib";		// File storing the distribution of offset corrections.
const char *frequency_file = "/var/local/frequency-vars";	// File storing clock frequency offset and change at 5 minute intervals
const char *log_file = "/var/log/pps-client.log";			// File storing activity and errors.

/**
 * Writes to the log file.
 */
void writeToLog(bool writeDate){
	int fd = open(log_file, O_CREAT | O_WRONLY | O_APPEND);
	if (writeDate){
		time_t t = time(NULL);
		struct tm *tmp = localtime(&t);
		strftime(time_buf, sizeof(time_buf), "%F %H:%M:%S ", tmp);
		write(fd, time_buf, strlen(time_buf));
	}
	write(fd, log_buf, strlen(log_buf));
	close(fd);
}

/**
 * Writes the last 24 hours of clock frequency offset and max
 * frequency change suring each 5 minute interval indexed by
 * timestamp at each 5 minute interval.
 */
void writeFrequencyVars(void){
	char buf[100];

	remove(frequency_file);
	int fd = open(frequency_file, O_CREAT | O_WRONLY | O_APPEND);

	for (int i = 0; i < 288; i++){
		int j = recIndex + i;
		if (j >= 288){
			j -= 288;
		}
		sprintf(buf, "%ld %lf %lf\n", timestampRec[j], freqOffsetRec[j], maxfreqChange[j]);
		write(fd, buf, strlen(buf));
	}

	close(fd);
}

/**
 * Records the clock frequency offset and max
 * frequency change reported by the linux kernel
 * in each 5 minute interval with these values
 * indexed by a timestamp in a set of circular
 * buffers with a length of 24 hours. This
 * function is called once each minute but
 * only writes to a file every 5 minutes.
 */
void recordFrequencyVars(void){
	freqOffsetSum += clockFrequencyOffset;

	if (clockFrequencyOffset < minFrequency){
		minFrequency = clockFrequencyOffset;
	}
	if (clockFrequencyOffset > maxFrequency){
		maxFrequency = clockFrequencyOffset;
	}

	intervalCount += 1;
	if (intervalCount == maxInterval){
		maxfreqChange[recIndex] = maxFrequency - minFrequency;

		freqOffsetRec[recIndex] = freqOffsetSum * 0.2;
		timestampRec[recIndex] = t.tv_sec;

		recIndex += 1;
		if (recIndex == 288){
			recIndex = 0;
		}

		writeFrequencyVars();

		intervalCount = 0;
		freqOffsetSum = 0.0;
		minFrequency = 1e6;
		maxFrequency = -1e6;
	}
}

/**
 * Constructs a distribution of offset correction values with
 * zero offset at index 50.
 */
void buildDistrib(int offset){
	int idx = offset + 50;
	if (idx < 0){
		idx = 0;
	}
	else if (idx > 100){
		idx = 100;
	}
	offsetDistrib[idx] += 1;
}

/**
 * Writes the accumulated usecOffset distribution to disk
 * once each minute.
 */
void writeDistribFile(void){
	char buf[50];

	remove(distrib_file);
	int fd = open(distrib_file, O_CREAT | O_WRONLY | O_APPEND);

	for (int i = 0; i < 101; i++){
		sprintf(buf, "%d %d\n", i-50, offsetDistrib[i]);
		write(fd, buf, strlen(buf));
	}

	close(fd);
}

/**
 * Maintains offsetAccum which contains a second-by-second
 * running sum of the last minute of offset corrections.
 */
void fillFifo(int inVal){

	offsetAccum += inVal;

	if (fifoCount == 60){
		int outVal = fifo_buf[fifo_idx];
		offsetAccum -= outVal;
	}

	if (fifoCount < 60){
		fifoCount += 1;
	}

	fifo_buf[fifo_idx] = inVal;
	fifo_idx += 1;
	if (fifo_idx == 60){
		fifo_idx = 0;
	}
}

/**
 * Responds to the PPS hardware interrupt.
 */
void PPShandler(void){
	gettimeofday(&t, NULL);							// Get time at rising edge of PPS. This time record is
													// delayed by the interrupt response latency.
	seq_num += 1;

	usecOffset = t.tv_usec - sysDelay;				// Update usecOffset with time corrected by estimated processor latency.
	if (t.tv_usec > 500000){						// If sysDelay is too short, could get a negative value
		usecOffset -= 1000000;						// so correct for that.
	}

	if (isInitialized && showGlitch == 0 &&			// Guard against occasional extreme offsets that can corrupt
			(usecOffset > 100 || usecOffset < -100)){	// offset and frequency corrections. These occur infrequently
		skipCorrection = true;						// but have seen a few. Largest recorded: 458340 microsecs!

		showGlitch = 10;							// Interval during which to display the aftermath of the glitch.

		sprintf(log_buf, "PPS_Client received a miss-timed interrupt\n");
		writeToLog(WRITE_DATE);
		sprintf(log_buf, "    causing the next clock offset correction to be ignored.\n");
		writeToLog(NO_DATE);

		verbose = false;							// Stop verbose display after displaying the effect of the
	}												// glitch. This is overridden during the showGlitch interval.
	else {
		skipCorrection = false;

		if (showGlitch > 0){						// Display will stop after showGlitch has reached zero.
			showGlitch -= 1;
		}
	}

	if (skipCorrection == false){
		t1.tv_sec = 0;
		offsetCorrection = usecOffset / INV_GAIN;	// This provides proportional feedback to the system clock PI controller.
		t1.tv_usec = -offsetCorrection;
		adjtime(&t1, NULL);							// Adjust offset on each interrupt. For values on the order of less
	}												// than a few hundred microseconds, this is the actual kernel adjustment.

	int usec = usecOffset;
	int sec = t.tv_sec;
	if (usec < 0){
		usec = 1000000 + usec;
		sec -= 1;
	}
	double timestamp = sec + usec * 1e-6;

	if (skipCorrection == false && seq_num > 100){	// Allow time for the second-by-second correction to settle

		if (usecOffset < minOffset){
			minOffset = usecOffset;					// Record minimum offset
		}
		else if (usecOffset > maxOffset){
			maxOffset = usecOffset;					// Record maximum offset
		}

		buildDistrib(offsetCorrection);
		fillFifo(usecOffset);

		avgOffset = (double)offsetAccum * NORM;

		if (avgOffset < 1.0 && avgOffset > -1.0)
			isInitialized = true;					// The first successful frequency correction has occurred.

		if (fifoCount == 60 && (fifo_idx == 0)){	// Adjust frequency from the average offset over the last 60 counts.
													// freqOffset is the integrator providing the integral feedback to the
			double freqOffset = lastFreq - SCALED_GAIN * avgOffset;	// system clock PI controller.
			lastFreq = freqOffset;					// Save the current freqOffset

			t3.modes = ADJ_FREQUENCY;
			t3.freq = (long)(freqOffset + 0.5);
			adjtimex(&t3);

			t3.modes = 0;							// Get the clock frequency offset reported by the linux kernel.
			adjtimex(&t3);
			clockFrequencyOffset = (double)t3.freq * INV_FREQ_SCALE;

			recordFrequencyVars();
			writeDistribFile();
		}
	}

	if (verbose == true){							// Go verbose if verbose is true
		printf("%f %d offset: %d freqOffsetRec: %lf minOffset: %d maxOffset: %d\n",
				timestamp, seq_num, usecOffset, clockFrequencyOffset, minOffset, maxOffset);
	}

	if (showGlitch > 0){							// Write to log file while showGlitch > 0
		sprintf(log_buf, "%f %d offset: %d freqOffset: %lf minOffset: %d maxOffset: %d\n",
				timestamp, seq_num, usecOffset, clockFrequencyOffset, minOffset, maxOffset);
		writeToLog(NO_DATE);
	}

	memset(pps_buf, 0, 60 * sizeof(char));
	sprintf(pps_buf, "%lf#%d\n", timestamp, seq_num);
	remove(assert_file);
	int pfd = open(assert_file, O_CREAT | O_WRONLY);
	write(pfd, pps_buf, strlen(pps_buf));			// Write PPS timestamp to assert_file
	close(pfd);
}

/**
 * Responds to the SIGINT signal by toggling verbose display.
 */
void INThandler(int sig){
	if (verbose == true){
		verbose = false;
	}
	else {
		verbose = true;
	}
}

/**
 * Responds to the SIGTERM signal by starting the exit sequence.
 */
void TERMhandler(int sig){
	signal(SIGTERM, SIG_IGN);
	exit_requested = true;
	signal(SIGTERM, TERMhandler);
}

/**
 * Catches the SIGHUP signal, causing it to be ignored.
 */
void HUPhandler(int sig){
	signal(SIGHUP, SIG_IGN);
}

/**
 * Runs the delay loop that waits for the PPS hardware interrupt.
 */
void pps_sync_client(void){
	struct timeval t1;
	struct timespec t2;

	signal(SIGHUP, HUPhandler);						// Install handler to ignore SIGHUP.
	signal(SIGTERM, TERMhandler);
	signal(SIGINT, INThandler);

	memset(&t, 0, sizeof(struct timex));
	t3.modes = ADJ_FREQUENCY;
	adjtimex(&t3);									// Initialize system clock frequency and offset.

	seq_num = 0;

	gettimeofday(&t1, NULL);

	remove(assert_file);

	sprintf(log_buf, "PPS client starting ...\n");
	writeToLog(WRITE_DATE);

	t2.tv_sec = 0;
	t2.tv_nsec = sampleIntvl;

	memset(pps_buf, 0, 60 * sizeof(char));

	memset(fifo_buf, 0, 60 * sizeof(int));

	memset(offsetDistrib, 0, 301 * sizeof(int));

						// Set up a one second delay loop that stays in synch with wallclock time
						// by continuously re-timeing about 1 msec before the second rolls over.
	for (;;){
		nanosleep(&t2, NULL);

		if (exit_requested){
			remove("/var/run/pps-client.pid");
			sprintf(log_buf, "PPS client stopped.\n");
			writeToLog(WRITE_DATE);
			return;
		}

		gettimeofday(&t1, NULL);

		if (t1.tv_usec < 999000){
			t2.tv_sec = 1;
			t2.tv_nsec = (999000 - t1.tv_usec) * 1000;
		}
		else {
			t2.tv_sec = 0;
			t2.tv_nsec = (ONE_SECOND - t1.tv_usec + 999000) * 1000;
		}
	}
}

/**
 * Creates a daemon PID file for pps-client.
 */
int createPIDfile(void){
	int pfd = open("/var/run/pps-client.pid", O_RDWR | O_CREAT | O_EXCL);
	if (pfd == -1)
		return -1;									// Already running.

	pid_t ppid = getpid();

	char buf[20];
	sprintf(buf, "%d\n", ppid);
	if (write(pfd, buf, strlen(buf)) == -1)
	{
		close(pfd);
		return -1;									// Write failed.
	}

	fchmod(pfd, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
	close(pfd);

	return 0;										// OK
}

/**
 * Reads the PID of the child process when
 * the parent process needs to kill it.
 */
pid_t getChildPID(void){
	pid_t pid = 0;

	char buf[20];
	memset(buf, 0, 20 * sizeof(char));

	int pfd = open("/var/run/pps-client.pid", O_RDONLY);
	read(pfd, buf, 19);
	sscanf(buf, "%d\n", &pid);
	close(pfd);
	if (pid > 0){
		return pid;
	}
	return -1;
}

int main(int argc, char *argv[])
{
	pid_t pid = fork();								// Fork a duplicate child of this process.

	if (pid > 0){									// This is the parent process.

		sleep(10);									// On boot, allow time for NTP to start and child PID file to be created.

		int rv = system("service ntp stop");		// Stop NTP,
		if (rv != 0){
			sprintf(log_buf, "\'service ntp stop\' failed! Is NTP installed?\n");
			writeToLog(WRITE_DATE);
			kill(getChildPID(), SIGTERM);
			return rv;
		}

		rv = system("ntpd -gq");					// Force NTP to update the time but from here on NTP is no longer running.
		if (rv != 0){
			sprintf(log_buf, "\'ntpd -gq\' failed!\n");
			writeToLog(WRITE_DATE);
			kill(getChildPID(), SIGTERM);
			return rv;
		}
		return rv;									// Return from the parent process and leave the child running.
	}

	if (pid < 0){									// Error: unable to fork a child from parent,
		sprintf(log_buf, "Fork in main() failed.\n");
		writeToLog(WRITE_DATE);
		return pid;									// so return the error code.
	}
						// pid == 0 for the child process which now will run this code as a daemon.
	verbose = false;
	if (argc > 1){
		if (strcmp(argv[1], "-v") == 0){
			verbose = true;
		}
	}

	if (createPIDfile() == -1){						// Create the PID file for this process
		return -1;
	}

	struct sched_param param;						// Process must be run as root
	param.sched_priority = 99;						// to get real-time priority.
	sched_setscheduler(0, SCHED_FIFO, &param);		// Else this has no effect.

	wiringPiSetup();
	pinMode(GPIO1, INPUT);							// RPi header pin 12 for input.
	wiringPiISR (GPIO1, INT_EDGE_RISING,  PPShandler);

	pps_sync_client();								// Synchronize to PPS.

    return 0;
}
