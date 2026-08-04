#include <gui/menus/sink.h>
#include <signal_path/signal_path.h>
#include <core.h>

namespace sinkmenu {
    void init() {
        sigpath::sinkManager.loadSinksFromConfig();
    }

    void draw(void* ctx) {
        sigpath::sinkManager.showMenu();
    }
};