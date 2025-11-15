#pragma once
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
// Message queues:
#define MSG_QUEUE_MSGSZ sizeof(union _MsgQueueUnion)

#define ID_INITIALIZE_PUT_ON_BRIDGE 1
#define ID_BOAT_ARRIVED 2
#define ID_GET_OFF_BOAT 3
#define ID_BOAT_DEPARTS 4
#define ID_IS_ON_BOAT 5
#define ID_PIDMASK 0x40000000

union _MsgQueueUnion {
    struct {
        pid_t pid;
        bool hasBike;
    } putOnBridge;
    struct {
        size_t boatSHMSize;
        key_t boatSHMKey;
        int spotIndex;
    } putOnBoat;
    struct {
        pid_t pid;
    } getOffBoat;
    struct BoatReference {
        size_t boatSHMSize;
        key_t boatSHMKey;
        pid_t boatPid;
    } incomingBoat;
};

struct MsgQueueMessage {
    long id;
    union _MsgQueueUnion contents;
};

#define MSGQUEUE_SEND(x) if(-1 == msgsnd(msgqueue, x, MSG_QUEUE_MSGSZ, 0)) { msg("Failed to MSGQUEUE_SEND! Errno: %d", errno); abort(); }
#define MSGQUEUE_RECV_GLOBAL(x) if(-1 == msgrcv(msgqueue, x, MSG_QUEUE_MSGSZ, -ID_PIDMASK, 0)) { msg("Failed to MSGQUEUE_RECV_GLOBAL! Errno: %d", errno); abort(); }
#define MSGQUEUE_RECV_C_DIRECT(x) if(-1 == msgrcv(msgqueue, x, MSG_QUEUE_MSGSZ, getpid() | ID_PIDMASK, 0)) { msg("Failed to MSGQUEUE_RECV_C_DIRECT! Errno: %d", errno); abort(); }

// Signals:

#define SIG_PLACE_ON_BRIDGE SIGUSR1
#define SIG_PLACE_ON_BOAT SIGUSR1
#define SIG_GET_OFF_BRIDGE SIGUSR2
#define SIG_GET_OFF_BOAT SIGUSR2
#define SIG_BOAT_REACHED_DESTINATION SIGUSR1
#define SIG_BOAT_EARLY_LEAVE SIGUSR1
#define SIG_BOAT_TERMINATE SIGUSR2


// SHM:

struct BoatContents {
    int nextFreeSpot;
    int freeBikeSpots;
    int spotCount;
    int bikeSpotCount;
    key_t destinationMessageQueue;
    pid_t spaces[0]; // This is an array of `spotCount' elements. This is the simplest way to declare it
    // and C doesn't mind.
};
