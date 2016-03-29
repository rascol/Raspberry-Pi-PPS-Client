/*
 * install-pps-client.cpp
 *
 *  Created on: Mar 17, 2016
 *      Author: ray
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

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
	strcpy(cmd, "mv ./pkg/pps-client.ko /lib/modules/");
	strcat(cmd, version);
	strcat(cmd, "/kernel/drivers/misc/pps-client.ko");

	printf("Moving pps-client.ko to /lib/modules/`uname -r`/kernel/drivers/misc/pps-client.ko\n");
	system(cmd);

	printf("Moving pps-client.conf to /etc/pps-client.conf\n");
	system("mv ./pkg/pps-client.conf /etc/pps-client.conf");

	printf("Moving pps-client-remove to /usr/sbin/pps-client-remove\n");
	system("mv ./pkg/pps-client-remove /usr/sbin/pps-client-remove");

	system("rm -rf ./pkg");
	system("rm pkg.tar.gz");

	delete fbuf;

	printf("Done.\n");
	return 0;
}


