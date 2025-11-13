#include "signal_helpers.h"
#include <signal.h>
#include <pthread.h>

static pthread_mutex_t signalWaitMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t signalConditionWaiter = PTHREAD_COND_INITIALIZER;
static int lastSignal = -1;
static void signalHandler(int signal) {
    lastSignal = signal;
    pthread_cond_broadcast(&signalConditionWaiter);
}

void registerSignals() {
    signal(SIGUSR1, signalHandler);
    signal(SIGUSR2, signalHandler);
}

int waitForUserSignal() {
    pthread_cond_wait(&signalConditionWaiter, &signalWaitMutex);
    int ls = lastSignal;
    lastSignal = -1;
    return ls;
}
