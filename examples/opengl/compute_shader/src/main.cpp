#include <automata_engine.hpp>

#define APP_NAME "ComputeShader"

namespace ae = automata_engine;

typedef uint32_t u32;

void GameUpdateAndRender(ae::game_memory_t *gameMemory);

typedef struct game_state {
  GLuint computeShader;
  GLuint outputImage;
  GLenum outputFormat;
  GLuint outputImageFramebuffer;
} game_state_t;

game_state_t *getGameState(ae::game_memory_t *gameMemory) {
  return (game_state_t *)gameMemory->data;
}

void ae::HandleWindowResize(game_memory_t *gameMemory, int newWidth,
                            int newHeight) {}

void ae::PreInit(game_memory_t *gameMemory) {
  ae::defaultWinProfile = AUTOMATA_ENGINE_WINPROFILE_NORESIZE;
  ae::defaultWindowName = APP_NAME;
  ae::defaultWidth = 1280;
  ae::defaultHeight = 720;
}

void ae::Init(game_memory_t *gameMemory) {
  ae::GL::initGlew();
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

  auto winInfo = ae::platform::getWindowInfo();

  glViewport(0, 0, winInfo.width, winInfo.height);

  game_state_t *gameState = getGameState(gameMemory);

  gameState->computeShader = ae::GL::createComputeShader("res\\compute.glsl");
  if ((int)gameState->computeShader == -1) {
    ae::setFatalExit();
    return;
  }

  glUseProgram(gameState->computeShader);

  // make the UAV texture.
  glGenTextures(1, &gameState->outputImage);
  glBindTexture(GL_TEXTURE_2D, gameState->outputImage);
  gameState->outputFormat = GL_RGBA32F;
  glTexStorage2D(GL_TEXTURE_2D, 1, gameState->outputFormat, winInfo.width,
                 winInfo.height);

  // wrap UAV in framebuffer object.
  {
    glGenFramebuffers(1, &gameState->outputImageFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gameState->outputImageFramebuffer);
    GLint level = 0;
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           gameState->outputImage, level);
    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      // Handle framebuffer creation error
      // ...
      AELoggerError("cowabunga!");
      ae::setFatalExit();
      return;
    }
    // Unbind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  ae::bifrost::registerApp("app", GameUpdateAndRender);
  ae::setUpdateModel(AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC);
  ae::platform::setVsync(true);
}

void ae::InitAsync(game_memory_t *gameMemory) {
  gameMemory->setInitialized(true);
}

// TODO: would be nice if it wasn't a requirement to do this thing. and instead
// we could just be like, "we aren't using sound stuff."
void ae::OnVoiceBufferEnd(game_memory_t *gameMemory, intptr_t voiceHandle) {}
void ae::OnVoiceBufferProcess(game_memory_t *gameMemory, intptr_t voiceHandle,
                              float *dst, float *src, uint32_t samplesToWrite,
                              int channels, int bytesPerSample) {}

void GameUpdateAndRender(ae::game_memory_t *gameMemory) {
  game_state_t *gameState = getGameState(gameMemory);

  glClearColor(1.0f, 1.0f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  auto winInfo = ae::platform::getWindowInfo();

  // Do the compute draw.
  {
    GLuint unit = 0;
    GLint arrayIdx = 0;
    GLint level = 0;
    GLboolean bindArray = GL_FALSE;
    glBindImageTexture(unit, gameState->outputImage, level, bindArray, arrayIdx,
                       GL_WRITE_ONLY, gameState->outputFormat);

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
    glBlitFramebuffer(0, 0, winInfo.width, winInfo.height, 0, 0, winInfo.width,
                      winInfo.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // reset.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  // engine overlay.
  ae::super::updateAndRender(gameMemory);
}

void ae::Close(game_memory_t *gameMemory) {
  game_state_t *gameState = getGameState(gameMemory);
  glDeleteProgram(gameState->computeShader);
  glDeleteTextures(1, &gameState->outputImage);
}