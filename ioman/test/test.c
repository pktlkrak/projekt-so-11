#include "../ioman.h"

int main() {
    struct IOManInitPacket init = { 2, RED, 0 };
    iomanConnect(&init, "Test Client");

    for(int i = 0; i<10000; i++) msg("Hello there for the %d time!", i + 1);
    return 0;
}
