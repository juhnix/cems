//
// ems.h
//
// $Id: ems.h 48 2021-08-26 10:58:21Z juh $

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <syslog.h>
#include <time.h>
#include <getopt.h>
#include <string.h>
#include <mosquitto.h>
#include <libMsbClientC.h>  // in /usr/local/include

#define DATAPATH "/usr/local/var"
#define RCVQUEUE "/ems_bus_rx"
#define MAXNAME 100
#define MAXPATH 1000
#define VERSION "$Id: ems.h 48 2021-08-26 10:58:21Z juh $"
#define LOGIT(MSG)   if (Daemon) syslog(LOG_INFO, "%s", MSG); else fprintf(stderr, "%s\n", MSG);
#define LOGERR(MSG)   if (Daemon) syslog(LOG_ERR, "%s", MSG); else fprintf(stderr, "%s\n", MSG);
#define SHMKEY 2048
#define CONFIGFILE "/usr/local/etc/ems.cfg"
#define BROKER "192.168.17.1"
#define PORT "1883"
#define BROADCAST_ID 0x00
#define MASTER_ID 0x08
#define CLIENT_ID 0x0b
#define RX_QUEUE_NAME "/ems_bus_rx"
#define TX_QUEUE_NAME "/ems_bus_tx"
#define EMSTTY "/dev/ttyAMA0"
#define UUID "54f04be2-0337-11ec-9820-1b23e3bbd31b"
#define TOKEN "bbd31b"
#define CLASS "SmartObject"
#define NAME "emsval"

enum varType { CHAR = 0, INT = 1, FLOAT = 2 };

struct _ems_ {
    int heartbeatDecode;
    int heartbeatSerio;
    int heartbeatDb;
    int heartbeatMqtt;
    int heartbeatMsb;
    int status;
    int code1;
    int code2;
    int model;
    int error1;
    int error2;
    int error3;
    int errorCode;
    int ubaCode;
    int burnerCode;
    int ecoMode;
    int summerMode;
    int debug;
    int pid;
    int serioPid;
    int decodePid;
    int dbPid;
    int mqttPid;
    int mqttAvail;
    int msbPid;
    int msbAvail;
    time_t lastData;
    struct mosquitto *mosq;
    int power;
    float current;
    int starts;
    int opTime;
    int pump;
    int boilerState;
    int waterState;
    int loadingPump;
    int circPump;
    int circState1;
    int circState2;
    float setTemperature; // target temperature for boiler out
    float setWaterTemp;
    float tempOutside;
    float tempInside;
    float tempBoiler;
    float tempWater;
    float tempExhaust;
    char configFile[MAXNAME];
    char broker[MAXNAME];
    char cert[MAXNAME];
    int port;
    char datapath[MAXPATH];
    char emstty[MAXPATH];
    char rxqueue[MAXNAME];
    char txqueue[MAXNAME];
    char msbUrl[MAXNAME];
    char msbPort[MAXNAME];
    int32_t interval;
    char msbCert[MAXNAME];
    char msbUuid[MAXNAME];
    char msbToken[MAXNAME];
    char msbClass[MAXNAME];
    char msbName[MAXNAME];
    char msbDescription[MAXNAME];
    msbClient *client;
};

typedef struct _ems_ ems;

ems *emsPtr;

// global vars (yeah, I know ...)
int ShmId;
int Debug;
char DaemonName[MAXNAME]; // use this for daemon name
int Daemon;
int Line;
