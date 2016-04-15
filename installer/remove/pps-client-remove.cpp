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

	printf("Removing /usr/sbin/interrupt-timer\n");
	system("rm /usr/sbin/interrupt-timer");

	printf("Removing /lib/modules/`uname -r`/kernel/drivers/misc/interrupt-timer.ko\n");
	system("rm /lib/modules/`uname -r`/kernel/drivers/misc/interrupt-timer.ko");

	printf("Removing /usr/sbin/pulse-generator\n");
	system("rm /usr/sbin/pulse-generator");

	printf("Removing /lib/modules/`uname -r`/kernel/drivers/misc/pulse-generator.ko\n");
	system("rm /lib/modules/`uname -r`/kernel/drivers/misc/pulse-generator.ko");

	printf("Removing /var/log/pps-client.log\n");
	system("rm /var/log/pps-client.log");

	printf("Removing /usr/share/doc/pps-client\n");
	system("rm -rf /usr/share/doc/pps-client");

	printf("Removing /usr/sbin/pps-client-remove\n");
	char cmd[50];
	strcpy(cmd, "rm /usr/sbin/");
	strcat(cmd, argv[0]);
	system(cmd);

	return 0;
}
