#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "global_config.h"

void createDispatcherFileName(int key, char *buffer, int n) {
    snprintf(buffer, n, "/tmp/ex11_%d.msg", key);
}

#define F_EXEC(a, ...) if(!(pid = fork())) execl(a, a, __VA_ARGS__, NULL)
#define WAIT_FOR_COMPLETION waitpid(pid, NULL, 0)

pid_t lastBoatPid;

struct DispatcherInit {
    int key;
};

void *_startDispatcher(void *_init) {
    pid_t pid;
    struct DispatcherInit *init = _init;

    char keyname[64];
    createDispatcherFileName(init->key, keyname, sizeof(keyname));
    int fd = creat(keyname, DEFAULT_PERMS);
    assert(fd > 1);
    close(fd);
    // printf("Starting dispatcher %s\n", keyname);
    F_EXEC("./dispatcher/main", keyname);
    // printf("Started dispatcher - PID is %d\n", pid);
    WAIT_FOR_COMPLETION;
    unlink(keyname);
    free(init);
    return NULL;
}

void startDispatcher(int key) {
    struct DispatcherInit *init = malloc(sizeof(struct DispatcherInit));
    init->key = key;

    pthread_t thread;
    pthread_create(&thread, NULL, _startDispatcher, (void *) init);
    pthread_detach(thread);
    sleep(1);
}

struct BoatInit {
    int dispatcher1Key, dispatcher2Key;
};

void *_startBoat(void *_init) {
    struct BoatInit *init = _init;
    pid_t pid;

    char keyname1[64];
    createDispatcherFileName(init->dispatcher1Key, keyname1, sizeof(keyname1));
    char keyname2[64];
    createDispatcherFileName(init->dispatcher2Key, keyname2, sizeof(keyname2));
    // printf("Starting boat going between dispatchers %s and %s\n", keyname1, keyname2);
    F_EXEC("./boat/main", keyname1, keyname2);
    // printf("Started boat - PID is %d\n", pid);
    lastBoatPid = pid;
    WAIT_FOR_COMPLETION;
    free(init);
    return NULL;
}

void startBoat(int dispatcher1Key, int dispatcher2Key) {
    struct BoatInit *init = malloc(sizeof(struct BoatInit));
    init->dispatcher1Key = dispatcher1Key;
    init->dispatcher2Key = dispatcher2Key;

    pthread_t thread;
    pthread_create(&thread, NULL, _startBoat, (void *) init);
    pthread_detach(thread);
    sleep(1);
}

struct PassengerInit {
    int boundDispatcher;
    bool hasBike;
};

void *_startPassenger(void *_init) {
    struct PassengerInit *init = _init;
    pid_t pid;

    char keyname[64];
    createDispatcherFileName(init->boundDispatcher, keyname, sizeof(keyname));
    // printf("Starting passenger bound to dispatcher %s\n", keyname);
    if(init->hasBike) F_EXEC("./passenger/main", keyname, "1");
    else F_EXEC("./passenger/main", keyname);

    // printf("Started passenger - PID is %d\n", pid);
    WAIT_FOR_COMPLETION;
    free(init);
    return NULL;
}

void startPassenger(int boundDispatcher, bool hasBike) {
    struct PassengerInit *init = malloc(sizeof(struct PassengerInit));
    init->boundDispatcher = boundDispatcher;
    init->hasBike = hasBike;
    pthread_t thread;
    pthread_create(&thread, NULL, _startPassenger, (void *) init);
    pthread_detach(thread);
}

void killBoat(){
    if(lastBoatPid == -1) return;
    kill(lastBoatPid, SIGTERM);
    lastBoatPid = -1;
}

const char *MENU =
    "=== Ex. 11 - Simulation Menu ===\n"
    "1. Start the overload simulation\n"
    "2. Manual control - spawn boat and endpoints, then ask for further instructions.\n> "
;

void overloadSimulation() {
    int howManyPassengersPerEndpoint = 0;
    printf("How many passengers per endpoint (50%% chance of them having a bike)? ");
    scanf("%d\n", &howManyPassengersPerEndpoint);
    startDispatcher(1234);
    startDispatcher(5678);
    startBoat(1234, 5678);
    for(int i = 0; i<howManyPassengersPerEndpoint; i++) {
        startPassenger(1234, rand() & 1);
        startPassenger(5678, rand() & 1);
    }
    printf("Please press any key to stop the simulation");
    getc(stdin);
    killBoat();
}

void manualControl() {
    startDispatcher(1234);
    startDispatcher(5678);
    startBoat(1234, 5678);
    printf("Please enter the info about passengers to spawn in the format '<endpoint: 0/1> <count> <bikes: 0/1>\nEnter 0 0 0 to end the simulation.\n");
    for(;;) {
        int ep, count, bike;
        printf("> ");
        fflush(stdout);
        if(scanf("%d %d %d", &ep, &count, &bike) != 3 || (ep != 0 && ep != 1)) {
            printf("Invalid data.\n");
            continue;
        }
        if(ep == 0 && count == 0 && bike == 0) break;
        ep = ep ? 5678 : 1234;
        for(int i = 0; i<count; i++) {
            startPassenger(ep, bike);
        }
    }
    killBoat();
}

int main() {
    signal(SIGCHLD, SIG_IGN);

    printf(
        "Simulation parameters:\n"
        "K (Bridge capacity): %d\n"
        "N (Boat capacity): %d\n"
        "M (Boat bike capacity): %d\n"
        "T1 (Boat docked wait time [s]): %d\n"
        "T2 (Boat trip time [s]): %d\n"
        "R (Boat max cycles): %d\n",
        BRIDGE_CAPACITY,
        BOAT_CAPACITY,
        BOAT_BIKE_CAPACITY,
        BOAT_WAIT_TIME,
        BOAT_TRIP_TIME,
        BOAT_MAX_CYCLES
    );

    for(;;) {
        int choice;

        printf(MENU);
        if(scanf("%d", &choice) != 1) goto badCase;

        switch(choice) {
            case 1:
                overloadSimulation();
                break;
            case 2:
                manualControl();
                break;
            default:
            badCase:
                printf("Invalid choice!\n");
                continue;
        }
    }
}
