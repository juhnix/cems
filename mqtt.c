//
// mqtt.c
//
// functions for emsMqtt
//
// $Id: mqtt.c 20 2021-08-08 22:49:19Z juh $

// Disable the warning about declaration shadowing because it affects too 
 // many valid cases. 
# pragma GCC diagnostic ignored "-Wshadow"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "ems.h"

void mosqLogCallback(struct mosquitto *mosq, void *userdata, int level, const char *str);

int initMosquitto(ems *emsPtr) {
    int result, retries, errval, loop;
    char message[1000], error[100];

    //initialize mosquitto
    emsPtr->mqttAvail = false;

    errval = 0;

    sprintf(message, "emsMqtt-%d", emsPtr->mqttPid);
    emsPtr->mosq = mosquitto_new(message, true, NULL);
    if (emsPtr->mosq == NULL)
	errval = errno;
    
    if (errval != 0) {
	switch (errval) {
	case ENOMEM:
	    sprintf(error, "out of memory");
	    break;
	case EINVAL:
	    sprintf(error, "invalid input parameters");
	    break;
	default:
	    sprintf(error, "unknown error");
	    break;
	}
	sprintf(message, "%s/mqtt/initMosquitto: could not initialize mosquitto for publishing, error %s", DaemonName, error);
	LOGERR(message);
	return (-1);
    }
    else {
	sprintf(message, "%s/mqtt/initMosquitto: initialized mosquitto for publishing.\n", DaemonName);
	LOGIT(message);
    }

    mosquitto_log_callback_set(emsPtr->mosq, mosqLogCallback);
    
    retries = 0;
    
 retry:

    sprintf(message, "%s/mqtt: trying to connect to broker >%s< (ptr = 0x%0x)",
	    DaemonName, emsPtr->broker, (int)emsPtr);
    LOGIT(message);

    // check if cert is given

    if (emsPtr->cert != NULL && strlen(emsPtr->cert) > 5)
	result =  mosquitto_tls_set(emsPtr->mosq, emsPtr->cert, NULL, NULL, NULL, NULL);
    
    result = mosquitto_connect_async(emsPtr->mosq, emsPtr->broker, emsPtr->port, 60); // get live sign from broker after 60 s
    
    switch (result) {
    case MOSQ_ERR_CONN_PENDING: // -1
	sprintf(message, "%s/mqtt: connection pending", DaemonName);
	break;
    case MOSQ_ERR_SUCCESS: // 0
	sprintf(message, "%s/mqtt: successfully connected to broker >%s<", DaemonName, emsPtr->broker);
	break;
    case MOSQ_ERR_NOMEM: // 1
	sprintf(message, "%s/mqtt: not enough memory", DaemonName);
	break;
    case MOSQ_ERR_PROTOCOL: // 2
	sprintf(message, "%s/mqtt: protocol error", DaemonName);
	break;
    case MOSQ_ERR_INVAL: // 3
	sprintf(message, "%s/mqtt: the input parameters were invalid. mosq = %d, broker = %s, port = %d",
		DaemonName, (int)emsPtr->mosq, emsPtr->broker, emsPtr->port);
	break;
    case MOSQ_ERR_NO_CONN: // 4
	sprintf(message, "%s/mqtt: no connection", DaemonName);
	break;
    case MOSQ_ERR_CONN_REFUSED: // 5
	sprintf(message, "%s/mqtt: connection refused", DaemonName);
	break;
    case MOSQ_ERR_NOT_FOUND: // 6
	sprintf(message, "%s/mqtt: error not found (?)", DaemonName);
	break;
    case MOSQ_ERR_CONN_LOST: // 7
	sprintf(message, "%s/mqtt: connection lost", DaemonName);
	break;
    case MOSQ_ERR_TLS: // 8
	sprintf(message, "%s/mqtt: tls error", DaemonName);
	break;
    case MOSQ_ERR_PAYLOAD_SIZE: // 9
	sprintf(message, "%s/mqtt: payload size error", DaemonName);
	break;
    case MOSQ_ERR_NOT_SUPPORTED: // 10
	sprintf(message, "%s/mqtt: error (?) not supported", DaemonName);
	break;
    case MOSQ_ERR_AUTH: // 11
	sprintf(message, "%s/mqtt: authentification error", DaemonName);
	break;
    case MOSQ_ERR_UNKNOWN: // 13
	sprintf(message, "%s/mqtt: unknown error", DaemonName);
	break;
    case MOSQ_ERR_PROXY: // 16
	sprintf(message, "%s/mqtt: proxy error", DaemonName);
	break;
    case MOSQ_ERR_ACL_DENIED: // 1
	sprintf(message, "%s/mqtt: acl error", DaemonName);
	break;
    case MOSQ_ERR_ERRNO:
	sprintf(message, "%s/mqtt: a system call returned an error. errno %d (%s)", DaemonName, errno, strerror(errno));
	break;
    case MOSQ_ERR_EAI:
	sprintf(message, "%s/mqtt: could not connect to broker >%s< (MOSQ_ERR_EAI), retry after 10 seconds, retry %d",
		DaemonName, emsPtr->broker, retries);
	retries++;
	if (result) { // we got a non-zero value back
	    LOGERR(message);
	}

	sleep(10);
	if (retries < 7)
	    goto retry;
	else {
	    emsPtr->mqttAvail = false;
	    return (result);
	}
	break;
    default:
	sprintf(message, "%s/mqtt: error return from connect %d, %s", DaemonName, result, mosquitto_strerror(result));
	break;
    }

    if (result) { // we got a non-zero value back
	LOGERR(message);
	emsPtr->mqttAvail = false;
	return (result);
    }
    else {
	LOGIT(message);
	emsPtr->mqttAvail = true;
    }
    
    loop = mosquitto_loop_start(emsPtr->mosq);
    
    if (loop != MOSQ_ERR_SUCCESS) {
	sprintf(message, "%s/mqtt: Unable to start mosquitto loop: %i", DaemonName, loop);
	LOGERR(message);
	emsPtr->mqttAvail = false;
	return (loop);
    }
    else {
	sprintf(message, "%s/mqtt: started mosquitto loop %i", DaemonName, loop);
	LOGIT(message);
    }
    
    return (result);
}    

