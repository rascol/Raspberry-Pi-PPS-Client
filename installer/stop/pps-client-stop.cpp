/*
 * pps-client-stop.cpp
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

const char *version = "pps-client-stop v1.0.1";

/**
 * Looks for "gps_pps_io" in the list printed
 * by the lsmod system command.
 *
 * @returns "true" if "gps_pps_io" is in the
 * lsmod list, else "false"
 */
bool driverIsLoaded(void){
	struct stat sbuf;
	const char *filename = "/run/shm/pps-msg";
	const char *cmd = "lsmod | grep gps_pps_io > /run/shm/pps-msg";
	int rv;
	rv = system(cmd);
	if (rv == -1 || WIFEXITED(rv) == false){
		printf("system command failed: %s\n", cmd);
	}
	rv = stat(filename, &sbuf);
	remove(filename);

	if (rv == -1 || sbuf.st_size == 0){
		return false;
	}
	return true;
}

int sysCommand(const char *cmd){
	int rv = system(cmd);
	if (rv == -1 || WIFEXITED(rv) == false){
		printf("System command failed: %s\n", cmd);
		return -1;
	}
	return 0;
}

bool driverHasUnloaded(void){
	for (int i = 0; i < 10; i++){			// Wait for driver to unload
		sleep(1);
		fprintf(stdout,".");
		fflush(stdout);
		if (! driverIsLoaded()){
			fprintf(stdout,"\n");
			return true;
		}
	}
	printf("Driver did not unload.\n");
	return false;
}

int main(void){
	char cmd[500];
	char *end;
	char buf[50];
	int daemonPID = 0;
	struct stat statbuf;
	int rv;

	uid_t uid = geteuid();
	if (uid != 0){
		printf("Requires superuser privileges. Please sudo this command.\n");
		exit(EXIT_FAILURE);
	}

	const char *filename = "/run/shm/pps-msg";

	rv = sysCommand("pidof pps-client > /run/shm/pps-msg");
	if (rv == -1){
		printf("Failed to stop pps-client.\n");
		exit(EXIT_FAILURE);
	}

	int fd = open(filename, O_RDONLY);
	if (fd == -1){
		printf("Unable to open %s\n", filename);
		printf("Failed to stop pps-client.\n");
		exit(EXIT_FAILURE);
	}

	rv = fstat(fd, &statbuf);
	if (rv == -1){
		close(fd);
		printf("Unable to read %s params. Error: %s\n", filename, strerror(errno));
		printf("Failed to stop pps-client.\n");
		exit(EXIT_FAILURE);
	}

	if (statbuf.st_size == 0){
		printf("PPS-Client is not running.\n");
		close(fd);
		return 0;
	}

	memset(buf, 0, 50);
	rv = read(fd, buf, 50);
	close(fd);
	if (rv == -1){
		printf("unable to read: %s\n", filename);
		exit(EXIT_FAILURE);
	}

	remove(filename);

	sscanf(buf, "%d\n", &daemonPID);

	if (daemonPID == 0){
		printf("PPS-Client is not running.\n");
		return 0;
	}

	strcpy(cmd, "kill ");
	strcat(cmd, buf);
	end = strpbrk(cmd, "\n");			// Zero terminate the kill command
	if (end != NULL){
		*end = '\0';
	}

	rv = sysCommand(cmd);				// Do the kill command
	if (rv == -1){
		printf("Failed to stop pps-client.\n");
		exit(EXIT_FAILURE);
	}

	fprintf(stdout, "Closing PPS-Client");
	fflush(stdout);

	if (driverHasUnloaded()){
		return 0;
	}
										// Try this if driver did not unload:
	sysCommand("rmmod gps_pps_io");
	sysCommand("rm -f /dev/gps_pps_io");

	return 0;
}


