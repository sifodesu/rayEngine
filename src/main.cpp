#include "engine.h"
#include <iostream>
#include <typeinfo>


int main(int argc, char** argv) {
    Engine engine(argc, argv);
    engine.game_loop();
        
    return 0;
}
