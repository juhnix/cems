// msb.c
// msb functions for emsMsb
//
// $Id: msb.c 48 2021-08-26 10:58:21Z juh $
//

#define __USE_XOPEN2K8
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>      // syslog messages
#include <string.h>      // for strcpy()
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include <unistd.h>      // for usleep()

#include "ems.h"

extern int usleep (__useconds_t __useconds);
char* msbObjectSelfDescription(const msbObject* object);


int initMsb(ems *myEmsPtr) {
    int result = 0;
    char message[MAXPATH], error[MAXPATH];
    uuid_t uuid;

    // use this only for session id
    uuid_generate(uuid);
    char unp[37];
    uuid_unparse(uuid, unp);
    char t[33];
    int i = 0, j = 0;
    for (; i < 37; ++i)
	if (unp[i] != '-')
	    t[j++] = unp[i];
    
    strcpy((char *)uuid, myEmsPtr->msbUuid);

    sprintf(message, "initMsb: uuid = %s, token = %s, t = %s", myEmsPtr->msbUuid, myEmsPtr->msbToken, t);
    LOGIT(message);

/**
 * @brief Create an Msb client
 *
 * @param url URL to MSB
 * @param origin for websocket header, usually NULL is okay
 * @param uuid Service UUID
 * @param token Service token
 * @param service_class Service class ("Application" or "Smart Object")
 * @param name Service name
 * @param description Service description
 * @param tls Use tls
 * @param client_cert Path to client certificate
 * @param client_key Path to client key
 * @param ca_cert  Path to ca certificate
 *
 * @return Created client
 *
 * msbClient* msbClientNewClientURL(char* url, char* origin, char* uuid, char* token, char* service_class, char* name,
								  char* description,
								  bool tls, char* client_cert, char* client_key, char* ca_cert);

    msbClient* msbClient = msbClientNewClientURL("http://msb-ws.uef.ipa.fraunhofer.de", NULL, UUID, TOKEN, CLASS, NAME\
, DESCRIPTION, false, NULL, NULL, NULL);

*/
    myEmsPtr->client = msbClientNewClientURL(
					     myEmsPtr->msbUrl,
					     NULL, // origin
					     myEmsPtr->msbUuid,
					     myEmsPtr->msbToken,
					     myEmsPtr->msbClass,
					     myEmsPtr->msbName,
					     myEmsPtr->msbDescription,
					     false, // use tls TODO: check if url starts with wss and set it to true
					     NULL, // path to client cert
					     NULL, // path to client key
					     NULL  // path to ca certificate
					     );

    sprintf(message, "initMsb: created client, ptr 0x%0x", (unsigned int)myEmsPtr->client);
    LOGIT(message);
    
    if (Debug)
	msbClientSetDebug(myEmsPtr->client, true);
    else
	msbClientSetDebug(myEmsPtr->client, false);
    Line = __LINE__;    
    msbClientUseSockJSPath(myEmsPtr->client,
			   "000", // server_id
			   t, // session_id
			   "websocket" // transport_id
			   );
    Line = __LINE__;    
    msbClientSetSockJSFraming(myEmsPtr->client,
			       1 // framing active
			       );
    Line = __LINE__;    

    msbClientSetFunctionCacheSize(myEmsPtr->client, 100);
    Line = __LINE__;    
    msbClientSetEventCacheSize(myEmsPtr->client, 100);
    Line = __LINE__;    


    msbClientRunClientStateMachine(myEmsPtr->client);
    Line = __LINE__;    

    json_object* dataformat = json_object_new_object();
    
    int bl = myEmsPtr->debug ? 0 : 1;
    msbClientAddConfigParam(myEmsPtr->client, "debug", MSB_BOOL, MSB_NONE, &bl);
    Line = __LINE__;    
    
    int32_t interval = myEmsPtr->interval;
    msbClientAddConfigParam(myEmsPtr->client, "interval", MSB_INTEGER, MSB_INT32, &interval);
    Line = __LINE__;    

    json_object *dataObject;
    dataObject = json_object_new_object();
    json_object_object_add(dataformat, "dataObject", dataObject);
    json_object_object_add(dataObject, "$ref", json_object_new_string("#/definitions/complexevent"));

    json_object* event = json_object_new_object();
    json_object_object_add(event, "type", json_object_new_string("object"));
    json_object_object_add(event, "additionalProperties", json_object_new_boolean(false));

    json_object* required = json_object_new_array();
    json_object_array_add(required, json_object_new_string("tempboiler"));
    json_object_array_add(required, json_object_new_string("tempwater"));
    json_object_array_add(required, json_object_new_string("tempexhaust"));
    json_object_array_add(required, json_object_new_string("tempinside"));
    json_object_array_add(required, json_object_new_string("tempoutside"));

    json_object_object_add(event, "required", required);

    json_object* properties = json_object_new_object();
    json_object* tempboiler = json_object_new_object();
    json_object* tempwater = json_object_new_object();
    json_object* tempexhaust = json_object_new_object();
    json_object* tempinside = json_object_new_object();
    json_object* tempoutside = json_object_new_object();
    json_object_object_add(tempboiler, "type", json_object_new_string("number"));
    json_object_object_add(tempboiler, "format", json_object_new_string("double"));
    json_object_object_add(tempwater, "type", json_object_new_string("number"));
    json_object_object_add(tempwater, "format", json_object_new_string("double"));
    json_object_object_add(tempexhaust, "type", json_object_new_string("number"));
    json_object_object_add(tempexhaust, "format", json_object_new_string("double"));
    json_object_object_add(tempinside, "type", json_object_new_string("number"));
    json_object_object_add(tempinside, "format", json_object_new_string("double"));
    json_object_object_add(tempoutside, "type", json_object_new_string("number"));
    json_object_object_add(tempoutside, "format", json_object_new_string("double"));

    json_object_object_add(properties, "tempboiler", tempboiler);
    json_object_object_add(properties, "tempwater", tempwater);
    json_object_object_add(properties, "tempexhaust", tempexhaust);
    json_object_object_add(properties, "tempinside", tempinside);
    json_object_object_add(properties, "tempoutside", tempoutside);
    json_object_object_add(event, "properties", properties);

    json_object_object_add(dataformat, "complexevent", event);
    Line = __LINE__;    

    msbClientAddComplexEvent(myEmsPtr->client,
			     "emsvalues", // event id
			     "EMS+ Values", // event name
			     "Values from Buderus via EMS bus", // event description
			     dataformat, // format described in json object
			     false // is NO array
			     );
    Line = __LINE__;    

    // register at msb
    //    result = msbClientRegister(myEmsPtr->client);
    //sprintf(message, "initMsb: registered client, result = %d", result);
    //LOGIT(message);

    return (result);    
}

