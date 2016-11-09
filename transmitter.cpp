/* File 	: transmitter.c */
#include "dcomm.h"
#define BUFMAX 1 /* Maximum size of buffer that can be sent */

/* NETWORKS */
int sockfd, port; // sock file descriptor and port number
struct hostent *server;
struct sockaddr_in receiverAddr; // receiver host information
int receiverAddrLen = sizeof (receiverAddr);

/* FILE AND BUFFERS */
FILE *tFile; // file descriptor
char *receiverIP; // buffer for Host IP address
char buf[BUFMAX + 1]; // buffer for character to send
char xbuf[BUFMAX + 1]; // buffer for receiving XON/XOFF characters

/* FLAGS */
bool isXON = true; // flag for XON/XOFF sent
int isSocketOpen; // flag to indicate if connection from socket is done


/* FUNCTIONS AND PROCEDURES */
void error(const char *message);
void *childProcess(void *threadid);

int main(int argc, char *argv[]) {
    pthread_t thread[1];

    if (argc < 4) {
        // case if arguments are less than specified
        printf("Please use the program with arguments: %s <target-ip> <port> <filename>\n", argv[0]);
        return 0;
    }

    if ((server = gethostbyname(argv[1])) == NULL)
        error("FATAL: Receiver Address is Unknown or Wrong.\n");

    // creating IPv4 data stream socket
    printf("Membuat socket untuk koneksi ke %s:%s ...\n", argv[1], argv[2]);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        error("FATAL: Gagal membuat socket.\n");

    // flag set to 1 (connection is established)
    isSocketOpen = 1;

    // initializing the socket host information
    memset(&receiverAddr, 0, sizeof (receiverAddr));
    receiverAddr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &receiverAddr.sin_addr.s_addr, server->h_length);
    receiverAddr.sin_port = htons(atoi(argv[2]));

    // open the text file
    tFile = fopen(argv[argc - 1], "r");
    if (tFile == NULL)
        error("FATAL: File teks tidak ditemukan.\n");

    if (pthread_create(&thread[0], NULL, childProcess, 0) != 0)
        error("FATAL: Gagal membuat thread untuk child process.\n");

    // this is the parent process
    // use as char transmitter from the text file
    // connect to receiver, and read the file per character
    int counter = 1;
    while ((buf[0] = fgetc(tFile)) != EOF) {
        if (isXON) {
            if (sendto(sockfd, buf, BUFMAX, 0, (const struct sockaddr *) &receiverAddr, receiverAddrLen) != BUFMAX)
                error("FATAL: sendto() buffer dikirim lebih dari batas size\n");

            printf("Mengirim byte ke-%d: ", counter++);
            switch (buf[0]) {
                case CR: printf("\'CR\'\n");
                    break;
                case LF: printf("\'LF\'\n");
                    break;
                case Endfile:
                    printf("\'EOF\'\n");
                    break;;
                default: printf("\'%c\'\n", buf[0]);
                    break;
            }
        } else {
            while (!isXON) {
                printf("Menunggu XON...\n");
                sleep(1);
            }
            if (sendto(sockfd, buf, BUFMAX, 0, (const struct sockaddr *) &receiverAddr, receiverAddrLen) != BUFMAX)
                error("FATAL: endto() buffer dikirim lebih dari batas size\n");

            printf("Mengirim byte ke-%d: ", counter++);
            switch (buf[0]) {
                case CR: printf("\'Carriage Return\'\n");
                    break;
                case LF: printf("\'Line Feed\'\n");
                    break;
                case Endfile:
                    printf("\'End of File\'\n");
                    break;
                    //case 25:  break;
                default: printf("\'%c\'\n", buf[0]);
                    break;
            }
        }
        sleep(1);
    }

    // sending endfile to receiver, marking the end of data transfer
    buf[0] = Endfile;
    sendto(sockfd, buf, BUFMAX, 0, (const struct sockaddr *) &receiverAddr, receiverAddrLen);
    fclose(tFile);

    close(sockfd);
    isSocketOpen = 0;

    return 0;
}

void error(const char *message) {
    perror(message);
    exit(1);
}

void *childProcess(void *threadid) {
    // child process
    // read if there is XON/XOFF sent by receiver using recvfrom()
    struct sockaddr_in sourceAddress;
    int srcLen = sizeof (sourceAddress);
    while (isSocketOpen) {
        if (recvfrom(sockfd, xbuf, BUFMAX, 0, (struct sockaddr *) &sourceAddress, (socklen_t *) & srcLen) != BUFMAX)
            error("FATAL: recvfrom() receive buffer with size more than expected.\n");
        if (xbuf[0] == XOFF) {
            isXON = false;
            printf("XOFF diterima.\n");
        } else if (xbuf[0] == XON) {
            isXON = true;
            printf("XON diterima.\n");
        }
    }

    pthread_exit(NULL);
}
