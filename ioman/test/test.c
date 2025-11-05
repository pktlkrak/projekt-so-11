#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../ioman.h"

int main() {
    struct IOManInitPacket init = { 2, RED, 0 };
    iomanConnect(&init, "Test Client");
    iomanTakeoverStdio(true);

    for(int i = 0; i<10000; i++) msg("Hello there for the %d time!", i + 1);
    printf("Hello there!!\n");
    sleep(2);
    fflush(stdout);
    fprintf(stderr, "Error stream!\n");

    return 0;
}
