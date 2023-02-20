// emsDecode.c
//
// $Id: emsDecode.c 65 2022-11-24 22:00:31Z juh $
//

#define _POSIX_C_SOURCE 200809L

#include "ems.h"
#include "emsDevices.h"

#define LEN 8192

// forward declarations
int getConfig(enum varType, void *var, char *defVal, char *cFile, char *group, char *key);
extern int usleep (__useconds_t __useconds);

// global vars to keep track of opTime and starts
long int OpTime = 0;
long int Starts = 0;

int main(int argc , char *argv[])
{
    mqd_t fd;
    char buff[LEN], message[MAXPATH], message2[MAXPATH], queueName[MAXNAME];
    int sterr, i, c, len, result;
    float temp, temp2, current, nightTemp, dayTemp, holidayTemp;
    int power, intval, setTemp, setWater, year, month, day, hour, minute, second, dst, dayOfWeek;
    long int starts, opTime;
    int status, hcMode, summerThreshold;
    key_t key = SHMKEY;
    pid_t daemonPid = 0;
    pid_t sid;
    time_t currentTime, lastTime, duration = 0, liveSign;
    float consumption = 0.0;
    FILE *fp;
    int tries = 0;

    // default: run as daemon, no debug output    
    Daemon = true;
    Debug = false;

#define DAEMON_NAME "emsDecode"
    sprintf(DaemonName, "%s", DAEMON_NAME);

    while ((c = getopt (argc, argv, "vnhr:")) != -1) {
        switch (c) {
        case 'v': // be verbose
            Debug = true;
            break;
        case 'n': // don't daemonize
            Daemon = false;
            break;
	case 'r':
	    strncpy(queueName, optarg, MAXNAME);
	    break;
        case 'h':
        case '?':
            fprintf (stderr, "%s:\n\tOption -v activates debug mode,\n", argv[0]);
            fprintf (stderr, "\tOption -?/-h show this information\n");
            fprintf (stderr, "\tOption -V show the version information\n");
            fprintf (stderr, "\tOption -r # sets receive message queue name to #\n");
            exit(0);
            break;
            
        default:
            break;
        }
    }

    if (Daemon) {
        //Set our Logging Mask and open the Log
        //setlogmask(LOG_UPTO(LOG_NOTICE));
        openlog(DaemonName, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
        syslog(LOG_INFO, "%s running as daemon", DaemonName);
    }   

    // try to get shared memory, if it already exists or create it
    ShmId = shmget(key, sizeof(ems), 0);
    if (ShmId < 0) {
        sprintf(message,
                "%s: could not get shared memory, shmid = %d, error %d, %s. Trying to create it...",
                DaemonName, ShmId, errno, strerror(errno));
        LOGERR(message);
	// try to create it
	ShmId = shmget(key, sizeof(ems), IPC_CREAT | 0666);
	if (errno != 0 && ShmId < 0) {
	    sprintf(message, "%s: could not create shared memory, error %s, errno = %d, shmid = %d",
		    DaemonName, strerror(errno), errno, ShmId);
	    LOGERR(message);
	    exit (errno);
	}
    }

    emsPtr = shmat(ShmId, NULL, 0);
    sprintf(message,
	    "%s: attached shared memory, shmid = %d, ptr = %x",
	    DaemonName, ShmId, (int)emsPtr);
    LOGIT(message);

    if (emsPtr != (void *) -1) {
        // init status vars
        emsPtr->heartbeatDecode = time(NULL);
    } else {
        sprintf(message,  "%s: could not attach shared memory at line %d, errno = %d (%s)",
                DaemonName, __LINE__, errno, strerror(errno));
        LOGERR(message);
        _exit(1);
    }

    // check for name of receive message queue
    if (strlen(emsPtr->rxqueue) > 0) {
	sprintf(message, "%s: rxqueue already set to >%s< ", DaemonName, emsPtr->rxqueue);
    } else {
	result = getConfig(CHAR, &emsPtr->rxqueue, RX_QUEUE_NAME, CONFIGFILE, "EMS", "rxqueue");
	if (result)
	    sprintf(message, "%s: rxqueue not defined, set to default >%s<", DaemonName, RX_QUEUE_NAME);
	else
	    sprintf(message, "%s: rxqueue set to >%s< ", DaemonName, emsPtr->rxqueue);
    }
    LOGIT(message);

    // open queue for received packets
    fd = mq_open(emsPtr->rxqueue, O_RDONLY);
    if (fd == -1) {
	sterr = errno;
	sprintf(message, "%s: couldn't open the message queue. Error : %s\n", DaemonName, strerror(sterr));
	LOGERR(message);
	exit(sterr);
    }

    if (Daemon) {
        //Set our Logging Mask and open the Log
        //setlogmask(LOG_UPTO(LOG_NOTICE));
        openlog(DaemonName, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
        syslog(LOG_INFO, "%s running as daemon", DaemonName);
    }   

    // try to get shared memory, if it already exists or create it
    ShmId = shmget(key, sizeof(ems), 0);
    if (ShmId < 0) {
        sprintf(message,
                "%s: could not get shared memory, shmid = %d, error %d, %s. Try %d / 30",
                DaemonName, ShmId, errno, strerror(errno), tries);
        LOGERR(message);
	// try to create it
	ShmId = shmget(key, sizeof(ems), IPC_CREAT | 0666);
	if (errno != 0 && ShmId < 0) {
	    sprintf(message, "decodeEms: could not create shared memory, error %s, errno = %d, shmid = %d", strerror(errno), errno, ShmId);
	    LOGERR(message);
	    exit (errno);
	}
    }

    emsPtr = shmat(ShmId, NULL, 0);
    sprintf(message,
	    "%s: attached shared memory, shmid = %d, ptr = %x",
	    DaemonName, ShmId, (int)emsPtr);
    LOGIT(message);

    if (emsPtr != (void *) -1) {
        // init status vars
        emsPtr->heartbeatDecode = time(NULL);
    } else {
        sprintf(message,  "%s: could not attach shared memory at line %d, errno = %d (%s)",
                DaemonName, __LINE__, errno, strerror(errno));
        LOGERR(message);
        _exit(1);
    }

    // clear receive buffer    
    memset(buff, 0, LEN);

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
		fp = fopen("/run/emsDecode.pid", "w");
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
	    emsPtr->decodePid = sid;
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

    for (;;) {
	len = mq_receive(fd, buff, sizeof(buff), NULL);
	if (len == -1) {
	    if (Debug) {
		sprintf(message, "%s: message could not be received", DaemonName);
		LOGIT(message);
	    }
	    usleep(100000);
	}   
	else {
	    if (Debug) {
		sprintf(message, "%s: received message: %d bytes,", DaemonName, len);
		for (i = 0; i < len; i++) {
		    sprintf(message2, " %02x", buff[i]);
		    strcat(message, message2);
		}
		LOGIT(message);
	    }
	    emsPtr->heartbeatDecode = time(NULL);
	    switch (buff[0]) { // from
	    case 0x08:
		// MC110
		sprintf(message, "MC110: ");
		switch (buff[1]) { // to
		case 0:
		    // to ALL
		    switch (buff[2]) { // ems telegram type
		    case 0xbf:
			// UBAErrorMessage
			// model type byte 5, err1 byte 9, err2 byte 10, err3 byte 11, errdec byte 12/13
			emsPtr->model = buff[5];
			emsPtr->error1 = buff[9];
			emsPtr->error2 = buff[10];
			emsPtr->error3 = buff[11];
			intval = (int)(256 * buff[12] + buff[13]);
			emsPtr->errorCode = intval;
			sprintf(message2, " model %02x, errcode %02x %02x %02x, status %d",
				buff[5], buff[9], buff[10], buff[11], intval);
			snprintf(message, MAXPATH - strlen(message2),
				 " from MC110: UBAErrorMessage, %s", message2);
			LOGERR(message);
			break;
		    case 0xd1:
			// UBAOutdoorTempMessage
			// outdoor temp at byte 4/5 [0.1°C]
			intval = (int)(256 * buff[4] + buff[5]);
			temp = (float)intval / 10.0;
			emsPtr->tempOutside = temp;
			sprintf(message2, " outdoor %2.1f °C", temp);
			snprintf(message, MAXPATH - strlen(message2),
				 " from MC110: UBAOutdoorTempMessage, %s", message2);
			if (Debug)
			    LOGIT(message);
			break;
		    case 0xe3:
			// unknown message
			//Boiler temp at byte 15/16 [0.1°C], power byte 17 [%]
			intval = (int)(256 * buff[15] + buff[16]);
			temp = (float)intval / 10.0;
			power = (int)buff[17];
			sprintf(message2, " boiler %2.1f °C, power %d %%",
				temp, power);
			snprintf(message, MAXPATH - strlen(message2),
				 " from MC110: unknown type e3, %s", message2);
			if (Debug)
			    LOGIT(message);
			break;
		    case EMS_TYPE_UBAMonitorFast:
			// UBAMonitorFast
			if (buff[3] == 0x0) {
			    // boiler temp at byte 11/12 [0.1°C], power byte 14 [%], code byte 15
			    emsPtr->setTemperature = (float)buff[10];
			    intval = (int)(256 * buff[11] + buff[12]);
			    temp = (float)intval / 10.0;
			    emsPtr->tempBoiler = temp;
			    power = (int)buff[14];
			    emsPtr->power = power;
			    emsPtr->loadingPump = (int)(buff[15] & 0x4); // bit 2
			    emsPtr->ubaCode = buff[15];
			    intval = (int)(256 * buff[23] + buff[24]);
			    current = (float)intval / 10.0;
			    emsPtr->current = current;
			    sprintf(message2, " boiler %2.1f °C, power %d %%, current %2.1f µA", temp, power, current);
			    snprintf(message, MAXPATH - strlen(message2),
				     " from MC110: UBAMonitorFast, %s", message2);
			    liveSign = emsPtr->heartbeatDecode % 600; 
			    if (Debug || (liveSign == 0))
				LOGIT(message);
			} else if (buff[3] == 0x1b) {
			    // exhaust temp at byte 8/9 [0.1°C], intake at byte 4/5 ?
			    intval = (int)(256 * buff[8] + buff[9]);
			    temp = (float)intval / 10.0;
			    if (temp < 200.0)
				emsPtr->tempExhaust = temp;
			    intval = (int)(256 * buff[4] + buff[5]);
			    temp = (float)intval / 10.0;
			    sprintf(message2, " exhaust temp %2.1f °C, intake %2.1f °C",
				    emsPtr->tempExhaust, temp);
			    snprintf(message, MAXPATH - strlen(message2),
				     " from MC110: UBAMonitorFast, %s", message2);
			    if (Debug)
				LOGIT(message);
			}
			break;
		    case EMS_TYPE_UBAMonitorSlow:
			// UBAMonitorSlow
			// starts at 12/13/14, operating time at 15/16/17, ignition at 4.3,
			//  pump at 4.5, water pump 4.7, blower 4.2
			starts = (int)(256*256*buff[12] + 256*buff[13] + buff[14]);
			if (starts == 0 || starts > 1000000) {
			    // do nothing
			} else {
			    emsPtr->starts = starts;
			}
			opTime = (long int)(256 * 256 * (unsigned char)buff[15] + 256 * (unsigned char)buff[16] + (unsigned char)buff[17]);
			if (opTime == 0 || opTime == 15361 || opTime > 600000 || opTime < 10) {
			    // do nothing
			} else {
			    emsPtr->opTime = opTime;
			    if (OpTime == 0) {
				// set it for first time
				OpTime = opTime;
			    }
			    else {
				// check plausibility
				if (opTime > OpTime + 100 || opTime + 100 < OpTime) {
				    emsPtr->opTime = OpTime;
				}
				else /* if (opTime < OpTime - 100)*/ {
				    OpTime = opTime;
				}
			    }
			}
			emsPtr->status = buff[4];
			emsPtr->burner = buff[4] & 0x04;
			emsPtr->blower = buff[4] & 0x02;
			emsPtr->circPump = buff[4] & 0x80; // bit 7
			emsPtr->pump = buff[4] & 0x20; // bit 5
			sprintf(message2,
				" starts %ld, op.time %ld h %ld m, status byte %02x, burner %d, circ %d, pump %d",
				starts, opTime / 60, opTime % 60,
				emsPtr->status, emsPtr->burner, emsPtr->circPump, emsPtr->pump);
			snprintf(message, MAXPATH - strlen(message2), " from MC110: UBAMonitorSlow, %s", message2);
			if (Debug)
			    LOGIT(message);
			break;
		    case EMS_TYPE_UBAMonitorWater:
			// UBA Monitor Hot Water
			// water temp at byte 5/6 [0.1°C], set value water at byte 4 [°C], loading pump at byte 17.2
			intval = (int)(256 * buff[5] + buff[6]);
			temp = (float)intval / 10.0;
			emsPtr->tempWater = temp;
			setWater = (int)buff[4];
			emsPtr->setWaterTemp = (float)setWater;
			emsPtr->circPump = buff[17] & 0x4; // bit 2
			emsPtr->circState1 = buff[16];
			emsPtr->circState2 = buff[17];
			sprintf(message2, " warm water temp %2.1f °C, set temp is %d °C, circ %s, %02x %02x",
				temp, setWater, emsPtr->circPump ? "on" : "off", buff[16], buff[17]);
			snprintf(message, MAXPATH - strlen(message2), " from MC110: UBA Monitor Hot Water, %s", message2);
			if (Debug)
			    LOGIT(message);
			break;
		    case 0xff:
			// ems+
			if (buff[3] == 0x0 && buff[4] ==  0x7) {
			    switch(buff[5]) {
			    case 0xe4:
				sprintf(message, " from MC110: ?_UBA Status (%02x) ems+ (%d bytes): ",
					buff[5], len);
				emsPtr->code1 = buff[9];
				emsPtr->code2 =	buff[10];
				if (buff[13] > 0) {
				    emsPtr->setTemperature = (float)buff[13];
				    sprintf(message2, "set temp = %2.1f, ", emsPtr->setTemperature);
				}
				if (buff[15] > 0) {
				    emsPtr->setTemperature = (float)buff[15];
				    sprintf(message2, "set temp = %2.1f, ", emsPtr->setTemperature);
				}
				strcat(message, message2);
				sprintf(message2, " %02x %02x", buff[9], buff[10]);
				strcat(message, message2);
				if (Debug)
				    LOGIT(message);
				break;
			    default:
				sprintf(message, " from MC110, undecoded message: (%02x) ems+ (%d bytes), ",
					buff[5], len);
				for (i = 0; i < len; i++) {
				    sprintf(message2, " %02x", buff[i]);
				    strcat(message, message2);
				}
				LOGERR(message);
				break;
			    }
			}
			break;
		    default:
			sprintf(message, " from MC110, undecoded message: (%02x) ems (%d bytes), ",
				buff[2], len);
			for (i = 0; i < len; i++) {
			    sprintf(message2, " %02x", buff[i]);
			    strcat(message, message2);
			}
			LOGERR(message);
			break;
		    } // types
		    break;
			    
		case 0x0B:
		    // addressed to us
		    switch (buff[2]) {
		    default:
			// wtf
			sprintf(message, " from MC110, to us (0x0b), undecoded message: (%02x) ems (%d bytes), ",
				buff[2], len);
			for (i = 0; i < len; i++) {
			    sprintf(message2, " %02x", buff[i]);
			    strcat(message, message2);
			}
			LOGERR(message);
			break;
		    }
		    break;
			    
		case 0x10:
		    // addressed to RC310
		    switch (buff[2]) {
		    case 0x14:
			// response operation time
			opTime = (long int)(256 * 256 * (unsigned char)(buff[5] & 0x3) + 256 * (unsigned char)buff[6] + (unsigned char)buff[7]);
			sprintf(message, " from MC110 to RC310, answer UBABetriebszeit (%ld) ", opTime);
			for (i = 0; i < len; i++) {
			    sprintf(message2, " %02x", buff[i]);
			    strcat(message, message2);
			}
			LOGIT(message);
			break;
			
		    default:
			sprintf(message, " from MC110 to RC310, undecoded message (%02x), %d bytes: ",
				buff[2], len);
			for (i = 0; i < len; i++) {
			    sprintf(message2, " %02x", buff[i]);
			    strcat(message, message2);
			}
			LOGERR(message);
			break;
		    }
		    break;
			    
		default:
		    sprintf(message, " from MC110 to (%02x), undecoded message: (%02x), %d bytes: ",
			    buff[1], buff[2], len);
		    for (i = 0; i < len; i++) {
			sprintf(message2, " %02x", buff[i]);
			strcat(message, message2);
		    }
		    LOGERR(message);
		    break;
		} // adressed to
		break;
		    
	    case 0x10:
		// RC310
		sprintf(message, "RC310: ");
		switch (buff[2]) {
		case 0x06:
		    // RCTimeMessage
		    // year (-2000) byte 4, month at 5, day at 7
		    // hour at 6, min at 8, sec at 9, dayOfWeek at 10
		    // dst at 11.0
		    year = buff[4] + 2000;
		    month = buff[5];
		    day = buff[7];
		    hour = buff[6];
		    minute = buff[8];
		    second = buff[9];
		    dayOfWeek = buff[10];
		    dst = buff[11];
		    sprintf(message2, "%02d:%02d:%02d, %04d-%02d-%02d, day %d, %s",
			    hour, minute, second, year, month, day, dayOfWeek,
			    dst == 0 ? "" : "dst");
		    snprintf(message, MAXPATH - strlen(message2),
			     " from RC310: RCTimeMessage (%d bytes): %s", len, message2);
		    if (Debug)
			LOGIT(message);
		    break;
		case 0x3d:
		    // Working Mode Heating Circuit 1
		    nightTemp = (float)buff[5] / 2.0;
		    dayTemp = (float)buff[6] / 2.0;
		    holidayTemp = (float)buff[7] / 2.0;
		    hcMode = buff[11];
		    summerThreshold = buff[26];
		    sprintf(message2, "nightTemp = %.1f, dayTemp = %.1f, holidayTemp = %.1f, hcMode = %d, summerThreshold = %d",
			   nightTemp, dayTemp, holidayTemp, hcMode, summerThreshold);
		    snprintf(message, MAXPATH - strlen(message2),
			     " from RC310: Working Mode Heating Circuit 1 (%d bytes): %s", len, message2);
		    //if (Debug)
		    LOGIT(message);
		    break;
		case 0xff:
		    // ems+
		    if (buff[3] == 0x0 && buff[4] ==  0x1) {
			switch(buff[5]) {
			case 0xa5:
			    // indoor temp at byte 6/7 [0.1°C]
			    intval = (int)(256 * buff[6] + buff[7]);
			    temp = (float)intval / 10.0;
			    emsPtr->tempInside = temp;
			    sprintf(message2, "indoor temp %0.2f", temp);
			    snprintf(message, MAXPATH - strlen(message2),
				     " from RC310: (ems+)RC310-Heizkreise (%02x): %s",
				     buff[5], message2);
			    if (Debug)
				LOGIT(message);
			    break;
			default:
			    sprintf(message, " from RC310: undecoded message (%02x) ems+ (%d bytes): ",
				    buff[5], len);
			    for (i = 0; i < len; i++) {
				sprintf(message2, " %02x", buff[i]);
				strcat(message, message2);
			    }
			    LOGERR(message);
			    break;
			}
		    }
		    break;
		    
		default:
		    sprintf(message, " from RC310: undecoded message (%02x) %d bytes: ",
			    buff[2], len);
		    for (i = 0; i < len; i++) {
			sprintf(message2, " %02x", buff[i]);
			strcat(message, message2);
		    }
		    LOGERR(message);
		    break;
		}
		break;    
	    default:
		sprintf(message, " from (%02x): undecoded message (%02x) %d bytes: ",
			buff[0], buff[2], len);
		for (i = 0; i < len; i++) {
		    sprintf(message2, " %02x", buff[i]);
		    strcat(message, message2);
		}
		LOGERR(message);
		break;
	    }  // sender
	}  // did receive message

    } // for (;;)

    exit(0);
}
