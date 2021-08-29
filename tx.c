#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>

#include "ems.h"
#include "queue.h"
#include "serial.h"
#include "emsSerio.h"
#include "rx.h"
#include "crc.h"

int tx_retries = -1;
uint8_t tx_buf[MAX_PACKET_SIZE];
size_t tx_len;
uint8_t client_id = CLIENT_ID;

void tx_break() {
    char message[MAXPATH];
    int ret;
    // Send a BREAK signal on the EMS bus
    // Each message must be closed with a 9-bit low level on the bus.
    // Posix termios does not support 9-bit tty, so just enable a even parity bit.
    // The interface then sends a 9th zero bit after sending 0x00.
    ret = set_parity(1);
    if (ret != 0) {
        return;
    }
    if (Debug) {
	sprintf(message, "WR 0x%02hhx BREAK", BREAK_OUT[0]);
	LOGIT(message);
    }
    write(port, BREAK_OUT, 1);
    set_parity(0);
}

ssize_t tx_packet(uint8_t *msg, size_t len) {
    char message[MAXPATH];
    size_t i;
    uint8_t echo;

    print_packet(1, LOG_PACKET, msg, len);

    // Write the message by character while checking the echoed characters from the MASTER_ID
    for (i = 0; i < len; i++) {
        sprintf(message, "WR 0x%02hhx", msg[i]);
	if (Debug)
	    LOGIT(message);
        if (write(port, &msg[i], 1) != 1) {
            sprintf(message, "write() failed");
	    LOGERR(message);
            return(i);
        }
        if (rx_wait() != 1) {
            sprintf(message, "Echo not received after 200 ms");
	    LOGERR(message);
            return(i);
        }
        if (read(port, &echo, 1) != 1) {
            sprintf(message, "read() failed after successful select");
	    LOGERR(message);
            return(i);
        };
        sprintf(message, "RD 0x%02hhx", echo);
	if (Debug)
	    LOGIT(message);
        if (msg[i] != echo) {
            sprintf(message, "TX fail: send 0x%02x but echo is 0x%02x", msg[i], echo);
	    LOGERR(message);
            return(i);
        }
        if (echo == 0xff) {
            // Parity escaping also doubles a 0xff
            if (read(port, &echo, 1) != 1) {
                sprintf(message, "read() failed");
		LOGERR(message);
                return(i);
            }
            sprintf(message, "RD 0x%02hhx", echo);
	    LOGIT(message);
            if (echo != 0xff) {
                sprintf(message, "TX fail: parity escaping expected 0xff but got 0x%02x", echo);
		LOGERR(message);
                return(i);
            }
        }
    }

    tx_break();
    if (rx_break() == -1) {
        sprintf(message, "TX fail: packet not ACKed by MASTER_ID");
	LOGERR(message);
        return(0);
    }

    return(i);
}

void handle_poll() {
    char message[MAXPATH];
    ssize_t ret;
    struct timeval now;
    int64_t have_bus;

    // We got polled by the MASTER_ID. Send a message or release the bus.
    // Todo: Send more than one message
    // Todo: Release the bus after sending a message (does not work)
    if (tx_retries < 0 || tx_retries > MAX_TX_RETRIES) {
        if (tx_retries > MAX_TX_RETRIES) {
            sprintf(message, "TX failed 5 times. Dropping message.");
	    LOGERR(message);
            tx_retries = -1;
        }
        // Pick a new message
        //ret = msgrcv(tx_queue, &tx_buf, sizeof(tx_buf), 0, IPC_NOWAIT | MSG_NOERROR);
        ret = mq_receive(tx_queue, (char *)tx_buf, MAX_PACKET_SIZE, 0);
        if (ret > 0) {
            tx_retries = 0;
            tx_len = (size_t)ret;
            if (tx_len >= 6) {
                // Set the source ID and CRC value
                tx_buf[0] = client_id;
                tx_buf[tx_len - 1] = calc_crc(tx_buf, tx_len);
            }
        }
    }

    gettimeofday(&now, NULL);
    have_bus = (now.tv_sec - got_bus.tv_sec) * 1000000 + now.tv_usec - got_bus.tv_usec;
    sprintf(message, "Occupying bus since %lli us", have_bus);
    if (Debug)
	LOGIT(message);

    if (tx_retries >= 0 && have_bus < MAX_BUS_TIME) {
        print_packet(1, LOG_PACKET, tx_buf, tx_len);
        if ((size_t)tx_packet(tx_buf, tx_len) == tx_len) {
            tx_retries = -1;
            if (tx_buf[1] == 0x00) {
                // Release bus
                print_packet(1, LOG_PACKET, &client_id, 1);
                if (tx_packet(&client_id, 1) != 1) {
                    sprintf(message, "TX poll reply failed");
		    LOGERR(message);
                }
                state = RELEASED;
            } else if (tx_buf[1] & 0x80) {
                read_expected[0] = tx_buf[1] & 0x7f;
                read_expected[1] = tx_buf[0];
                read_expected[2] = tx_buf[2];
                read_expected[3] = tx_buf[3];
                state = READ;
            } else {
                // Write command
                state = WROTE;
            }
        } else {
            sprintf(message, "TX failed, %i/%i", tx_retries, MAX_TX_RETRIES);
	    LOGERR(message);
            tx_retries++;
            state = RELEASED;
        }
    } else {
        // Nothing to send.
        print_packet(1, LOG_MAC, &client_id, 1);
        if (tx_packet(&client_id, 1) != 1) {
            sprintf(message, "TX poll reply failed");
	    LOGERR(message);
        }
        state = RELEASED;
    }
}
