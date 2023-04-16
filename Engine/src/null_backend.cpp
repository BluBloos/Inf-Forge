
// this backend is used by tests or whatever else.
// it kicks in at compile-time when no other backend is active/available.
#if !defined(AUTOMATA_ENGINE_METAL_BACKEND) && !defined(_MSC_VER)

#include <automata_engine.hpp>

// -------- AE external vars --------

float ae::platform::lastFrameTime = 1 / 60.0f;
float ae::platform::lastFrameTimeTotal = 1 / 60.0f;
bool ae::platform::_globalRunning = true;
bool ae::platform::_globalVsync = false;
int automata_engine::platform::_globalProgramResult = 0;
ae::update_model_t ae::platform::GLOBAL_UPDATE_MODEL = 
    ae::AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC; // default.

// -------- AE internal vars --------

// -------- AE platform functions --------

void ae::platform::freeLoadedFile(loaded_file_t file) {}

ae::loaded_file_t ae::platform::readEntireFile(const char *fileName) {
    return {};
}

void automata_engine::platform::fprintf_proxy(int h, const char *fmt, ...) {}

void automata_engine::platform::free(void *data) {
    if (data)
{

}
}

void *automata_engine::platform::alloc(uint32_t bytes) {
    return nullptr;
}

void ae::platform::setVsync(bool b) {
}


// -------- AE platform functions --------

#endif