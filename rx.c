#include <stdint.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>

#include "ems.h"
#include "serial.h"
#include "emsSerio.h"
#include "queue.h"
#include "tx.h"

size_t rx_len;
uint8_t rx_buf[MAX_PACKET_SIZE];
enum STATE state = RELEASED;
uint8_t polled_id, client_id;
uint8_t read_expected[HDR_LEN];
struct timeval got_bus;

int rx_wait() {
    fd_set rfds;
    struct timeval tv;

    // Wait maximum 200 ms for the BREAK
    FD_ZERO(&rfds);
    FD_SET(port, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 1000*200; // 200 ms
    return(select(FD_SETSIZE, &rfds, NULL, NULL, &tv));
}

// Read a BREAK from the MASTER_ID.
int rx_break() {
    char message[MAXPATH];
    int ret;
    uint8_t echo;

    ret = rx_wait();
    if (ret != 1) {
        sprintf(message, "select() failed: %i", ret);
	LOGERR(message);
        return(-1);
    }
    for (size_t i = 0; i < sizeof(BREAK_IN) - 1; i++) {
        ret = read(port, &echo, 1);
        if (ret != 1 || echo != BREAK_IN[i]) {
            sprintf(message, "TX fail: expected break char 0x%02x but got 0x%02x", echo,
                BREAK_IN[i]);
	    LOGERR(message);
            return(-1);
        }
    }
    return(0);
}

// Loop that reads single characters until a full packet is received.
void rx_packet(int *abort) {
    char message[MAXPATH];
    uint8_t c;
    unsigned int parity = 0;
    unsigned int parity_errors = 0;

    rx_len = 0;
    while (*abort != 1) {
        if (read(port, &c, 1) != 1)
            continue;
        if (parity == 0 && c == 0xff) {
            // We got a parity mark character.
            parity = 1;
            continue;
        }
        if (parity == 1) {
            // Character after parity mark
            if (c == 0x00) {
                // 0xff 0x00 -> This is the mark, next is the marked character.
                parity = 2;
                continue;
            }
            parity = 0;
            if (c != 0xFF) {
                // Cannot be... Ignore that character.
                continue;
            }
        }
        if (parity == 2) {
            if (c == 0x00) {
                // A parity marked 0x00. This is the end message signal.
                return;
            }
            // A character with parity mark.
            // This should not happen as other charaters are not sent with a parity bit.
            parity_errors++;
            parity = 0;
        }

        // Discard all character above the message limit and warn.
        if (rx_len >= MAX_PACKET_SIZE) {
            if (rx_len == MAX_PACKET_SIZE) {
                sprintf(message, "Maximum packet size reached. Following characters ignored."
                               "\tIs your serial connected and is it detecting breaks?");
		LOGERR(message);
	    }
            continue;
        }
        rx_buf[rx_len++] = c;
    }
}

// Handler on a received packet
void rx_done() {
    char message[MAXPATH];
    uint8_t dst;

    // Handle MAC packages first. They always have length 1.
    // MASTER_ID poll requests (bus assigns) have bit 7 set (0x80).
    // Bus release messages is the device ID, between 8 and 127 (0x08-0x7f).
    // When a device has the bus, it can:
    // - Broadcast a message (destination is 0x00) (no response)
    // - Send a write request to another device (destination is device ID) (ACKed with 0x01)
    // - Read another device (desination is ORed with 0x80) (Answer comes immediately)
    if (rx_len == 1) {
        print_packet(0, LOG_MAC, rx_buf, rx_len);
        if (rx_buf[0] == 0x01) {
            // Got an ACK. Warn if there was no write from the bus-owning device.
            if (state != WROTE) {
                sprintf(message, "Got an ACK without prior write message from 0x%02hhx", polled_id);
		LOGERR(message);
                stats.rx_mac_errors++;
            }
            if (polled_id == client_id) {
                // The ACK is for us after a write command. We can send another message.
                handle_poll();
            } else {
                state = ASSIGNED;
            }
        } else if (rx_buf[0] >= 0x08 && rx_buf[0] < 0x80) {
            // Bus release.
            if (state != ASSIGNED) {
                sprintf(message, "Got bus release from 0x%02hhx without prior poll request", rx_buf[0]);
		LOGERR(message);
                stats.rx_mac_errors++;
            }
            polled_id = 0;
            state = RELEASED;
        } else if (rx_buf[0] & 0x80) {
            // Bus assign. We may not be in released state it the queried device did not exist.
            if (state != RELEASED && state != ASSIGNED) {
                sprintf(message, "Got bus assign to 0x%02hhx without prior bus release from %02hhx", rx_buf[0], polled_id);
		LOGERR(message);
                stats.rx_mac_errors++;
            }
            polled_id = rx_buf[0] & 0x7f;
            if (polled_id == client_id) {
                gettimeofday(&got_bus, NULL);
                handle_poll();
            } else {
                state = ASSIGNED;
            }
        } else {
            sprintf(message, "Ignored unknown MAC package 0x%02hhx", rx_buf[0]);
	    LOGERR(message);
            stats.rx_mac_errors++;
        }
        return;
    }

    print_packet(0, LOG_PACKET, rx_buf, rx_len);

    stats.rx_total++;
    if (rx_len < 6) {
        sprintf(message, "Ignored short package");
	LOGERR(message);
        if (state == WROTE || state == READ)
            state = ASSIGNED;
        stats.rx_short++;
        return;
    }

    // The MASTER_ID can always send when the bus is not assigned (as it's senseless to poll himself).
    // This implementation does not implement the bus timeouts, so it may happen that the MASTER_ID
    // sends while this program still thinks the bus is assigned.
    // So simply accept messages from the MASTER_ID and reset the state if it was not a read request
    // from a device to the MASTER_ID
    if (rx_buf[0] == 0x08 && (state != READ || memcmp(read_expected, rx_buf, HDR_LEN))) {
        state = RELEASED;
    } else if (state == ASSIGNED) {
        if (rx_buf[0] != polled_id && rx_buf[0] != MASTER_ID) {
            sprintf(message, "Ignored package from 0x%02hhx instead of polled 0x%02hhx or MASTER_ID",
                   rx_buf[0], polled_id);
	    LOGERR(message);
            stats.rx_sender++;
            return;
        }
        dst = rx_buf[1] & 0x7f;
        if (rx_buf[1] & 0x80) {
            if (dst < 0x08) {
                sprintf(message, "Ignored read from 0x%02hhx to invalid address 0x%02hhx",
                    rx_buf[0], dst);
		LOGERR(message);
                stats.rx_format++;
                return;
            }
            // Write request, prepare immediate answer
            read_expected[0] = dst;
            read_expected[1] = rx_buf[0];
            read_expected[2] = rx_buf[2];
            read_expected[3] = rx_buf[3];
            state = READ;
        } else {
            if (dst > 0x00 && dst < 0x08) {
                sprintf(message, "Ignored write from 0x%02hhx to invalid address 0x%02hhx",
                    rx_buf[0], dst);
		LOGERR(message);
                stats.rx_format++;
                return;
            }
            if (dst >= 0x08) {
                state = WROTE;
            }
            // Else is broadcast, do nothing than forward.
        }
    } else if (state == READ) {
        // Handle immediate read response.
        state = ASSIGNED;
        if (memcmp(read_expected, rx_buf, HDR_LEN)) {
            sprintf(message, "Ignored not expected read header: %02hhx %02hhx %02hhx %02hhx",
                rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);
	    LOGERR(message);
            stats.rx_format++;
            return;
        }
        if (polled_id == client_id) {
            handle_poll();
        }
    } else if (state == WROTE) {
        sprintf(message, "Received package from 0x%02hhx when waiting for write ACK", rx_buf[0]);
	LOGERR(message);
        stats.rx_sender++;
        return;
    } else if (rx_buf[0] != MASTER_ID) {
        sprintf(message, "Received package from 0x%02hhx when bus is not assigned", rx_buf[0]);
	LOGERR(message);
        stats.rx_sender++;
        return;
    }

    // Do not check the CRC here. It adds too much delay and we risk missing a poll cycle.
    stats.rx_success++;
    if (mq_send(rx_queue, (char *)rx_buf, rx_len, 0) == -1) {
        sprintf(message, "RX: Could not add packet to queue: %s", strerror(errno));
	LOGERR(message);
    }
}

