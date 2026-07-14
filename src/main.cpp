#include "engine.h"

int main(int argc, char** argv)
{
    Engine engine(argc, argv);
    engine.game_loop();

    return engine.exitCode();
}
