#include <automata_engine.hpp>

#include <main.hpp>

#include "../../shared/monkey_demo.hpp"

// clang-format off
  // for rendering the cube outline.
  // clang-format off
// for rendering the cube outline.
static float cubeVertices[] = {
    // positions        // texture coords
    // front                              // pos' from Z-perspective
    1, 1, 0,  1, 1, 0,0,1,       // ff, top right
    1, 0, 0,  1, 0, 0,0,1,       // ff, bottom right
    0, 1, 0,  0, 1, 0,0,1,       // ff, top left

    1, 0, 0,  1, 0, 0,0,1,        // ff, bottom right
    0, 0, 0,  0, 0, 0,0,1,        // ff, bottom left
    0, 1, 0,  0, 1, 0,0,1,        // ff, top left

    // right
    1, 1, -1, 1, 1, 1,0,0,        // bf, top right
    1, 0, -1, 1, 0, 1,0,0,        // bf, bottom right
    1, 1, 0,  0, 1, 1,0,0,        // ff, top right

    1, 0, -1, 1, 0, 1,0,0,        // bf, bottom right
    1, 0, 0,  0, 0, 1,0,0,        // ff, bottom right
    1, 1, 0,  0, 1, 1,0,0,        // ff, top right

    // back
    0, 1, -1,  1, 1, 0,0,-1,        // bf, top left
    0, 0, -1,  1, 0, 0,0,-1,        // bf, bottom left
    1, 1, -1,  0, 1, 0,0,-1,        // bf, top right

    0, 0, -1,  1, 0, 0,0,-1,        // bf, bottom left
    1, 0, -1,  0, 0, 0,0,-1,        // bf, bottom right
    1, 1, -1,  0, 1, 0,0,-1,        // bf, top right

    // left (new)
    0, 1, 0,   1, 1, -1,0,0,        // ff, top left
    0, 0, 0,   0, 1, -1,0,0,        // ff, bottom left
    0, 1, -1,  0, 1, -1,0,0,        // bf, top left

    0, 0, 0,   0, 1, -1,0,0,        // ff, bottom left
    0, 0, -1,  0, 0, -1,0,0,        // bf, bottom left
    0, 1, -1,  0, 1, -1,0,0,        // bf, top left

    // top
    1, 1, -1,  1, 1, 0,1,0,        // bf, top right
    1, 1, 0,   1, 0, 0,1,0,        // ff, top right
    0, 1, -1,  0, 1, 0,1,0,        // bf, top left

    1, 1, 0,   1, 0, 0,1,0,        // ff, top right
    0, 1, 0,   0, 0, 0,1,0,        // ff, top left
    0, 1, -1,  0, 1, 0,1,0,        // bf, top left

    // bottom (final attempt les go)
    1, 0, 0,   1, 0, 0,-1,0,        // ff, bottom right
    1, 0, -1,  0, 1, 0,-1,0,        // bf, bottom right
    0, 0, 0,   0, 1, 0,-1,0,        // ff, bottom left

    1, 0, -1,  0, 1, 0,-1,0,        // bf, bottom right
    0, 0, -1,  1, 1, 0,-1,0,        // bf, bottom left
    0, 0, 0,   0, 1, 0,-1,0,        // ff, bottom left

};
  // clang-format on
  // clang-format on

