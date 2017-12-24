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

const char *version = "pps-client-installer v1.4.0";
const char *cfgVersion = "1.2.0";

char *configbuf = NULL;
unsigned char *fbuf = NULL;

const char *defaultLines[] = {
	"pps-gpio=4",
	"output-gpio=17",
	"intrpt-gpio=22",
	"serialPort=/dev/serial0"
};

bool isDefaultLine(char *line){
	if (line[0] == '\n' 					// Empty line
			|| line[0] == '#'){			// Commented-out line
		return true;
	}

	char *start = NULL;

	int count = sizeof(defaultLines) / sizeof(char *);

	for (int i = 0; i < count; i++){
		start = strstr(line, defaultLines[i]);
		if (start != NULL && start == line){
			return true;
		}
	}
	return false;
}

bool allOptsCommentedOut(char *configbuf){

	char *line = configbuf;

	while (line[0] != '\0'){
		if (isDefaultLine(line)){
			line = strstr(line, "\n\0");
			if (line[0] == '\n'){
				line += 1;
			}
		}
		else if (line[0] != '\0'){
			return false;
		}
	}
	return true;
}

/**
 * Returns the Linux kernel identifier
 * string that would result from typing
 * "uname -r" at the command line.
 */
char *getUname(char *str, int sz){
	int rv = system("uname -r > /run/shm/uname");
	if (rv == -1 || WIFEXITED(rv) == false){
		printf("getUname() System command failed\n");
		exit(EXIT_FAILURE);
	}
	int fd = open("/run/shm/uname", O_RDONLY);
	rv = read(fd, str, sz);
	if (rv == -1){
		printf("getUname() Read /run/shm/uname failed\n");
		exit(EXIT_FAILURE);
	}
	close(fd);
	remove("/run/shm/uname");

	sz = strcspn(str, "\r\n");
	str[sz] = '\0';

	return str;
}

