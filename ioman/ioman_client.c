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

#include "ioman.h"

static char FORMAT_BUFFER[65536];
static int _socket = 0;

static void _iomanClose() {
    if(_socket != 0)
        close(_socket);
    _socket = 0;
}

void iomanConnect(struct IOManInitPacket *packet, const char *name) {
    _socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);

    // Prepare the address:
    struct sockaddr_un unixAddress;
    unixAddress.sun_family = AF_UNIX;
    strncpy(unixAddress.sun_path, IOMAN_SOCKET_PATH, sizeof(unixAddress.sun_path) - 1);
    if(connect(_socket, (struct sockaddr *) &unixAddress, sizeof(unixAddress)) != 0) {
        printf("Failed to connect to ioman server!\n");
        abort();
    }
    // Perform init:
    packet->nameLength = strlen(name);
    packet->pid = getpid();
    write(_socket, (char *) packet, sizeof(*packet));
    write(_socket, name, packet->nameLength);

    // Register exit hook..
    atexit(_iomanClose);
}

void msg(const char *format, ...) {
    if(_socket == 0) {
        printf("You are not connected to the ioman host yet! Call connect() first!\n");
        abort();
    }
    va_list args;
    va_start(args, format);
    int n = vsnprintf(FORMAT_BUFFER, sizeof(FORMAT_BUFFER), format, args);
    if(n < sizeof(FORMAT_BUFFER) && n > 0) {
        unsigned int nConv = (unsigned int) n;
        write(_socket, (char *) &nConv, sizeof(nConv));
        write(_socket, FORMAT_BUFFER, nConv);
    }
    va_end(args);
}
