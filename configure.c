// configure.c - read paramters from config file
//
// $Id: configure.c 154 2020-04-06 15:51:50Z juh $

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>    // for mode constants
#include <time.h>        // for time()    
#include <fcntl.h>       // for IPC_CREAT
#include <sys/types.h>   // for key_t
#include <sys/shm.h>     // for shmat()
#include <sys/prctl.h>   // for prctl()
#include <stdlib.h>      // for exit()
#include <errno.h>       // for errno
#include <unistd.h>      // for sleep()
#ifdef USEPIG
#include <pigpio.h>      // GPIO handling
#endif
#include <signal.h>
#include <sys/ipc.h>

#include <syslog.h>      // syslog messages
#include <stdbool.h>     // to have definitions of true and false
#include <linux/prctl.h>
#include "parser/parser.h" // config file handling

#include "ems.h"

int getConfig(enum varType varType, void *var, char *defVal, char *cFile, char *group, char *key);
char *readKeyVal(char *cFile, char *group, char *key);
int readKeyInt(char *cFile, char *group, char *key);
float readKeyFloat(char *cFile, char *group, char *key);

int getConfig(enum varType varType, void *var, char *defVal, char *cFile, char *group, char *key) {
    char *result = NULL;
    int iResult, *i, j;
    float fResult, *f;
    int useDefault = false;

    switch (varType)
    {
    case CHAR:
	result = readKeyVal(cFile, group, key);
	if (result == NULL) {
	    strcpy((char *) var, defVal);
	    useDefault = true;
	}
	else {
	    strcpy((char *) var, result);
	    useDefault = false;
	}
	break;
	    
    case INT:
	i = var;
	iResult = readKeyInt(cFile, group, key);
	if (iResult == -1) {
	    iResult = atoi(defVal);
	    *i = iResult;
	    useDefault = true;
	}
	else {
	    *i = iResult;
	    useDefault = false;
	}
	break;

    case FLOAT:
	f = var;
	fResult = readKeyFloat(cFile, group, key);
	if (fResult == -1.0) {
	    fResult = atof(defVal);
	    *f = fResult;
	    useDefault = true;
	}
	else {
	    *f = fResult;
	    useDefault = false;
	}
	break;

    default:
	useDefault = true;
	break;
    }

    return (useDefault);
}
	    

char Result[1000];

char *readKeyVal(char *cFile, char *group, char *key) {
    char *val;
    struct parsefile *file;
    struct entry *g;

    /* pass 2nd argument as 0 to parseopen to prevent automatic 
       processing of the entries in the configuration file. */
    file = parseopen(cFile, 0, 0);
    if (file == NULL)
	return(NULL);
    
    g = file->root;
    // parse groups
    while (g) {
	struct entry *l;
	    
	l = g->sub;

	// lines in group
	while (l) {
	    struct entry *v;
		
	    v = l->sub;

	    // objects in line
	    while (v) {
		struct entry *kv;

		kv = v->sub;
		if (kv) {
		    if ((strcmp(group, name(g)) == 0) && (strcmp(key, name(v)) == 0)
			&& type(v) == 2 && type(kv) == 3) {
			strcpy(Result, name(kv));
			printf("readKeyVal: read key %s from group %s as >%s<\n", key, group, Result);
			parseclose(file);
			return (Result);
		    }
		}
		v = v->next;
	    }
	    l = l->next;
	}
	g = g->next;
    }
    
    parseclose(file);
    return (NULL);
}

int readKeyInt(char *cFile, char *group, char *key) {
    int val, res;

    struct parsefile *file;
    struct entry *g;

    /* pass 2nd argument as 0 to parseopen to prevent automatic 
       processing of the entries in the configuration file. */
    file = parseopen(cFile, 0, 0);
    if (file == NULL)
	return(-1);
    
    g = file->root;
    // parse groups
    while (g) {
	struct entry *l;
	    
	l = g->sub;

	// lines in group
	while (l) {
	    struct entry *v;
		
	    v = l->sub;

	    // objects in line
	    while (v) {
		struct entry *kv;

		kv = v->sub;
		if (kv) {
		    if ((strcmp(group, name(g)) == 0) && (strcmp(key, name(v)) == 0)
			&& type(v) == 2 && type(kv) == 3) {
			// try to be clever about hex values
			res = sscanf(name(kv), "%i", &val);
			printf("readKeyInt: read key %s from group %s as >%s<, val %d, sscanf res = %d\n",
			       key, group, name(kv), val, res);
			    
			return (val);
		    }
		}
		v = v->next;
	    }
	    l = l->next;
	}
	g = g->next;
    }
    
    parseclose(file);
    return (-1);
}

float readKeyFloat(char *cFile, char *group, char *key) {
    float val;

    struct parsefile *file;
    struct entry *g;

    /* pass 2nd argument as 0 to parseopen to prevent automatic 
       processing of the entries in the configuration file. */
    file = parseopen(cFile, 0, 0);
    if (file == NULL)
	return(-1);
    
    g = file->root;
    // parse groups
    while (g) {
	struct entry *l;
	    
	l = g->sub;

	// lines in group
	while (l) {
	    struct entry *v;
		
	    v = l->sub;

	    // objects in line
	    while (v) {
		struct entry *kv;

		kv = v->sub;
		if (kv) {
		    if ((strcmp(group, name(g)) == 0) && (strcmp(key, name(v)) == 0)
			&& type(v) == 2 && type(kv) == 3) {
			printf("readKeyFloat: read key %s from group %s as >%s<, val %f\n",
			       key, group, name(kv), atof(name(kv)));
			return (atof(name(kv)));
		    }
		}
		v = v->next;
	    }
	    l = l->next;
	}
	g = g->next;
    }
    
    parseclose(file);
    return (-1.0);
}
