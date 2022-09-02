#include <emscripten.h>
//#include <spdlog/spdlog.h>
#include <emscripten/html5.h>

#include "WasmOcctView.hpp"
#include "AppManager.hpp"

//! Dummy main loop callback for a single shot.
extern "C" void onMainLoop()
{
    // do nothing here - viewer updates are handled on demand
    emscripten_cancel_main_loop();
}

EMSCRIPTEN_KEEPALIVE int main()
{
    //SPDLOG_INFO("Emscripten SDK {}.{}.{}", __EMSCRIPTEN_major__, __EMSCRIPTEN_minor__, __EMSCRIPTEN_tiny__);

    emscripten_set_main_loop (onMainLoop, -1, 0);

    WasmOcctView& view = WasmOcctView::Instance();
    view.run();

    return 0;
}