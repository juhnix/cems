//
// emsMsb.c - send ems values to msb
//
// $Id: emsMsb.c 64 2022-11-24 21:45:19Z juh $

#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <mosquitto.h>
#include "ems.h"

// forward declarations

int initMsb(ems *emsPtr);
int msb(ems *emsPtr);
int getConfig(enum varType, void *var, char *defVal, char *cFile, char *group, char *key);
void SIGgen_handler_msb(int);
extern int usleep (__useconds_t __useconds);

// (module-)global vars
int LastboilerState;
time_t LastBoilerOn, LastBoilerOff;

#define SVN "$Id: emsMsb.c 64 2022-11-24 21:45:19Z juh $"

int main (int argc, char** argv) {
    key_t key = SHMKEY;
    pid_t daemonPid = 0;
    pid_t sid;
    time_t currentTime, lastTime, duration = 0;
    float consumption = 0.0;
    FILE *fp;
    int tries = 0;
    int i, c, result, second = false;
    char value[100], topic[500], message[500];
    unsigned long timeSteps[] = {60, 300, 600, 900 };
    int loop, j;
    float average[4][4];
    char filename[MAXPATH];

    // default: run as daemon
    Daemon = 1;

    while ((c = getopt(argc, argv, "vnhV")) != -1) {
	switch (c) {
	case 'v': // be verbose
	    Debug = 1;
	    break;
	    
	case 'V': // show version and exit
#ifdef SVN_REV
	    fprintf (stderr, "emsMsb, svn rev: %s\n", SVN_REV);
#else
	    fprintf (stderr, "emsMsb, svn info: %s\n", SVN);
#endif
	    exit (0);
	    break;
	    
	case 'n': // run in foreground
	    Daemon = 0;
	    break;

	case 'h':
	case '?':
	    fprintf (stderr, "%s:\tOption -v activates debug mode,\n", argv[0]);
	    fprintf (stderr, "\tOption -n disables daemon mode\n");
	    fprintf (stderr, "\tOption -x terminates running daemon\n");
	    fprintf (stderr, "\tOption -?/-h show this information\n");
	    fprintf (stderr, "\tOption -V shows the version information\n");
	    fprintf (stderr, "signalling with SIGUSR1 will enable debug mode (each process separately switchable)\n");
	    fprintf (stderr, "signalling main process with SIGUSR2 will reread configuration (/usr/local/etc/ems.cfg)\n");
	    exit(0);
	    break;
	    
	default:
	    exit(0);
	}
    }

#undef DAEMON_NAME
#define DAEMON_NAME "emsMsb"
    sprintf(DaemonName, "%s", DAEMON_NAME);
    
    if (Daemon) {
	//Set our Logging Mask and open the Log
	//setlogmask(LOG_UPTO(LOG_NOTICE));
	openlog(DaemonName, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
	syslog(LOG_INFO, "emsMsb running as daemon");
    }	
    // try to get shared memory, if it already exists...
 retry:
    ShmId = shmget(key, sizeof(ems), 0);
    if (ShmId < 0) {
	sprintf(message,
		"%s: could not get shared memory, shmid = %d, error %d, %s. Try %d / 30",
		DaemonName, ShmId, errno, strerror(errno), tries);
	LOGERR(message);
	sleep(10);
	if (tries++ < 30) // wait for about 5 minutes
	    goto retry;
	else
	    _exit(errno);
    }
    else {
	emsPtr = shmat(ShmId, NULL, 0);
	sprintf(message,
		"%s: attached shared memory, shmid = %d, ptr = %x",
		DaemonName, ShmId, (int)emsPtr);
	LOGIT(message);
    }

    if (emsPtr != (void *) -1) {
	// init status vars
	emsPtr->heartbeatMsb = time(NULL);
    } else {
	sprintf(message,  "%s: could not attach shared memory at line %d, errno = %d (%s)",
		DaemonName, __LINE__, errno, strerror(errno));
	LOGERR(message);
	_exit(1);
    }

    // get msb settings from config file
    result = getConfig(CHAR, &emsPtr->msbUrl, "192.168.17.1", CONFIGFILE, "V4K", "url");
    if (result)
	sprintf(message, "%s: url not defined, set to default >%s<", DaemonName, "192.168.17.1");
    else
	sprintf(message, "%s: url set to >%s< ", DaemonName, emsPtr->msbUrl);
    LOGIT(message);
    result = getConfig(CHAR, &emsPtr->msbPort, "", CONFIGFILE, "V4K", "port");
    if (result)
	sprintf(message, "%s: port not defined, set to default >%s<", DaemonName, "");
    else
	sprintf(message, "%s: port set to >%s< ", DaemonName, emsPtr->msbPort);
    LOGIT(message);
    result = getConfig(CHAR, &emsPtr->msbCert, "", CONFIGFILE, "V4K", "cert");
    if (result)
	sprintf(message, "%s: cert not defined, set to default >%s<", DaemonName, "");
    else
	sprintf(message, "%s: cert set to >%s< ", DaemonName, emsPtr->msbCert);
    LOGIT(message);
    result = getConfig(CHAR, &emsPtr->msbUuid, UUID, CONFIGFILE, "V4K", "uuid");
    if (result)
	sprintf(message, "%s: uuid not defined, set to default >%s<", DaemonName, UUID);
    else
	sprintf(message, "%s: uuid set to >%s< ", DaemonName, emsPtr->msbUuid);
    LOGIT(message);
    result = getConfig(CHAR, &emsPtr->msbToken, TOKEN, CONFIGFILE, "V4K", "token");
    if (result)
	sprintf(message, "%s: token not defined, set to default >%s<", DaemonName, TOKEN);
    else
	sprintf(message, "%s: token set to >%s< ", DaemonName, emsPtr->msbToken);
    LOGIT(message);
    result = getConfig(CHAR, &emsPtr->msbClass, CLASS, CONFIGFILE, "V4K", "class");
    if (result)
	sprintf(message, "%s: class not defined, set to default >%s<", DaemonName, CLASS);
    else
	sprintf(message, "%s: class set to >%s< ", DaemonName, emsPtr->msbClass);
    LOGIT(message);
    result = getConfig(CHAR, &emsPtr->msbName, NAME, CONFIGFILE, "V4K", "name");
    if (result)
	sprintf(message, "%s: name not defined, set to default >%s<", DaemonName, NAME);
    else
	sprintf(message, "%s: name set to >%s< ", DaemonName, emsPtr->msbName);
    LOGIT(message);
    result = getConfig(CHAR, &emsPtr->msbDescription, "n.n.", CONFIGFILE, "V4K", "description");
    if (result)
	sprintf(message, "%s: description not defined, set to default >%s<", DaemonName, "n.n.");
    else
	sprintf(message, "%s: description set to >%s< ", DaemonName, emsPtr->msbDescription);
    LOGIT(message);
    result = getConfig(INT, &emsPtr->interval, "1000000", CONFIGFILE, "V4K", "interval");
    if (result)
	sprintf(message, "%s: interval not defined, set to default >%d<", DaemonName, 1000000);
    else
	sprintf(message, "%s: interval set to >%d< ", DaemonName, emsPtr->interval);
    LOGIT(message);
    
    // install signal handler for other signals (USR1, SEGV,...)
    if (signal(SIGUSR1, SIGgen_handler_msb) == SIG_ERR) {
	sprintf(message, "%s: SIGUSR1 install error", DaemonName);
	LOGERR(message);
	_exit(errno);
    }
    if (signal(SIGSEGV, SIGgen_handler_msb) == SIG_ERR) {
	sprintf(message, "%s: SIGSEGV install error", DaemonName);
	LOGERR(message);
	_exit(errno);
    }
    if (signal(SIGTERM, SIGgen_handler_msb) == SIG_ERR) {
	sprintf(message, "%s: SIGTERM install error", DaemonName);
	LOGERR(message);
	_exit(errno);
    }
    
    if (Daemon) {
	// create process
	daemonPid = fork();

	if (daemonPid < 0) {
	    syslog(LOG_ERR, "could not fork %s daemon process", DaemonName);
	    _exit(errno);
	}
	else {
	    // check if parent or son
	    if (daemonPid == 0) {
		// son, will continue
		syslog(LOG_INFO, "%s daemon started", DaemonName);
	    }
	    else {
		fp = fopen("/run/emsMsb.pid", "w");
		if (fp == NULL)
		    _exit(errno);
		else {
		    fprintf(fp, "%d\n", daemonPid);
		    fclose(fp);
		}
		// parent must die to be able to detach controlling tty
		exit(0);
	    }
	}
	// get pid of this (forked) process
	sid = getpid();
	if (sid < 0) {
	    _exit(errno);
	}
	else {
	    emsPtr->msbPid = sid;
	}
	sprintf(message, "%s: running with pid %d", DaemonName, sid);
	LOGIT(message);

	//Change Directory
	//If we cant find the directory we exit with failure.
	//if ((chdir("/")) < 0) { exit(EXIT_FAILURE); }
	
	//Close Standard File Descriptors
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	sprintf(message, "%s: closed stdin, stdout and stderr", DaemonName);
	LOGIT(message);
    } // daemon mode
    else {
	fprintf(stderr, "%s: running in foreground\n", DaemonName);
    }

    // init state check
    LastboilerState = emsPtr->boilerState;

    // initialize msb
    result = initMsb(emsPtr);

    for (;;) {
	// get actual time
	currentTime = time(NULL);

	// tell we're alive
	emsPtr->heartbeatMsb = currentTime;
	
	// check if ems process is running
	if (currentTime > emsPtr->heartbeatDecode + 60) {
	    sprintf(message,
		    "%s: ems decode process did not update his heatbeat for more than 60s, should (but will not) terminate", DaemonName);
	    LOGERR(message);
	    //exit (42);
	}

	// send values to msb
	result = msb(emsPtr);

	// wait for interval
	usleep(emsPtr->interval);
    }
}

void  SIGgen_handler_msb(int sig)
{
    //signal(sig, SIG_IGN);
    char message[1000];

    switch (sig)
	{
	case SIGUSR1:
	    if (Debug) {
		sprintf(message,"%s/SIGgen_handler_msb: switch debug mode off, got signal %d (%s) ", DaemonName, sig, strsignal(sig));
		Debug = false;
	    }
	    else {
		sprintf(message,"%s/SIGgen_handler_msb: switch debug mode on, got signal %d (%s) ", DaemonName, sig, strsignal(sig));
		Debug = true;
	    }
	    LOGIT(message);
	    break;

	case SIGINT:
	    sprintf(message,"%s/SIGgen_handler_msb: got signal %d (%s), terminating", DaemonName, sig, strsignal(sig));
	    LOGERR(message);
	    exit (1);
	    break;

	case SIGSEGV:
	    sprintf(message,"%s/SIGgen_handler_msb: got signal %d (%s), line %d ", DaemonName, sig, strsignal(sig), Line);
	    LOGERR(message);
	    exit (11);
	    break;

	case SIGTERM:
	    sprintf(message,"%s/SIGgen_handler_msb: got signal %d (%s), terminating", DaemonName, sig, strsignal(sig));
	    LOGIT(message);
	    exit (0);
	    break;

	default:
	    break;
	}
}

