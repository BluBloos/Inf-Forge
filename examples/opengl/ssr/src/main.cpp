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

// clang-format off
  // for rendering the cube outline.
  float cubeVertices[] = {
      // positions        // texture coords
      // front                              // pos' from Z-perspective
      1.0f, 1.0f, 0.0f,  1.0f, 1.0f,        // ff, top right
      1.0f, 0.0f, 0.0f,  1.0f, 0.0f,        // ff, bottom right
      0.0f, 1.0f, 0.0f,  0.0f, 1.0f,        // ff, top left
      
      0.0f, 1.0f, 0.0f,  0.0f, 1.0f,        // ff, top left
      1.0f, 0.0f, 0.0f,  1.0f, 0.0f,        // ff, bottom right
      0.0f, 0.0f, 0.0f,  0.0f, 0.0f,        // ff, bottom left
      
      // right
      1.0f, 1.0f, 0.0f,  0.0f, 1.0f,        // ff, top right
      1.0f, 1.0f, -1.0f,  1.0f, 1.0f,        // bf, top right
      1.0f, 0.0f, -1.0f,  1.0f, 0.0f,        // bf, bottom right

      1.0f, 1.0f, 0.0f,  0.0f, 1.0f,        // ff, top right
      1.0f, 0.0f, -1.0f,  1.0f, 0.0f,        // bf, bottom right
      1.0f, 0.0f, 0.0f,  0.0f, 0.0f,        // ff, bottom right

      // back
      1.0f, 1.0f, -1.0f,  0.0f, 1.0f,        // bf, top right
      0.0f, 1.0f, -1.0f,  1.0f, 1.0f,        // bf, top left
      0.0f, 0.0f, -1.0f,  1.0f, 0.0f,        // bf, bottom left

      1.0f, 1.0f, -1.0f,  0.0f, 1.0f,        // bf, top right
      0.0f, 0.0f, -1.0f,  1.0f, 0.0f,        // bf, bottom left
      1.0f, 0.0f, -1.0f,  0.0f, 0.0f,        // bf, bottom right

      // left
      0.0f, 1.0f, -1.0f,  0.0f, 1.0f,        // bf, top left
      0.0f, 1.0f, 0.0f,  1.0f, 1.0f,        // ff, top left
      0.0f, 0.0f, -1.0f,  0.0f, 0.0f,        // bf, bottom left

      0.0f, 1.0f, 0.0f,  1.0f, 1.0f,        // ff, top left
      0.0f, 0.0f, 0.0f,  0.0f, 1.0f,        // ff, bottom left
      0.0f, 0.0f, -1.0f,  0.0f, 0.0f,        // bf, bottom left

      // top
      0.0f, 1.0f, -1.0f,  0.0f, 1.0f,        // bf, top left
      1.0f, 1.0f, -1.0f,  1.0f, 1.0f,        // bf, top right
      1.0f, 1.0f, 0.0f,  1.0f, 0.0f,        // ff, top right

      0.0f, 1.0f, -1.0f,  0.0f, 1.0f,        // bf, top left
      1.0f, 1.0f, 0.0f,  1.0f, 0.0f,        // ff, top right
      0.0f, 1.0f, 0.0f,  0.0f, 0.0f,        // ff, top left

      // bottom
      1.0f, 0.0f, -1.0f,  0.0f, 1.0f,        // bf, bottom right
      0.0f, 0.0f, -1.0f,  1.0f, 1.0f,        // bf, bottom left
      1.0f, 0.0f, 0.0f,  0.0f, 0.0f,        // ff, bottom right

      0.0f, 0.0f, -1.0f,  1.0f, 1.0f,        // bf, bottom left
      0.0f, 0.0f, 0.0f,  0.0f, 1.0f,        // ff, bottom left
      1.0f, 0.0f, 0.0f,  0.0f, 0.0f,        // ff, bottom right
  };
  // clang-format on

  gameState->cubeVbo =
      ae::GL::createAndSetupVbo(2, ae::GL::vertex_attrib_t(GL_FLOAT, 3), ae::GL::vertex_attrib_t(GL_FLOAT, 2));
  glBindBuffer(GL_ARRAY_BUFFER, gameState->cubeVbo.glHandle);
  glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
  gameState->cubeVao = ae::GL::createAndSetupVao(1, ae::GL::vertex_attrib_desc_t(0, {0, 1}, gameState->cubeVbo, false));
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
  glBindVertexArray(gameState->suzanneVao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gameState->suzanneIbo.glHandle);
  glDrawElements(GL_TRIANGLES, gameState->suzanneIbo.count, GL_UNSIGNED_INT, NULL);

  // draw the cube that's going to do the reflection stuff.
  GL_CALL(glBindVertexArray(gameState->cubeVao));
  GL_CALL(ae::GL::setUniformMat4f(gameState->gameShader,
      "umodel",
      ae::math::buildMat4fFromTransform({.pos = ae::math::vec3_t(-3, -3, -3 /*NOTE: VAO_CUBE render Z:[0,-1]*/),
          .eulerAngles                        = ae::math::vec3_t(),
          .scale                              = ae::math::vec3_t(1, 1, 1)})));
  GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 36));
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