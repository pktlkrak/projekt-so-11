#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "ioman.h"
#define TAKEOVER_LINE_BUFFER_LENGTH 65536

static char FORMAT_BUFFER[65536];
static int _socket = 0;

static int _stderrPipe = 0, _stderrPipeWrite = 0;
static int _stdoutPipe = 0, _stdoutPipeWrite = 0;
static int _takeoverReadEndPipe = 0, _takeoverWriteEndPipe = 0;
static pthread_t _takeoverThread;
static char *_inputStderrBuffer = NULL, *_inputStdoutBuffer = NULL;
static int _inputStderrBufferCursor = 0, _inputStdoutBufferCursor = 0;
static bool _forwardToOriginal = false;

static int _originalStdout = 0, _originalStderr = 0;

static void _iomanClose() {
    if(_inputStderrBuffer) {
        fflush(stdout);
        fflush(stderr);
        write(_takeoverWriteEndPipe, "1", 1);
        pthread_join(_takeoverThread, NULL);
        free(_inputStderrBuffer);
        free(_inputStdoutBuffer);
        _inputStderrBuffer = NULL;
        _inputStdoutBuffer = NULL;

        close(_stderrPipe);
        close(_stderrPipeWrite);
        close(_stdoutPipe);
        close(_stdoutPipeWrite);
        close(_takeoverReadEndPipe);
        close(_takeoverWriteEndPipe);
    }
    if(_socket != 0)
        close(_socket);
    _socket = 0;
}

static inline int max(int a, int b) { return a > b ? a : b; }
static void *_iomanTakeoverThread(void *) {
    fd_set readFDs;
    for(;;) {
        FD_ZERO(&readFDs);
        int fdmax = max(max(_stdoutPipe, _stderrPipe), _takeoverReadEndPipe) + 1;
        FD_SET(_stdoutPipe, &readFDs);
        FD_SET(_stderrPipe, &readFDs);
        FD_SET(_takeoverReadEndPipe, &readFDs);
        select(fdmax, &readFDs, NULL, NULL, NULL);
        if(FD_ISSET(_stderrPipe, &readFDs)) {
            char c;
            read(_stderrPipe, &c, 1);
            if(_forwardToOriginal)
                write(_originalStderr, &c, 1);
            if(c == '\n' || _inputStderrBufferCursor == (TAKEOVER_LINE_BUFFER_LENGTH -1)) {
                // Flush
                _inputStderrBuffer[_inputStderrBufferCursor] = 0;
                msg("STDERR: %s", _inputStderrBuffer);
                _inputStderrBufferCursor = 0;
            } else {
                _inputStderrBuffer[_inputStderrBufferCursor++] = c;
            }
            continue;
        }
        if(FD_ISSET(_stdoutPipe, &readFDs)) {
            char c;
            read(_stdoutPipe, &c, 1);
            if(_forwardToOriginal)
                write(_originalStderr, &c, 1);
            if(c == '\n' || _inputStdoutBufferCursor == (TAKEOVER_LINE_BUFFER_LENGTH -1)) {
                // Flush
                _inputStdoutBuffer[_inputStdoutBufferCursor] = 0;
                msg("STDOUT: %s", _inputStdoutBuffer);
                _inputStdoutBufferCursor = 0;
            } else {
                _inputStdoutBuffer[_inputStdoutBufferCursor++] = c;
            }
            continue;
        }
        if(FD_ISSET(_takeoverReadEndPipe, &readFDs)) {
            char c;
            read(_takeoverReadEndPipe, &c, 1);
            return NULL;
        }
    }
    return NULL;
}

void iomanTakeoverStdio(bool enableForwardToOriginal) {
    int _pipe[2];

    _forwardToOriginal = enableForwardToOriginal;
    _originalStdout = dup(1);
    _originalStderr = dup(2);

    pipe(_pipe);
    _stderrPipe = _pipe[0];
    _stderrPipeWrite = _pipe[1];
    dup2(_stderrPipeWrite, 2);

    pipe(_pipe);
    _stdoutPipe = _pipe[0];
    _stdoutPipeWrite = _pipe[1];
    dup2(_stdoutPipeWrite, 1);

    pipe2(_pipe, O_NONBLOCK);
    _takeoverReadEndPipe = _pipe[0];
    _takeoverWriteEndPipe = _pipe[1];

    _inputStderrBuffer = malloc(TAKEOVER_LINE_BUFFER_LENGTH);
    _inputStdoutBuffer = malloc(TAKEOVER_LINE_BUFFER_LENGTH);
    pthread_create(&_takeoverThread, NULL, _iomanTakeoverThread, NULL);
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
