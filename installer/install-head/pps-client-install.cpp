/*
 * pps-client-install.cpp
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
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

const char *version = "pps-client-installer v1.0.1";

/**
 * Returns the Linux kernel identifier
 * string that would result from typing
 * "uname -r" at the command line.
 */
char *getUname(char *str, int sz){
	system("uname -r > /run/shm/uname");
	int fd = open("/run/shm/uname", O_RDONLY);
	read(fd, str, sz);
	close(fd);
	remove("/run/shm/uname");

	sz = strcspn(str, "\r\n");
	str[sz] = '\0';

	return str;
}

int main(int argc, char *argv[]){
	unsigned char pkg_start[8];
	pkg_start[0] = 0xff;
	pkg_start[1] = 0x00;
	pkg_start[2] = 0xff;
	pkg_start[3] = 0x00;
	pkg_start[4] = 0xff;
	pkg_start[5] = 0x00;
	pkg_start[6] = 0xff;
	pkg_start[7] = 0x00;

	uid_t uid = geteuid();
	if (uid != 0){
		printf("Requires superuser privileges. Please sudo this command.\n");
		return 1;
	}

	unsigned char *ps = pkg_start;

	struct stat stat_buf;

	char *version = strpbrk(argv[0], "0123456789");
	char os_version[20];

	getUname(os_version, 20);

	if (strcmp(version, os_version) != 0){
		printf("Cannot install. pps-client version %s mismatches kernel version %s\n", version, os_version);
		return 1;
	}

	int fd = open(argv[0], O_RDONLY);					// Prepare to read this program as a binary file
	if (fd == -1){
		printf("File not found: %s\n", argv[0]);
		return 1;
	}

	fstat(fd, &stat_buf);
	int sz = stat_buf.st_size;							// Program size including tar file attachment

	unsigned char *fbuf = new unsigned char[sz];		// A buffer to hold the binary file

	ssize_t rd = read(fd, fbuf, sz);					// Read the program into the buffer
	close(fd);
	if (rd == -1){
		printf("Error reading %s\n", argv[0]);
		delete fbuf;
		return 1;
	}

	unsigned char *ptar = fbuf;
	int i;
	for (i = 0; i < (int)rd; i++, ptar += 1){			// Get a pointer to the tar file separator

		if (ptar[0] == ps[0] && ptar[1] == ps[1] && ptar[2] == ps[2] && ptar[3] == ps[3]
				&& ptar[4] == ps[4] && ptar[5] == ps[5] && ptar[6] == ps[6] && ptar[7] == ps[7]){
			ptar += 8;									// ptar now points to the tar file
			break;
		}
	}

	if (i == rd){
		printf("pkg_start not found.\n");
		delete fbuf;
		return 1;
	}

	sz -= i + 8;										// Set to the size of the tar file
														// Create the tar archive
	int fd2 = open("pkg.tar.gz", O_CREAT | O_WRONLY);
	if (fd2 == -1){
		printf("Unable to create the tar file\n");
		delete fbuf;
		return 1;
	}

	ssize_t wrt = write(fd2, ptar, sz);					// Write to the tar file from ptar

	if (wrt == -1){
		close(fd2);
		printf("Error writing tar file.\n");
		return 1;
	}
	if (wrt != sz){
		close(fd2);
		printf("Incomplete write to tar file. sz: %d wrt: %d\n", sz, wrt);
		return 1;
	}

	fchmod(fd2, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	close(fd2);

	int rv = system("tar xzvf pkg.tar.gz");
	if (rv == -1){
		return 0;
	}

	printf("Moving pps-client to /usr/sbin/pps-client\n");
	system("mv ./pkg/pps-client /usr/sbin/pps-client");

	printf("Moving pps-client.sh to /etc/init.d/pps-client\n");
	system("mv ./pkg/pps-client.sh /etc/init.d/pps-client");
	system("chmod +x /etc/init.d/pps-client");
	system("chown root /etc/init.d/pps-client");
	system("chgrp root /etc/init.d/pps-client");

	char *cmd = (char *)fbuf;

	printf("Moving gps-pps-io.ko to /lib/modules/`uname -r`/kernel/drivers/misc/gps-pps-io.ko\n");
	strcpy(cmd, "mv ./pkg/gps-pps-io.ko /lib/modules/");
	strcat(cmd, version);
	strcat(cmd, "/kernel/drivers/misc/gps-pps-io.ko");
	system(cmd);

	int fdc = open("/etc/pps-client.conf", O_RDONLY);
	if (fdc == -1){
		printf("Moving pps-client.conf to /etc/pps-client.conf\n");
		system("mv ./pkg/pps-client.conf /etc/pps-client.conf");
	}
	else {
		printf("Existing file: /etc/pps-client.conf was not replaced.\n");
		close(fdc);
	}

	printf("Moving pps-client-remove to /usr/sbin/pps-client-remove\n");
	system("mv ./pkg/pps-client-remove /usr/sbin/pps-client-remove");

	printf("Moving pps-client-stop to /usr/sbin/pps-client-stop\n");
	system("mv ./pkg/pps-client-stop /usr/sbin/pps-client-stop");
	system("chmod +x /usr/sbin/pps-client-stop");

	printf("Moving interrupt-timer to /usr/sbin/interrupt-timer\n");
	system("mv ./pkg/interrupt-timer /usr/sbin/interrupt-timer");
	system("chmod +x /usr/sbin/interrupt-timer");

	printf("Moving interrupt-timer.ko to /lib/modules/`uname -r`/kernel/drivers/misc/interrupt-timer.ko\n");
	strcpy(cmd, "mv ./pkg/interrupt-timer.ko /lib/modules/");
	strcat(cmd, version);
	strcat(cmd, "/kernel/drivers/misc/interrupt-timer.ko");
	system(cmd);

	printf("Moving pulse-generator to /usr/sbin/pulse-generator\n");
	system("mv ./pkg/pulse-generator /usr/sbin/pulse-generator");
	system("chmod +x /usr/sbin/pulse-generator");

	printf("Moving pulse-generator.ko to /lib/modules/`uname -r`/kernel/drivers/misc/pulse-generator.ko\n");
	strcpy(cmd, "mv ./pkg/pulse-generator.ko /lib/modules/");
	strcat(cmd, version);
	strcat(cmd, "/kernel/drivers/misc/pulse-generator.ko");
	system(cmd);

	printf("Moving NormalDistribParams to /usr/sbin/NormalDistribParams\n");
	system("mv ./pkg/NormalDistribParams /usr/sbin/NormalDistribParams");
	system("chmod +x /usr/sbin/NormalDistribParams");

	printf("Moving README.md to /usr/share/doc/pps-client/README.md\n");
	system("mkdir /usr/share/doc/pps-client");
	system("mv ./pkg/README.md /usr/share/doc/pps-client/README.md");

	system("mkdir /usr/share/doc/pps-client/figures");
	system("mv ./pkg/frequency-vars.png /usr/share/doc/pps-client/figures/frequency-vars.png");
	system("mv ./pkg/offset-distrib.png /usr/share/doc/pps-client/figures/offset-distrib.png");
	system("mv ./pkg/StatusPrintoutOnStart.png /usr/share/doc/pps-client/figures/StatusPrintoutOnStart.png");
	system("mv ./pkg/StatusPrintoutAt10Min.png /usr/share/doc/pps-client/figures/StatusPrintoutAt10Min.png");
	system("mv ./pkg/RPi_with_GPS.jpg /usr/share/doc/pps-client/figures/RPi_with_GPS.jpg");
	system("mv ./pkg/InterruptTimerDistrib.png /usr/share/doc/pps-client/figures/InterruptTimerDistrib.png");
	system("mv ./pkg/SingleEventTimerDistrib.png /usr/share/doc/pps-client/figures/SingleEventTimerDistrib.png");
	system("mv ./pkg/time.png /usr/share/doc/pps-client/figures/time.png");

	printf("Moving Doxyfile to /usr/share/doc/pps-client/Doxyfile\n");
	system("mv ./pkg/Doxyfile /usr/share/doc/pps-client/Doxyfile");

	printf("Moving pps-client.md to /usr/share/doc/pps-client/client/pps-client.md\n");
	system("mkdir /usr/share/doc/pps-client/client");
	system("mv ./pkg/client/pps-client.md /usr/share/doc/pps-client/client/pps-client.md");

	system("mkdir /usr/share/doc/pps-client/client/figures");
	system("mv ./pkg/client/figures/accuracy_verify.jpg /usr/share/doc/pps-client/client/figures/accuracy_verify.jpg");
	system("mv ./pkg/client/figures/interrupt-delay-comparison.png /usr/share/doc/pps-client/client/figures/interrupt-delay-comparison.png");
	system("mv ./pkg/client/figures/InterruptTimerDistrib.png /usr/share/doc/pps-client/client/figures/InterruptTimerDistrib.png");
	system("mv ./pkg/client/figures/jitter-spike.png /usr/share/doc/pps-client/client/figures/jitter-spike.png");
	system("mv ./pkg/client/figures/pps-jitter-distrib.png /usr/share/doc/pps-client/client/figures/pps-jitter-distrib.png");
	system("mv ./pkg/client/figures/pps-offsets-stress.png /usr/share/doc/pps-client/client/figures/pps-offsets-stress.png");
	system("mv ./pkg/client/figures/pps-offsets-to-300.png /usr/share/doc/pps-client/client/figures/pps-offsets-to-300.png");
	system("mv ./pkg/client/figures/pps-offsets-to-720.png /usr/share/doc/pps-client/client/figures/pps-offsets-to-720.png");
	system("mv ./pkg/client/figures/StatusPrintoutAt10Min.png /usr/share/doc/pps-client/client/figures/StatusPrintoutAt10Min.png");
	system("mv ./pkg/client/figures/StatusPrintoutOnStart.png /usr/share/doc/pps-client/client/figures/StatusPrintoutOnStart.png");
	system("mv ./pkg/client/figures/wiring.png /usr/share/doc/pps-client/client/figures/wiring.png");
	system("mv ./pkg/client/figures/interrupt-delay-comparison-RPi3.png /usr/share/doc/pps-client/client/figures/interrupt-delay-comparison-RPi3.png");
	system("mv ./pkg/client/figures/pps-jitter-distrib-RPi3.png /usr/share/doc/pps-client/client/figures/pps-jitter-distrib-RPi3.png");

	system("rm -rf ./pkg");
	system("rm pkg.tar.gz");

	delete fbuf;

	printf("Done.\n");
	return 0;
}


