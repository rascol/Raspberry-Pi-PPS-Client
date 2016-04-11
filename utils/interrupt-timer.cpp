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

char strbuf[200];
const char *version = "interrupt-timer v0.1.0";

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

	memset(strbuf, 0, 200 * sizeof(char));

	char *insmod = strbuf;
	strcpy(insmod, "/sbin/insmod /lib/modules/`uname -r`/kernel/drivers/misc/interrupt-timer.ko gpio_num=");
	strcat(insmod, gpio);

	system("rm -f /dev/interrupt-timer");			// Clean up any old device files.

	system(insmod);									// Issue the insmod command

	char *mknod = strbuf;
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
 * Read the interrupt delay value recorded by
 * pps-client.
 */
int getInterruptDelay(int *delay){

	int delay_fd = open("/run/shm/pps-sysDelay", O_RDONLY);
	if (delay_fd == -1){
		printf("Error: pps-client is not running.\n");
		return -1;
	}
	read(delay_fd, strbuf, 50);
	strbuf[50] = '\0';
	close(delay_fd);

	char *end = strchr(strbuf, '#');
	end[0] = '\0';
	sscanf(strbuf, "%d", delay);
	return 0;
}

int main(int argc, char *argv[]){

	int outFormat = 0;
	int tm[2];
	int delay;

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
			printf("interrupt-timer: driver unloaded\n");
			driver_unload();
			return 0;
		}
		if (strcmp(argv[1], "-s") == 0){
			outFormat = 1;
			goto start;
		}
		if (strcmp(argv[1], "-d") == 0){
			goto start;
		}

		printf("Usage:\n");
		printf("  interrupt-timer load-driver <gpio-number>\n");
		printf("  interrupt-timer unload-driver\n");
		printf("After loading the driver, calling interrupt-timer\n");
		printf("causes it to wait for interrupts then output the\n");
		printf("date-time when one occurs. The following command\n");
		printf("args modify the format of the date-time output:\n\n");
		printf("  -s Outputs seconds since the epoch.\n");
		printf("  -d Outputs in date format (default).\n");
		printf("The program will exit on ctrl-c or when no\n");
		printf("interrupts are received within 10 seconds.\n");

		return 0;
	}

start:
	if (geteuid() != 0){
		printf("Requires superuser privileges. Please sudo this command.\n");
		return 0;
	}

	printf(version);
	printf("\n");

	struct sched_param param;						// Process must be run as
	param.sched_priority = 98;						// root to change priority.
	sched_setscheduler(0, SCHED_FIFO, &param);		// Else, this has no effect.

	int intrpt_fd = open("/dev/interrupt-timer", O_RDONLY); // Open the interrupt-timer device driver.
	if (intrpt_fd == -1){
		printf("interrupt-timer: Driver is not loaded. Exiting.\n");
		return 1;
	}

	for (;;){
		int dvrv = read(intrpt_fd, (void *)tm, 2 * sizeof(int));
		if (dvrv > 0){
			int rv = getInterruptDelay(&delay);
			if (rv == -1){
				return 1;
			}

			tm[1] -= delay;
			if (tm[1] < 0){
				tm[1] = 1000000 + tm[1];
				tm[0] -= 1;
			}

			if (outFormat == 0){			// Print in date-time format
				const char *timefmt = "%F %H:%M:%S";
				char timeStr[30];

				strftime(timeStr, 30, timefmt, localtime((const time_t*)(&tm[0])));

				printf("%s.%06d\n", timeStr, tm[1]);
			}
			else {							// Print as seconds
				double time = (double)tm[0] + 1e-6 * tm[1];
				printf("%lf\n", time);
			}
		}
		else {
			printf("No interrupt: Driver timeout at 10 seconds.\n");
			break;
		}
	}
	close(intrpt_fd);

	return 0;
}




