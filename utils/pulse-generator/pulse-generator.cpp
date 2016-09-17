/*
 * pulse-generator.cpp
 *
 * Created on: Apr 9, 2016
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sched.h>
#include <sys/time.h>
#include <fcntl.h>

#define USECS_PER_SEC 1000000
#define ON_TIME 3
#define DELAYED 1
#define NONE 2
#define GPIO_A 0
#define GPIO_B 1

const char *version = "pulse-generator v1.0.0";

struct pulseGeneratorGlobalVars {
	char strbuf[200];
	int pulseTime1;
	int pulseTime2;
	bool badRead;
} g;

const char *pulse_verify_file = "/mnt/usbstorage/PulseVerify";


/**
 * Reads the major number assigned to pulse-generator
 * from "/proc/devices" as a string which is
 * returned in the majorPos char pointer. This
 * value is used to load the hardware driver that
 * pulse-generator requires.
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

	char *pos = strstr(fbuf, "pulse-generator");
	if (pos == NULL){
		printf("Can't find pulse-generator in \"/run/shm/proc_devices\"\n");
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
 * Loads the hardware driver required by pulse-generator which
 * is expected to be available in the file:
 * "/lib/modules/'uname -r'/kernel/drivers/misc/pulse-generator.ko".
 */
int driver_load(char *gpio1, char *gpio2){

	memset(g.strbuf, 0, 200 * sizeof(char));

	char *insmod = g.strbuf;

	strcpy(insmod, "/sbin/insmod /lib/modules/`uname -r`/kernel/drivers/misc/pulse-generator.ko gpio_num1=");
	strcat(insmod, gpio1);

	if (gpio2 != NULL){
		strcat(insmod, " gpio_num2=");
		strcat(insmod, gpio2);
	}

	system("rm -f /dev/pulse-generator");			// Clean up any old device files.

	system(insmod);									// Issue the insmod command

	char *mknod = g.strbuf;
	strcpy(mknod, "mknod /dev/pulse-generator c ");
	char *major = copyMajorTo(mknod + strlen(mknod));
	if (major == NULL){								// No major found! insmod failed.
		printf("driver_load() error: No major found!\n");
		system("/sbin/rmmod pulse-generator");
		return -1;
	}
	strcat(mknod, " 0");

	system(mknod);									// Issue the mknod command

	system("chgrp root /dev/pulse-generator");
	system("chmod 664 /dev/pulse-generator");

	return 0;
}

/**
 * Unloads the pulse-generator kernel driver.
 */
