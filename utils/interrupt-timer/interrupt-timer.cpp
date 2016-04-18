/*
 * interrupt-timer.cpp
 *
 * Created on: Apr 7, 2016
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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sched.h>
#include <math.h>

#define INTRPT_DISTRIB_LEN 61
#define SECS_PER_DAY 86400
#define SECS_PER_MIN 60
#define ON_TIME 0
#define DELAYED 1
#define SENTINEL 2
#define FILE_NOT_FOUND -1
#define HIDDEN_DELAY 1
#define FILEBUF_SZ 5000
#define STRBUF_SZ 200

struct interruptTimerGlobalVars {
	int outFormat;

	char strbuf[STRBUF_SZ];
	char filebuf[FILEBUF_SZ];
	int seconds;
	int minutes;
	int days;

	int scaleCenter;

	int intrptCount;
	int interruptDistrib[INTRPT_DISTRIB_LEN];

	int lastIntrptFileno;

	double probability;
} g;

const char *interrupt_distrib_file = "/var/local/intrpt-distrib-forming";
const char *last_intrpt_distrib_file = "/var/local/intrpt-distrib";
const char *pulse_verify_file = "/mnt/pi/PulseVerify";

const char *version = "interrupt-timer v0.1.0";
const char *timefmt = "%F %H:%M:%S";

/**
 * Reads the major number assigned to interrupt-timer
 * from "/proc/devices" as a string which is
 * returned in the majorPos char pointer. This
 * value is used to load the hardware driver that
 * interrupt-timer requires.
 */
char *copyMajorTo(char *majorPos){

	struct stat stat_buf;

	const char *filename = "/run/shm/proc_devices";

	system("cat /proc/devices > /run/shm/proc_devices"); 	// "/proc/devices" can't be handled like
															// a normal file so we copy it to a file.
	int fd = open(filename, O_RDONLY);
	if (fd == -1){
		return NULL;
	}

	fstat(fd, &stat_buf);
	int sz = stat_buf.st_size;

	char *fbuf = new char[sz+1];

	int rv = read(fd, fbuf, sz);
	if (rv == -1){
		close(fd);
		remove(filename);
		delete(fbuf);
		return NULL;
	}
	close(fd);
	remove(filename);

	fbuf[sz] = '\0';

	char *pos = strstr(fbuf, "interrupt-timer");
	if (pos == NULL){
		printf("Can't find interrupt-timer in \"/run/shm/proc_devices\"\n");
		delete fbuf;
		return NULL;
	}
	char *end = pos - 1;
	*end = 0;

	pos -= 2;
	char *pos2 = pos;
	while (pos2 == pos){
		pos -= 1;
		pos2 = strpbrk(pos,"0123456789");
	}
	strcpy(majorPos, pos2);

	delete fbuf;
	return majorPos;
}

/**
 * Loads the hardware driver required by interrupt-timer which
 * is expected to be available in the file:
 * "/lib/modules/'uname -r'/kernel/drivers/misc/interrupt-timer.ko".
 */
int driver_load(char *gpio){

	memset(g.strbuf, 0, STRBUF_SZ * sizeof(char));

	char *insmod = g.strbuf;
	strcpy(insmod, "/sbin/insmod /lib/modules/`uname -r`/kernel/drivers/misc/interrupt-timer.ko gpio_num=");
	strcat(insmod, gpio);

	system("rm -f /dev/interrupt-timer");			// Clean up any old device files.

	system(insmod);									// Issue the insmod command

	char *mknod = g.strbuf;
	strcpy(mknod, "mknod /dev/interrupt-timer c ");
	char *major = copyMajorTo(mknod + strlen(mknod));
	if (major == NULL){								// No major found! insmod failed.
		printf("driver_load() error: No major found!\n");
		system("/sbin/rmmod interrupt-timer");
		return -1;
	}
	strcat(mknod, " 0");

	system(mknod);									// Issue the mknod command

	system("chgrp root /dev/interrupt-timer");
	system("chmod 664 /dev/interrupt-timer");

	return 0;
}

/**
 * Unloads the interrupt-timer kernel driver.
 */
