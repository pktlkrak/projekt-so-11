#pragma once
#include <stdbool.h>

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

#ifdef __cplusplus
extern "C" {
#endif
#ifdef IOMAN_CLIENT
void iomanConnect(struct IOManInitPacket *packet, const char *name);
void msg(const char *format, ...);
void iomanTakeoverStdio(bool forwardToOriginal);
#endif
#ifdef __cplusplus
}
#endif

#define __packed __attribute__((packed))