void closeMsb(ems *myEmsPtr) {
    char message[1000], error[1000];
    uuid_t uuid;

    msbClientHaltClientStateMachine(myEmsPtr->client);
}

int msb(ems *myEmsPtr) {
    int result;
    char message[1000], error[1000];
    uuid_t uuid;

    
    // msbClientRunAutomatThreadI(myEmsPtr->client); //automat in thread

    json_object *dataObjectA;
	
    dataObjectA = json_object_new_object();

    json_object_object_add(dataObjectA, "tempboiler", json_object_new_double(myEmsPtr->tempBoiler));
    json_object_object_add(dataObjectA, "tempwater", json_object_new_double(myEmsPtr->tempWater));
    json_object_object_add(dataObjectA, "tempexhaust", json_object_new_double(myEmsPtr->tempExhaust));
    json_object_object_add(dataObjectA, "tempinside", json_object_new_double(myEmsPtr->tempInside));
    json_object_object_add(dataObjectA, "tempoutside", json_object_new_double(myEmsPtr->tempOutside));

    usleep(100000);

    // for (int i = 0; i < 3; i++) {

    Line = __LINE__;    

    msbClientPublishComplex(
			    myEmsPtr->client,
			    "emsvalues", // event id
			    HIGH, // priority
			    dataObjectA, // data
			    NULL // correlation id string, auto generated if NULL
			    );
    Line = __LINE__;    

    while (myEmsPtr->client->dataOutInterfaceFlag == 1) {
	usleep(50000);
    }
    Line = __LINE__;    
    
    //    }

    // get self description
    char* msg = msbObjectSelfDescription(myEmsPtr->client->msbObjectData);
    if (myEmsPtr->debug > 1)
	LOGIT(msg);
    Line = __LINE__;    

    int flag = myEmsPtr->client->dataOutInterfaceFlag;
    
    sprintf(message, "msb: published data, flag = %d, tempboiler = %.1f, tempwater = %.1f",
	    flag, myEmsPtr->tempBoiler, myEmsPtr->tempWater);
    if (Debug || !Daemon)
	LOGIT(message);
    
    return (flag);
}