void driver_unload(void){
	system("/sbin/rmmod interrupt-timer");
	system("rm -f /dev/interrupt-timer");
}

/**
 * Writes an accumulating statistical distribution at regular intervals
 * to disk and rolls over the accumulating data to a new file every
 * epochInterval days and begins a new distribution file.
 */
void writeDistribution(int distrib[], int len, int scaleZero, int epochInterval,
		int *last_epoch, const char *distrib_file, const char *last_distrib_file){

	remove(distrib_file);
	int fd = open(distrib_file, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR | S_IROTH);
	if (fd == -1){
		return;
	}
	for (int i = 0; i < len; i++){
		sprintf(g.strbuf, "%d %d\n", i-scaleZero, distrib[i]);
		write(fd, g.strbuf, strlen(g.strbuf));
	}
	close(fd);

	int epoch = g.days / epochInterval;
	if (epoch != *last_epoch ){
		*last_epoch = epoch;
		remove(last_distrib_file);
		rename(distrib_file, last_distrib_file);
		memset(distrib, 0, len * sizeof(int));
	}
}

/**
 * Writes a distribution to disk containing 60 additional
 * interrupt delays approximately every minute. Collects
 * one day of interrupt delay samples before rolling over
 * a new file.
 */
void writeInterruptDistribFile(void){
	int scaleZero = -(g.scaleCenter - (INTRPT_DISTRIB_LEN - 1) / 2);
	writeDistribution(g.interruptDistrib, INTRPT_DISTRIB_LEN, scaleZero, 1,
			&g.lastIntrptFileno, interrupt_distrib_file, last_intrpt_distrib_file);
}

/**
 * Accumulates a distribution of interrupt delay.
 */
void buildInterruptDistrib(int intrptDelay){
	int len = INTRPT_DISTRIB_LEN - 1;
	int idx;

	if (g.intrptCount == 0){
		g.scaleCenter = intrptDelay;
		g.intrptCount += 1;
		return;
	}
	else if (g.intrptCount < 60){				// During the first 60 counts get a rough scale center.
		if (intrptDelay > g.scaleCenter){
			g.scaleCenter += 1;
		}
		else if (intrptDelay < g.scaleCenter){
			g.scaleCenter -= 1;
		}
		g.intrptCount += 1;
		return;
	}

	idx = intrptDelay - g.scaleCenter;			// Normalize idx to the scale center.
												// Since that normalizes it to zero,
	idx = idx + len / 3;						// offset idx to the lower third of
												// the g.interruptDistrib array.
	if (idx > len){
		idx = len;
	}
	else if (idx < 0){
		idx = 0;
	}

	g.intrptCount += 1;
	g.interruptDistrib[idx] += 1;
}

/**
 * Read the sysDelay value recorded by
 * pps-client.
 */
int getSysDelay(int *sysDelay){

	int delay_fd = open("/run/shm/pps-sysDelay", O_RDONLY);
	if (delay_fd == -1){
		printf("Error: pps-client is not running.\n");
		return -1;
	}
	read(delay_fd, g.strbuf, 50);
	g.strbuf[50] = '\0';
	close(delay_fd);

	char *end = strchr(g.strbuf, '#');
	end[0] = '\0';
	sscanf(g.strbuf, "%d", sysDelay);
	return 0;
}

int readVerify(void){
	int val;
	int fd = open(pulse_verify_file, O_RDONLY);
	if (fd == -1){
		return fd;
	}
	read(fd, &val, sizeof(int));
	close(fd);
	return val;
}

