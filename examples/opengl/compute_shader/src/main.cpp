#include <automata_engine.hpp>

typedef uint32_t u32;
#define DllExport extern "C" __declspec(dllexport)

#include "main.hpp"

// NOTE: this is called every time that the game DLL is hot-loaded.
DllExport void GameOnHotload(ae::game_memory_t *gameMemory)
{
    ae::engine_memory_t *EM = gameMemory->pEngineMemory;

    // NOTE: this inits the automata engine module globals.
    ae::initModuleGlobals();
    // TODO: we might be able to merge these two calls?
    ae::setEngineContext(EM);

    // NOTE: ImGui has global state. therefore, we need to make sure
    // that we use the same state that the engine is using.
    // there are different global states since both the engine and the game DLL statically link to ImGui.
    ImGui::SetCurrentContext(EM->pfn.imguiGetCurrentContext());
    ImGuiMemAllocFunc allocFunc;
    ImGuiMemFreeFunc  freeFunc;
    void *            userData;
    EM->pfn.imguiGetAllocatorFunctions(&allocFunc, &freeFunc, &userData);
    ImGui::SetAllocatorFunctions(allocFunc, freeFunc, userData);

    ae::bifrost::clearAppTable(gameMemory);
    ae::bifrost::registerApp(gameMemory, "app", GameUpdateAndRender);

    // load the open GL funcs into this DLL.
    // NOTE: this fails the first time since this DLL is hot-loaded before OpenGL is initialized.
    if (EM->bOpenGLInitialized) glewInit();
}

// TODO: can we get rid of this shutdown thing?
// NOTE: this is called every time that the game DLL is unloaded.
DllExport void GameOnUnload() { ae::shutdownModuleGlobals(); }

// NOTE: this is called by the engine to get the "current app" as defined by the game.
DllExport ae::PFN_GameFunctionKind GameGetUpdateAndRender(ae::game_memory_t *gameMemory)
{
    return ae::bifrost::getCurrentApp(gameMemory);
}

game_state_t *getGameState(ae::game_memory_t *gameMemory) { return (game_state_t *)gameMemory->data; }

DllExport void GamePreInit(ae::game_memory_t *gameMemory)
{
    ae::engine_memory_t *EM = gameMemory->pEngineMemory;

    EM->defaultWinProfile = ae::AUTOMATA_ENGINE_WINPROFILE_NORESIZE;
    EM->defaultWidth      = 1280;
    EM->defaultHeight     = 720;
}

DllExport void GameInit(ae::game_memory_t *gameMemory)
{
    ae::engine_memory_t *EM = gameMemory->pEngineMemory;

    // NOTE: need to init glew here since the first call from *OnHotload fails due to DLL being loaded before OpenGL is init.
    if (EM->bOpenGLInitialized) glewInit();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    auto winInfo = EM->pfn.getWindowInfo();

    glViewport(0, 0, winInfo.width, winInfo.height);

    game_state_t *gameState = getGameState(gameMemory);

    gameState->computeShader = ae::GL::createComputeShader("res\\compute.glsl");
    if ((int)gameState->computeShader == -1) {
        EM->setFatalExit();
        return;
    }

    glUseProgram(gameState->computeShader);

    // make the UAV texture.
    glGenTextures(1, &gameState->outputImage);
    glBindTexture(GL_TEXTURE_2D, gameState->outputImage);
    gameState->outputFormat = GL_RGBA32F;
    glTexStorage2D(GL_TEXTURE_2D, 1, gameState->outputFormat, winInfo.width, winInfo.height);

    // wrap UAV in framebuffer object.
    {
        glGenFramebuffers(1, &gameState->outputImageFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, gameState->outputImageFramebuffer);
        GLint level = 0;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gameState->outputImage, level);
        // Check framebuffer completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            // Handle framebuffer creation error
            // ...
            AELoggerError("cowabunga!");
            EM->setFatalExit();
            return;
        }
        // Unbind the framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    gameMemory->setInitialized(true);
}

void GameUpdateAndRender(ae::game_memory_t *gameMemory)
{
    ae::engine_memory_t *EM        = gameMemory->pEngineMemory;
    game_state_t *       gameState = getGameState(gameMemory);

    glClearColor(1.0f, 1.0f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    auto winInfo = EM->pfn.getWindowInfo();

    // Do the compute draw.
    {
        GLuint    unit      = 0;
        GLint     arrayIdx  = 0;
        GLint     level     = 0;
        GLboolean bindArray = GL_FALSE;
        glBindImageTexture(
            unit, gameState->outputImage, level, bindArray, arrayIdx, GL_WRITE_ONLY, gameState->outputFormat);

        GLuint groupsX = (winInfo.width + (7)) / 8;
        GLuint groupsY = (winInfo.height + (7)) / 8;
        glDispatchCompute(groupsX, groupsY, 1);
    }

    /* NOTE: gl memory barriers are only required when doing incoherent memory
  accesses. these are:

  Writes (atomic or otherwise) via Image Load Store
  Writes (atomic or otherwise) via Shader Storage Buffer Objects
  Atomic operations on Atomic Counters
  Writes to variables declared as shared in Compute Shaders (but not output
  variables in Tessellation Control Shaders)
  */

    // Reads and writes via framebuffer object attachments after the barrier will
    // reflect data written by shaders prior to the barrier.
    glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);

    // do the copy
    {
        // set read source.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        // set draw dest.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gameState->outputImageFramebuffer);
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        // Copy the texture to the back buffer
        glBlitFramebuffer(
            0, 0, winInfo.width, winInfo.height, 0, 0, winInfo.width, winInfo.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // reset.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // engine overlay.
    ae::super::updateAndRender(gameMemory);
}

DllExport void GameClose(ae::game_memory_t *gameMemory)
{
    game_state_t *gameState = getGameState(gameMemory);
    glDeleteProgram(gameState->computeShader);
    glDeleteTextures(1, &gameState->outputImage);
}