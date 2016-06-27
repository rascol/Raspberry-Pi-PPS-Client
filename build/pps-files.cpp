/*
 * pps-files.cpp
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

#include "../build/pps-client.h"
extern struct ppsClientGlobalVars g;

const char *last_distrib_file = "/var/local/error-distrib";			// File storing the completed distribution of offset corrections.
const char *distrib_file = "/var/local/error-distrib-forming";		// File storing an incompleted distribution of offset corrections.
const char *last_jitter_distrib_file = "/var/local/jitter-distrib";	// File storing the completed distribution of offset corrections.
const char *jitter_distrib_file = "/var/local/jitter-distrib-forming";// File storing an incompleted distribution of offset corrections.
const char *log_file = "/var/log/pps-client.log";					// File storing activity and errors.
const char *old_log_file = "/var/log/pps-client.old.log";			// File storing activity and errors.
const char *config_file = "/etc/pps-client.conf";					// The pps-client configuration file.
const char *assert_file = "/run/shm/pps-assert";					// The timestamps of the time corrections each second
const char *sysDelay_file = "/run/shm/pps-sysDelay";				// The current sysDelay value updated each second
const char *last_intrpt_distrib_file = "/var/local/intrpt-distrib";	// File storing the completed distribution of offset corrections.
const char *interrupt_distrib_file = "/var/local/intrpt-distrib-forming";// File storing an incomplete distribution of offset corrections.
const char *displayParams_file = "/run/shm/display-params";			// Temporary file storing params for the status display
const char *arrayData_file = "/run/shm/save-data";
const char *sysDelay_distrib_file = "/var/local/sysDelay-distrib-forming";
const char *last_sysDelay_distrib_file = "/var/local/sysDelay-distrib";
const char *ntp_config_file = "/etc/ntp.conf";
const char *ntp_config_bac = "/etc/ntp.conf.bac";
const char *ntp_config_part = "/etc/ntp.conf.part";
const char *pidFilename = "/var/run/pps-client.pid";

const char *space = " ";
const char *num = "0123456789.";

extern const char *version;
/**
 * Local file-scope shared variables.
 */
static struct filesLocalVars {
	struct stat configFileStat;
	time_t modifyTime;
	int lastJitterFileno;
	int lastSysDelayFileno;
	int lastErrorFileno;
	int lastIntrptFileno;
} f;

const char *config_str[] = {
		"error-distrib",
		"alert-pps-lost",
		"jitter-distrib",
		"calibrate",
		"interrupt-distrib",
		"sysdelay-distrib",
		"exit-lost-pps",
		"fix-delay-peak",
		"show-remove-noise"
};

struct saveFileData arrayData[] = {
	{"rawError", g.rawErrorDistrib, "/var/local/raw-error-distrib", ERROR_DISTRIB_LEN, 2, RAW_ERROR_ZERO},
	{"intrptError", g.intrptErrorDistrib, "/var/local/intrpt-error-distrib", ERROR_DISTRIB_LEN, 2, RAW_ERROR_ZERO},
	{"frequency-vars", NULL, "/var/local/frequency-vars", 0, 3, 0},
	{"pps-offsets", NULL, "/var/local/pps-offsets", 0, 4, 0}
};


void couldNotOpenMsgTo(char *logbuf, const char *filename){
	strcpy(logbuf, "ERROR: could not open \"");
	strcat(logbuf, filename);
	strcat(logbuf, "\": ");
	strcat(logbuf, strerror(errno));
	strcat(logbuf, "\n");
}

void errorReadingMsgTo(char *logbuf, const char *filename){
	strcpy(logbuf, "ERROR: reading \"");
	strcat(logbuf, filename);
	strcat(logbuf, "\" was interrupted: ");
	strcat(logbuf, strerror(errno));
	strcat(logbuf, "\n");
}

/**
 * Appends logbuf to the log file.
 */
