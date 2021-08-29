#define MAX_PACKET_SIZE 32
#define SRCPOS 0
#define DSTPOS 1
#define HDR_LEN 4

#define BREAK_IN "\xFF\x00\x00"
#define BREAK_OUT "\x00"
#define MAX_TX_RETRIES 5
#define ACK_LEN 1
#define ACK_VALUE 0x01
#define MAX_BUS_TIME 200 * 1000

#define LOG_ERROR 0x01   // Error messages
//#define LOG_INFO 0x02    // Informational messages on start and stop
#define LOG_VERBOSE 0x04 // Verbose message on start and stop and bus timing
#define LOG_PACKET 0x08  // Output received and transmitted packets
#define LOG_MAC 0x10     // Output sync (token) information
#define LOG_CHAR 0x20    // Output single characters

struct STATS {
    unsigned int rx_mac_errors;
    unsigned int rx_total;
    unsigned int rx_success;
    unsigned int rx_short;
    unsigned int rx_sender; // Bad senders
    unsigned int rx_format; // Bad format
    unsigned int rx_crc;
    unsigned int tx_total;
    unsigned int tx_fail;
};

enum STATE { RELEASED, ASSIGNED, WROTE, READ };