void driver_unload(void){
	system("/sbin/rmmod pulse-generator");
	system("rm -f /dev/pulse-generator");
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

void writeVerifyVal(int i){
	int fd = open(pulse_verify_file, O_CREAT | O_WRONLY | O_TRUNC);
	fchmod(fd, 644);
	sprintf(g.strbuf, "%d", i);
	write(fd, g.strbuf, strlen(g.strbuf)+1);
	close(fd);
}

void writePulseStatus(int readData[], int pulseTime){
	if (g.badRead){
		writeVerifyVal(NONE);
		printf("pulse-gemerator: Bad read from driver\n");
		return;
	}

	int pulseEnd = readData[1];
	if (pulseEnd > pulseTime){
		writeVerifyVal(DELAYED);
		printf("Pulse was delayed by system latency.\n");
	}
	else {
		writeVerifyVal(ON_TIME);
	}
}

/**
 * Generates one or two once-per-second pulses at the
 * microsecond offsets given on the command line.
 */
int main(int argc, char *argv[]){

	int pulseStart1 = 0, pulseStart2 = 0;
	int writeData[2];
	int readData[2];

	struct timeval tv1;
	struct timespec ts2;
	int pulseEnd1 = 0, pulseEnd2 = 0;

	memset(&g, 0, sizeof(struct pulseGeneratorGlobalVars));
	g.pulseTime1 = -1;
	g.pulseTime2 = -1;
	g.badRead = false;

	if (argc > 1){
		if (strcmp(argv[1], "load-driver") == 0){
			if (geteuid() != 0){
				printf("Requires superuser privileges. Please sudo this command.\n");
				return 1;
			}

			if (argv[2] == NULL){
				printf("GPIO number is a required second arg.\n");
				printf("Could not load driver.\n");
				return 1;
			}

			if (argc == 4){
				if (driver_load(argv[2], argv[3]) == -1){
					printf("Could not load pulse-generator driver. Exiting.\n");
					return 1;
				}
			}
			else {
				if (driver_load(argv[2], NULL) == -1){
					printf("Could not load pulse-generator driver. Exiting.\n");
					return 1;
				}
			}

			printf("pulse-generator: driver loaded\n");
			return 0;
		}
		if (strcmp(argv[1], "unload-driver") == 0){
			if (geteuid() != 0){
				printf("Requires superuser privileges. Please sudo this command.\n");
				return 1;
			}

			printf("pulse-generator: driver unloaded\n");
			driver_unload();
			return 0;
		}
		if (strcmp(argv[1], "-p") == 0 && argc == 3){
			sscanf(argv[2], "%d", &g.pulseTime1);
			if (g.pulseTime1 >= 0){
				goto start;
			}
		}
		if (strcmp(argv[1], "-p") == 0 && argc == 4){
			sscanf(argv[2], "%d", &g.pulseTime1);
			sscanf(argv[3], "%d", &g.pulseTime2);

			if (g.pulseTime1 >= 0 && g.pulseTime2 > 0){
				goto start;
			}
		}

		printf("Usage:\n");
		printf("Load driver with one or two output GPIOs:\n");
		printf("  sudo pulse-generator load-driver <gpio-num1> [gpio-num2]\n");
		printf("After loading the driver, calling pulse-generator\n");
		printf("with the following flag and value(s) causes it to\n");
		printf("generate one or two once-per-second pulse(s) at\n");
		printf("specified time(s) offset from the rollover of the\n");
		printf("second:\n");
		printf("  -p <microseconds> [microseconds]\n");
		printf("When the driver is no longer needed:\n");
		printf("  sudo pulse-generator unload-driver\n");

		return 0;
	}

start:
	char timeStr[100];
	const char *timefmt = "%F %H:%M:%S";

	if (geteuid() != 0){
		printf("Requires superuser privileges. Please sudo this command.\n");
		return 1;
	}

	printf(version);
	printf("\n");

	struct sched_param param;							// Process must be run as
	param.sched_priority = 99;							// root to change priority.
	sched_setscheduler(0, SCHED_FIFO, &param);			// Else, this has no effect.

	int fd = open("/dev/pulse-generator", O_RDWR);		// Open the pulse-generator device driver.
	if (fd == -1){
		printf("pulse-generator: Driver is not loaded. Exiting.\n");
		return 1;
	}

	pulseStart1 = g.pulseTime1 - 250;					// This will start the driver about 250 microsecs ahead of the
														// pulse write time thus allowing about 50 usec coming out of sleep
														// plus 150 usecs of system response latency. A spin loop in the
														// driver will chew up the excess time until the write at g.pulseTime1.
	if (g.pulseTime2 > g.pulseTime1){
		pulseStart2 = g.pulseTime2 - 250;				// Same for pulseStart2.
	}

	gettimeofday(&tv1, NULL);
	ts2 = setSyncDelay(pulseStart1, tv1.tv_usec);		// Sleep to pulseStart1

	for (;;){
		nanosleep(&ts2, NULL);

		writeData[0] = GPIO_A;							// Write to the first GPIO outuput
		writeData[1] = g.pulseTime1;
		write(fd, writeData, 2 * sizeof(int));			// Request a write at g.pulseTime1.

		if (read(fd, readData, 2 * sizeof(int)) < 0){	// Read the time the write actually occurred
			g.badRead = true;
		}
		else {
			pulseEnd1 = readData[1];
		}

		if (g.pulseTime2 > g.pulseTime1){				// If there is a pulseTime2, generate a second pulse.

			gettimeofday(&tv1, NULL);
			ts2.tv_nsec = (pulseStart2 - tv1.tv_usec) * 1000;
			ts2.tv_sec = 0;
			nanosleep(&ts2, NULL);						// Sleep to pulseStart2

			writeData[0] = GPIO_B;						// Write to the second GPIO output
			writeData[1] = g.pulseTime2;
			write(fd, writeData, 2 * sizeof(int));		// Request a write at pulseTime2.

			if (read(fd, readData, 2 * sizeof(int)) < 0){	// Read the time the write actually occurred.
				g.badRead = true;
			}
			else {
				pulseEnd2 = readData[1];
			}

			writePulseStatus(readData, g.pulseTime2);

			strftime(timeStr, 100, timefmt, localtime((const time_t*)(&(readData[0]))));
			printf("%s %d %d\n", timeStr, pulseEnd1, pulseEnd2);
		}
		else {
			writePulseStatus(readData, g.pulseTime1);

			strftime(timeStr, 100, timefmt, localtime((const time_t*)(&(readData[0]))));
			printf("%s %d\n", timeStr, pulseEnd1);
		}

		g.badRead = false;

		gettimeofday(&tv1, NULL);
		ts2 = setSyncDelay(pulseStart1, tv1.tv_usec);			// Sleep to pulseStart1
	}

	close(fd);											// Close the pulse-generator device driver.

	return 0;
}