int outputSingeEventTime(int tm[]){
	char timeStr[50];

	int fd = open(last_intrpt_distrib_file, O_RDONLY);
	if (fd == -1){
		strcpy(g.strbuf, "File not found: ");
		strcat(g.strbuf, last_intrpt_distrib_file);
		strcat(g.strbuf, "\n");
		printf(g.strbuf);
		return -1;
	}

	int sz = FILEBUF_SZ;
	int len = INTRPT_DISTRIB_LEN;
	double tmp;

	read(fd, (void *)g.filebuf, sz);
	close(fd);

	char *lines[len];
	double probDistrib[len];

	lines[0] = strtok(filebuf, "\r\n\0");
	for (int i = 1; i < len; i++){
		lines[i] = strtok(NULL, "\r\n\0");
	}

	double sum = 0.0;
	for (int i = 0; i < len; i++){
		sscanf(lines[i], "%lf %lf", &tmp, probDistrib + i);
		sum += probDistrib[i];
	}
	double norm = 1.0 / sum;
	for (int i = 0; i < len; i++){
		probDistrib[i] *= norm;
	}

	int lowIdx, maxIdx = 0, hiIdx;
	double maxVal = 0.0;
	for (int i = 0; i < len; i++){
		if (probDistrib[i] > maxVal){
			maxVal = probDistrib[i];
			maxIdx = i;
		}
	}
	double tailProb = 0.5 * (1.0 - g.probability);

	double tailSum = 0.0;
	for (int i = 0; i < len; i++){
		tailSum += probDistrib[i];
		if (tailSum > tailProb){
			lowIdx = i - 1;
			break;
		}
	}
	tailSum = 0.0;
	for (int i = len - 1; i >= 0; i--){
		tailSum += probDistrib[i];
		if (tailSum > tailProb){
			hiIdx = i + 1;
			break;
		}
	}

	int lowVal = tm[1] - (hiIdx - maxIdx);
	int hiVal = tm[1] + (maxIdx - lowIdx);

	double mean = 0.5 * (double)(lowVal + hiVal);
	int meanVal = (int)round(mean);

	if (meanVal > 1000000){
		tm[1] = meanVal - 1000000;
		tm[0] += 1;
	}
	else if (meanVal < 0){
		tm[1] = 1000000 + meanVal;
		tm[0] -= 1;
	}
	else {
		tm[1] = meanVal;
	}

	double tol = (double)hiVal - meanVal;
	int tolerance = (int)round(tol);

	if (g.outFormat == 0){						// Print in date-time format
		strftime(timeStr, 50, timefmt, localtime((const time_t*)(&tm[0])));
		printf("%s.%06d +/- 0.%06d\n", timeStr, tm[1], tolerance);
	}
	else {										// Print as seconds
		double time = (double)tm[0] + 1e-6 * tm[1];
		printf("%lf +/- 0.%06d\n", time, tolerance);
	}

	return 0;
}

int outputRepeatingEventTime(int tm[]){
	char timeStr[50];

	int v = readVerify();
	if (v == ON_TIME){
		buildInterruptDistrib(tm[1]);
	}
	else if (v == DELAYED){
		printf("interrupt-timer: Skipping delayed pulse from pulse-generator.\n");
	}
	else if (v == SENTINEL){
		printf("interrupt-timer: Bad read on verify. Read too late.\n");
	}
	else if (v == FILE_NOT_FOUND){
		printf("interrupt-timer Error: Verify file not found.\n");
		return -1;
	}

	if (g.outFormat == 0){						// Print in date-time format
		strftime(timeStr, 50, timefmt, localtime((const time_t*)(&tm[0])));
		printf("%s.%06d\n", timeStr, tm[1]);
	}
	else {										// Print as seconds
		double time = (double)tm[0] + 1e-6 * tm[1];
		printf("%lf\n", time);
	}

	return 0;
}

