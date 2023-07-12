#include <automata_engine.hpp>

#include <main.hpp>

#include "../../shared/monkey_demo.hpp"

void ae::Init(game_memory_t *gameMemory) {

  MonkeyDemoInit(gameMemory);

  ae::GL::initGlew();
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glEnable(GL_DEPTH_TEST);

  auto winInfo = ae::platform::getWindowInfo();

  glViewport(
    0, 0, winInfo.width, winInfo.height);

  game_state_t *gameState = getGameState(gameMemory);

  // TODO: is it possible to NOT collapse the directory structure of the assets as found?
  gameState->gameShader = ae::GL::createShader("res\\vert.glsl",
                                               "res\\frag.glsl");
  if ((int)gameState->gameShader == -1) {
    ae::setFatalExit();
    return;
  }

  glUseProgram(gameState->gameShader);

  // create the texture used for SRV.
  ae::loaded_image_t bitmap = ae::io::loadBMP("res\\highres_checker.bmp");
  if (bitmap.pixelPointer == nullptr) { ae::setFatalExit(); return; }
  gameState->checkerTexture = ae::GL::createTexture(bitmap.pixelPointer, bitmap.width, bitmap.height);
  ae::io::freeLoadedImage(bitmap);

  // I think is like "bind texture into slot 0 and interpret as TEX 2D"
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gameState->checkerTexture);

  // load the OBJ into VAO.
  gameState->suzanne = ae::io::loadObj("res\\monke.obj");
  if (gameState->suzanne.vertexData == nullptr) { ae::setFatalExit(); return; }
  ae::GL::objToVao(gameState->suzanne, &gameState->suzanneIbo, &gameState->suzanneVbo, 
    &gameState->suzanneVao);
  gameState->suzanneIndexCount = gameState->suzanneIbo.count;
  glBindVertexArray(gameState->suzanneVao);
}

void ae::InitAsync(game_memory_t *gameMemory) { gameMemory->setInitialized(true); }

void MonkeyDemoRender(ae::game_memory_t *gameMemory)
{
  game_state_t *gameState = getGameState(gameMemory);
  // NOTE(Noah): Depth test is enabled, also clear depth buffer.
  // Depth testing culls frags that are occluded by other frags.
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  // set pos of monke
  ae::GL::setUniformMat4f(gameState->gameShader, "umodel", 
    ae::math::buildMat4fFromTransform(gameState->suzanneTransform));
  // set camera transforms
  ae::GL::setUniformMat4f(gameState->gameShader, "uproj", buildProjMat(gameState->cam));
  ae::GL::setUniformMat4f(gameState->gameShader, "uview", buildViewMat(gameState->cam));
  // Do the draw call
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gameState->suzanneIbo.glHandle);
  glDrawElements(GL_TRIANGLES, gameState->suzanneIbo.count, GL_UNSIGNED_INT, NULL);
}

void ae::Close(game_memory_t *gameMemory) {
  game_state_t *gameState = getGameState(gameMemory);
  ae::io::freeObj(gameState->suzanne);
  glDeleteProgram(gameState->gameShader);
  glDeleteTextures(1, &gameState->checkerTexture);
  glDeleteBuffers(1, &gameState->suzanneIbo.glHandle);
  glDeleteBuffers(1, &gameState->suzanneVbo.glHandle);
  glDeleteVertexArrays(1, &gameState->suzanneVao);
}