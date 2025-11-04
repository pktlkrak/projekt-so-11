#pragma once
#ifndef IOMAN_SERVER
#define IOMAN_CLIENT
#endif

#define IOMAN_SOCKET_PATH "/tmp/ioman.sock"

enum {
    BLACK = 30,
    RED,
    GREEN,
    YELLOW,
    BLUE,
    PURPLE,
    CYAN,
    WHITE
};

struct IOManInitPacket {
    int indentDepth;
    int color;
    int pid;
    unsigned short nameLength;
};

#ifdef IOMAN_CLIENT
void iomanConnect(struct IOManInitPacket *packet, const char *name);
void msg(const char *format, ...);
#endif