int main(int argc, char *argv[]){

	int tm[2];
	int sysDelay;
	bool singleEvent = false;
	bool argRecognized = false;

	if (argc > 1){
		if (strcmp(argv[1], "load-driver") == 0){
			if (geteuid() != 0){
				printf("Requires superuser privileges. Please sudo this command.\n");
				return 0;
			}
			if (argv[2] == NULL){
				printf("GPIO number is a required second arg.\n");
				printf("Could not load driver.\n");
				return 0;
			}
			if (driver_load(argv[2]) == -1){
				printf("Could not load interrupt-timer driver. Exiting.\n");
				return 1;
			}

			printf("interrupt-timer: driver loaded\n");
			return 0;
		}
		if (strcmp(argv[1], "unload-driver") == 0){
			if (geteuid() != 0){
				printf("Requires superuser privileges. Please sudo this command.\n");
				return 0;
			}
			printf("interrupt-timer: driver unloading\n");
			driver_unload();
			return 0;
		}

		for (int i = 1; i < argc; i++){
			if (strcmp(argv[i], "-s") == 0){
				g.outFormat = 1;
				argRecognized = true;
				continue;
			}
			if (strcmp(argv[i], "-p") == 0){

				char *num = strpbrk(argv[i+1], "0123456789.");
				if (num == NULL){
					printf("-p must be followed by the probability that the timed\n");
					printf("event is within the tolerance that will be estimated.\n");
					return 0;
				}
				scanf(argv[2], "%lf", &g.probability);
				if (g.probability == 0){
					printf("A positive non-zero probability is required\n");
					return 0;
				}
				if (g.probability > 0.999){
					g.probability = 0.999;
				}
				singleEvent = true;
				argRecognized = true;
				continue;
			}
		}

		if (argRecognized){
			goto start;
		}

		printf("Usage:\n");
		printf("  sudo interrupt-timer load-driver <gpio-number>\n");
		printf("  sudo interrupt-timer unload-driver\n");
		printf("where gpio-number is the GPIO of the pin on which\n");
		printf("the interrupt will be captured.\n");
		printf("After loading the driver, calling interrupt-timer\n");
		printf("causes it to wait for interrupts then output the\n");
		printf("date-time when each occurs. The following command\n");
		printf("arg modifies the format of the date-time output:\n\n");
		printf("  -s Outputs seconds since the epoch.\n");
		printf("otherwise outputs in date format (default).\n\n");
		printf("The program will exit on ctrl-c or when no\n");
		printf("interrupts are received within 5 minutes.\n");
		printf("Specifying a probability causes interrupt-timer to\n");
		printf("function as a single event timer that outputs both\n");
		printf("an event time and an estimated tolerance on that time:\n");
		printf("  -p <probability>\n");
		printf("where that is the probability (<= 0.999) that the time is within\n");
		printf("the estimated tolerance.\n\n");

		return 0;
	}

start:
	if (geteuid() != 0){
		printf("Requires superuser privileges. Please sudo this command.\n");
		return 0;
	}

	remove(pulse_verify_file);

	printf(version);
	printf("\n");

	memset(&g, 0, sizeof(struct interruptTimerGlobalVars));

	struct sched_param param;								// Process must be run as
	param.sched_priority = 99;								// root to change priority.
	sched_setscheduler(0, SCHED_FIFO, &param);				// Else, this has no effect.

	int intrpt_fd = open("/dev/interrupt-timer", O_RDONLY); // Open the interrupt-timer device driver.
	if (intrpt_fd == -1){
		printf("interrupt-timer: Driver is not loaded. Exiting.\n");
		return 1;
	}

	struct timespec ts2;

	for (;;){
		int dvrv = read(intrpt_fd, (void *)tm, 2 * sizeof(int));
		if (dvrv > 0){
			int rv = getSysDelay(&sysDelay);
			if (rv == -1){
				return 1;
			}
			tm[1] -= (sysDelay + HIDDEN_DELAY);				// time microseconds corrected for system interrupt sysDelay
			if (tm[1] < 0){									// If negative after correction, fix the time.
				tm[1] = 1000000 + tm[1];
				tm[0] -= 1;
			}

			if (singleEvent == false){

				ts2.tv_sec = 0;								// Allow time for pulse-generator to write the verify file.
				ts2.tv_nsec = 100000;
				nanosleep(&ts2, NULL);

				rv = outputRepeatingEventTime(tm);
				if (rv == -1){
					return 1;
				}
			}
			else {
				rv = outputSingeEventTime(tm);
				if (rv == -1){
					return 1;
				}
			}
		}
		else {
			printf("No interrupt: Driver timeout at 5 minutes.\n");
			break;
		}

		if (singleEvent == false){
			g.seconds += 1;
			if (g.seconds % SECS_PER_MIN == 0){
				if (g.minutes > 1){
					writeInterruptDistribFile();
				}
				g.minutes += 1;
			}
			if (g.seconds % SECS_PER_DAY == 0){
				g.days += 1;
			}
		}
	}
	close(intrpt_fd);

	return 0;
}




