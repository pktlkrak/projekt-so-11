#include "signal_helpers.h"
#include <signal.h>
#include <pthread.h>

static pthread_mutex_t signalWaitMutex = PTHREAD_MUTEX_INITIALIZER;
static int lastSignal = -1;
static void signalHandler(int signal) {
    lastSignal = signal;
    pthread_mutex_unlock(&signalWaitMutex);
}

void registerSignals() {
    signal(SIGUSR1, signalHandler);
    signal(SIGUSR2, signalHandler);
    pthread_mutex_lock(&signalWaitMutex);
}

int waitForUserSignal() {
    pthread_mutex_lock(&signalWaitMutex);
    int ls = lastSignal;
    lastSignal = -1;
    return ls;
}
