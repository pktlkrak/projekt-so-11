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
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include "global_config.h"

#define IOMAN_SERVER
#include "ioman.h"

#define QUEUE_UPDATE_FLUSH_TIME 100000000ULL
#define MSG_QUEUE_LENGTH 20

/*
IOMan is the first component of the project. It manages console I/O - makes sure the logs are printed
synchronously and manages colors and indentation
*/

// Log file FD:
int logFD = -1;

// Mutex for the output:
pthread_mutex_t output = PTHREAD_MUTEX_INITIALIZER;

// A helper method which will create the indent, message, etc...
void printMessage(unsigned long long int ts, int backgroundColor, int foregroundColor, int pid, int indent, const char *name, const char *fmt, ...) {
    pthread_mutex_lock(&output);
    va_list args;
    va_start(args, fmt);
    // Handle the indent:
    for(int i = 0; i<indent; i++) printf(" ");
    // Handle the background / foreground color switching and name printing.
    printf("\e[0;%dm\e[%dm<%llu>[%d:%s]: ", backgroundColor + 10, foregroundColor, ts, pid, name);
    // Print the actual log:
    vprintf(fmt, args);
    // Reset colors
    printf("\e[0m\n");
    va_end(args);
    // If we have a logfile set, write it:
    if(logFD != -1) {
        va_start(args, fmt);
        for(int i = 0; i<indent; i++) dprintf(logFD, " ");
        dprintf(logFD, "<%llu>[%d:%s]: ", ts, pid, name);
        vdprintf(logFD, fmt, args);
        va_end(args);
        dprintf(logFD, "\n");
    }
    pthread_mutex_unlock(&output);
}

static unsigned long long int getTimestamp() {
    unsigned long long int ms = 0;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    ms += spec.tv_sec * 1000000000ULL;
    ms += spec.tv_nsec;
    return ms;
}

struct MessageQueueEntry {
    unsigned long long int timestamp;
    int background, foreground, pid, indent;
    char *name, *contents;
} messageQueue[MSG_QUEUE_LENGTH];
pthread_mutex_t queueMutex = PTHREAD_MUTEX_INITIALIZER;
unsigned long long int lastUpdatedQueue = 0;

// Assumes mutex locked:
int getOldestInQueue(bool allowZero) {
    int index = -1;
    unsigned long long int minTimestamp = 0xFFFFFFFFFFFFFFFFULL;
    for(int i = 0; i<MSG_QUEUE_LENGTH; i++) {
        if(!allowZero && messageQueue[i].timestamp == 0) continue;
        if(minTimestamp > messageQueue[i].timestamp) {
            minTimestamp = messageQueue[i].timestamp;
            index = i;
        }
    }

    return index;
}

void printQueuedMessage(struct MessageQueueEntry *entry) {
    if(entry->contents) {
        printMessage(
            entry->timestamp,
            entry->background,
            entry->foreground,
            entry->pid,
            entry->indent,
            entry->name,
            "%s",
            entry->contents
        );
        free(entry->contents);
    }
    if(entry->name) free(entry->name);
    entry->name = NULL;
    entry->contents = NULL;
    entry->timestamp = 0;
}

void queueMessage(unsigned long long int timestamp, int backgroundColor, int foregroundColor, int pid, int indent, const char *name, const char *contents) {
    pthread_mutex_lock(&queueMutex);
    // Find the oldest entry in the queue
    int oldestEntry = getOldestInQueue(true);
    // Print it out:
    struct MessageQueueEntry *entry = &messageQueue[oldestEntry];
    printQueuedMessage(entry);
    // And emplace the new message onto the queue
    if(entry->contents != NULL) free(entry->contents);
    if(entry->name != NULL) free(entry->name);
    entry->timestamp = timestamp;
    entry->background = backgroundColor;
    entry->foreground = foregroundColor;
    entry->pid = pid;
    entry->indent = indent;
    entry->name = strdup(name);
    entry->contents = strdup(contents);
    lastUpdatedQueue = getTimestamp();
    pthread_mutex_unlock(&queueMutex);
}

thread_local char LOG_INTERNAL_BUFFER[32768];
#define LOG_INTERNAL(x, ...) do { snprintf(LOG_INTERNAL_BUFFER, sizeof(LOG_INTERNAL_BUFFER), x, __VA_ARGS__); queueMessage(getTimestamp(), WHITE, BLACK, getpid(), 0, "IOMAN", LOG_INTERNAL_BUFFER); } while(0)


// If we don't get any new messages in a set duration of time, free all the messages
void *queueEmptyingThread(void *) {
    for(;;) {
        pthread_mutex_lock(&queueMutex);
        if(getTimestamp() - lastUpdatedQueue > QUEUE_UPDATE_FLUSH_TIME) {
            int next;
            while((next = getOldestInQueue(false)) != -1) {
                printQueuedMessage(&messageQueue[next]);
            }
            lastUpdatedQueue = getTimestamp();
        }
        pthread_mutex_unlock(&queueMutex);
        sleep(1);
    }
}

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
    struct {
        unsigned long long int timestamp;
        char inboundTextBuffer[65536];
    } __packed inboundMessage;

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
        status = read(socketFD, &inboundMessage, messageLength + sizeof(inboundMessage.timestamp));
        inboundMessage.inboundTextBuffer[messageLength] = 0;
        CHECK_STATUS;
        queueMessage(inboundMessage.timestamp, BLACK + 10, initPacket.color, initPacket.pid, initPacket.indentDepth, name, inboundMessage.inboundTextBuffer);
    }
    kill:
    LOG_INTERNAL("Client %s has disconnected!", name);
    close(socketFD);
    if(name) free(name);
}

#undef CHECK_STATUS
#undef READ_CHECK_STATUS

void closeLog() {
    if(logFD != -1) {
        close(logFD);
        logFD = -1;
    }
}

int main(int argc, char **argv) {
    bool fileAppend = false;
    const char *logFile = NULL;
    char opt;
    while((opt = getopt(argc, argv, "al:")) != -1) {
        switch(opt) {
            case 'a':
                fileAppend = true;
                break;
            case 'l':
                logFile = optarg;
                break;
            default:
                printf("Usage: %s [-l logFile [-a]]\n", *argv);
                exit(1);
        }
    }
    if(logFile != NULL) {
        if(fileAppend) {
            logFD = open(logFile, O_APPEND | O_WRONLY | O_CREAT, DEFAULT_PERMS);
        } else {
            unlink(logFile);
            logFD = creat(logFile, DEFAULT_PERMS);
        }
        if(logFD < 1) {
            printf("Failed to open the log file! (%d)\n", errno);
            return 2;
        }
        atexit(closeLog);
    }
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

    // Start the queue reodering & emptying thread
    memset(&messageQueue, 0, sizeof(messageQueue));
    pthread_t emptyThread;
    pthread_create(&emptyThread, NULL, queueEmptyingThread, NULL);
    pthread_detach(emptyThread);

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
