#include <vector>
#include "objComponent.h"

class GObject {
    public:
    void routine();

    private:
    std::vector<ObjComponent*> components;
};