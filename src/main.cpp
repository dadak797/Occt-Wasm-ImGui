#include "WasmOcctView.hpp" 

#include <emscripten.h>
#include <emscripten/html5.h>


// Dummy main loop callback for a single shot.
extern "C" void onMainLoop()
{
    // do nothing here - viewer updates are handled on demand
    emscripten_cancel_main_loop();
}

EMSCRIPTEN_KEEPALIVE int main()
{
    SPDLOG_INFO("Start Application");

    // setup a dummy single-shot main loop callback just to shut up a useless Emscripten error message on calling eglSwapInterval()
    emscripten_set_main_loop (onMainLoop, -1, 0);

    WasmOcctView& aViewer = WasmOcctView::Instance();
    aViewer.run();

    return 0;
}
