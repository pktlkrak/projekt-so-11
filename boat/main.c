#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "ioman.h"
#include "messages.h"
#include "global_config.h"
#include <sys/msg.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <pthread.h>

#define BOAT_SHM_SIZE (sizeof(struct BoatContents) + BOAT_CAPACITY * sizeof(pid_t))
#define WAIT_FOREVER sleep(0xFFFFFFFFU)

struct BoatContents *self;
int shmFd;
bool shouldEndTrip = false;

void signalHandler(int a) {
    shouldEndTrip = true;
    msg("Got the early termination signal!");
}

int shmId;
void exitHandler() {
    shmdt(self);
    shmctl(shmId, IPC_RMID, NULL);
}



int msgqueueA;
int msgqueueB;
void terminate(int _) {
    int msgqueue = msgqueueA;
    struct MsgQueueMessage endMessage = { ID_END_OF_SIM };
    MSGQUEUE_SEND(&endMessage);
    msgqueue = msgqueueB;
    MSGQUEUE_SEND(&endMessage);

    // Free all the passengers on board:
    for(int i = 0; i<BOAT_CAPACITY; i++) {
        if(self->spaces[i] != 0) kill(self->spaces[i], SIGTERM);
    }
    exit(0);
}

int main(int argc, char **argv) {
    if(argc < 3) {
        printf("Usage: %s <initial dispatcher message queue> <desination dispatcher message queue>\n", *argv);
        return -1;
    }

    // Initialize logging:
    key_t messageQueueKeyA = controlFileToMsgQueueKey(argv[1]);
    key_t messageQueueKeyB = controlFileToMsgQueueKey(argv[2]);
    struct IOManInitPacket init = { 6, GREEN };
    char ownName[64];
    memset(ownName, 0, sizeof(ownName));
    snprintf(ownName, sizeof(ownName), "Boat %08x <==> %08x", messageQueueKeyA, messageQueueKeyB);
    iomanConnect(&init, ownName);
    iomanTakeoverStdio(false);

    // Initialize signals
    sigset_t earlyLeaveSignal, waitForAcceptance;
    sigaddset(&earlyLeaveSignal, SIG_BOAT_EARLY_LEAVE);
    sigprocmask(SIG_BLOCK, &earlyLeaveSignal, NULL);
    struct timespec waitTime = { BOAT_WAIT_TIME, 0 };
    signal(SIG_BOAT_TERMINATE, signalHandler);
    signal(SIGTERM, terminate);

    // Initialize msgqueue:
    msgqueueA = msgget(messageQueueKeyA, 0);
    msgqueueB = msgget(messageQueueKeyB, 0);
    int msgqueue = msgqueueA;
    assert(msgqueue >= 0);
    msg("Boat is ready. (Message queue ids are: %d %d)", msgqueueA, msgqueueB);

    srand(time(NULL));
    key_t shmKey = rand();
    msg("Boat SHM ID is %08x", shmKey);
    shmId = shmget(shmKey, BOAT_SHM_SIZE, IPC_CREAT | DEFAULT_PERMS);
    self = (struct BoatContents *) shmat(shmId, NULL, 0);
    msg("Boat storage address = %p", self);
    memset(self, 0, sizeof(*self) + BOAT_CAPACITY * sizeof(*self->spaces));
    self->spotCount = BOAT_CAPACITY;
    self->freeBikeSpots = self->bikeSpotCount = BOAT_BIKE_CAPACITY;
    self->destinationMessageQueue = messageQueueKeyA;
    atexit(exitHandler);

    int cycles = 0;
    // Main loop:
    for(;;) {
        msg("Arrived to dispatcher %08x with %d passengers", msgqueue == msgqueueA ? messageQueueKeyA : messageQueueKeyB, self->nextFreeSpot);
        for(int i = 0; i<self->nextFreeSpot; i++) msg("- %d", self->spaces[i]);
        // Tell the current dispatcher that a boat had arrived.
        struct MsgQueueMessage arrivalMessage = { ID_BOAT_ARRIVED, { .incomingBoat = { BOAT_SHM_SIZE, shmKey, getpid(), 0 }}};
        MSGQUEUE_SEND(&arrivalMessage);
        // And tell all our passengers too:
        msg("Notifying all passengers that the boat has arrived");
        int passengerCount = self->nextFreeSpot;
        for(int i = 0; i<passengerCount; i++) {
            msg("Sending signal to pid %d", self->spaces[i]);
            kill(self->spaces[i], SIG_BOAT_REACHED_DESTINATION);
        }
        // Wait for the dispatcher to accept us.
        sigaddset(&waitForAcceptance, SIG_BOAT_ACCEPTED_TO_DISP);
        int _sig;
        sigwait(&waitForAcceptance, &_sig);

        // Wait before leaving...
        sigaddset(&earlyLeaveSignal, SIG_BOAT_EARLY_LEAVE);
        sigtimedwait(&earlyLeaveSignal, NULL, &waitTime);
        if(shouldEndTrip || cycles >= BOAT_MAX_CYCLES) {
            msg("This was the boat's last trip. Its simulation is stopped.");
            if(self->nextFreeSpot == 0) {
                raise(SIGTERM);
            }

            struct MsgQueueMessage terminationMessage = { ID_BOAT_ARRIVED, { .incomingBoat = { BOAT_SHM_SIZE, shmKey, getpid(), 1 }}};
            MSGQUEUE_SEND(&terminationMessage);
            for(int i = 0; i<self->nextFreeSpot; i++) {
                msg("Sending 'leave' signal to pid %d", self->spaces[i]);
                kill(self->spaces[i], SIG_BOAT_REACHED_DESTINATION);
                self->spaces[i] = 0;
            }
            // Wait for SIGTERM.
            WAIT_FOREVER;
        }
        msg("Boat leaving for its %d. journey (out of %d)...", cycles + 1, BOAT_MAX_CYCLES);
        // Leave
        struct MsgQueueMessage leaveMessage = { ID_BOAT_DEPARTS };
        MSGQUEUE_SEND(&leaveMessage);
        // Wait for the confirmation from the boat.
        struct MsgQueueMessage confMessage;
        MSGQUEUE_RECV_C_DIRECT(&confMessage);
        // Wait for the duration of the transit...
        sleep(BOAT_TRIP_TIME);
        if(msgqueue == msgqueueA) {
            msg("Switching msgqueue from %d to %d", msgqueue, msgqueueB);
            msgqueue = msgqueueB;
            self->destinationMessageQueue = messageQueueKeyB;
        } else {
            msg("Switching msgqueue from %d to %d", msgqueue, msgqueueA);
            msgqueue = msgqueueA;
            self->destinationMessageQueue = messageQueueKeyA;
        }
        ++cycles;
    }
}
