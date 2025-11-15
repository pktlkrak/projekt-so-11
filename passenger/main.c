#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "ioman.h"
#include "messages.h"
#include "signal_helpers.h"
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/mman.h>

int main(int argc, char **argv) {
    // Initialize signals:
    registerSignals();

    if(argc < 2) {
        printf("Usage: %s <initial dispatcher message queue>\n", *argv);
        return -1;
    }

    bool hasBike = false;
    if(argc > 2) {
        hasBike = *argv[2] == '1';
    }

    // Initialize logging:
    struct IOManInitPacket init = { 2, RED };
    iomanConnect(&init, "Passenger");
    // iomanTakeoverStdio(false);

    // Initialize msgqueue:
    key_t currentlyBoundDispatcher = atoi(argv[1]);
    int msgqueue = msgget(currentlyBoundDispatcher, 0);
    assert(msgqueue >= 0);
    msg("Passenger%s is ready.", hasBike ? " with bike" : "");

    // Actual client loop:
    waitingForBridge: for(;;) {
        struct MsgQueueMessage initMessage = {
            ID_INITIALIZE_PUT_ON_BRIDGE, { .putOnBridge = { getpid(), hasBike }}
        };
        msg("Asking the dispatcher to be put on the bridge...");
        MSGQUEUE_SEND(&initMessage);
        switch(waitForUserSignal()) {
            case SIG_PLACE_ON_BRIDGE:
                msg("Dispatcher agreed. I am on the bridge now!");
                break;
            default:
                printf("Unknown signal received! Dispatcher in bad state.\n");
                return -2;
                break;
        }
        struct MsgQueueMessage placeOnBoatMessage;
        MSGQUEUE_RECV_C_DIRECT(&placeOnBoatMessage);
        int boatSHMKey = placeOnBoatMessage.contents.putOnBoat.boatSHMKey;
        size_t boatSHMSize = placeOnBoatMessage.contents.putOnBoat.boatSHMSize;
        int mySpotIndex = placeOnBoatMessage.contents.putOnBoat.spotIndex;
        if(mySpotIndex == -1) {
            msg("The dispatcher told me to get off the bridge.");
            continue waitingForBridge;
        }
        msg("The dispatcher has informed me that I should be placed on the boat.");
        int boatSHMId = shmget(boatSHMKey, boatSHMSize, 0);
        struct BoatContents *boatSHM = shmat(boatSHMId, NULL, 0);
        // TODO: Place self on boat - semaphore.
        boatSHM->spaces[mySpotIndex] = getpid();
        struct MsgQueueMessage placedOnBoatMessage = { ID_IS_ON_BOAT };
        MSGQUEUE_SEND(&placedOnBoatMessage);

        msg("Placed self at spot %d of boat shm %08x. Waiting on signal from the boat.", mySpotIndex, boatSHMKey);
        msgqueue = -1;
        switch(waitForUserSignal()) {
            case SIG_BOAT_REACHED_DESTINATION:
                msg("The boat has reached its destination. New dispatcher ID is %d", boatSHM->destinationMessageQueue);
                msgqueue = msgget(boatSHM->destinationMessageQueue, 0);
                assert(msgqueue >= 0);
                break;
            default:
                printf("Unknown signal received! Dispatcher in bad state.\n");
                return -4;
        }
        struct MsgQueueMessage leaveBoatMessage = {
            ID_GET_OFF_BOAT, { .getOffBoat = { .pid = getpid() }},
        };
        msg("Asking the dispatcher to let me get off the boat...");
        MSGQUEUE_SEND(&leaveBoatMessage);
        switch(waitForUserSignal()) {
            case SIG_GET_OFF_BOAT: break;
            default:
                printf("Unknown signal received! Dispatcher in bad state.\n");
                return -3;
        }
        msg("I left the boat.");
        shmdt(boatSHM);
        return 0;
    }
}
