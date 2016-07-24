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

const char *version = "pps-client-stop v1.0.0";

bool driverIsLoaded(void){
	struct stat sbuf;
	const char *filename = "/run/shm/pps-msg";

	system("lsmod | grep pps_client > /run/shm/pps-msg");
	int rv = stat(filename, &sbuf);
	remove(filename);

	if (rv == -1 || sbuf.st_size == 0){
		return false;
	}
	return true;
}

int main(void){
	char cmd[500];
	char *end;
	char buf[50];
	int daemonPID = 0;

	uid_t uid = geteuid();
	if (uid != 0){
		printf("Requires superuser privileges. Please sudo this command.\n");
		return 1;
	}

	const char *filename = "/run/shm/pps-msg";

	system("pidof pps-client > /run/shm/pps-msg");

	int fd = open(filename, O_RDONLY);
	if (fd == -1){
		printf("pps-client is not running.\n");
		goto end;
	}

	memset(buf, 0, 50);
	read(fd, buf, 50);

	close(fd);
	remove(filename);

	sscanf(buf, "%d\n", &daemonPID);

	if (daemonPID == 0){
		printf("pps-client is not running.\n");
		goto end;
	}

	strcpy(cmd, "kill ");
	strcat(cmd, buf);
	end = strpbrk(cmd, "\n");
	if (end != NULL){
		*end = '\0';
	}

	system(cmd);

	printf("Closing pps-client.\n");

	for (int i = 0; i < 15; i++){			// Wait for driver to unload
		sleep(1);
		if (! driverIsLoaded()){
			return 0;
		}
	}
end:
	if (! driverIsLoaded()){
		return 0;
	}

	system("rm -f /dev/pps-client");
	system("rmmod pps-client");

	return 0;
}


