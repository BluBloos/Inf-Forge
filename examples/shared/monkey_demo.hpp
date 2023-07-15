void MonkeyDemoRender(ae::game_memory_t *gameMemory); // forward declare. to define for GFX API backend specific version.
void MonkeyDemoUpdate(ae::game_memory_t *gameMemory);

static game_state_t *getGameState(ae::game_memory_t *gameMemory) { return (game_state_t *)gameMemory->data; }

void ae::PreInit(game_memory_t *gameMemory)
{
    ae::defaultWinProfile = AUTOMATA_ENGINE_WINPROFILE_NORESIZE;
    ae::defaultWindowName = AUTOMATA_ENGINE_PROJECT_NAME;
}

static void MonkeyDemoInit(ae::game_memory_t *gameMemory)
{
    game_state_t *gd      = getGameState(gameMemory);
    auto          winInfo = ae::platform::getWindowInfo();

    *gd = {}; // zero it out.

    gd->cam.trans.scale              = ae::math::vec3_t(1.0f, 1.0f, 1.0f);
    gd->cam.fov                      = 90.0f;
    gd->cam.nearPlane                = 0.01f;
    gd->cam.farPlane                 = 100.0f;
    gd->cam.width                    = winInfo.width;
    gd->cam.height                   = winInfo.height;
    gd->suzanneTransform.scale       = ae::math::vec3_t(1.0f, 1.0f, 1.0f);
    gd->suzanneTransform.pos         = ae::math::vec3_t(0.0f, 0.0f, -3.0f);
    gd->suzanneTransform.eulerAngles = {};

    ae::bifrost::registerApp(AUTOMATA_ENGINE_PROJECT_NAME, MonkeyDemoUpdate);
    ae::setUpdateModel(ae::AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC);
}

static void MonkeyDemoUpdate(ae::game_memory_t *gameMemory)
{
    auto             winInfo   = ae::platform::getWindowInfo();
    game_state_t    *gd        = getGameState(gameMemory);
    const ae::user_input_t &userInput = ae::platform::getUserInput();

    float speed = 5.f * ae::timing::lastFrameVisibleTime;

    static bool             bSpin               = true;
    static bool             lockCamYaw          = false;
    static bool             lockCamPitch        = false;
    static float            ambientStrength     = 0.1f;
    static float            specularStrength    = 0.5f;
    static ae::math::vec4_t lightColor          = {1, 1, 1, 1};
    static ae::math::vec3_t lightPos            = {0, 1, 0};
    static float            cameraSensitivity   = 3.0f;
    static bool             optInFirstPersonCam = false;

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
        bool static bFocusedLastFrame = true;  // assume for first frame.
        const bool bExitingFocus      = bFocusedLastFrame && !ae::platform::isWindowFocused();

        if (userInput.mouseRBttnDown[0] || userInput.mouseRBttnDown[1]) {
            // exit GUI.
            if (!optInFirstPersonCam) ae::platform::showMouse(false);
            optInFirstPersonCam = true;
        }

        if ((userInput.keyDown[0][ae::GAME_KEY_ESCAPE] || userInput.keyDown[1][ae::GAME_KEY_ESCAPE]) || bExitingFocus) {
            // enter GUI.
            if (optInFirstPersonCam) ae::platform::showMouse(true);
            optInFirstPersonCam = false;
        }

        bFocusedLastFrame = ae::platform::isWindowFocused();
    }

    if (optInFirstPersonCam) {
        // clamp mouse cursor.
        ae::platform::setMousePos((int)(winInfo.width / 2.0f), (int)(winInfo.height / 2.0f));
    }

    const float fullTimeStep = ae::timing::lastFrameVisibleTime * 0.5f;

    static float lastDeltaX[2] = {0.f, 0.f};
    static float lastDeltaY[2] = {0.f, 0.f};

    auto simulateWorldStep = [&](uint32_t                inputIdx,
                                 float                   timeStep,
                                 const ae::math::vec3_t &beginPosVector,
                                 const ae::math::vec3_t &beginEulerAngles,
                                 ae::math::vec3_t       *pEndPosVector,
                                 ae::math::vec3_t       *pEndEulerAngles) {
        *pEndPosVector = beginPosVector;
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
            
            yaw   = ae::math::atan2(deltaX, gd->cam.nearPlane) * rotationFactor;
            float pitch = ae::math::atan2(deltaY, gd->cam.nearPlane) * rotationFactor;

            *pEndEulerAngles += ae::math::vec3_t(0.0f, yaw, 0.0f);
            *pEndEulerAngles += ae::math::vec3_t(-pitch, 0.0f, 0.0f);
        }

        // clamp camera pitch
        float pitchClamp = PI / 2.0f - 0.01f;
        if (pEndEulerAngles->x < -pitchClamp) pEndEulerAngles->x = -pitchClamp;
        if (pEndEulerAngles->x > pitchClamp) pEndEulerAngles->x = pitchClamp;

        float movementSpeed = 5.f;
        float linearStep = movementSpeed * timeStep;
        
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
            }
            else if (userInput.keyDown[inputIdx][ae::GAME_KEY_SPACE]) {
                movDir += ae::math::vec3_t(0.0f, 1, 0.0f);
            }
            
            // NOTE: the camera is rotating smoothly. the below math is the result
            // of doing the integral for that circular motion.
            auto movDirNorm = ae::math::normalize(movDir);

            if (ae::math::abs(yaw) >= 0.0001f)
            {
                auto  t0       = movDirNorm * movementSpeed * 1.414f;  // original velocity vector.
                float oneOverW = timeStep / yaw;

                float r_x =
                    pEndPosVector->x + oneOverW * (t0.x * ae::math::sin(yaw) - t0.z * ae::math::cos(yaw) + t0.z);
                float r_z =
                    pEndPosVector->z + oneOverW * (t0.z * ae::math::sin(yaw) + t0.x * ae::math::cos(yaw) - t0.x);

                pEndPosVector->x = r_x;
                pEndPosVector->z = r_z;
            }
            else
            {
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

    if (bSpin)
        gd->suzanneTransform.eulerAngles += ae::math::vec3_t(0.0f, 2.0f * ae::timing::lastFrameVisibleTime, 0.0f);

    // TODO: look into the depth testing stuff more deeply on the hardware side of things.
    // what is something that we can only do because we really get it?

    ae::super::updateAndRender(gameMemory);

    MonkeyDemoRender(gameMemory);
}

// TODO: for any AE callbacks that the game doesn't care to define, don't make it a requirement
// to still have the function.
void ae::OnVoiceBufferEnd(game_memory_t *gameMemory, intptr_t voiceHandle) {}
void ae::OnVoiceBufferProcess(game_memory_t *gameMemory,
    intptr_t                                 voiceHandle,
    float                                   *dst,
    float                                   *src,
    uint32_t                                 samplesToWrite,
    int                                      channels,
    int                                      bytesPerSample)
{}
void ae::HandleWindowResize(game_memory_t *gameMemory, int newWidth, int newHeight) {}
