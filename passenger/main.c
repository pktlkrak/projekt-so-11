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
    if(argc < 2) {
        printf("Usage: %s <initial dispatcher message queue>\n", *argv);
        return -1;
    }

    // Initialize logging:
    struct IOManInitPacket init = { 2, RED };
    iomanConnect(&init, "Passenger");
    // iomanTakeoverStdio(false);

    // Initialize signals:
    registerSignals();

    // Initialize msgqueue:
    key_t currentlyBoundDispatcher = atoi(argv[1]);
    int msgqueue = msgget(currentlyBoundDispatcher, 0);
    assert(msgqueue >= 0);
    msg("Passenger is ready.");

    // Actual client loop:
    waitingForBridge: for(;;) {
        struct MsgQueueMessage initMessage = {
            ID_INITIALIZE_PUT_ON_BRIDGE, { .putOnBridge = { getpid() }}
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
        switch(waitForUserSignal()) {
            case SIG_GET_OFF_BRIDGE: 
                msg("The dispatcher told me to get off the bridge.");
                continue waitingForBridge;
            case SIG_PLACE_ON_BOAT:
                msg("The dispatcher has informed me that I should be placed on the boat.");
                break;
        }
        struct MsgQueueMessage placeOnBoatMessage;
        msg("Waiting for the message from the boat about the new dispatcher's key...");
        MSGQUEUE_RECV_C_DIRECT(&placeOnBoatMessage);
        int boatShmId = placeOnBoatMessage.contents.putOnBoat.boatSHMId;
        size_t boatSHMSize = placeOnBoatMessage.contents.putOnBoat.boatSHMSize;
        int mySpotIndex = placeOnBoatMessage.contents.putOnBoat.spotIndex;
        int boatSHMFD = shmget(boatShmId, boatSHMSize, 0);
        struct BoatContents *boatSHM = (struct BoatContents *) mmap(NULL, boatSHMSize, PROT_READ | PROT_WRITE, MAP_SHARED, boatSHMFD, 0);
        // TODO: Place self on boat - semaphore.

        msg("Disconnecting self from the initial dispatcher. Waiting on signal from the boat.");
        msgqueue = -1;
        switch(waitForUserSignal()) {
            case SIG_BOAT_REACHED_DESTINATION:
                msgqueue = boatSHM->destinationMessageQueue;
                assert(msgqueue >= 0);
                msg("The boat has reached its destination. New dispatcher ID is %08x", msgqueue);
                break;
            default:
                printf("Unknown signal received! Dispatcher in bad state.\n");
                return -4;
        }
        msgqueue = msgget(
            msgqueue,
            0
        );
        struct MsgQueueMessage leaveBoatMessage = {
            ID_GET_OFF_BOAT,
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
        munmap(boatSHM, boatSHMSize);
        close(boatSHMFD);
        return 0;
    }
}
