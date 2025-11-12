#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "ioman.h"
#include "messages.h"
#include "signal_helpers.h"
#include <sys/msg.h>

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
                printf("Unknown signal received! Dispatcher in bad state.");
                return -2;
                break;
        }
        struct MsgQueueMessage requestOnBoatPlacement = {
            ID_PUT_ON_BOAT,
        };
        msg("Asking the dispatcher to be put on the boat...");
        MSGQUEUE_SEND(&requestOnBoatPlacement);
        switch(waitForUserSignal()) {
            case SIG_GET_OFF_BRIDGE: 
                msg("The dispatcher told me to get off the bridge.");
                continue waitingForBridge;
            case SIG_PLACE_ON_BOAT:
                msg("The dispatcher has placed me on the boat.");
                break;
        }
        struct MsgQueueMessage reachedOppositeStopMessage;
        msg("Waiting for the message from the boat about the new dispatcher's key...");
        MSGQUEUE_READ_C_DIRECT(&reachedOppositeStopMessage);
        msgqueue = msgget(
            reachedOppositeStopMessage.contents.reachedOppositeStop.newDispatcher,
            0
        );
        assert(msgqueue >= 0);
        msg("Got new dispatcher key: %x", reachedOppositeStopMessage.contents.reachedOppositeStop.newDispatcher);
        struct MsgQueueMessage leaveBoatMessage = {
            ID_GET_OFF_BOAT,
        };
        msg("Asking the dispatcher to let me get off the boat...");
        MSGQUEUE_SEND(&leaveBoatMessage);
        switch(waitForUserSignal()) {
            case SIG_GET_OFF_BOAT: break;
            default:
                printf("Unknown signal received! Dispatcher in bad state.");
                return -3;
        }
        msg("I left the boat.");
        return 0;
    }
}