void ae::Init(game_memory_t *gameMemory) {

  MonkeyDemoInit(gameMemory);

  ae::GL::initGlew();
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glEnable(GL_DEPTH_TEST);

  auto winInfo = ae::platform::getWindowInfo();

  game_state_t *gameState = getGameState(gameMemory);

  // TODO: is it possible to NOT collapse the directory structure of the assets as found?
  gameState->gameShader = ae::GL::createShader("res\\opaque_vert.glsl", "res\\opaque_frag.glsl");

  gameState->SSR_shader = ae::GL::createShader("res\\screen_vert.glsl", "res\\SSR_frag.glsl");

  gameState->lightingPass = ae::GL::createShader("res\\screen_vert.glsl", "res\\deferred_frag.glsl");

  if (((int)gameState->lightingPass == -1) || ((int)gameState->SSR_shader == -1) ||
      ((int)gameState->gameShader == -1)) {
      ae::setFatalExit();
      return;
  }

  glUseProgram(gameState->gameShader);

  // load the OBJ into VAO.
  gameState->suzanne = ae::io::loadObj("res\\monke.obj");
  if (gameState->suzanne.vertexData == nullptr) {
      ae::setFatalExit();
      return;
  }
  ae::GL::objToVao(gameState->suzanne, &gameState->suzanneIbo, &gameState->suzanneVbo, &gameState->suzanneVao);
  gameState->suzanneIndexCount = gameState->suzanneIbo.count;

  gameState->cubeVbo = ae::GL::createAndSetupVbo(3,
      ae::GL::vertex_attrib_t(GL_FLOAT, 3),
      ae::GL::vertex_attrib_t(GL_FLOAT, 2),
      ae::GL::vertex_attrib_t(GL_FLOAT, 3));
  glBindBuffer(GL_ARRAY_BUFFER, gameState->cubeVbo.glHandle);
  glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
  gameState->cubeVao =
      ae::GL::createAndSetupVao(1, ae::GL::vertex_attrib_desc_t(0, {0, 1, 2}, gameState->cubeVbo, false));

  // create the second gbuffer for rendering SSR UV image into.
  {
      GLuint &gBuffer2 = gameState->gBuffer2;
      glGenFramebuffers(1, &gBuffer2);
      glBindFramebuffer(GL_FRAMEBUFFER, gBuffer2);

      // SSR UVs.
      GLuint &gUVs = gameState->gUVs;
      glGenTextures(1, &gUVs);
      glBindTexture(GL_TEXTURE_2D, gUVs);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, winInfo.width, winInfo.height, 0, GL_RG, GL_FLOAT, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gUVs, 0);
  }

  // create the gbuffer sort of thing.
  {
      GLuint &gBuffer = gameState->gBuffer;
      glGenFramebuffers(1, &gBuffer);
      glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

      GLuint &gNormal = gameState->gNormal;
      GLuint &gColor  = gameState->gColor;
      GLuint &gPos    = gameState->gPos;

      // normals.
      glGenTextures(1, &gNormal);
      glBindTexture(GL_TEXTURE_2D, gNormal);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, winInfo.width, winInfo.height, 0, GL_RGB, GL_FLOAT, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

      // pos.
      glGenTextures(1, &gPos);
      glBindTexture(GL_TEXTURE_2D, gPos);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, winInfo.width, winInfo.height, 0, GL_RGB, GL_FLOAT, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gPos, 0);

      // color.
      glGenTextures(1, &gColor);
      glBindTexture(GL_TEXTURE_2D, gColor);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, winInfo.width, winInfo.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gColor, 0);

      // - tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
      unsigned int attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
      glDrawBuffers(_countof(attachments), attachments);

      // add the depth buffer to this framebuffer.
      {
          GLuint &depthTex = gameState->gDepth;
          glGenTextures(1, &depthTex);
          glBindTexture(GL_TEXTURE_2D, depthTex);
          glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_DEPTH_COMPONENT,
              winInfo.width,
              winInfo.height,
              0,
              GL_DEPTH_COMPONENT,
              GL_FLOAT,
              NULL);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
          GLfloat borderColor[] = {0.0, 0.0, 0.0, 0.0};
          glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
      }

      // Assert complete attachements for the framebuffer.
      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { assert(false); }
  }

  // create the texture used for SRV.
  {
      ae::loaded_image_t bitmap = ae::io::loadBMP("res\\highres_checker.bmp");
      if (bitmap.pixelPointer == nullptr) {
          ae::setFatalExit();
          return;
      }
      gameState->checkerTexture = ae::GL::createTexture(bitmap.pixelPointer, bitmap.width, bitmap.height);
      ae::io::freeLoadedImage(bitmap);

      // I think is like "bind texture into slot 0 and interpret as TEX 2D"
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, gameState->checkerTexture);
  }
}

void ae::InitAsync(game_memory_t *gameMemory) { gameMemory->setInitialized(true); }

