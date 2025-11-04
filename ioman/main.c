#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>

#define IOMAN_SERVER
#include "ioman.h"

/*
IOMan is the first component of the project. It manages console I/O - makes sure the logs are printed
synchronously and manages colors and indentation
*/


// Mutex for the output:
pthread_mutex_t output = PTHREAD_MUTEX_INITIALIZER;

// A helper method which will create the indent, message, etc...
void printMessage(int backgroundColor, int foregroundColor, int pid, int indent, const char *name, const char *fmt, ...) {
    pthread_mutex_lock(&output);
    va_list args;
    va_start(args, fmt);
    // Handle the indent:
    for(int i = 0; i<indent; i++) printf(" ");
    // Handle the background / foreground color switching and name printing.
    printf("\e[0;%dm\e[%dm[%d:%s]: ", backgroundColor + 10, foregroundColor, pid, name);
    // Print the actual log:
    vprintf(fmt, args);
    // Reset colors
    printf("\e[0m\n");
    va_end(args);
    pthread_mutex_unlock(&output);
}

#define LOG_INTERNAL(x, ...) printMessage(WHITE, BLACK, getpid(), 0, "IOMAN", x, __VA_ARGS__)

// I'm only checking if status < 1, since SEQPACKET is used.
#define CHECK_STATUS if(status < 1) goto kill;
#define READ_CHECK_STATUS(x) status = read(socketFD, (char *) &x, sizeof(x)); CHECK_STATUS
void *singleSocketThread(void *_socketFD) {
    int socketFD = (int) _socketFD;
    // Packet format is different for init vs other messages.
    // For init, it is IOManInitPacket followed by a string of n bytes, where n is IOManInitPacket.nameLength.
    struct IOManInitPacket initPacket;
    char *name = NULL;
    int status;
    unsigned short messageLength;
    char inboundTextBuffer[65536];

    // Handle the init:
    READ_CHECK_STATUS(initPacket);
    name = malloc(initPacket.nameLength);
    status = read(socketFD, name, initPacket.nameLength);
    name[initPacket.nameLength] = 0;
    CHECK_STATUS;
    LOG_INTERNAL("A new client %s (%d) has connected!", name, initPacket.pid);

    // Init done. Main loop:
    for(;;) {
        READ_CHECK_STATUS(messageLength);
        inboundTextBuffer[messageLength] = 0;
        status = read(socketFD, inboundTextBuffer, messageLength);
        CHECK_STATUS;
        printMessage(BLACK + 10, initPacket.color, initPacket.pid, initPacket.indentDepth, name, "%s", inboundTextBuffer);
    }
    kill:
    LOG_INTERNAL("Client %s has disconnected!", name);
    close(socketFD);
    if(name) free(name);
    return NULL;
}

#undef CHECK_STATUS
#undef READ_CHECK_STATUS

int main() {
    // Make sure we're the leading instance that will receive connections (and clean up any other instance's socket file)
    unlink(IOMAN_SOCKET_PATH);
    LOG_INTERNAL("Starting ioman...", 0);
    // Using seqpackets to simplify receiving code (this code doesn't need to work on anything except for Linux, so I will lean into Linux-specific APIs)
    int listenSocketFD = socket(AF_UNIX, SOCK_SEQPACKET, 0);

    // Prepare the address:
    struct sockaddr_un unixAddress;
    unixAddress.sun_family = AF_UNIX;
    strncpy(unixAddress.sun_path, IOMAN_SOCKET_PATH, sizeof(unixAddress.sun_path) - 1);

    // Bind & listen:
    if(bind(listenSocketFD, (struct sockaddr *) &unixAddress, sizeof(unixAddress)) == -1) {
        LOG_INTERNAL("Cannot bind to socket %s", IOMAN_SOCKET_PATH);
        return -1;
    }
    if(listen(listenSocketFD, 1) == -1){
        LOG_INTERNAL("Cannot listen on socket %s", IOMAN_SOCKET_PATH);
        return -1;
    }
    pthread_t tempThreadId;
    for(;;) {
        // Wait for new clients to join:
        int clientFD = -1;
        do{
            clientFD = accept(listenSocketFD, NULL, NULL);
        }while(clientFD == -1);

        // For every client that joins, create a thread and manage it separately.
        pthread_create(&tempThreadId, NULL, singleSocketThread, (void *) clientFD);
        pthread_detach(tempThreadId);
    }
}
