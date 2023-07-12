void MonkeyDemoRender(ae::game_memory_t *gameMemory); // forward declare. to define for GFX API backend specific version.
void MonkeyDemoUpdate(ae::game_memory_t *gameMemory);

static game_state_t *getGameState(ae::game_memory_t *gameMemory) { return (game_state_t *)gameMemory->data; }

void ae::PreInit(game_memory_t *gameMemory)
{
    ae::defaultWinProfile = AUTOMATA_ENGINE_WINPROFILE_NORESIZE;
    ae::defaultWindowName = "MonkeyDemo";
}

static void MonkeyDemoInit(ae::game_memory_t *gameMemory)
{
    game_state_t *gd      = getGameState(gameMemory);
    auto          winInfo = ae::platform::getWindowInfo();

    *gd = {}; // zero it out.

    gd->cam.trans.scale              = ae::math::vec3_t(1.0f, 1.0f, 1.0f);
    gd->cam.fov                      = 90.0f;
    gd->cam.nearPlane                = 0.01f;
    gd->cam.farPlane                 = 1000.0f;
    gd->cam.width                    = winInfo.width;
    gd->cam.height                   = winInfo.height;
    gd->suzanneTransform.scale       = ae::math::vec3_t(1.0f, 1.0f, 1.0f);
    gd->suzanneTransform.pos         = ae::math::vec3_t(0.0f, 0.0f, -3.0f);
    gd->suzanneTransform.eulerAngles = {};

    ae::bifrost::registerApp("spinning_monkey", MonkeyDemoUpdate);
    ae::setUpdateModel(ae::AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC);
}

static void MonkeyDemoUpdate(ae::game_memory_t *gameMemory)
{
    auto             winInfo   = ae::platform::getWindowInfo();
    game_state_t    *gd        = getGameState(gameMemory);
    ae::user_input_t userInput = {};
    ae::platform::getUserInput(&userInput);

    float speed = 5.f * ae::timing::lastFrameVisibleTime;

    static bool             bSpin               = true;
    static bool             lockCamYaw          = false;
    static bool             lockCamPitch        = false;
    static float            ambientStrength     = 0.1f;
    static float            specularStrength    = 0.5f;
    static ae::math::vec4_t lightColor          = {1, 1, 1, 1};
    static ae::math::vec3_t lightPos            = {0, 1, 0};
    static float            cameraSensitivity   = 1.0f;
    static bool             optInFirstPersonCam = false;

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
    ImGui::Begin("MonkeyDemo");

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
    ImGui::Checkbox("bSpin", &bSpin);
    ImGui::Checkbox("lockCamYaw", &lockCamYaw);
    ImGui::Checkbox("lockCamPitch", &lockCamPitch);
    ImGui::SliderFloat("ambientStrength", &ambientStrength, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("specularStrength", &specularStrength, 0.0f, 1.0f, "%.3f");
    ImGui::ColorPicker4("lightColor", &lightColor[0]);
    ImGui::InputFloat3("lightPos", &lightPos[0]);
    ImGui::SliderFloat("cameraSensitivity", &cameraSensitivity, 1, 10);

    ImGui::Text("userInput.deltaMouseX: %d", userInput.deltaMouseX);
    ImGui::Text("userInput.deltaMouseY: %d", userInput.deltaMouseY);

    ae::ImGuiRenderMat4("camProjMat", buildProjMatForVk(gd->cam));
    ae::ImGuiRenderMat4("camViewMat", buildViewMat(gd->cam));
    ae::ImGuiRenderMat4((char *)(std::string(gd->suzanne.modelName) + "Mat").c_str(),
        ae::math::buildMat4fFromTransform(gd->suzanneTransform));
    ae::ImGuiRenderVec3("camPos", gd->cam.trans.pos);
    ae::ImGuiRenderVec3((char *)(std::string(gd->suzanne.modelName) + "Pos").c_str(), gd->suzanneTransform.pos);
    ImGui::End();
#endif

    ae::math::mat3_t camBasis =
        ae::math::mat3_t(buildRotMat4(ae::math::vec3_t(0.0f, gd->cam.trans.eulerAngles.y, 0.0f)));
    if (userInput.keyDown[ae::GAME_KEY_W]) { gd->cam.trans.pos += camBasis * ae::math::vec3_t(0.0f, 0.0f, -speed); }
    if (userInput.keyDown[ae::GAME_KEY_A]) { gd->cam.trans.pos += camBasis * ae::math::vec3_t(-speed, 0.0f, 0.0f); }
    if (userInput.keyDown[ae::GAME_KEY_S]) { gd->cam.trans.pos += camBasis * ae::math::vec3_t(0.0f, 0.0f, speed); }
    if (userInput.keyDown[ae::GAME_KEY_D]) { gd->cam.trans.pos += camBasis * ae::math::vec3_t(speed, 0.0f, 0.0f); }
    if (userInput.keyDown[ae::GAME_KEY_SHIFT]) { gd->cam.trans.pos += ae::math::vec3_t(0.0f, -speed, 0.0f); }
    if (userInput.keyDown[ae::GAME_KEY_SPACE]) { gd->cam.trans.pos += ae::math::vec3_t(0.0f, speed, 0.0f); }

    bool static bFocusedLastFrame = true;  // assume for first frame.
    const bool bExitingFocus      = bFocusedLastFrame && !ae::platform::isWindowFocused();

    // check if opt in.
    if (userInput.mouseRBttnDown) {
        // exit GUI.
        if (!optInFirstPersonCam) ae::platform::showMouse(false);
        optInFirstPersonCam = true;
    }

    if (userInput.keyDown[ae::GAME_KEY_ESCAPE] || bExitingFocus) {
        // enter GUI.
        if (optInFirstPersonCam) ae::platform::showMouse(true);
        optInFirstPersonCam = false;
    }

    bFocusedLastFrame = ae::platform::isWindowFocused();

    if (optInFirstPersonCam) {
        // we'll assume that this is in pixels.
        float deltaX = userInput.deltaMouseX * cameraSensitivity;
        float deltaY = userInput.deltaMouseY * cameraSensitivity;

        if (lockCamYaw) deltaX = 0.f;
        if (lockCamPitch) deltaY = 0.f;

        float r = tanf(gd->cam.fov * DEGREES_TO_RADIANS / 2.0f) * gd->cam.nearPlane;

        deltaX *= r / (winInfo.width * 0.5f);
        deltaY *= r / (winInfo.height * 0.5f);

        float yaw   = ae::math::atan2(deltaX, gd->cam.nearPlane);
        float pitch = ae::math::atan2(deltaY, gd->cam.nearPlane);

        gd->cam.trans.eulerAngles += ae::math::vec3_t(0.0f, yaw, 0.0f);
        gd->cam.trans.eulerAngles += ae::math::vec3_t(-pitch, 0.0f, 0.0f);

        // clamp mouse cursor.
        ae::platform::setMousePos((int)(winInfo.width / 2.0f), (int)(winInfo.height / 2.0f));
    }

    // clamp camera pitch
    float pitchClamp = PI / 2.0f - 0.01f;
    if (gd->cam.trans.eulerAngles.x < -pitchClamp) gd->cam.trans.eulerAngles.x = -pitchClamp;
    if (gd->cam.trans.eulerAngles.x > pitchClamp) gd->cam.trans.eulerAngles.x = pitchClamp;

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
