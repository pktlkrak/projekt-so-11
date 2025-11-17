#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "global_config.h"
#include "ioman.h"
#include "messages.h"
#include <sys/msg.h>
#include <pthread.h>
#include <vector>
#include <sys/mman.h>
#include <sys/shm.h>
#include <fcntl.h>

bool awaitTermination = false;
int msgqueue;

struct Passenger {
    pid_t pid;
    bool hasBike;
};

pthread_mutex_t bridgeMutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t somethingHappenedToTheBridge = PTHREAD_COND_INITIALIZER;
pthread_mutex_t somethingHappenedToTheBridgeMutex = PTHREAD_MUTEX_INITIALIZER;
std::vector<Passenger> passengersWaitingForBridge;
Passenger bridge[BRIDGE_CAPACITY];
int bridgeCursor = 0;
struct BoatContents *dockedBoat = NULL;
key_t boatSHMKey = -1;
int boatSHMID = -1;
size_t boatSHMSize = -1;
pid_t boatPid;
bool boatLocked = false;

#define BRIDGE_LOCK pthread_mutex_lock(&bridgeMutex)
#define BRIDGE_UNLOCK pthread_mutex_unlock(&bridgeMutex)
#define BRIDGE_CHANGED pthread_cond_broadcast(&somethingHappenedToTheBridge)

bool putPassengerOnBridge(const Passenger &passenger) {
    bool bikeProblems = false;
    if(passenger.hasBike) {
        bool bikeWouldNotFitIntoShip = false;
        if(dockedBoat) {
            bikeWouldNotFitIntoShip = dockedBoat->freeBikeSpots == 0;
        }
        bool bikeWouldNotFitOntoBridge = bridgeCursor >= (BRIDGE_CAPACITY - 1);
        bikeProblems = bikeWouldNotFitIntoShip || bikeWouldNotFitOntoBridge;
    }
    if(bikeProblems || bridgeCursor == BRIDGE_CAPACITY || boatLocked) return false;
    bridge[bridgeCursor] = passenger;
    msg("Passenger %d placed onto the bridge. (Br. slot %d)", bridge[bridgeCursor].pid, bridgeCursor);
    bridgeCursor++;
    if(passenger.hasBike) {
        bridge[bridgeCursor] = passenger;
        bridge[bridgeCursor].hasBike = false;
        msg("Their bike takes slot %d", bridgeCursor);
        bridgeCursor++;
    }
    kill(passenger.pid, SIG_PLACE_ON_BRIDGE);
    return true;
}

void initializePassenger(Passenger passenger) {
    // Is the bridge full?
    BRIDGE_LOCK;
    if(!putPassengerOnBridge(passenger)) {
        // Doesn't fit. Put it on the queue waiting for the bridge to free up.
        msg("The passenger does not fit on the bridge.");
        passengersWaitingForBridge.emplace_back(passenger);
    }
    // Ask the bridge thread to manage that new passenger of ours.
    BRIDGE_CHANGED;
    BRIDGE_UNLOCK;
}

// Treats the bridge mutex as locked.
bool moveWaitingPassengerOntoBridge() {
    msg("Putting one passenger onto the bridge...");
    for(auto i = passengersWaitingForBridge.begin(); i<passengersWaitingForBridge.end(); i++) {
        msg("Trying to put passenger %d on the bridge...", i->pid);
        if(putPassengerOnBridge(*i)) {
            msg("Succeeded.");
            // Safe to do. We won't interate over this loop again.
            passengersWaitingForBridge.erase(i);
            return true;
        } else {
            msg("Failed.");
        }
    }
    return false;
}

// Treats the bridge mutex as locked.
Passenger shiftFirstWaitingFromBridge() {
    Passenger first = bridge[0];
    for(int i = 1; i<bridgeCursor; i++) {
        bridge[i - 1] = bridge[i];
    }
    bridgeCursor--;
    // Do we have any passengers waiting for the bridge to free up? If so, let the first one step onto it.
    if(!passengersWaitingForBridge.empty()) {
        moveWaitingPassengerOntoBridge();
    }
    return first;
}