void MonkeyDemoRender(ae::game_memory_t *gameMemory)
{
  auto winInfo = ae::platform::getWindowInfo();
  game_state_t *gameState = getGameState(gameMemory);

  const float iResolution[3] = {winInfo.width, winInfo.height, 1};

  // opaque pass.
  auto fnRenderGeometry = [&]()
  {
    // NOTE(Noah): Depth test is enabled, also clear depth buffer.
    // Depth testing culls frags that are occluded by other frags.
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(gameState->gameShader);

    glViewport(
      0, 0, winInfo.width, winInfo.height);

    // set pos of monke
    ae::GL::setUniformMat4f(gameState->gameShader, "umodel", 
      ae::math::buildMat4fFromTransform(gameState->suzanneTransform));
    // set camera transforms
    ae::GL::setUniformMat4f(gameState->gameShader, "uproj", buildProjMat(gameState->cam));
    ae::GL::setUniformMat4f(gameState->gameShader, "uview", buildViewMat(gameState->cam));
    GL_CALL(ae::GL::setUniformMat4f(
        gameState->gameShader, "uModelRotate", ae::math::buildRotMat4(gameState->suzanneTransform.eulerAngles)));

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
    GL_CALL(ae::GL::setUniformMat4f(
        gameState->gameShader, "uModelRotate", ae::math::mat4_t())); // identity.

    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 36));
  };

  // render geometry to the gbuffer.
  glBindFramebuffer(GL_FRAMEBUFFER, gameState->gBuffer);
  fnRenderGeometry();

  // do the fullscreen pass for SSR.
  {
    glBindFramebuffer(GL_FRAMEBUFFER, gameState->gBuffer2);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(gameState->SSR_shader);

    // set the samplers to point to the right texture units
    glUniform1i(glGetUniformLocation(gameState->SSR_shader, "iDepth"), 1);
    glUniform1i(glGetUniformLocation(gameState->SSR_shader, "iPos"), 2);

    // bind gbuffer images.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gameState->gNormal);
    GL_CALL(glActiveTexture(GL_TEXTURE1));
    glBindTexture(GL_TEXTURE_2D, gameState->gDepth);
    GL_CALL(glActiveTexture(GL_TEXTURE2));
    glBindTexture(GL_TEXTURE_2D, gameState->gPos);

    glUniform3fv(glGetUniformLocation(gameState->SSR_shader, "iResolution"), 1, iResolution);
    glUniform1f(glGetUniformLocation(gameState->SSR_shader, "iFarPlane"), gameState->cam.farPlane);
    glUniform1f(glGetUniformLocation(gameState->SSR_shader, "iNearPlane"), gameState->cam.nearPlane);
    float iCamTanHalfFov = tanf(gameState->cam.fov/2.0f*DEGREES_TO_RADIANS);
    float aspectRatio = (float)gameState->cam.height / (float)gameState->cam.width;
    glUniform1f(glGetUniformLocation(gameState->SSR_shader, "iCamTanHalfFov"), iCamTanHalfFov);
    glUniform1f(glGetUniformLocation(gameState->SSR_shader, "iCamTanHalfFovTimesAspect"), iCamTanHalfFov * aspectRatio);

    // NOTE: we reuse the cube VAO but without modifying the viewport, we only render to the
    // top right of the screen.
    glViewport(
      -winInfo.width, -winInfo.height, 2*winInfo.width, 2*winInfo.height);

    // NOTE: here we reuse the cube VAO, where we draw just once face to do a fullscreen pass.
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));

    
  }

  // finally do the lighting pass.
  {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glUseProgram(gameState->lightingPass);

      // set the samplers to point to the right texture units
      glUniform1i(glGetUniformLocation(gameState->lightingPass, "iNormals"), 0);
      glUniform1i(glGetUniformLocation(gameState->lightingPass, "iAlbedo"), 1);
      glUniform1i(glGetUniformLocation(gameState->lightingPass, "iPos"), 2);
      glUniform1i(glGetUniformLocation(gameState->lightingPass, "iUVs"), 3);

      // set the uniforms.
      glUniform3fv(glGetUniformLocation(gameState->lightingPass, "iResolution"), 1, iResolution);

      // bind the gbuffer images.
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, gameState->gNormal);
      GL_CALL(glActiveTexture(GL_TEXTURE1));
      glBindTexture(GL_TEXTURE_2D, gameState->gColor);
      GL_CALL(glActiveTexture(GL_TEXTURE2));
      glBindTexture(GL_TEXTURE_2D, gameState->gPos);
      GL_CALL(glActiveTexture(GL_TEXTURE3));
      glBindTexture(GL_TEXTURE_2D, gameState->gUVs);

      // NOTE: here we reuse the already bound VAO + viewport.      
      GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
  }

  // reset bound texture.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gameState->checkerTexture);

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