#include "signal_helpers.h"
#include <signal.h>
#include <pthread.h>
#include <assert.h>

void registerSignals() {
    sigset_t signals;
    sigaddset(&signals, SIGUSR1);
    sigaddset(&signals, SIGUSR2);
    sigprocmask(SIG_BLOCK, &signals, NULL);
}

int waitForUserSignal() {
    sigset_t signals;
    int signal;
    sigaddset(&signals, SIGUSR1);
    sigaddset(&signals, SIGUSR2);
    assert(sigwait(&signals, &signal) == 0);
    return signal;
}