pthread_mutex_t enqueuedPlaceOnBoatMutex = PTHREAD_MUTEX_INITIALIZER;
int enqueuedPlaceOnBoat = 0;
#define ENQ_LOCK pthread_mutex_lock(&enqueuedPlaceOnBoatMutex)
#define ENQ_UNLOCK pthread_mutex_unlock(&enqueuedPlaceOnBoatMutex)

void* bridgeCheckingThread(void *) {
    msg("Bridge thread created!");
    for(;;) {
        pthread_cond_wait(&somethingHappenedToTheBridge, &somethingHappenedToTheBridgeMutex);
        if(awaitTermination) return NULL;

        BRIDGE_LOCK;
        // Do we have a boat docked with free spaces and there's someone on the bridge?
        if(dockedBoat != NULL) {
            ENQ_LOCK;
            while(!boatLocked && dockedBoat->nextFreeSpot < dockedBoat->spotCount && bridgeCursor > 0) {
                // Move the first element on the bridge to the boat.
                Passenger toPlaceOnBoat = shiftFirstWaitingFromBridge();
                if(toPlaceOnBoat.hasBike) {
                    // This space is taken by the bike itself. It is 100% guaranteed that the next spot will
                    // be the cyclist.
                    shiftFirstWaitingFromBridge();
                    if(dockedBoat->freeBikeSpots == 0) {
                        // The cyclist wouldn't fit onto the boat with their bike
                        // Shift onto the list of passengers waiting in front of the bridge
                        msg("The passenger %d wanted to get on the boat, but there's no more space for bikes. Telling them to step off the bridge", toPlaceOnBoat.pid);
                        MsgQueueMessage msgPlaceOnBoat = { ID_PIDMASK | toPlaceOnBoat.pid, { .putOnBoat = { 0, 0, -1 }}};
                        MSGQUEUE_SEND(&msgPlaceOnBoat);
                        continue;
                    } else {
                        // Will fit on the boat. Decrement the counter
                        --dockedBoat->freeBikeSpots;
                    }
                }
                msg("Signaling %d to take spot %d on the boat.", toPlaceOnBoat.pid, dockedBoat->nextFreeSpot);
                MsgQueueMessage msgPlaceOnBoat = { ID_PIDMASK | toPlaceOnBoat.pid, { .putOnBoat = { boatSHMSize, boatSHMKey, dockedBoat->nextFreeSpot++ }}};
                // Tell the passenger to take the designated space on the boat:
                MSGQUEUE_SEND(&msgPlaceOnBoat);
                ++enqueuedPlaceOnBoat;
            }
            ENQ_UNLOCK;
        }
        BRIDGE_UNLOCK;
    }
}

void evictEveryoneFromBridge() {
    if(bridgeCursor > 0) {
        // Evict everyone from the bridge
        msg("Evicting everyone from the bridge.");
        for(int i = bridgeCursor - 1; i >= 0; i--) {
            if(bridge[i].hasBike) continue;
            msg("Evicting %d from the bridge", bridge[i].pid);
            MsgQueueMessage msgPlaceOnBoat = { ID_PIDMASK | bridge[i].pid, { .putOnBoat = { 0, 0, -1 }}};
            MSGQUEUE_SEND(&msgPlaceOnBoat);
        }
        bridgeCursor = 0; // Bridge empty.
    }
}
// DEADLOCKS
void boatLeaves() {
    BRIDGE_LOCK;
    assert(dockedBoat);
    shmdt(dockedBoat);
    dockedBoat = NULL;
    boatSHMID = -1;
    boatSHMSize = -1;
    boatSHMKey = -1;

    evictEveryoneFromBridge();
    BRIDGE_UNLOCK;
}

void acceptPeopleOntoBridge() {
    BRIDGE_LOCK;
    while(!passengersWaitingForBridge.empty() && bridgeCursor < BRIDGE_CAPACITY) {
        if(!moveWaitingPassengerOntoBridge()) break;
    }
    // Start packing people onto the boat:
    BRIDGE_CHANGED;
    BRIDGE_UNLOCK;
}

