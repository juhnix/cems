/*
  emsMonitor.c
  display (live) some values of ems shared memory segment, runs as unpriviledeged user

  $Id: emsMonitor.c 62 2022-03-06 17:20:16Z juh $
*/

#include <termios.h>
#include <stdbool.h>
#include <wchar.h>

#include "ems.h"
#include "emsDevices.h"

char SVN[] = "$Id: emsMonitor.c 62 2022-03-06 17:20:16Z juh $";
char hLine[] = "───────────────────────────────────────────────────────────────────────────";

// declarations
void itoa(int value, char* str, int base);

int main(int argc, char *argv[]) {
    int c, res;
    key_t key = SHMKEY;
    int shmid, i, j, loopB = 0, loopC = 0, loopW = 0, *ivalue, length;
    char *data, message[MAXPATH], message2[MAXPATH], localTime[MAXPATH], buff[16], name[MAXNAME];
    char *active[] = {"|", "/", "-", "\\", "|", "/", "-", "\\", "0"};
    time_t t, ct, delta, currentTime;
    struct tm *tm;
    int tempSens, cycles, interval, config = 0;
    static struct termios oldt, newt;
    struct termios orig_term, raw_term;

    while ((c = getopt (argc, argv, "vnxhVi:k:")) != -1) {
        switch (c) {
        case 'i':
            shmid = atoi(optarg);
            break;
        case 'k':
            key = atoi(optarg);
            break;
        case 'v': // be verbose
            Debug = true;
            break;
        case 'V': // show version and exit
            fprintf (stderr, "emsMonitor, svn info: %s\n", SVN);
            exit (0);
            break;
        case 'h':
        case '?':
            fprintf (stderr, "%s:\n\tOption -v activates debug mode,\n", argv[0]);
            fprintf (stderr, "\tOption -?/-h show this information\n");
            fprintf (stderr, "\tOption -V show the version information\n");
            fprintf (stderr, "\tOption -i # sets the shmid to #\n");
            exit(0);
            break;
            
        default:
            break;
        }
    }
    
    // getting already existing shared memory...
    shmid = shmget(key, sizeof(ems), 0);

    if (shmid < 0) {
        fprintf(stderr, "emsMonitor: shmget returned %d, could not get shared memory, error %s, errno = %d", shmid, strerror(errno), errno);
        exit(errno);
    }
    else {
        emsPtr = shmat(shmid, NULL, 0);  // <- this NULL is discussable - the size could be wrong...
        printf("emsMonitor: attached shared memory, shmid = %d\n", shmid);
    }

    // Get terminal settings and save a copy for later
    tcgetattr(STDIN_FILENO, &orig_term);
    raw_term = orig_term;

    // Turn off echoing and canonical mode
    raw_term.c_lflag &= ~(ECHO | ICANON);
    
    // Set min character limit and timeout to 0 so read() returns immediately
    // whether there is a character available or not
    raw_term.c_cc[VMIN] = 0;
    raw_term.c_cc[VTIME] = 0;
    
    // Apply new terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);
    
    char ch = 0;
    
    // start loop forever (or pressing q, whatever first :-))
    for (;;) {
        printf("\033c"); // clear screen
	length = (strlen(SVN)+1) * 3;
	printf("┌────────────%.*s┐\n", length, hLine);
	printf("│ emsMonitor %s │\n", SVN);
	printf("└────────────%.*s┘\n", length, hLine);
        printf("status of ems: %02x (%02x %02x), model: %d \n",
               emsPtr->status, emsPtr->code1, emsPtr->code2, emsPtr->model);

        // get current time
        ct = time(NULL);
        currentTime = ct;
        tm = localtime(&ct);
        strftime (message, MAXPATH, "current time %H:%M:%S\t",tm);
        fputs(message, stdout);
	
        // get time(s) of heartbeats of processes
        t = (time_t)emsPtr->heartbeatDecode;
        printf("offsets: decode %ds, ", (int)(ct - t));
        t = (time_t)emsPtr->heartbeatSerio;
        printf("serio %ds, ", (int)(ct - t));
        t = (time_t)emsPtr->heartbeatMsb;
        printf("msb %ds, ", (int)(ct - t));
        t = (time_t)emsPtr->heartbeatMqtt;
        printf("mqtt %ds \n", (int)(ct - t));

	//printf("───────────────────────────\n");
	
        // show actual values or configuration
        if (!config) {
	    // check for name of system
	    sprintf(name, "unknown system %1$d (%1$#x)", emsPtr->model);
	    for (i = 0; i < (int)sizeof(emsDev); i++) {
		if (emsDev[i].code == emsPtr->model) {
		    strcpy(name, emsDev[i].name);
		    break;
		}
	    }
	    length = strlen(name) * 3;
	    printf("┌%.*s┐\n", length, hLine);
	    printf("│%s│\n", name);
	    printf("└%.*s┘\n", length, hLine);
            printf("nominal temps, boiler: %02.1f °C, water: %02.1f °C\n",
                   emsPtr->setTemperature, emsPtr->setWaterTemp);
	    printf("───────────────────────────\n");
	    printf("boiler: %2.1f °C, water: %2.1f °C, outside: %2.1f °C, inside: %2.1f °C\n",
		   emsPtr->tempBoiler, emsPtr->tempWater, emsPtr->tempOutside, emsPtr->tempInside);
	    printf("flame current: %2.1f µA, temp exhaust: %2.1f °C, power: %d %%\n",
		   emsPtr->current, emsPtr->tempExhaust, emsPtr->power);
	    printf("operation time %ld h %ld m, starts %ld, average op time %.1f\n",
		   emsPtr->opTime / 60, emsPtr->opTime % 60, emsPtr->starts,
		(double)emsPtr->opTime / (double)emsPtr->starts);
	    printf("pump = %d, boilerState = %d, waterState = %d, loadingPump = %d\n",
		   emsPtr->pump, emsPtr->boilerState, emsPtr->waterState, emsPtr->loadingPump);
	    printf("circPump = %d, circState1 = %d, circState2 = %d\n",
		   emsPtr->circPump, emsPtr->circState1, emsPtr->circState2);
	    
            if (emsPtr->power > 0) {
                printf("boiler %s ", active[loopB]);
                loopB++;
                if (loopB > 7)
                    loopB = 0;
            }
            else
                printf("boiler %s ", active[8]);
	    
	    if (emsPtr->circPump == 1) {
                printf("circ pump %s ", active[loopC]);
                loopC++;
                if (loopC > 7)
                    loopC = 0;
            }
            else
                printf("circ pump %s ", active[8]);
	    
            printf("pump is %s, ", emsPtr->pump ? "on" : "off");
            
            itoa(emsPtr->status, buff, 2);
            printf("\n%s\tR1: %s R2: %s, R3: %s, R4: %s\n",
		   buff,
                   buff[7] == '1' ? "set" : "off",
                   buff[6] == '1' ? "set" : "off",
                   buff[5] == '1' ? "set" : "off",
                   buff[4] == '1' ? "set" : "off"
                   );
	    printf("msb interval: %d µs\n",
                   emsPtr->interval);
	}
	else { // if (!config) - show configuration values
	    printf("configuration file: %s\n", emsPtr->configFile);
            printf("mqtt broker: %s, port: %d, tls: %s\n",
                   emsPtr->broker, emsPtr->port, strlen(emsPtr->cert) > 0 ? "yes" : "no");
	    printf("receive queue: %s, transmit queue: %s\n", emsPtr->rxqueue, emsPtr->txqueue);
	    printf("msb url: %s, uuid: %s, token: %s\n",
                   emsPtr->msbUrl, emsPtr->msbUuid, emsPtr->msbToken);

	}
	printf("───────────────────────────\n");
	printf(" press 'q' to quit, 'c' to switch data/configuration, 'd' to increase debug, 'x' to decrease debug level,\n");
	printf(" 'w' water desinfect, 't' stop desinfect, 's' toggle summer mode\n");
	printf(" 'p'/'r' to activate/stop circulation pump for warm water\n");
	
	int len = read(STDIN_FILENO, &ch, 1);
	if (len == 1) {
            if (Debug)
                printf("You pressed char 0x%02x : %c\n", ch,
                       (ch >= 32 && ch < 127) ? ch : ' ');
            
            switch (ch) {
            case 'q':
            case 'Q':
            case 27: // Ctrl-C
                while (read(STDIN_FILENO, &ch, 1) == 1);
                
                // Restore original terminal settings
                tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
                //emsPtr->remoteOverride = false; // give control back to hardware switch
                exit(0);
                break;
		
            case 'c':
            case 'C':
                if (config)
                    config = false;
                else
                    config = true;
                break;

            case 'p':
                // set circulation pump on
                emsPtr->circPump = 1;
                break;
                 
            case 'r':
                // set circulation pump off
                emsPtr->circPump = 0;
                break;
                
            case 'd':
            case 'D':
                emsPtr->debug++;
                break;
                
            case 's':
            case 'S':
                if (emsPtr->summerMode)
                    emsPtr->summerMode = false;
                else
                    emsPtr->summerMode = true;
                break;
                
            case 'x':
            case 'X':
                if (emsPtr->debug > 0)
                    emsPtr->debug--;
                break;

            default:
                printf("\n\n\noops, %c unknown command\n", ch);
                sleep(5);
                break;
            }
        }
   
        // wait a second...
        sleep(1);
    }
}

