/* File : T1_rx.cpp */
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include "dcomm.h"

/* Delay to adjust speed of consuming buffer, in milliseconds */
#define DELAY 500

/* Define receive buffer size */
#define RXQSIZE 8

/* Supplementary */
#define bzero(p, size) (void)memset((p), 0 , (size))
#define MIN_UPPERLIMIT 5
#define MAX_LOWERLIMIT 2

/* FUNCTIONS AND PROCEDURES */
static Byte *rcvchar(int sockfd, QTYPE *queue);
static Byte *q_get(QTYPE *, Byte *);
void *childRProcess(void * threadid);
void errOut(const char* message);

Byte rxbuf[RXQSIZE];
QTYPE rcvq = {0, 0, 0, RXQSIZE, rxbuf};
QTYPE *rxq = &rcvq;
Byte sent_xonxoff = XON;
Boolean send_xon = false, send_xoff = false;
int endFileReceived;

/* Socket */
int sockfd; // listen on sock_fd
struct sockaddr_in host;
struct sockaddr_in sourceAddress;
unsigned int sourceLength = sizeof (sourceAddress);

int main(int argc, char *argv[]) {
    //Creating Thread
    pthread_t thread[1];

    //Binding Socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        printf("FATAL: Gagal membuat socket.\n");

    bzero((char*) &host, sizeof (host));
    host.sin_family = AF_INET;
    host.sin_port = htons(atoi(argv[1]));
    host.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*) &host, sizeof (host)) < 0)
        errOut("FATAL: Binding gagal.\n");

    endFileReceived = 0;

    memset(rxbuf, 0, sizeof (rxbuf));

    /* Create child process with threading */
    if (pthread_create(&thread[0], NULL, childRProcess, 0))
        errOut("FATAL: Gagal membuat thread untuk child process.\n");

    /* parent process: unlimited looping */
    Byte c;
    while (1) {
        c = *(rcvchar(sockfd, rxq));

        if (c == Endfile)
            endFileReceived = 1;
    }

    return 0;
}


static Byte *rcvchar(int sockfd, QTYPE *queue) {
    /*
        Insert code here.
        Read a character from socket and put it to the receive
        buffer.
        If the number of characters in the receive buffer is above
        certain level, then send XOFF and set a flag (why?).
        Return a pointer to the buffer where data is put.
    */
    Byte* current;
    char tempBuf[1];
    char b[1];
    static int counter = 1;

    if (recvfrom(sockfd, tempBuf, 1, 0, (struct sockaddr *) &sourceAddress, &sourceLength) < 0)
        errOut("FATAL: Gagal menerima karakter dari socket\n");

    current = (Byte *) malloc(sizeof (Byte));
    *current = tempBuf[0];

    if (*current != Endfile) {
        printf("Menerima byte ke-%d: ", counter++);
        switch (*current) {
            case CR: printf("\'CR\'");
                break;
            case LF: printf("\'LF\'");
                break;
            case Endfile:
                printf("\'EOF\'");
                break;
            default: printf("\'%c\'", *current);
                break;
        }
        printf(".\n");
    }

    // adding char to buffer and resync the buffer queue
    if (queue->count < 8) {
        queue->rear = (queue->count > 0) ? (queue->rear + 1) % 8 : queue->rear;
        queue->data[queue->rear] = *current;
        queue->count++;
    }

    // if the buffer reaches Minimum Upperlimit, send XOFF to Transmitter
    if (queue->count >= (MIN_UPPERLIMIT) && sent_xonxoff == XON) {
        printf("Buffer > minimum upperlimit.\nMengirim XOFF.");
        send_xoff = true;
        send_xon = false;
        b[0] = sent_xonxoff = XOFF;

        if (sendto(sockfd, b, 1, 0, (struct sockaddr *) &sourceAddress, sourceLength) < 0)
            errOut("FATAL: Gagal mengirim XOFF.\n");
    }

    return current;
}

void *childRProcess(void *threadid) {
    Byte *data,
    *current = NULL;

    while (1) {
        /* Call q_get */
        current = q_get(rxq, data);
        /* Can introduce some delay here. */
        sleep(2);
    }

    pthread_exit(NULL);
}

/* q_get returns a pointer to the buffer where data is read or NULL if
* buffer is empty.
*/
static Byte *q_get(QTYPE *queue, Byte *data) {
    Byte *current = NULL;
    char b[1];
    static int counter = 1;

    /* Only consume if the buffer is not empty */

    if (queue->count > 0) {
        current = (Byte *) malloc(sizeof (Byte));
        *current = queue->data[queue->front];
        // incrementing front (circular) and reducing number of elements
        queue->front++;
        if (queue->front == 8) {
            queue->front = 0;
        }
        queue->count--;

        printf("Mengkonsumsi byte ke-%d: ", counter++);
        switch (*current) {
            case CR: printf("\'Carriage Return\'\n");
                break;
            case LF: printf("\'Line Feed\'\n");
                break;
            case Endfile:
                printf("\'End of File\'\n");
                break;
            default: printf("\'%c\'\n", *current);
                break;
        }
    }

    /* Insert code here.  Retrieve data from buffer, save it to "current" and "data"
    If the number of characters in the receive buffer is below certain  level, then send
    XON. Increment front index and check for wraparound. */
    if (queue->count <= MAX_LOWERLIMIT && sent_xonxoff == XOFF) {
        printf("Buffer < maximum lowerlimit\nMengirim XON.");
        send_xoff = false;
        send_xon = true;

        b[0] = sent_xonxoff = XON;
        if (sendto(sockfd, b, 1, 0, (struct sockaddr *) &sourceAddress, sourceLength) < 0)
            errOut("FATAL: Gagal mengirim XON.\n");
    }
    // return the Byte consumed
    return current;
}

void errOut(const char *message) {
    perrOut(message);
    exit(1);
}