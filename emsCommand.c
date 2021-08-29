//
// emsCommand.c
// sends commands to ems bus
//
// $Id: emsCommand.c 36 2021-08-17 22:00:04Z juh $

#define _POSIX_C_SOURCE 200809L

#include "ems.h"

#define LEN 8192

// forward declarations
int getConfig(enum varType, void *var, char *defVal, char *cFile, char *group, char *key);
extern int usleep (__useconds_t __useconds);

int main(int argc , char *argv[])
{
    mqd_t fd;
    char buff[LEN], message[MAXPATH], message2[MAXPATH], queueName[MAXNAME];
    int sterr, i, c, len, result;
    float temp, temp2, current, nightTemp, dayTemp, holidayTemp;
    int power, intval, setTemp, setWater, year, month, day, hour, minute, second, dst, dayOfWeek;
    int starts, opTime, status, hcMode, summerThreshold;
    key_t key = SHMKEY;
    pid_t daemonPid = 0;
    pid_t sid;
    time_t currentTime, lastTime, duration = 0;
    float consumption = 0.0;
    FILE *fp;
    int tries = 0;

    // default: no debug output    
    Debug = false;

#define DAEMON_NAME "ems-command"
    sprintf(DaemonName, "%s", DAEMON_NAME);

    while ((c = getopt (argc, argv, "vht:")) != -1) {
        switch (c) {
        case 'v': // be verbose
            Debug = true;
            break;
	case 't':
	    strncpy(queueName, optarg, MAXNAME);
	    break;
        case 'h':
        case '?':
        default:
            fprintf (stderr, "%s:\n\tOption -v activates debug mode,\n", argv[0]);
            fprintf (stderr, "\tOption -?/-h show this information\n");
            fprintf (stderr, "\tOption -V show the version information\n");
            fprintf (stderr, "\tOption -t # sets send message queue name to #\n");
            exit(0);
            break;
        }
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
        currentTime = time(NULL);
    } else {
        sprintf(message,  "%s: could not attach shared memory at line %d, errno = %d (%s)",
                DaemonName, __LINE__, errno, strerror(errno));
        LOGERR(message);
        _exit(1);
    }

    // check for name of send message queue
    if (strlen(emsPtr->txqueue) > 0) {
	sprintf(message, "%s: txqueue already set to >%s< ", DaemonName, emsPtr->txqueue);
    } else {
	result = getConfig(CHAR, &emsPtr->txqueue, TX_QUEUE_NAME, CONFIGFILE, "EMS", "txqueue");
	if (result)
	    sprintf(message, "%s: txqueue not defined, set to default >%s<", DaemonName, TX_QUEUE_NAME);
	else
	    sprintf(message, "%s: txqueue set to >%s< ", DaemonName, emsPtr->txqueue);
    }
    LOGIT(message);

    // open queue for packets to send
    fd = mq_open(emsPtr->txqueue, O_RDWR | O_NONBLOCK);
    if (fd == -1) {
	sterr = errno;
	sprintf(message, "%s: couldn't open the message queue. Error : %s\n", DaemonName, strerror(sterr));
	LOGERR(message);
	exit(sterr);
    }

    // clear send buffer    
    memset(buff, 0, LEN);

    // command to RC310, send Working Mode Heating Circuit 1
    // Byte 0	Byte 1	        Byte 2	Byte 3	Byte 4	Byte 5	Byte 6
    // 0x0B	0x10|0x80=0x90	0x3D	0x00	0x2A	CRC	0x00

    buff[0] = 0x0b;
    buff[1] = 0x90;
    buff[2] = 0x3d;
    buff[3] = 0x00;
    buff[4] = 0x2a;
    buff[5] = 0x00;
    buff[6] = 0x00;
    len = 7;
    
    result = mq_send(fd, (char *)buff, len, 0);

    if (result == -1) {
        sprintf(message, "%s: Could not add packet to queue: %s", DaemonName, strerror(errno));
	LOGERR(message);
    }
    else {
        sprintf(message, "%s: added packet to queue: %s", DaemonName, emsPtr->txqueue);
	LOGIT(message);
    }

}
