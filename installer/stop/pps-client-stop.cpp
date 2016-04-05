/*
 * pps-client-stop.cpp
 *
 *  Created on: Mar 31, 2016
 *      Author: ray
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(void){
	char cmd[500];

	uid_t uid = geteuid();
	if (uid != 0){
		printf("Requires superuser privileges. Please sudo this command.\n");
		return 1;
	}

	if(system("ps -e | grep pps-client > /run/shm/pps-client-pid.txt") == -1){
		printf("Unable to do \"ps -e | grep pps-client > /run/shm/pps-client-pid.txt\"\n");
		return 1;
	}

	int fd = open("/run/shm/pps-client-pid.txt", O_RDONLY);

	strcpy(cmd, "kill ");
												// Append the PID value if any to cmd
	read(fd, cmd + strlen(cmd), 500 - strlen(cmd));

	char *end = strchr(cmd, '\n');				// Get only the first line
	if (end != NULL){
		*end = '\0';							// by ending at '\n'.
	}

	if (strstr(cmd, "pps-client-") != NULL){	// This is not pps-client.
		printf("pps-client is not running.\n");	// It's pps-client-stop or
		return 0;								// pps-client-remove.
	}

	char *pid = strpbrk(cmd, "0123456789");		// Locate the PID value
	if (pid == NULL){
		printf("pps-client is not running.\n");
		return 0;
	}

	end = strpbrk(pid, " \r\n");				// Get the space after the PID value
	*end = '\0';								// Terminate PID value at the space

	printf("Closing pps-client\n");

	system(cmd);								// Issue: "kill PID"

	return 0;
}


