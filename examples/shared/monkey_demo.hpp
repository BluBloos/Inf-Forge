void MonkeyDemoRender(
    ae::game_memory_t *gameMemory);  // forward declare. to define for GFX API backend specific version.
void MonkeyDemoUpdate(ae::game_memory_t *gameMemory);

static game_state_t *getGameState(ae::game_memory_t *gameMemory) { return (game_state_t *)gameMemory->data; }

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

DllExport void GamePreInit(ae::game_memory_t *gameMemory)
{
    ae::engine_memory_t *EM = gameMemory->pEngineMemory;
    EM->defaultWinProfile   = ae::AUTOMATA_ENGINE_WINPROFILE_NORESIZE;
}

static void MonkeyDemoInit(ae::game_memory_t *gameMemory)
{
    ae::engine_memory_t *EM      = gameMemory->pEngineMemory;
    game_state_t *       gd      = getGameState(gameMemory);
    auto                 winInfo = EM->pfn.getWindowInfo();

    *gd = {};  // default initialize.

    gd->cam.trans.scale              = ae::math::vec3_t(1.0f, 1.0f, 1.0f);
    gd->cam.fov                      = 90.0f;
    gd->cam.nearPlane                = 0.01f;
    gd->cam.farPlane                 = 100.0f;
    gd->cam.width                    = winInfo.width;
    gd->cam.height                   = winInfo.height;
    gd->suzanneTransform.scale       = ae::math::vec3_t(1.0f, 1.0f, 1.0f);
    gd->suzanneTransform.pos         = ae::math::vec3_t(0.0f, 0.0f, -3.0f);
    gd->suzanneTransform.eulerAngles = {};

    // init default settings in ImGui panel.
    gd->bSpin               = true;
    gd->lockCamYaw          = false;
    gd->lockCamPitch        = false;
    gd->ambientStrength     = 0.1f;
    gd->specularStrength    = 0.5f;
    gd->lightColor          = {1, 1, 1, 1};
    gd->lightPos            = {0, 1, 0};
    gd->cameraSensitivity   = 3.0f;
    gd->optInFirstPersonCam = false;
    gd->bFocusedLastFrame   = true;  // assume for first frame.

    ae::bifrost::registerApp(gameMemory, AUTOMATA_ENGINE_PROJECT_NAME, MonkeyDemoUpdate);
}

