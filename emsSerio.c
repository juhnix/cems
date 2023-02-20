// emsSerio.c
//
//  handels I/O with ems bus via UART
//  based on ems_serio.c, written by CreaValix (Alexander Simon) and 
//   beimoe
// $Id$

#define _GNU_SOURCE 1

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include "ems.h"
#include "serial.h"
#include "defines.h"
#include "queue.h"
#include "rx.h"

#define handle_error_en(en, msg) do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

struct termios tios;
int waitingfor;
struct STATS stats;
pthread_t readloop = 0;
int Logging = 0;

// forward declarations
int getConfig(enum varType, void *var, char *defVal, char *cFile, char *group, char *key);

void print_stats() {
    char message[MAXPATH];
    
    if (!(Logging & LOG_INFO))
        return;
    
    sprintf(message, "Statistics");
    LOGIT(message);
    sprintf(message, "RX bus access errors    %d", stats.rx_mac_errors);
    LOGIT(message);
    sprintf(message, "RX total                %d", stats.rx_total);
    LOGIT(message);
    sprintf(message, "RX success              %d", stats.rx_success);
    LOGIT(message);
    sprintf(message, "RX too short            %d", stats.rx_short);
    LOGIT(message);
    sprintf(message, "RX wrong sender         %d", stats.rx_sender);
    LOGIT(message);
    sprintf(message, "RX CRC errors           %d", stats.rx_format);
    LOGIT(message);
    sprintf(message, "TX total                %d", stats.tx_total);
    LOGIT(message);
    sprintf(message, "TX failures             %d", stats.tx_fail);
    LOGIT(message);
}

void print_packet(int out, int loglevel, uint8_t *msg, size_t len) {
    char message[MAXPATH];

    // update heartbeat after every received telegram
    emsPtr->heartbeatSerio = time(NULL);
    
    if (!(Logging & loglevel))
        return;
    char text[3 + MAX_PACKET_SIZE * 3 + 2 + 1];
    int pos = 0;
    pos += sprintf(&text[0], out ? "TX:" : "RX:");
    for (size_t i = 0; i < len; i++) {
        pos += sprintf(&text[pos], " %02hhx", msg[i]);
        if (i == 3 || i == len - 2) {
            pos += sprintf(&text[pos], " ");
        }
    }
    sprintf(message, "%s", text);
    LOGIT(message);
}

void stop_handler() {
    close_queues(emsPtr);
    close_serial(emsPtr);
    readloop = 0;
}

void *read_loop() {
    int abort;

    {
        // Do not accept signals. They should be handled by calling code.
        // We close cleanly when we're asked to stop with pthread_cancel.
        sigset_t set;
        int ret;
        sigemptyset(&set);
        sigaddset(&set, SIGQUIT);
        ret = pthread_sigmask(SIG_BLOCK, &set, NULL);
        if (ret != 0)
            handle_error_en(ret, "pthread_sigmask");
    }

    char message[MAXPATH];

    pthread_cleanup_push(stop_handler, NULL);

    sprintf(message, "Starting EMS bus access"); 
    LOGIT(message);
    
    while (1) {
        rx_packet(&abort);
        rx_done();
    }
    pthread_cleanup_pop(1);
    return NULL;
}

int start(ems *emsPtrL) {
    int ret;
    char message[MAXPATH];

    ret = open_serial(emsPtrL->emstty);
    if (ret != 0) {
        snprintf(message, MAXPATH - strlen(emsPtrL->emstty), "Failed to open %s: %i (%d)", emsPtrL->emstty, ret, errno);
	LOGERR(message);
        return (-1);
    } else {
	snprintf(message, MAXPATH - strlen(emsPtrL->emstty), "Serial port %s opened", emsPtrL->emstty);
	LOGIT(message);
    }
    
    ret = setup_queue(&tx_queue, emsPtrL->txqueue);
    if (tx_queue == -1) {
        sprintf(message, "Failed to open TX message queue: %i %s (%d)", tx_queue, strerror(ret), ret);
	LOGERR(message);
        return (-1);
    }
    else {
	sprintf(message, "opened TX message queue: %i (%d)", tx_queue, ret);
	LOGIT(message);
    }
    
    ret = setup_queue(&rx_queue, emsPtrL->rxqueue);
    if (rx_queue == -1) {
        sprintf(message, "Failed to open RX message queue: %i  %s (%d)", rx_queue, strerror(ret), ret);
	LOGERR(message);
        return(-1);
    } else {
	sprintf(message, "Connected to message queues");
	LOGIT(message);
    }
    
    ret = pthread_create(&readloop, NULL, &read_loop, NULL);
    if (ret != 0)
        handle_error_en(ret, "pthread_create");

    return(0);
}

