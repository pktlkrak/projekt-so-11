#include "../signal_helpers.h"
#include <stdio.h>

int main() {
    registerSignals();
    printf("Waiting for user signals...\n");
    for(;;) {
        int output = waitForUserSignal();
        printf("Got signal: %d\n", output);
    }
    return 0;
}
