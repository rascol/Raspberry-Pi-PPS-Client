/*
 * pps-client-remove.cpp
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

const char *version = "pps-client-remove v1.1.0";

int sysCommand(const char *cmd){
	int rv = system(cmd);
	if (rv == -1 || WIFEXITED(rv) == false){
		printf("system command failed: %s\n", cmd);
		return -1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	uid_t uid = geteuid();
	if (uid != 0){
		printf("Requires superuser privileges. Please sudo this command.\n");
		return 1;
	}

	if (argc > 1 && strcmp(argv[1], "-a") == 0){
		printf("Removing /etc/pps-client.conf\n");
		sysCommand("rm -f /etc/pps-client.conf");
	}

	sysCommand("service pps-client stop");
	sysCommand("chkconfig --del pps-client");
	sysCommand("pps-client-stop");					// In case not started as a service

	printf("Removing /usr/sbin/pps-client\n");
	sysCommand("rm -f /usr/sbin/pps-client");

	printf("Removing /usr/sbin/pps-client-stop\n");
	sysCommand("rm -f /usr/sbin/pps-client-stop");

	printf("Removing /etc/init.d/pps-client\n");
	sysCommand("rm -f /etc/init.d/pps-client");

	printf("Removing /lib/modules/`uname -r`/kernel/drivers/misc/gps-pps-io.ko\n");
	sysCommand("rm -f /lib/modules/`uname -r`/kernel/drivers/misc/gps-pps-io.ko");

	printf("Removing /usr/sbin/interrupt-timer\n");
	sysCommand("rm -f /usr/sbin/interrupt-timer");

	printf("Removing /lib/modules/`uname -r`/kernel/drivers/misc/interrupt-timer.ko\n");
	sysCommand("rm -f /lib/modules/`uname -r`/kernel/drivers/misc/interrupt-timer.ko");

	printf("Removing /usr/sbin/pulse-generator\n");
	sysCommand("rm -f /usr/sbin/pulse-generator");

	printf("Removing /lib/modules/`uname -r`/kernel/drivers/misc/pulse-generator.ko\n");
	sysCommand("rm -f /lib/modules/`uname -r`/kernel/drivers/misc/pulse-generator.ko");

	printf("Removing /var/log/pps-client.log\n");
	sysCommand("rm -f /var/log/pps-client.log");

	printf("Removing /usr/share/doc/pps-client\n");
	sysCommand("rm -rf /usr/share/doc/pps-client");

	printf("Removing /usr/sbin/pps-client-remove\n");
	char cmd[50];
	strcpy(cmd, "rm -f /usr/sbin/");
	strcat(cmd, argv[0]);
	sysCommand(cmd);

	return 0;
}