int stop() {
    if (!readloop)
        return(-1);
    pthread_cancel(readloop);
    return(0);
}

void sig_stop() {
    stop();
}

int main(int argc, char *argv[]) {
    int ret;
    int c, res, result;
    key_t key = SHMKEY;    
    struct sigaction signal_action;
    char message[MAXPATH];
    pid_t daemonPid = 0, sid = 0;
    FILE *fp;

    // default: run as daemon, no debug output
    Daemon = true;
    Debug = false;
    
    while ((c = getopt (argc, argv, "vnhl:")) != -1) {
        switch (c) {
        case 'v': // be verbose
            Debug = true;
            break;
        case 'n': // don't daemonize
            Daemon = false;
            break;
	case 'l':
	    Logging = atoi(optarg);
	    break;
        case 'h':
        case '?':
            fprintf (stderr, "%s:\n\tOption -v activates debug mode,\n", argv[0]);
            fprintf (stderr, "\tOption -?/-h show this information\n");
            fprintf (stderr, "\tOption -V show the version information\n");
            fprintf (stderr, "\tOption -l # sets the loging type to #\n");
            exit(0);
            break;
            
        default:
            break;
        }
    }

#define DAEMON_NAME "emsSerio"
    sprintf(DaemonName, "%s", DAEMON_NAME);

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
        emsPtr->heartbeatSerio = time(NULL);
    } else {
        sprintf(message,  "%s: could not attach shared memory at line %d, errno = %d (%s)",
                DaemonName, __LINE__, errno, strerror(errno));
        LOGERR(message);
        _exit(1);
    }

    // get names for serial device and message queues
    result = getConfig(CHAR, &emsPtr->emstty, EMSTTY, CONFIGFILE, "EMS", "emstty");
    if (result)
	sprintf(message, "%s: emstty not defined, set to default >%s<", DaemonName, EMSTTY);
    else
	snprintf(message, MAXPATH - strlen(emsPtr->emstty), "%s: emstty set to >%s< ", DaemonName, emsPtr->emstty);
    LOGIT(message);
    result = getConfig(CHAR, &emsPtr->rxqueue, RX_QUEUE_NAME, CONFIGFILE, "EMS", "rxqueue");
    if (result)
	sprintf(message, "%s: rxqueue not defined, set to default >%s<", DaemonName, RX_QUEUE_NAME);
    else
	sprintf(message, "%s: rxqueue set to >%s< ", DaemonName, emsPtr->rxqueue);
    LOGIT(message);
    result = getConfig(CHAR, &emsPtr->txqueue, TX_QUEUE_NAME, CONFIGFILE, "EMS", "txqueue");
    if (result)
	sprintf(message, "%s: txqueue not defined, set to default >%s<", DaemonName, TX_QUEUE_NAME);
    else
	sprintf(message, "%s: txqueue set to >%s< ", DaemonName, emsPtr->txqueue);
    LOGIT(message);

    // we want to run as daemon, so we have to fork (the daemon will then
    // start a thread, as in original design)

    if (Daemon) {
	// create process
	daemonPid = fork();

	if (daemonPid < 0) {
	    syslog(LOG_ERR, "could not fork daemon process %s", DaemonName);
	    _exit(errno);
	}
	else {
	    // check if parent or son
	    if (daemonPid == 0) {
		// son, will continue
		syslog(LOG_INFO, "%s daemon started", DaemonName);
	    }
	    else {
		fp = fopen("/run/emsSerio.pid", "w");
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
	    emsPtr->serioPid = sid;
	}
	sprintf(message, "%s: running with pid %d", DaemonName, sid);
	LOGIT(message);

	//Change Directory
	//If we cant find the directory we exit with failure.
	//if ((chdir("/")) < 0) { exit(EXIT_FAILURE); }
	
	//Close Standard File Descriptors
	close(STDIN_FILENO);
	//close(STDOUT_FILENO);
	close(STDERR_FILENO);

	sprintf(message, "%s: closed stdin, stdout and stderr", DaemonName);
	LOGIT(message);
    } // daemon mode
    else {
	fprintf(stderr, "%s: running in foreground\n", DaemonName);
    }

    // start thread
    ret = start(emsPtr);

    // Set signal handler and wait for the thread
    signal_action.sa_handler = sig_stop;
    sigemptyset(&signal_action.sa_mask);
    signal_action.sa_flags = 0;
    sigaction(SIGINT, &signal_action, NULL);
    sigaction(SIGHUP, &signal_action, NULL);
    sigaction(SIGTERM, &signal_action, NULL);

    pthread_join(readloop, NULL);
    print_stats();

    return (ret);
}
