#pragma once
#include <assert.h>

// Message queues:
#define MSG_QUEUE_MSGSZ sizeof(union _MsgQueueUnion)

#define ID_INITIALIZE_PUT_ON_BRIDGE 1
#define ID_PUT_ON_BOAT 2
#define ID_GET_OFF_BOAT 3
#define ID_PIDMASK 0x40000000

union _MsgQueueUnion {
    struct {
        pid_t pid;
    } putOnBridge;
    struct {} putOnBoat;
    struct {
        key_t newDispatcher;
    } reachedOppositeStop;
};

struct MsgQueueMessage {
    long id;
    union _MsgQueueUnion contents;
};

#define MSGQUEUE_SEND(msg) assert(0 == msgsnd(msgqueue, msg, MSG_QUEUE_MSGSZ, 0))
#define MSGQUEUE_READ_DISP(msg) assert(0 < msgrcv(msgqueue, msg, MSG_QUEUE_MSGSZ, -ID_PIDMASK, 0))
#define MSGQUEUE_READ_C_DIRECT(msg) assert(0 < msgrcv(msgqueue, msg, MSG_QUEUE_MSGSZ, getpid() | ID_PIDMASK, 0))

// Signals:

#define SIG_PLACE_ON_BRIDGE SIGUSR1
#define SIG_PLACE_ON_BOAT SIGUSR1
#define SIG_GET_OFF_BRIDGE SIGUSR2
#define SIG_GET_OFF_BOAT SIGUSR2