void writeToLog(char *logbuf){
	struct stat info;

	bufferStatusMsg(logbuf);

	stat(log_file, &info);
	if (info.st_size > 100000){			// Prevent unbounded log file growth
		remove(old_log_file);
		rename(log_file, old_log_file);
	}

	int fd = open(log_file, O_CREAT | O_WRONLY | O_APPEND);
	if (fd == -1){
		couldNotOpenMsgTo(logbuf, log_file);
		printf(logbuf);
		return;
	}
	time_t t = time(NULL);
	struct tm *tmp = localtime(&t);
	strftime(g.strbuf, STRBUF_SZ, "%F %H:%M:%S ", tmp);
	write(fd, g.strbuf, strlen(g.strbuf));
	write(fd, logbuf, strlen(logbuf));
	fchmod(fd, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
	close(fd);
}

/**
 * Concatenates msg to a message buffer, savebuf, which is
 * saved to a tmpfs memory file by writeStatusStrings() each
 * second. These messages can be read and displayed to the
 * command line by showStatusEachSecond().
 */
void bufferStatusMsg(const char *msg){

	if (g.isVerbose){
		fprintf(stdout, msg);
	}

	int msglen = strlen(g.savebuf);
	int parmslen = strlen(msg);

	if (msglen + parmslen > MSGBUF_SZ){
		return;
	}

	strcat(g.savebuf, msg);
}

/**
 * Writes status strings accumulated in a message buffer,
 * g.savebuf, to a tmpfs memory file, displayParams_file,
 * once each second. This file can be displayed in real
 * time by invoking the pps-client program with the -v
 * command line param while the pps-client daemon is
 * running.
 */
int writeStatusStrings(void){

	int fSize = strlen(g.savebuf);

	remove(displayParams_file);
	int fd = open_logerr(displayParams_file, O_CREAT | O_WRONLY);
	if (fd == -1){
		return -1;
	}
	write(fd, g.savebuf, fSize);
	close(fd);

	g.savebuf[0] = '\0';
	return 0;
}

/**
 * Reads a file with error logging.
 */
int read_logerr(int fd, char *buf, int sz, const char *filename){
	int rv = read(fd, buf, sz);
	if (rv == -1){
		errorReadingMsgTo(g.logbuf, filename);
		writeToLog(g.logbuf);
		return rv;
	}
	return rv;
}

/**
 * Opens a file with error logging.
 */
int open_logerr(const char* filename, int flags){
	int fd = open(filename, flags);
	if (fd == -1){
		couldNotOpenMsgTo(g.logbuf, filename);
		writeToLog(g.logbuf);
		return -1;
	}
	if ((flags & O_CREAT) == O_CREAT){
		fchmod(fd, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
	}
	return fd;
}

/**
 * Writes the message recorded in file to the log buffer
 * for transfer to pps-client.log.
 */
int writeFileMsgToLogbuf(const char *file, char *logbuf){
	struct stat stat_buf;
	int rv;

	int fd = open(file, O_RDONLY);
	if (fd == -1){
		couldNotOpenMsgTo(logbuf, file);
		printf(logbuf);
		return -1;
	}
	fstat(fd, &stat_buf);
	int sz = stat_buf.st_size;

	if (sz >= LOGBUF_SZ-1){
		rv = read(fd, logbuf, LOGBUF_SZ-1);
		if (rv == -1){
			errorReadingMsgTo(logbuf, file);
			printf(logbuf);
			return rv;
		}
		logbuf[LOGBUF_SZ-1] = '\0';
	}
	else {
		rv = read(fd, logbuf, sz);
		if (rv == -1){
			errorReadingMsgTo(logbuf, file);
			printf(logbuf);
			return rv;
		}
		logbuf[sz] = '\0';
	}
	close(fd);
	remove(file);

	return 0;
}

/**
 * Writes the char string in file to pps-client.log.
 */
int writeFileMsgToLog(const char *file){
	writeFileMsgToLogbuf(file, g.logbuf);
	writeToLog(g.logbuf);
	return 0;
}

/**
 * Reads the PID of the child process when
 * the parent process needs to kill it.
 */
pid_t getChildPID(void){
	pid_t pid = 0;
	const char *filename = "/var/run/pps-client.pid";

	memset(g.strbuf, 0, STRBUF_SZ);

	int pfd = open_logerr(filename, O_RDONLY);
	if (pfd == -1){
		return -1;
	}
	if (read_logerr(pfd, g.strbuf, 19, filename) == -1){
		close(pfd);
		return -1;
	}
	sscanf(g.strbuf, "%d\n", &pid);
	close(pfd);
	if (pid > 0){
		return pid;
	}
	return -1;
}

/**
 * Uses a system call to pidof to see if pps-client is running.
 * If a PID for pps exists returns "true". Else returns "false".
 */
bool ppsIsRunning(void){
	char buf[50];
	const char *filename = "/run/shm/pps-msg";

	system("pidof pps-client > /run/shm/pps-msg");

	int fd = open(filename, O_RDONLY);
	if (fd == -1){
		return false;
	}
	memset(buf, 0, 50);
	read(fd, buf, 50);

	int callerPID = 0, daemonPID = 0;					// If running both of these exist
	sscanf(buf, "%d %d\n", &callerPID, &daemonPID);

	close(fd);
	remove(filename);

	if (daemonPID == 0){								// Otherwise only the first exists.
		return false;
	}
	return true;
}

/**
 * Creates a PID file for the pps-client daemon.
 */
int createPIDfile(void){

	int pfd = open_logerr(pidFilename, O_RDWR | O_CREAT | O_EXCL);
	if (pfd == -1){
		sprintf(g.logbuf, "Error: Could not create a PID file.\n");
		writeToLog(g.logbuf);
		return -1;
	}

	pid_t ppid = getpid();

	sprintf(g.strbuf, "%d\n", ppid);
	if (write(pfd, g.strbuf, strlen(g.strbuf)) == -1)	// Try to write the PID
	{
		close(pfd);
		sprintf(g.logbuf, "Error writing a PID file for pps-client: %s\n", strerror(errno));
		writeToLog(g.logbuf);
		return -1;									// Write failed.
	}
	close(pfd);

	return ppid;
}

/**
 * Reads the pps-client config file and sets bits
 * in config_select to 1 or 0 corresponding to
 * whether a particular config_str appears in the
 * config file. The value string following each
 * config_str is then assigned to strPtrs. If the
 * config_str did not occur in the config file
 * then strPtrs has a NULL char* in that location.
 *
 * The values retured in strPtrs[] use the space
 * provided by fbuf.
 */
int readConfigFile(char *strPtrs[], char *fbuf, int size){
	struct stat stat_buf;

	int rvs = stat(config_file, &f.configFileStat);
	if (rvs == -1){
		return 0;									// No config file
	}

	timespec t = f.configFileStat.st_mtim;

	if (g.seq_num > 0 && f.modifyTime == t.tv_sec){
		return 0;									// Config file unchanged from last read
	}

	f.modifyTime = t.tv_sec;

	int fd = open_logerr(config_file, O_RDONLY);
	if (fd == -1){
		return -1;
	}

	fstat(fd, &stat_buf);
	int sz = stat_buf.st_size;

	if (sz >= size){
		sprintf(g.logbuf, "readConfigFile(): not enough space allocated for config file.\n");
		writeToLog(g.logbuf);
		return -1;
	}

	int rv = read_logerr(fd, fbuf, sz, config_file);
	if (rv == -1){
		return -1;
	}
	close(fd);

	int j = 0;
	char *pch = strtok(fbuf, "\n");
	while (pch != NULL){
		if (pch[0] != '#'){
			strPtrs[j] = pch;
			j += 1;
		}
		pch = strtok(NULL, "\n");
	}

	int i;

	fbuf[0] = 0;
	for (i = 0; i < j; i++){
		strcat(fbuf, strPtrs[i]);
		strcat(fbuf, "\n");
	}

	char **configVal = strPtrs;					// Use strPtrs to return values
	char *value;
	sz = (int)(sizeof(config_str)/sizeof(char*));
	for (i = 0; i < sz; i++){

		char *found = strstr(fbuf, config_str[i]);
		if (found != NULL){
			g.config_select |= 1 << i;			// Set a bit in config_select

			value = strpbrk(found, "=");
			value += 1;

			configVal[i] = value;				// Point to config_str[i] value in fbuf
												// The value will be terminated with '\n'.
		}
		else {
			g.config_select &= ~(1 << i);		// Clear a bit in config_select
			configVal[i] = NULL;
		}
	}

	for (i = 0; i < sz; i++){					// Terminate the value strings.
		if (configVal[i] != NULL){
			value = strpbrk(configVal[i], "\n");
			if (value != NULL){
				*value = 0;
			}
		}
	}

	return 0;
}

/**
 * Writes an accumulating statistical distribution at regular intervals
 * to disk and rolls over the accumulating data to a new file every
 * epochInterval days and begins a new distribution file.
 */
void writeDistribution(int distrib[], int len, int scaleZero, int epochInterval,
		int *last_epoch, const char *distrib_file, const char *last_distrib_file){

	remove(distrib_file);
	int fd = open_logerr(distrib_file, O_CREAT | O_WRONLY | O_APPEND);
	if (fd == -1){
		return;
	}
	for (int i = 0; i < len; i++){
		sprintf(g.strbuf, "%d %d\n", i-scaleZero, distrib[i]);
		write(fd, g.strbuf, strlen(g.strbuf));
	}
	close(fd);

	int epoch = g.days / epochInterval;
	if (epoch != *last_epoch ){
		*last_epoch = epoch;
		remove(last_distrib_file);
		rename(distrib_file, last_distrib_file);
		memset(distrib, 0, len * sizeof(int));
	}
}

/**
 * Writes a distribution to disk containing 60 additional calibration
 * interrupt delays approximately every minute. Collects one day
 * of interrupt delay samples before rolling over a new file.
 */
void writeInterruptDistribFile(void){
	if (g.delay_idx == 0 && g.delayCount > 0){
		writeDistribution(g.interruptDistrib, INTRPT_DISTRIB_LEN, 0, 1,
				&f.lastIntrptFileno, interrupt_distrib_file, last_intrpt_distrib_file);
	}
}

/**
 * Writes a distribution to disk containing 60 additional
 * sysDelay samples approximately every minute. Collects one
 * day of sysDelay samples before rolling over a new file.
 */
void writeSysdelayDistribFile(void){
	if (g.delay_idx == 0 && g.delayCount == g.delayPeriod && g.hardLimit == HARD_LIMIT_1){
		writeDistribution(g.sysDelayDistrib, INTRPT_DISTRIB_LEN, 0, 1, &f.lastSysDelayFileno,
				sysDelay_distrib_file, last_sysDelay_distrib_file);
	}
}

/**
 * Writes a distribution to disk approximately once a minute
 * containing 60 additional jitter samples recorded at the
 * occurrance of the PPS interrupt. The distribution is
 * rolled over to a new file every 24 hours.
 */
void writeJitterDistribFile(void){
	if (g.jitterCount == 0 && g.seq_num > SETTLE_TIME){
		int scaleZero = JITTER_DISTRIB_LEN / 2;
		writeDistribution(g.jitterDistrib, JITTER_DISTRIB_LEN, scaleZero, 1,
				&f.lastJitterFileno, jitter_distrib_file, last_jitter_distrib_file);
	}
}

/**
 * Writes a distribution to disk approximately once a minute
 * containing 60 additional time correction samples derived
 * from the PPS interrupt. The distribution is rolled over
 * to a new file every 24 hours.
 */
void writeErrorDistribFile(void){
	if (g.correctionFifo_idx == 0){
		int scaleZero = ERROR_DISTRIB_LEN / 2;
		writeDistribution(g.errorDistrib, ERROR_DISTRIB_LEN, scaleZero,
				1, &f.lastErrorFileno, distrib_file, last_distrib_file);
	}
}

/**
 * Writes the previously completed list of 10 minutes of recorded
 * time offsets and applied frequency offsets indexed by seq_num.
 */
void writeOffsets(const char *filename){
	int fd = open_logerr(filename, O_CREAT | O_WRONLY | O_TRUNC);
	if (fd == -1){
		return;
	}
	for (int i = 0; i < SECS_PER_10_MIN; i++){
		int j = g.recIndex2 + i;
		if (j >= SECS_PER_10_MIN){
			j -= SECS_PER_10_MIN;
		}
		sprintf(g.strbuf, "%d %d %lf\n", g.seq_numRec[j], g.offsetRec[j], g.freqOffsetRec2[j]);
		write(fd, g.strbuf, strlen(g.strbuf));
	}
	close(fd);
}

/**
 * Writes the last 24 hours of clock frequency offset and Allan
 * deviation in each 5 minute interval indexed by the timestamp
 * at each interval.
 */
void writeFrequencyVars(const char *filename){
	int fd = open_logerr(filename, O_CREAT | O_WRONLY | O_TRUNC);
	if (fd == -1){
		return;
	}
	for (int i = 0; i < NUM_5_MIN_INTERVALS; i++){
		int j = g.recIndex + i;							// Read the circular buffers relative to g.recIndx.
		if (j >= NUM_5_MIN_INTERVALS){
			j -= NUM_5_MIN_INTERVALS;
		}
		sprintf(g.strbuf, "%ld %lf %lf\n", g.timestampRec[j], g.freqOffsetRec[j], g.freqAllanDev[j]);
		write(fd, g.strbuf, strlen(g.strbuf));
	}
	close(fd);
}

/**
 * Tests configuration strings from /etc/pps-client.conf
 * for the "enable" keyword and returns "true" if it is
 * detected.
 */
bool isEnabled(int config, char *configVals[]){
	int i = round(log2(config));

	if (g.config_select & config){
		char *val = strstr(configVals[i], "enable");
		if (val != NULL){
			return true;
		}
	}
	return false;
}

/**
 * Tests configuration strings from /etc/pps-client.conf
 * for the "disable" keyword and returns "true" if it is
 * detected.
 */
bool isDisabled(int config, char *configVals[]){
	int i = round(log2(config));

	if (g.config_select & config){
		char *val = strstr(configVals[i], "disable");
		if (val != NULL){
			return true;
		}
	}
	return false;
}

/**
 * Tests configuration strings from /etc/pps-client.conf
 * for a numeric value and returns it in the value pointer.
 * The numeric value may be either int or double with the
 * value pointer and the presence of a decimal point
 * determining int or double.
 */
bool configHasValue(int config, char *configVals[], void *value){
	int i = round(log2(config));
	if (g.config_select & config){
		char *val = strpbrk(configVals[i], num);
		if (strpbrk(val, ".") != NULL){
			sscanf(val, "%lf", (double *)value);
		}
		else {
			scanf(val, "%d", (int *)value);
		}
		return true;
	}
	return false;
}

/**
 * Saves an array of doubles.
 */
bool saveDoubleArray(double distrib[], const char *filename, int len, int arrayZero){

	int fd = open_logerr(filename, O_CREAT | O_WRONLY | O_TRUNC);
	if (fd == -1){
		return false;
	}

	int fileMaxLen = len * MAX_LINE_LEN * sizeof(char);
	char *filebuf = new char[fileMaxLen];
	int fileLen = 0;

	filebuf[0] = '\0';
	for (int i = 0; i < len; i++){
		sprintf(g.strbuf, "%d %7.2lf\n", i - arrayZero, distrib[i]);
		fileLen += strlen(g.strbuf);
		strcat(filebuf, g.strbuf);
	}

	write(fd, filebuf, fileLen + 1);
	fsync(fd);

	delete filebuf;
	close(fd);
	return true;
}

/**
 * Reads the data label and filename of an array to be written
 * from a request passed from the command line. Then matches
 * the dataLabel to the corresponding arrayData which is then
 * passed to a routine that saves the array idendified by the
 * data label.
 */
void processWriteRequest(){
	struct stat buf;

	int rv = stat(arrayData_file, &buf);
	if (rv == -1){
		return;
	}

	int fd = open(arrayData_file, O_RDONLY);
	if (fd == -1){
		return;
	}

	char dataLabel[25];
	char filename[225];

	filename[0] = '\0';
	rv = read(fd, g.strbuf, STRBUF_SZ);
	sscanf(g.strbuf, "%s %s", dataLabel, filename);

	close(fd);
	remove(arrayData_file);

	int arrayLen = sizeof(arrayData) / sizeof(struct saveFileData);
	for (int i = 0; i < arrayLen; i++){
		if (strcmp(dataLabel, arrayData[i].label) == 0){
			if (strlen(filename) == 0){
				strcpy(filename, arrayData[i].filename);
			}
			if (arrayData[i].arrayType == 2){
				saveDoubleArray((double *)arrayData[i].array, filename, arrayData[i].arrayLen, arrayData[i].arrayZero);
				break;
			}
			if (arrayData[i].arrayType == 3){
				writeFrequencyVars(filename);
				break;
			}
			if (arrayData[i].arrayType == 4){
				writeOffsets(filename);
				break;
			}

		}
	}
}

/**
 * Processes the files and config settings specified
 * by the pps-client config file.
 */
void processFiles(char *configVals[], char *pbuf, int sz){

	readConfigFile(configVals, pbuf, sz);

	if (isEnabled(ERROR_DISTRIB, configVals)){
		writeErrorDistribFile();
	}

	if (isEnabled(JITTER_DISTRIB, configVals)){
		writeJitterDistribFile();
	}

	if (isEnabled(CALIBRATE, configVals)){
		g.doCalibration = true;
	}
	else if (isDisabled(CALIBRATE, configVals)){
		g.doCalibration = false;
	}

	if (isEnabled(EXIT_LOST_PPS, configVals)){
		g.exitOnLostPPS = true;
	}
	else if (isDisabled(EXIT_LOST_PPS, configVals)){
		g.exitOnLostPPS = false;
	}

	if (g.doCalibration && isEnabled(INTERRUPT_DISTRIB, configVals)){
		writeInterruptDistribFile();
	}

	if (g.doCalibration && isEnabled(SYSDELAY_DISTRIB, configVals)){
		writeSysdelayDistribFile();
	}

	if (isEnabled(FIX_DELAY_PEAK, configVals)){
		g.fixDelayPeak = true;
	}
	else if (isDisabled(FIX_DELAY_PEAK, configVals)){
		g.fixDelayPeak = false;
	}


	if (isEnabled(SHOW_REMOVE_NOISE, configVals)){
		g.showRemoveNoise = true;
	}
	else if (isDisabled(SHOW_REMOVE_NOISE, configVals)){
		g.showRemoveNoise = false;
	}


//	double value;
//	if (configHasValue(SET_GAIN, configVals, &value)){
//		integralGain1 = value;
//	}

	processWriteRequest();

	return;
}

/**
 * Write g.sysDelay to a temporary file each second.
 *
 * The sysDelay value can be read by other programs
 * that are timing external interrupts.
 */
void writeSysDelay(void){
	memset(g.strbuf, 0, STRBUF_SZ);
	sprintf(g.strbuf, "%d#%d\n", g.sysDelay + g.sysDelayShift, g.seq_num);
	remove(sysDelay_file);
	int pfd = open_logerr(sysDelay_file, O_CREAT | O_WRONLY);
	if (pfd == -1){
		return;
	}
	write(pfd, g.strbuf, strlen(g.strbuf) + 1);		// Write PPS sysDelay to sysDelay_file
	close(pfd);
}

/**
 * Write a timestamp provided as a double to a temporary
 * file each second.
 */
void writeTimestamp(double timestamp){
	memset(g.strbuf, 0, STRBUF_SZ);

	sprintf(g.strbuf, "%lf#%d\n", timestamp, g.seq_num);
	remove(assert_file);

	int pfd = open_logerr(assert_file, O_CREAT | O_WRONLY);
	if (pfd == -1){
		return;
	}
	write(pfd, g.strbuf, strlen(g.strbuf) + 1);		// Write PPS timestamp to assert_file
	close(pfd);
}

/**
 * Records a state params string to a buffer, savebuf, that
 * is saved to a tmpfs memory file by writeStatusStrings()
 * along with other relevant messages recorded during the
 * same second.
 */
void bufferStateParams(void){

	if (g.interruptLossCount == 0) {
		const char *timefmt = "%F %H:%M:%S";
		char timeStr[30];
		char printStr[150];

		strftime(timeStr, 30, timefmt, localtime(&g.pps_t_sec));

		char *printfmt = g.strbuf;

		if (g.sysDelayShift == 0){
			strcpy(printfmt, "%s.%06d  %d  jitter:");
		}
		else {
			strcpy(printfmt, "%s.%06d  %d *jitter:");
		}

		if (g.jitter < 0){
			strcat(printfmt, "%d");
			if (abs(g.jitter) > 9){
				strcat(printfmt, " freqOffset:");
			}
			else {
				strcat(printfmt, "  freqOffset:");
			}
		}
		else if (g.jitter > 9){
			strcat(printfmt, " %d freqOffset:");
		}
		else {
			strcat(printfmt, " %d  freqOffset:");
		}

		if (g.freqOffset < 0.0){
			strcat(printfmt, "%lf  avgCorrection:");
		}
		else {
			strcat(printfmt, " %lf  avgCorrection:");
		}

		if (g.avgCorrection < 0){
			strcat(printfmt, "%f  clamp: %d\n");
		}
		else {
			strcat(printfmt, " %f  clamp: %d\n");
		}

		sprintf(printStr, printfmt, timeStr, g.pps_t_usec, g.seq_num,
				g.jitter, g.freqOffset, g.avgCorrection, g.hardLimit);

		bufferStatusMsg(printStr);
	}
}

/**
 * Restarts NTP with error message handling.
 */
int restartNTP(void){
	int rv = system("service ntp restart > /run/shm/ntp-restart-msg");
	if (rv != 0){
		sprintf(g.logbuf, "\'service ntp restart\' failed with the following message:\n");
		writeToLog(g.logbuf);
	}
	writeFileMsgToLog("/run/shm/ntp-restart-msg");
	return rv;
}

/**
 * Replaces the NTP config file with the text in
 * fbuf using write before replace after first
 * backing up /etc/ntp.conf if the backup does
 * not already exist.
 */
int replaceNTPConfig(char *fbuf){
	int fd = open_logerr(ntp_config_part, O_CREAT | O_RDWR | O_TRUNC);
	if (fd == -1){
		return -1;
	}
	int fSize = strlen(fbuf);

	int wr = write(fd, fbuf, fSize);
	if (wr != fSize){
		close(fd);
		remove(ntp_config_part);
		sprintf(g.logbuf, "ERROR: Write of new \"/etc/ntp.conf\" failed. Original unchanged.\n");
		writeToLog(g.logbuf);
		return -1;
	}
	fsync(fd);
	close(fd);

	int fdb = open(ntp_config_bac, O_RDONLY);
	if (fdb == -1){
		if (errno == ENOENT){
			rename(ntp_config_file, ntp_config_bac);
		}
		else {
			couldNotOpenMsgTo(g.logbuf, ntp_config_bac);
			printf(g.logbuf);
		}
	}
	else {
		close(fdb);
		remove(ntp_config_file);
	}
	rename(ntp_config_part, ntp_config_file);

	return 0;
}

/**
 * Removes all lines containing "key1 key2" from the
 * text in fbuf.
 */
void removeConfigKeys(const char *key1, const char *key2, char *fbuf){

	char *pHead = NULL, *pTail = NULL, *pNxt = NULL;
	char *pLine = NULL;

	pLine = fbuf;

	while (strlen(pLine) > 0){
										// Search for key1 followed by key2
		while (pLine != NULL){										// If this is the next line

			pNxt = pLine;
			while (pNxt[0] == ' ' || pNxt[0] == '\t'){
				pNxt += 1;
			}

			if (strncmp(pNxt, key1, strlen(key1)) == 0){
				pHead = pNxt;
				pNxt += strlen(key1);

				while (pNxt[0] == ' ' || pNxt[0] == '\t'){
					pNxt += 1;
				}

				if (strncmp(pNxt, key2, strlen(key2)) == 0){
					pNxt += strlen(key2);

					while (pNxt[0] == ' ' || pNxt[0] == '\t' ||  pNxt[0] == '\n'){
						pNxt += 1;
					}
					pTail = pNxt;

					memmove(pHead, pTail, strlen(pTail)+1);
					pLine = pHead;								// Point pLine to any remaining
					break;										// tail of the file for removal
				}												// of any more lines containing
			}													// "key1 key2".
			pLine = strchr(pLine, '\n') + 1;
		}
	}
}

/**
 * Disables NTP control of system time by appending
 * "disable ntp" to the NTP config file and
 * then restarting NTP.
 */
int disableNTP(void){
	struct stat stat_buf;

	int fd = open_logerr(ntp_config_file, O_RDONLY);
	if (fd == -1){
		return -1;
	}

	fstat(fd, &stat_buf);
	int sz = stat_buf.st_size;

	char *fbuf = new char[sz + strlen("\ndisable ntp\n") + 1];

	int rv = read_logerr(fd, fbuf, sz, ntp_config_file);
	if (rv == -1 || rv != sz){
		delete fbuf;
		close(fd);
		return -1;
	}
	close(fd);

	sz = strlen(fbuf);
	if (fbuf[sz-1] == '\n'){
		strcat(fbuf, "disable ntp\n");
	}
	else {
		strcat(fbuf, "\ndisable ntp\n");
	}

	rv = replaceNTPConfig(fbuf);
	delete fbuf;

	if (rv == -1){
		return rv;
	}
	rv = restartNTP();
	return rv;
}

/**
 * Enables NTP control of system time by removing
 * "disable ntp" from the NTP config file and
 * then restarting NTP.
 */
int enableNTP(void){
	int rv = 0;
	struct stat stat_buf;
	int fd = 0;

	fd = open_logerr(ntp_config_file, O_RDONLY);
	if (fd == -1){
		return -1;
	}

	fstat(fd, &stat_buf);

	int sz = stat_buf.st_size;
	char *fbuf = new char[sz+1];

	rv = read_logerr(fd, fbuf, sz, ntp_config_file);
	if (rv == -1 || rv != sz){
		delete fbuf;
		close(fd);
		return -1;
	}
	close(fd);
	fbuf[sz] = 0;

	removeConfigKeys("disable", "ntp", fbuf);

	rv = replaceNTPConfig(fbuf);
	delete fbuf;

	if (rv == -1){
		return rv;
	}

	rv = restartNTP();
	return rv;
}

/**
 * Reads the major number assigned to pps-client
 * from "/proc/devices" as a string which is
 * returned in the majorPos char pointer. This
 * value is used to load the hardware driver that
 * pps-client requires to load PPS interrupt times.
 */
char *copyMajorTo(char *majorPos){

	struct stat stat_buf;

	const char *filename = "/run/shm/proc_devices";

	system("cat /proc/devices > /run/shm/proc_devices"); 	// "/proc/devices" can't be handled like
															// a normal file so we copy it to a file.
	int fd = open_logerr(filename, O_RDONLY);
	if (fd == -1){
		return NULL;
	}

	fstat(fd, &stat_buf);
	int sz = stat_buf.st_size;

	char *fbuf = new char[sz+1];

	int rv = read_logerr(fd, fbuf, sz, filename);
	if (rv == -1){
		close(fd);
		remove(filename);
		delete(fbuf);
		return NULL;
	}
	close(fd);
	remove(filename);

	fbuf[sz] = '\0';

	char *pos = strstr(fbuf, "pps-client");
	if (pos == NULL){
		sprintf(g.logbuf, "Can't find pps-client in \"/run/shm/proc_devices\"\n");
		writeToLog(g.logbuf);
		delete fbuf;
		return NULL;
	}
	char *end = pos - 1;
	*end = 0;

	pos -= 2;
	char *pos2 = pos;
	while (pos2 == pos){
		pos -= 1;
		pos2 = strpbrk(pos, num);
	}
	strcpy(majorPos, pos2);

	delete fbuf;
	return majorPos;
}

/**
 * Loads the hardware driver required by pps-client which
 * is expected to be available in the file:
 * "/lib/modules/'uname -r'/kernel/drivers/misc/pps-client.ko".
 */
int driver_load(void){

	char *insmod = g.strbuf;
	strcpy(insmod, "/sbin/insmod /lib/modules/`uname -r`/kernel/drivers/misc/pps-client.ko");

	system("rm -f /dev/pps-client");				// Clean up any old device files.

	system(insmod);									// Issue the insmod command

	char *mknod = g.strbuf;
	strcpy(mknod, "mknod /dev/pps-client c ");
	char *major = copyMajorTo(mknod + strlen(mknod));
	if (major == NULL){								// No major found! insmod failed.
		sprintf(g.logbuf, "driver_load() error: No major found!\n");
		writeToLog(g.logbuf);
		system("/sbin/rmmod pps-client");
		return -1;
	}
	strcat(mknod, " 0");

	system(mknod);									// Issue the mknod command

	system("chgrp root /dev/pps-client");
	system("chmod 664 /dev/pps-client");

	return 0;
}

/**
 * Unloads the pps-client hardware driver.
 */
void driver_unload(void){
	system("/sbin/rmmod pps-client");
	system("rm -f /dev/pps-client");
}

/**
 * Extracts the sequence number g.seq_num from
 * buf which contains second-by-second params
 * loaded from displayParams_file and returns
 * the value.
 */
int getSeqNum(const char *pbuf){

	char *pSpc, *pNum;
	int seqNum = 0;

	pSpc = strpbrk((char *)pbuf, space);			// Compiler bug: Fails if pbuf is not cast to (char *).
													// Looks like strpbrk arg 1 is implemented as (char *) not (const char *).
	pNum = strpbrk(pSpc, num);
	pSpc = strpbrk(pNum, space);
	pNum = strpbrk(pSpc, num);

	sscanf(pNum, "%d ", &seqNum);
	return seqNum;
}

/**
 * Reads the state params saved to shared memory by the
 * pps-client daemon and prints the param string to the
 * console each second.
 */
void showStatusEachSecond(void){
	struct timeval tv1;
	struct timespec ts2;
	char paramsBuf[MSGBUF_SZ];
	struct stat stat_buf;
	int seqNum, lastSeqNum = -1;

	int dispTime = 500000;									// Display at half second

	gettimeofday(&tv1, NULL);
	ts2 = setSyncDelay(dispTime, tv1.tv_usec);

	for (;;){
		if (g.exit_loop){
			break;
		}
		nanosleep(&ts2, NULL);

		int fd = open(displayParams_file, O_RDONLY);

		fstat(fd, &stat_buf);
		int sz = stat_buf.st_size;

		if (sz >= MSGBUF_SZ){
			printf("showStatusEachSecond() buffer too small. sz: %d\n", sz);
			close(fd);
			break;
		}

		read(fd, paramsBuf, sz);
		close(fd);

		paramsBuf[sz]= '\0';

		seqNum = getSeqNum(paramsBuf);

		if (seqNum != lastSeqNum){
			printf(paramsBuf);
		}
		lastSeqNum = seqNum;

		gettimeofday(&tv1, NULL);

		ts2 = setSyncDelay(dispTime, tv1.tv_usec);
	}
	printf("Exiting pps-client params display\n");
}

/**
 * Responds to the ctrl-c key combination by setting
 * the exit_loop flag. This causes an exit from the
 * showStatusEachSecond() function.
 */
void INThandler(int sig){
	g.exit_loop = true;
}

/**
 * Checks for and reports on missing arguments in a
 * command line request.
 */
bool missingArg(int argc, char *argv[], int i){
	if (i == argc - 1 || argv[i+1][0] == '-'){
		printf("Error: Missing argument for %s.\n", argv[i]);
		return true;
	}
	return false;
}

/**
 * Transmits a data save request to the pps-client daemon via
 * data written to a tmpfs shared memory file. This will be
 * read in the daemon by
 */
bool daemonSaveArray(const char *dataLabel, const char *filename){
	char buf[200];

	int fd = open_logerr(arrayData_file, O_CREAT | O_WRONLY | O_TRUNC);
	if (fd == -1){
		printf("Error: Open arrayData_file failed.\n");
		return false;
	}

	strcpy(buf, dataLabel);

	if (filename != NULL){
		strcat(buf, " ");
		strcat(buf, filename);
	}

	write(fd, buf, strlen(buf) + 1);

	close(fd);
	return true;
}

/**
 * Prints a list to the terminal of the command line
 * args for saving data that are recognized by pps-client.
 */
void printAcceptedArgs(void){
	printf("Accepts these (as a quoted string):\n");
	int arrayLen = sizeof(arrayData) / sizeof(struct saveFileData);
	for (int i = 0; i < arrayLen; i++){
		printf("\"%s\"\n", arrayData[i].label);
	}
}

/**
 * Reads a command line save data request and either forwards
 * the data to the daemon interface or prints entry errors
 * back to the terminal.
 */
bool parseSaveDataRequest(int argc, char *argv[], const char *dataLabel){

	int arrayLen = sizeof(arrayData) / sizeof(struct saveFileData);

	int i;
	for (i = 0; i < arrayLen; i++){
		if (strcmp(dataLabel, arrayData[i].label) == 0){
			break;
		}
	}
	if (i == arrayLen){
		printf("Arg \"%s\" not recognized\n", argv[i+1]);
		printAcceptedArgs();
		return false;
	}

	char *filename = NULL;
	for (int j = 1; j < argc; j++){
		if (strcmp(argv[j], "-f") == 0){
			if (missingArg(argc, argv, j)){
				printf("Requires a filename.\n");
				return false;
			}
			strncpy(g.strbuf, argv[j+1], STRBUF_SZ);
			g.strbuf[strlen(argv[j+1])] = '\0';
			filename = g.strbuf;
			break;
		}
	}

	if (filename != NULL){
		printf("Writing to file: %s\n", filename);
	}
	else {
		for (i = 0; i < arrayLen; i++){
			if (strcmp(dataLabel, arrayData[i].label) == 0){
				printf("Writing to default file: %s\n", arrayData[i].filename);
			}
		}
	}

	if (daemonSaveArray(dataLabel, filename) == false){
		return false;
	}
	return true;
}

/**
 * Provides command line access to the pps-client daemon.
 *
 * Checks if program is running. If not, returns -1.
 * If an error occurs returns -2. If the program is
 * running then returns 0 and prints a message to that
 * effect.
 *
 * Recognizes data save requests (-s) and forwards these
 * to the daemon interface.
 *
 * If verbose flag (-v) is read then also displays status
 * params of the running program to the terminal.
 *
 */
int accessDaemon(int argc, char *argv[]){
	bool verbose = false;

	if (! ppsIsRunning()){						// If not running,
		remove(pidFilename);					// remove a zombie pidFilename if one is present.
		return -1;
	}

	signal(SIGINT, INThandler);					// Set handler to enable exiting with ctrl-c.

	printf("pps-client v%s is running.\n", version);

	if (argc > 1){

		for (int i = 1; i < argc; i++){
			if (strcmp(argv[i], "-v") == 0){
				verbose = true;
			}
		}
		for (int i = 1; i < argc; i++){
			if (strcmp(argv[i], "-s") == 0){	// This is a save data request.
				if (missingArg(argc, argv, i)){
					printAcceptedArgs();
					return -2;
				}

				if (parseSaveDataRequest(argc, argv, argv[i+1]) == false){
					return -2;
				}
				break;
			}
		}
	}

	if (verbose){
		printf("Displaying second-by-second state params (ctrl-c to quit):\n");
		showStatusEachSecond();
	}

	return 0;
}