static void MonkeyDemoUpdate(ae::game_memory_t *gameMemory)
{
    ae::engine_memory_t *   EM        = gameMemory->pEngineMemory;
    auto                    winInfo   = EM->pfn.getWindowInfo();
    game_state_t *          gd        = getGameState(gameMemory);
    const ae::user_input_t &userInput = EM->userInput;

    float speed = 5.f * EM->timing.lastFrameVisibleTime;

    bool &            bSpin               = gd->bSpin;
    bool &            lockCamYaw          = gd->lockCamYaw;
    bool &            lockCamPitch        = gd->lockCamPitch;
    float &           ambientStrength     = gd->ambientStrength;
    float &           specularStrength    = gd->specularStrength;
    ae::math::vec4_t &lightColor          = gd->lightColor;
    ae::math::vec3_t &lightPos            = gd->lightPos;
    float &           cameraSensitivity   = gd->cameraSensitivity;
    bool &            optInFirstPersonCam = gd->optInFirstPersonCam;

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
    ImGui::Begin(AUTOMATA_ENGINE_PROJECT_NAME);

    ImGui::Text(
        "---CONTROLS---\n"
        "WASD to move\n"
        "Right click to enter first person cam.\n"
        "ESC to exit first person cam.\n"
        "SPACE to fly up\n"
        "SHIFT to fly down\n\n");

    ImGui::Text("face count: %d", gd->suzanneIndexCount / 3);

    ImGui::Text("");

    // inputs.
    ImGui::Checkbox("debugRenderDepth", &gd->debugRenderDepthFlag);
    ImGui::Checkbox("bSpin", &bSpin);
    ImGui::Checkbox("lockCamYaw", &lockCamYaw);
    ImGui::Checkbox("lockCamPitch", &lockCamPitch);
    ImGui::SliderFloat("ambientStrength", &ambientStrength, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("specularStrength", &specularStrength, 0.0f, 1.0f, "%.3f");
    ImGui::ColorPicker4("lightColor", &lightColor[0]);
    ImGui::InputFloat3("lightPos", &lightPos[0]);
    ImGui::SliderFloat("cameraSensitivity", &cameraSensitivity, 1, 10);

    ae::ImGuiRenderMat4("camProjMat", buildProjMatForVk(gd->cam));
    ae::ImGuiRenderMat4("camViewMat", buildViewMat(gd->cam));
    ae::ImGuiRenderMat4((char *)(std::string(gd->suzanne.modelName) + "Mat").c_str(),
        ae::math::buildMat4fFromTransform(gd->suzanneTransform));
    ae::ImGuiRenderVec3("camPos", gd->cam.trans.pos);
    ae::ImGuiRenderVec3((char *)(std::string(gd->suzanne.modelName) + "Pos").c_str(), gd->suzanneTransform.pos);
    ImGui::End();
#endif

    // check if opt in.
    {
        bool &     bFocusedLastFrame = gd->bFocusedLastFrame;
        const bool isFocused         = winInfo.isFocused;
        const bool bExitingFocus     = bFocusedLastFrame && !isFocused;

        if (userInput.mouseRBttnDown[0] || userInput.mouseRBttnDown[1]) {
            // exit GUI.
            if (!optInFirstPersonCam) EM->pfn.showMouse(false);
            optInFirstPersonCam = true;
        }

        if ((userInput.keyDown[0][ae::GAME_KEY_ESCAPE] || userInput.keyDown[1][ae::GAME_KEY_ESCAPE]) || bExitingFocus) {
            // enter GUI.
            if (optInFirstPersonCam) EM->pfn.showMouse(true);
            optInFirstPersonCam = false;
        }

        bFocusedLastFrame = isFocused;
    }

    if (optInFirstPersonCam) {
        // clamp mouse cursor.
        EM->pfn.setMousePos((int)(winInfo.width / 2.0f), (int)(winInfo.height / 2.0f));
    }

    const float fullTimeStep = EM->timing.lastFrameVisibleTime * 0.5f;

    float (& lastDeltaX)[2] = gd->lastDeltaX;
    float (&lastDeltaY)[2] = gd->lastDeltaY;

    auto simulateWorldStep = [&](uint32_t                inputIdx,
                                 float                   timeStep,
                                 const ae::math::vec3_t &beginPosVector,
                                 const ae::math::vec3_t &beginEulerAngles,
                                 ae::math::vec3_t *      pEndPosVector,
                                 ae::math::vec3_t *      pEndEulerAngles) {
        *pEndPosVector   = beginPosVector;
        *pEndEulerAngles = beginEulerAngles;

        float yaw = 0.f;
        if (optInFirstPersonCam) {
            // NOTE: we smooth the input because I have found that a stable input can produce a really stable output.
            // (tested via hardcoding some constant input).
            // so, the jerky camera stuff seems to come from responsiveness to the jerky player input.

            // we'll assume that this is in pixels.
            float deltaX = userInput.deltaMouseX[inputIdx];
            float deltaY = userInput.deltaMouseY[inputIdx];

            auto sigmoid = [](float x) -> float { return 1.f / (1.f + ae::math::pow(2.71828f, -x)); };

            float deltaDist = ae::math::sqrt(deltaX * deltaX + deltaY * deltaY);
            float T         = ae::math::min(1.f, ae::math::max(0.f, 0.8f - sigmoid(deltaDist - 30.f)));
            deltaX          = ae::math::lerp(deltaX, lastDeltaX[inputIdx], T);
            deltaY          = ae::math::lerp(deltaY, lastDeltaY[inputIdx], T);

            if (lockCamYaw) deltaX = 0.f;
            if (lockCamPitch) deltaY = 0.f;

            // NOTE: we store the last as the muted/smoothed version. so that's extra smoothness?
            lastDeltaX[inputIdx] = deltaX;
            lastDeltaY[inputIdx] = deltaY;

            deltaX *= cameraSensitivity;
            deltaY *= cameraSensitivity;

            float r = tanf(gd->cam.fov * DEGREES_TO_RADIANS / 2.0f) * gd->cam.nearPlane;
            float t = r * (float(winInfo.height) / winInfo.width);

            deltaX *= r / (winInfo.width * 0.5f);
            deltaY *= t / (winInfo.height * 0.5f);

            float rotationFactor = timeStep / fullTimeStep;

            yaw         = ae::math::atan2(deltaX, gd->cam.nearPlane) * rotationFactor;
            float pitch = ae::math::atan2(deltaY, gd->cam.nearPlane) * rotationFactor;

            *pEndEulerAngles += ae::math::vec3_t(0.0f, yaw, 0.0f);
            *pEndEulerAngles += ae::math::vec3_t(-pitch, 0.0f, 0.0f);
        }

        // clamp camera pitch
        float pitchClamp = PI / 2.0f - 0.01f;
        if (pEndEulerAngles->x < -pitchClamp) pEndEulerAngles->x = -pitchClamp;
        if (pEndEulerAngles->x > pitchClamp) pEndEulerAngles->x = pitchClamp;

        float movementSpeed = 5.f;
        float linearStep    = movementSpeed * timeStep;

        ae::math::mat3_t camBasisBegin =
            ae::math::mat3_t(buildRotMat4(ae::math::vec3_t(0.0f, beginEulerAngles.y, 0.0f)));

        {
            ae::math::vec3_t movDir = ae::math::vec3_t();
            if (userInput.keyDown[inputIdx][ae::GAME_KEY_W]) {
                movDir += camBasisBegin * ae::math::vec3_t(0.0f, 0.0f, -1);
            }
            if (userInput.keyDown[inputIdx][ae::GAME_KEY_A]) {
                movDir += camBasisBegin * ae::math::vec3_t(-1, 0.0f, 0.0f);
            }
            if (userInput.keyDown[inputIdx][ae::GAME_KEY_S]) {
                movDir += camBasisBegin * ae::math::vec3_t(0.0f, 0.0f, 1);
            }
            if (userInput.keyDown[inputIdx][ae::GAME_KEY_D]) {
                movDir += camBasisBegin * ae::math::vec3_t(1, 0.0f, 0.0f);
            }

            if (userInput.keyDown[inputIdx][ae::GAME_KEY_SHIFT]) {
                movDir += ae::math::vec3_t(0.0f, -1, 0.0f);
            } else if (userInput.keyDown[inputIdx][ae::GAME_KEY_SPACE]) {
                movDir += ae::math::vec3_t(0.0f, 1, 0.0f);
            }

            // NOTE: the camera is rotating smoothly. the below math is the result
            // of doing the integral for that circular motion.
            auto movDirNorm = ae::math::normalize(movDir);

            if (ae::math::abs(yaw) >= 0.0001f) {
                auto  t0       = movDirNorm * movementSpeed * 1.414f;  // original velocity vector.
                float oneOverW = timeStep / yaw;

                float r_x =
                    pEndPosVector->x + oneOverW * (t0.x * ae::math::sin(yaw) - t0.z * ae::math::cos(yaw) + t0.z);
                float r_z =
                    pEndPosVector->z + oneOverW * (t0.z * ae::math::sin(yaw) + t0.x * ae::math::cos(yaw) - t0.x);

                pEndPosVector->x = r_x;
                pEndPosVector->z = r_z;
            } else {
                pEndPosVector->x += movDirNorm.x * linearStep;
                pEndPosVector->z += movDirNorm.z * linearStep;
            }
            pEndPosVector->y += movDirNorm.y * linearStep;
        }
    };

    float timeStep = fullTimeStep;

    ae::math::vec3_t s1_posVector;
    ae::math::vec3_t s1_eulerAngles;
    simulateWorldStep(0, timeStep, gd->cam.trans.pos, gd->cam.trans.eulerAngles, &s1_posVector, &s1_eulerAngles);

    // compute the interpolated game state for rendering.
    simulateWorldStep(1, timeStep / 2.f, s1_posVector, s1_eulerAngles, &gd->cam.trans.pos, &gd->cam.trans.eulerAngles);

    // get the next state.
    ae::math::vec3_t s2_posVector;
    ae::math::vec3_t s2_eulerAngles;
    simulateWorldStep(1, timeStep, s1_posVector, s1_eulerAngles, &s2_posVector, &s2_eulerAngles);

    if (bSpin) gd->suzanneTransform.eulerAngles += ae::math::vec3_t(0.0f, 3.0f * EM->timing.lastFrameVisibleTime, 0.0f);

    // TODO: look into the depth testing stuff more deeply on the hardware side of things.
    // what is something that we can only do because we really get it?

    ae::super::updateAndRender(gameMemory);

    MonkeyDemoRender(gameMemory);
}