int sysCommand(const char *cmd){
	int rv = system(cmd);
	if (rv == -1 || WIFEXITED(rv) == false){
		printf("system command failed: %s\n", cmd);

		if (configbuf != NULL){
			delete[] configbuf;
		}
		if (fbuf != NULL){
			delete[] fbuf;
		}
		exit(EXIT_FAILURE);
	}
	return 0;
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
		exit(EXIT_FAILURE);
	}

	unsigned char *ps = pkg_start;

	struct stat stat_buf;

	char *version = strpbrk(argv[0], "0123456789");
	char os_version[20];

	getUname(os_version, 20);

	if (strcmp(version, os_version) != 0){
		printf("Cannot install. pps-client version %s mismatches kernel version %s\n", version, os_version);
		exit(EXIT_FAILURE);
	}

	int fd = open(argv[0], O_RDONLY);					// Prepare to read this program as a binary file
	if (fd == -1){
		printf("Program binary %s was not found\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	fstat(fd, &stat_buf);
	int sz = stat_buf.st_size;						// Program size including tar file attachment

	fbuf = new unsigned char[sz];					// A buffer to hold the binary file

	ssize_t rd = read(fd, fbuf, sz);					// Read the program into the buffer
	close(fd);
	if (rd == -1){
		printf("Error reading %s\n", argv[0]);
		delete fbuf;
		exit(EXIT_FAILURE);
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
		printf("pkg_start code was not found.\n");
		delete fbuf;
		exit(EXIT_FAILURE);
	}
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
	sz -= i + 8;										// Set to the size of the tar file
														// Create the tar archive
	int fd2 = open("pkg.tar.gz", O_CREAT | O_WRONLY, mode);
	if (fd2 == -1){
		printf("Unable to create the tar file\n");
		delete fbuf;
		exit(EXIT_FAILURE);
	}

	ssize_t wrt = write(fd2, ptar, sz);					// Write to the tar file from ptar

	if (wrt == -1){
		close(fd2);
		printf("Error writing tar file.\n");
		exit(EXIT_FAILURE);
	}
	if (wrt != sz){
		close(fd2);
		printf("Incomplete write to tar file. sz: %d wrt: %d\n", sz, wrt);
		exit(EXIT_FAILURE);
	}

	close(fd2);

	sysCommand("tar xzvf pkg.tar.gz");

	printf("Moving pps-client to /usr/sbin/pps-client\n");

	sysCommand("mv ./pkg/pps-client /usr/sbin/pps-client");

	printf("Moving pps-client.sh to /etc/init.d/pps-client\n");

	sysCommand("mv ./pkg/pps-client.sh /etc/init.d/pps-client");
	sysCommand("chmod +x /etc/init.d/pps-client");
	sysCommand("chown root /etc/init.d/pps-client");
	sysCommand("chgrp root /etc/init.d/pps-client");

	char *cmd = (char *)fbuf;

	printf("Moving gps-pps-io.ko to /lib/modules/%s/kernel/drivers/misc/gps-pps-io.ko\n", os_version);

	strcpy(cmd, "mv ./pkg/gps-pps-io.ko /lib/modules/");
	strcat(cmd, version);
	strcat(cmd, "/kernel/drivers/misc/gps-pps-io.ko");
	sysCommand(cmd);

	int fdc = open("/etc/pps-client.conf", O_RDONLY);
	if (fdc == -1){													// No config file.
		printf("Moving pps-client.conf to /etc/pps-client.conf\n");
		sysCommand("mv ./pkg/pps-client.conf /etc/pps-client.conf");
	}
	else {															// Config file exists.
		int configbufLen = 10000;
		char *configbuf = new char[configbufLen];
		memset(configbuf, 0, configbufLen);

		int rv = read(fdc, configbuf, configbufLen-1);
		if (rv == -1){
			printf("Read config file failed. Exiting.\n");
			exit(EXIT_FAILURE);
		}

		if (allOptsCommentedOut(configbuf)){
			printf("Moving pps-client.conf to /etc/pps-client.conf\n");
			sysCommand("mv ./pkg/pps-client.conf /etc/pps-client.conf");
		}
		else {
			printf("Modified file, /etc/pps-client.conf, was not replaced.\n");
		}

		delete[] configbuf;
		configbuf = NULL;

		close(fdc);
	}

	printf("Moving pps-client-remove to /usr/sbin/pps-client-remove\n");
	sysCommand("mv ./pkg/pps-client-remove /usr/sbin/pps-client-remove");

	printf("Moving pps-client-stop to /usr/sbin/pps-client-stop\n");
	sysCommand("mv ./pkg/pps-client-stop /usr/sbin/pps-client-stop");
	sysCommand("chmod +x /usr/sbin/pps-client-stop");

	printf("Moving interrupt-timer to /usr/sbin/interrupt-timer\n");
	sysCommand("mv ./pkg/interrupt-timer /usr/sbin/interrupt-timer");
	sysCommand("chmod +x /usr/sbin/interrupt-timer");

	printf("Moving interrupt-timer.ko to /lib/modules/`uname -r`/kernel/drivers/misc/interrupt-timer.ko\n");
	strcpy(cmd, "mv ./pkg/interrupt-timer.ko /lib/modules/");
	strcat(cmd, version);
	strcat(cmd, "/kernel/drivers/misc/interrupt-timer.ko");
	sysCommand(cmd);

	printf("Moving pulse-generator to /usr/sbin/pulse-generator\n");
	sysCommand("mv ./pkg/pulse-generator /usr/sbin/pulse-generator");
	sysCommand("chmod +x /usr/sbin/pulse-generator");

	printf("Moving pulse-generator.ko to /lib/modules/`uname -r`/kernel/drivers/misc/pulse-generator.ko\n");
	strcpy(cmd, "mv ./pkg/pulse-generator.ko /lib/modules/");
	strcat(cmd, version);
	strcat(cmd, "/kernel/drivers/misc/pulse-generator.ko");
	sysCommand(cmd);

	printf("Moving NormalDistribParams to /usr/sbin/NormalDistribParams\n");
	sysCommand("mv ./pkg/NormalDistribParams /usr/sbin/NormalDistribParams");
	sysCommand("chmod +x /usr/sbin/NormalDistribParams");

	printf("Moving README.md to /usr/share/doc/pps-client/README.md\n");
	sysCommand("mkdir /usr/share/doc/pps-client");
	sysCommand("mv ./pkg/README.md /usr/share/doc/pps-client/README.md");

	sysCommand("mkdir /usr/share/doc/pps-client/figures");
	sysCommand("mv ./pkg/frequency-vars.png /usr/share/doc/pps-client/figures/frequency-vars.png");
	sysCommand("mv ./pkg/offset-distrib.png /usr/share/doc/pps-client/figures/offset-distrib.png");
	sysCommand("mv ./pkg/StatusPrintoutOnStart.png /usr/share/doc/pps-client/figures/StatusPrintoutOnStart.png");
	sysCommand("mv ./pkg/StatusPrintoutAt10Min.png /usr/share/doc/pps-client/figures/StatusPrintoutAt10Min.png");
	sysCommand("mv ./pkg/RPi_with_GPS.jpg /usr/share/doc/pps-client/figures/RPi_with_GPS.jpg");
	sysCommand("mv ./pkg/InterruptTimerDistrib.png /usr/share/doc/pps-client/figures/InterruptTimerDistrib.png");
	sysCommand("mv ./pkg/SingleEventTimerDistrib.png /usr/share/doc/pps-client/figures/SingleEventTimerDistrib.png");
	sysCommand("mv ./pkg/time.png /usr/share/doc/pps-client/figures/time.png");

	printf("Moving Doxyfile to /usr/share/doc/pps-client/Doxyfile\n");
	sysCommand("mv ./pkg/Doxyfile /usr/share/doc/pps-client/Doxyfile");

	printf("Moving pps-client.md to /usr/share/doc/pps-client/client/pps-client.md\n");
	sysCommand("mkdir /usr/share/doc/pps-client/client");
	sysCommand("mv ./pkg/client/pps-client.md /usr/share/doc/pps-client/client/pps-client.md");

	sysCommand("mkdir /usr/share/doc/pps-client/client/figures");
	sysCommand("mv ./pkg/client/figures/accuracy_verify.jpg /usr/share/doc/pps-client/client/figures/accuracy_verify.jpg");
	sysCommand("mv ./pkg/client/figures/interrupt-delay-comparison.png /usr/share/doc/pps-client/client/figures/interrupt-delay-comparison.png");
	sysCommand("mv ./pkg/client/figures/InterruptTimerDistrib.png /usr/share/doc/pps-client/client/figures/InterruptTimerDistrib.png");
	sysCommand("mv ./pkg/client/figures/jitter-spike.png /usr/share/doc/pps-client/client/figures/jitter-spike.png");
	sysCommand("mv ./pkg/client/figures/pps-jitter-distrib.png /usr/share/doc/pps-client/client/figures/pps-jitter-distrib.png");
	sysCommand("mv ./pkg/client/figures/pps-offsets-stress.png /usr/share/doc/pps-client/client/figures/pps-offsets-stress.png");
	sysCommand("mv ./pkg/client/figures/pps-offsets-to-300.png /usr/share/doc/pps-client/client/figures/pps-offsets-to-300.png");
	sysCommand("mv ./pkg/client/figures/pps-offsets-to-720.png /usr/share/doc/pps-client/client/figures/pps-offsets-to-720.png");
	sysCommand("mv ./pkg/client/figures/StatusPrintoutAt10Min.png /usr/share/doc/pps-client/client/figures/StatusPrintoutAt10Min.png");
	sysCommand("mv ./pkg/client/figures/StatusPrintoutOnStart.png /usr/share/doc/pps-client/client/figures/StatusPrintoutOnStart.png");
	sysCommand("mv ./pkg/client/figures/wiring.png /usr/share/doc/pps-client/client/figures/wiring.png");
	sysCommand("mv ./pkg/client/figures/interrupt-delay-comparison-RPi3.png /usr/share/doc/pps-client/client/figures/interrupt-delay-comparison-RPi3.png");
	sysCommand("mv ./pkg/client/figures/pps-jitter-distrib-RPi3.png /usr/share/doc/pps-client/client/figures/pps-jitter-distrib-RPi3.png");

	sysCommand("rm -rf ./pkg");
	sysCommand("rm pkg.tar.gz");

	delete[] fbuf;

	printf("Done.\n");
	return 0;
}