void mosqLogCallback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
    char message[1000];
    // Print all log messages regardless of level
  
    switch (level){
    case MOSQ_LOG_DEBUG:
    case MOSQ_LOG_INFO:
    case MOSQ_LOG_NOTICE:
	if (Debug)
	    sprintf(message, "%s/mqtt/logCallBack: %i:%s\n", DaemonName, level, str);
	break;
    case MOSQ_LOG_WARNING:
    case MOSQ_LOG_ERR:
	sprintf(message, "%s/mqtt/logCallBack: %i:%s\n", DaemonName, level, str);
	break;

    default:
	break;
    }

    if (Debug)
	LOGIT(message);
}

void mqttPublish(ems *emsPtr, char *topic, void *val, int qos, bool retain) {
    char message[1000];
    int result;

    // check if already initialzed
    if (emsPtr->mosq == NULL) {
	sprintf(message, "%s/mqtt/mqttPublish: mosquitto not initialized, abort", DaemonName);
	LOGERR(message);
	return;
    }
    
    if (!emsPtr->mqttAvail) {
	sprintf(message, "%s/mqtt/mqttPublish: mosquitto not available, abort", DaemonName);
	LOGERR(message);
	return;
    }
    

    result = mosquitto_publish(emsPtr->mosq, NULL, topic, strlen(val), val, qos, retain);
	
    switch (result) {
	
    case MOSQ_ERR_SUCCESS:
	emsPtr->lastData = time(NULL);
	sprintf(message, "%s/mqtt/mqttPublish: successfull sent val %s for topic %s", DaemonName, (char *)val, topic);
	break;
    case MOSQ_ERR_INVAL:
	sprintf(message, "%s/mqtt/mqttPublish: the input parameters (%s, %s) were invalid.", DaemonName, topic, (char *)val);
	break;
    case MOSQ_ERR_NOMEM:
	sprintf(message, "%s/mqtt/mqttPublish: out of memory condition occurred.", DaemonName);
	break;
    case MOSQ_ERR_NO_CONN:
	sleep(5);
	mosquitto_reconnect(emsPtr->mosq);
	result = mosquitto_publish(emsPtr->mosq, NULL, topic, strlen(val), val, qos, retain);
	sprintf(message, "%s/mqtt/mqttPublish: the client wasn’t connected to a broker, tried to reconnect. Result of re-sending is %s",
		DaemonName, mosquitto_strerror(result));
	break;
    case MOSQ_ERR_PROTOCOL:
	sprintf(message, "%s/mqtt/mqttPublish: there is a protocol error communicating with the broker", DaemonName);
	break;
    case MOSQ_ERR_PAYLOAD_SIZE:
	sprintf(message, "%s/mqtt/mqttPublish: payloadlen (%d) is too large", DaemonName, strlen(val));
	break;
    case MOSQ_ERR_MALFORMED_UTF8:
	sprintf(message, "%s/mqtt/mqttPublish: the topic (%s) is not valid UTF-8", DaemonName, topic);
	break;
    default:
	sprintf(message, "%s/mqtt/mqttPublish: error (%d) from  mosquitto_publish(), %s", DaemonName, result, mosquitto_strerror(result));
	sleep(5);
	mosquitto_reconnect(emsPtr->mosq);
	break;
    }
    
    if (result) {
	LOGERR(message);
    }
    else {
	if (Debug)
	    LOGIT(message);
    }
}
