#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct Controller {
    uint64_t magic;

    Controller() : magic(0x63787870726f6265ull) {}

    uint64_t value() const {
        return magic;
    }
};

template <typename T>
class LocalUniquePtr {
public:
    explicit LocalUniquePtr(T *ptr = 0) : ptr_(ptr) {}

    ~LocalUniquePtr() {
        delete ptr_;
    }

    LocalUniquePtr(const LocalUniquePtr &) = delete;
    LocalUniquePtr &operator=(const LocalUniquePtr &) = delete;

    T *get() const {
        return ptr_;
    }

private:
    T *ptr_;
};

class Agent {
public:
    Agent() : controller_(new Controller()) {}

    Controller *GetTracingController() {
        Controller *controller = controller_.get();
        if (controller == 0) {
            return 0;
        }
        return controller;
    }

private:
    LocalUniquePtr<Controller> controller_;
};

static Agent global_agent;
static int ctor_count;

struct InitCounter {
    InitCounter() {
        ctor_count++;
    }
};

static InitCounter init_counter;

extern "C" int main() {
    printf("cxxprobe: start\n");
    Controller *controller = global_agent.GetTracingController();
    if (controller == 0) {
        printf("cxxprobe: controller null\n");
        return 1;
    }
    printf("cxxprobe: controller=0x%llx ctor_count=%d\n",
        (unsigned long long)controller->value(),
        ctor_count);
    if (controller->value() != 0x63787870726f6265ull || ctor_count != 1) {
        printf("cxxprobe: mismatch\n");
        return 2;
    }
    Agent local_agent;
    if (local_agent.GetTracingController() == 0) {
        printf("cxxprobe: local controller null\n");
        return 3;
    }
    printf("cxxprobe: ok\n");
    return 0;
}
