//
// emsMqtt.c - send ems values to mqtt broker
//
// $Id: emsMqtt.c 37 2021-08-17 22:00:53Z juh $

#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <mosquitto.h>
#include "ems.h"

// forward declarations

int initMosquitto(ems *emsPtr);
void mqttPublish(ems *emsPtr, char *topic, void *val, int qos, bool retain);
int getConfig(enum varType, void *var, char *defVal, char *cFile, char *group, char *key);
void SIGgen_handler_mqtt(int);

// (module-)global vars
int LastboilerState;
time_t LastBoilerOn, LastBoilerOff;

#define SVN "$Id: emsMqtt.c 37 2021-08-17 22:00:53Z juh $"

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
	    fprintf (stderr, "emsMqtt, svn rev: %s\n", SVN_REV);
#else
	    fprintf (stderr, "emsMqtt, svn info: %s\n", SVN);
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
#define DAEMON_NAME "ems-mqtt"
    sprintf(DaemonName, "%s", DAEMON_NAME);
    
    if (Daemon) {
	//Set our Logging Mask and open the Log
	//setlogmask(LOG_UPTO(LOG_NOTICE));
	openlog(DaemonName, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
	syslog(LOG_INFO, "emsMqtt running as daemon");
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
	emsPtr->heartbeatMqtt = time(NULL);
    } else {
	sprintf(message,  "%s: could not attach shared memory at line %d, errno = %d (%s)",
		DaemonName, __LINE__, errno, strerror(errno));
	LOGERR(message);
	_exit(1);
    }

    // get broker settings from config file
    result = getConfig(CHAR, &emsPtr->broker, BROKER, CONFIGFILE, "EMS", "broker");
    if (result)
	sprintf(message, "%s: broker not defined, set to default >%s<", DaemonName, BROKER);
    else
	sprintf(message, "%s: broker set to >%s< ", DaemonName, emsPtr->broker);
    LOGIT(message);
    result = getConfig(INT, &emsPtr->port, "1883", CONFIGFILE, "EMS", "port");
    if (result)
	sprintf(message, "%s: port not defined, set to default >%d<", DaemonName, 1883);
    else
	sprintf(message, "%s: broker set to >%d< ", DaemonName, emsPtr->port);
    LOGIT(message);
    
    // install signal handler for other signals (USR1, SEGV,...)
    if (signal(SIGUSR1, SIGgen_handler_mqtt) == SIG_ERR) {
	sprintf(message, "%s: SIGUSR1 install error", DaemonName);
	LOGERR(message);
	_exit(errno);
    }
    if (signal(SIGSEGV, SIGgen_handler_mqtt) == SIG_ERR) {
	sprintf(message, "%s: SIGSEGV install error", DaemonName);
	LOGERR(message);
	_exit(errno);
    }
    if (signal(SIGTERM, SIGgen_handler_mqtt) == SIG_ERR) {
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
		fp = fopen("/run/emsMqtt.pid", "w");
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
	    emsPtr->mqttPid = sid;
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

    // initialize mqtt
    result = mosquitto_lib_init();
    result = initMosquitto(emsPtr);

    sleep(20);  // let decode process decode something...
    
    for (;;) {
	// just to slow down a bit
	sleep(1);

	// get actual time
	currentTime = time(NULL);

	// tell we're alive
	emsPtr->heartbeatMqtt = currentTime;
	
	// check if ems process is running
	if (currentTime > emsPtr->heartbeatDecode + 60) {
	    sprintf(message,
		    "%s: ems decode process did not update his heatbeat for more than 60s, should (but will not) terminate", DaemonName);
	    LOGERR(message);
	    //exit (42);
	}
	
	// check for boilerState change
	if (LastboilerState != emsPtr->boilerState) {
	    // boilerstate changed
	    if (emsPtr->boilerState == 1) {
		// was off, switched on
		LastBoilerOn = currentTime;
	    }
	    else {
		// was on, switched off
		LastBoilerOff = currentTime;
		// check, if we should calculate the duration
		if (LastBoilerOff > LastBoilerOn && LastBoilerOn > 0) {
		    duration = LastBoilerOff - LastBoilerOn;
		    /*	    
			if (emsPtr->consumption > 0.0)
			    consumption = duration * emsPtr->consumption / 1000.0;
			else
			    consumption = 0.0;
		    */
		}
	    }
	    LastboilerState = emsPtr->boilerState;
	}

	lastTime = currentTime;

	// check if mqtt is available
	if (!emsPtr->mqttAvail) {
	    // re-initialize mqtt
	    result = initMosquitto(emsPtr);
	    sprintf(message, "%s: re-initialzed mosquitto, result %d", DaemonName, result);
	    LOGERR(message);
	}

	// publish status to mqtt server
	sprintf(value, "%d", emsPtr->status);
	sprintf(topic, "ems/status");
	mqttPublish(emsPtr, topic, value, 1, true);

	/*
	// if duration > 0, we should send a new operation interval
	if (duration > 0) {
	    sprintf(value, "%.2f", consumption);
	    sprintf(topic, "ems/consumption");
	    mqttPublish(emsPtr, topic, value, 1, false);
	    sprintf(value, "%d", duration);
	    sprintf(topic, "ems/duration");
	    mqttPublish(emsPtr, topic, value, 1, false);
	    // and clear duration again
	    duration = 0;
	    }*/
	
	sprintf(value, "%d", emsPtr->boilerState);
	mqttPublish(emsPtr, "ems/boilerState", value, 1, false);

	sprintf(value, "%d", emsPtr->waterState);
	mqttPublish(emsPtr, "ems/waterState", value, 1, false);
		
	sprintf(value, "%d", emsPtr->pump);
	mqttPublish(emsPtr, "ems/pump", value, 1, false);
	
	sprintf(value, "%d", emsPtr->circPump);
	mqttPublish(emsPtr, "ems/circPump", value, 1, false);
	
	sprintf(value, "%d", emsPtr->circState1);
	mqttPublish(emsPtr, "ems/circState1", value, 1, false);
	
	sprintf(value, "%d", emsPtr->circState2);
	mqttPublish(emsPtr, "ems/circState2", value, 1, false);

	sprintf(value, "%d", emsPtr->code1);
	mqttPublish(emsPtr, "ems/code1", value, 1, false);
		
	sprintf(value, "%d", emsPtr->code2);
	mqttPublish(emsPtr, "ems/code2", value, 1, false);
		
	sprintf(value, "%d", emsPtr->model);
	mqttPublish(emsPtr, "ems/model", value, 1, false);
		
	sprintf(value, "%d", emsPtr->error1);
	mqttPublish(emsPtr, "ems/error1", value, 1, false);
		
	sprintf(value, "%d", emsPtr->error2);
	mqttPublish(emsPtr, "ems/error2", value, 1, false);
		
	sprintf(value, "%d", emsPtr->error3);
	mqttPublish(emsPtr, "ems/error3", value, 1, false);
		
	sprintf(value, "%d", emsPtr->errorCode);
	mqttPublish(emsPtr, "ems/errorCode", value, 1, false);
		
	sprintf(value, "%d", emsPtr->ubaCode);
	mqttPublish(emsPtr, "ems/ubaCode", value, 1, false);
		
	sprintf(value, "%d", emsPtr->burnerCode);
	mqttPublish(emsPtr, "ems/burnerCode", value, 1, false);
		
	sprintf(value, "%d", emsPtr->loadingPump);
	mqttPublish(emsPtr, "ems/loadingPump", value, 1, false);
	
	sprintf(value, "%d", emsPtr->power);
	mqttPublish(emsPtr, "ems/power", value, 1, false);
	
	sprintf(value, "%d", emsPtr->starts);
	mqttPublish(emsPtr, "ems/starts", value, 1, false);
	
	sprintf(value, "%d", emsPtr->opTime);
	mqttPublish(emsPtr, "ems/opTime", value, 1, false);
	
	sprintf(value, "%.2f", emsPtr->setWaterTemp);
	mqttPublish(emsPtr, "ems/setWaterTemp", value, 1, false);
	
	sprintf(value, "%.2f", emsPtr->setTemperature);
	mqttPublish(emsPtr, "ems/setTemperature", value, 1, false);
	
	sprintf(value, "%.2f", emsPtr->tempBoiler);
	mqttPublish(emsPtr, "ems/tempBoiler", value, 1, false);

	sprintf(value, "%.2f", emsPtr->tempExhaust);
	mqttPublish(emsPtr, "ems/tempExhaust", value, 1, false);

    	sprintf(value, "%.2f", emsPtr->tempWater);
	mqttPublish(emsPtr, "ems/tempWater", value, 1, false);

    	sprintf(value, "%.2f", emsPtr->tempOutside);
	mqttPublish(emsPtr, "ems/tempOutside", value, 1, false);
	
    	sprintf(value, "%.2f", emsPtr->tempInside);
	mqttPublish(emsPtr, "ems/tempInside", value, 1, false);

    	sprintf(value, "%.2f", emsPtr->current);
	mqttPublish(emsPtr, "ems/current", value, 1, false);
    }
}

void  SIGgen_handler_mqtt(int sig)
{
    //signal(sig, SIG_IGN);
    char message[1000];

    switch (sig)
	{
	case SIGUSR1:
	    if (Debug) {
		sprintf(message,"%s/SIGgen_handler_mqtt: switch debug mode off, got signal %d (%s) ", DaemonName, sig, strsignal(sig));
		Debug = false;
	    }
	    else {
		sprintf(message,"%s/SIGgen_handler_mqtt: switch debug mode on, got signal %d (%s) ", DaemonName, sig, strsignal(sig));
		Debug = true;
	    }
	    LOGIT(message);
	    break;

	case SIGINT:
	    sprintf(message,"%s/SIGgen_handler_mqtt: got signal %d (%s), terminating", DaemonName, sig, strsignal(sig));
	    LOGERR(message);
	    exit (1);
	    break;

	case SIGSEGV:
	    sprintf(message,"%s/SIGgen_handler_mqtt: got signal %d (%s) ", DaemonName, sig, strsignal(sig));
	    LOGERR(message);
	    exit (11);
	    break;

	case SIGTERM:
	    sprintf(message,"%s/SIGgen_handler_mqtt: got signal %d (%s), terminating", DaemonName, sig, strsignal(sig));
	    LOGIT(message);
	    exit (0);
	    break;

	default:
	    break;
	}
}

