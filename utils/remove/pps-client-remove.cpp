/*
 * pps-client-remove.cpp
 *
 *  Created on: Mar 17, 2016
 *      Author: ray
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	uid_t uid = geteuid();
	if (uid != 0){
		printf("Requires superuser privileges. Please sudo this command.\n");
		return 1;
	}

	system("service pps-client stop");
	system("chkconfig --del pps-client");
	system("pps-client-stop");					// In case not started as a service

	printf("Removing /usr/sbin/pps-client\n");
	system("rm /usr/sbin/pps-client");

	printf("Removing /usr/sbin/pps-client-stop\n");
	system("rm /usr/sbin/pps-client-stop");

	printf("Removing /etc/init.d/pps-client\n");
	system("rm /etc/init.d/pps-client");

	printf("Removing /etc/pps-client.conf\n");
	system("rm /etc/pps-client.conf");

	printf("Removing /lib/modules/`uname -r`/kernel/drivers/misc/pps-client.ko\n");
	system("rm /lib/modules/`uname -r`/kernel/drivers/misc/pps-client.ko");

	printf("Removing /var/log/pps-client.log\n");
	system("rm /var/log/pps-client.log");

	printf("Removing /usr/sbin/pps-client-remove\n");
	char cmd[50];
	strcpy(cmd, "rm /usr/sbin/");
	strcat(cmd, argv[0]);
	system(cmd);

	return 0;
}