void prepareBoat(struct _MsgQueueUnion::BoatReference *boat) {
    boatSHMSize = boat->boatSHMSize;
    boatSHMKey = boat->boatSHMKey;
    boatPid = boat->boatPid;
    boatSHMID = shmget(boatSHMKey, boatSHMSize, 0);
    assert(boatSHMID >= 0);
    dockedBoat = (struct BoatContents *) shmat(boatSHMID, NULL, 0);
    // Do we have any passengers on the boat?
    if(dockedBoat->nextFreeSpot != 0) {
        // Make space for them on the bridge.
        boatLocked = true;
        evictEveryoneFromBridge();
    } else {
        msg("The boat has arrived empty - people can enter the bridge again.");
        acceptPeopleOntoBridge();
        boatLocked = false;
        BRIDGE_CHANGED;
    }
}

pthread_t bridgeThread;

int main(int argc, char **argv) {
    if(argc < 2) {
        printf("Usage: %s <initial dispatcher message queue>\n", *argv);
        return -1;
    }

    // Initialize logging:
    key_t myIdentifier = controlFileToMsgQueueKey(argv[1]);
    struct IOManInitPacket init = { 4, BLUE };
    char ownName[64];
    memset(ownName, 0, sizeof(ownName));
    snprintf(ownName, sizeof(ownName), "Dispatcher %08x", myIdentifier);
    iomanConnect(&init, ownName);
    // iomanTakeoverStdio(false);

    // Init the bridge thread
    pthread_create(&bridgeThread, NULL, bridgeCheckingThread, NULL);

    // Initialize msgqueue:
    msgqueue = msgget(myIdentifier, IPC_CREAT | DEFAULT_PERMS);
    assert(msgqueue >= 0);
    msg("Dispatcher is ready.");

    // Delete the message queue on exit:
    atexit([](void){ msgctl(msgqueue, IPC_RMID, NULL); });

    mainHandlingLoop:
    for(;;) {
        struct MsgQueueMessage inbound;
        MSGQUEUE_RECV_GLOBAL(&inbound);
        switch(inbound.id) {
            case ID_INITIALIZE_PUT_ON_BRIDGE:
                msg("Passenger %d asked to be put on the bridge.", inbound.contents.putOnBridge.pid);
                initializePassenger(Passenger { inbound.contents.putOnBridge.pid, inbound.contents.putOnBridge.hasBike });
                break;
            case ID_BOAT_ARRIVED:
                prepareBoat(&inbound.contents.incomingBoat);
                break;
            case ID_BOAT_DEPARTS:
                boatLeaves();
                break;
            case ID_GET_OFF_BOAT:
                assert(dockedBoat != NULL);
                assert(dockedBoat->nextFreeSpot > 0);
                assert(boatLocked);
                // Use this field as a counter only. All passengers must leave.
                // TODO: Put this passenger on the bridge temporarily?:
                msg("Passenger %d asked to be let off the boat.", inbound.contents.getOffBoat.pid);
                kill(inbound.contents.getOffBoat.pid, SIG_GET_OFF_BOAT);
                if(--dockedBoat->nextFreeSpot == 0) {
                    // Reset the boat state. Start accepting passengers.
                    memset(dockedBoat->spaces, 0, sizeof(pid_t) * dockedBoat->spotCount);
                    dockedBoat->freeBikeSpots = dockedBoat->bikeSpotCount;
                    dockedBoat->destinationMessageQueue = -1;
                    boatLocked = false;
                    // Make this show in the logs that the people get accepted only after the others have left the boat.
                    sleep(1);
                    msg("The boat has no passengers left, people can enter the bridge again.");
                    acceptPeopleOntoBridge();
                }
                break;
            case ID_IS_ON_BOAT:
                assert(dockedBoat);
                ENQ_LOCK;
                assert(enqueuedPlaceOnBoat > 0);
                if(--enqueuedPlaceOnBoat == 0 && dockedBoat->nextFreeSpot == dockedBoat->spotCount) {
                    // We filled the boat up. Signal it to leave early
                    msg("The boat is full. Making it leave early...");
                    kill(boatPid, SIG_BOAT_EARLY_LEAVE);
                }
                ENQ_UNLOCK;
                break;

        }
    }
}
