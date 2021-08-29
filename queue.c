#include <mqueue.h>
#include <errno.h>

#include "ems.h"
#include "defines.h"

mqd_t tx_queue;
mqd_t rx_queue;

int setup_queue(mqd_t *queue, char *name) {
    struct mq_attr queue_attr;
    int error = 0;
    
    queue_attr.mq_maxmsg = 32;
    queue_attr.mq_msgsize = MAX_PACKET_SIZE;
    *queue = mq_open(name, O_RDWR | O_NONBLOCK | O_CREAT, 0666, &queue_attr);
    error = errno;
    return (error);
}

void close_queues(ems *emsPtrL) {
    mq_close(rx_queue);
    mq_close(tx_queue);
    // and unlink it
    mq_unlink(emsPtrL->rxqueue);
    mq_unlink(emsPtrL->txqueue);    
}
