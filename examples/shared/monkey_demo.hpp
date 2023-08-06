// backend GFX API specific versions.
void MonkeyDemoRender(
    ae::game_memory_t *gameMemory);
void MonkeyDemoUpdate(ae::game_memory_t *gameMemory);
void MonkeyDemoHotload(ae::game_memory_t *gameMemory);

static game_state_t *getGameState(ae::game_memory_t *gameMemory) { return (game_state_t *)gameMemory->data; }

// NOTE: this is called every time that the game DLL is hot-loaded.
DllExport void GameOnHotload(ae::game_memory_t *gameMemory)
{
    ae::engine_memory_t *EM = gameMemory->pEngineMemory;

    // TODO: we might be able to merge these two calls?
    // for now, we need to ensure that we set the engine context
    // before we make the call to init module globals.
    ae::setEngineContext(EM);

    // NOTE: this inits the automata engine module globals.
    ae::initModuleGlobals();

    // NOTE: ImGui has global state. therefore, we need to make sure
    // that we use the same state that the engine is using.
    // there are different global states since both the engine and the game DLL statically link to ImGui.
    ImGui::SetCurrentContext(EM->pfn.imguiGetCurrentContext());
    ImGuiMemAllocFunc allocFunc;
    ImGuiMemFreeFunc  freeFunc;
    void *            userData;
    EM->pfn.imguiGetAllocatorFunctions(&allocFunc, &freeFunc, &userData);
    ImGui::SetAllocatorFunctions(allocFunc, freeFunc, userData);

    // NOTE: we write the app table on each hotload since any pointers to the functions
    // within the DLL from before are no longer valid.
    ae::bifrost::clearAppTable(gameMemory);
    ae::bifrost::registerApp(gameMemory, AUTOMATA_ENGINE_PROJECT_NAME, MonkeyDemoUpdate);

    // load the function pointers for the gfx API.
    MonkeyDemoHotload(gameMemory);
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
    auto                 winInfo = EM->pfn.getWindowInfo(false);

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
    gd->cameraSensitivity   = 2.5f;
    gd->optInFirstPersonCam = false;
    gd->bFocusedLastFrame   = true;  // assume for first frame.
}

// NOTE: the idea is that the game is some function f(x), where x is the input signal.
// this HandleInput function will be called as fast as possible so that the game can chose
// to update the game f(x) with granular steps if needed.
DllExport void GameHandleInput(ae::game_memory_t *gameMemory)
{
    ae::engine_memory_t *EM = gameMemory->pEngineMemory;
    game_state_t        *gd = getGameState(gameMemory);

    // TODO: we should be using std::atomic with optInFirstPersonCam.
    // the same goes for cam. indeed, the same goes for any data that is written by this function
    // and accessed also by the main update+render loop.
    //
    // likewise, the same goes for the ImGui panel settings below.
    // these are read here, but modified in the other thread. we could read half-written data.

    // ImGui panel settings.
    const bool &            lockCamYaw          = gd->lockCamYaw;
    const bool &            lockCamPitch        = gd->lockCamPitch;
    const float &           cameraSensitivity   = gd->cameraSensitivity;

    bool               &optInFirstPersonCam = gd->optInFirstPersonCam;
    ae::math::camera_t &cam                 = gd->cam;
    bool               &bFocusedLastFrame   = gd->bFocusedLastFrame;

    auto                    winInfo   = EM->pfn.getWindowInfo(true);

    // NOTE: the user input to handle is stored within the engine memory.
    // whenever this is called from the engine, the input is "dirty" and we ought to
    // handle it.
    const ae::user_input_t &userInput = EM->userInput;

    // TODO: can we call showMouse less often?

    auto enterGui = [&]() {
        EM->pfn.showMouse(true);
        optInFirstPersonCam = false;
    };

    auto exitGui = [&]() {
        EM->pfn.showMouse(false);
        optInFirstPersonCam = true;
    };

    // check if opt in.
    {
        if (userInput.mouseRBttnDown) { exitGui(); }

        if (userInput.keyDown[ae::GAME_KEY_ESCAPE]) { enterGui(); }

        {
            const bool isFocused     = winInfo.isFocused;
            const bool bExitingFocus = bFocusedLastFrame && !isFocused;

            if (bExitingFocus) { enterGui(); }

            bFocusedLastFrame = isFocused;
        }
    }

    // check for toggle the ImGui overlay.
    {
        bool &lastFrameF5 = gd->lastFrameF5;
        if (userInput.keyDown[ae::GAME_KEY_F5] && !lastFrameF5) {
            bool prevVal = EM->g_renderImGui.load();
            EM->g_renderImGui.store(!prevVal);  // = !;

            (!prevVal) ? enterGui() : exitGui();
        }
        lastFrameF5 = userInput.keyDown[ae::GAME_KEY_F5];
    }

    auto beginEulerAngles = cam.trans.eulerAngles;

    float yaw = 0.f;
    if (optInFirstPersonCam) {

        // we'll assume that this is in pixels.
        float deltaX = userInput.deltaMouseX;
        float deltaY = userInput.deltaMouseY;

        if (lockCamYaw) deltaX = 0.f;
        if (lockCamPitch) deltaY = 0.f;

        float r = ae::math::tan(cam.fov * DEGREES_TO_RADIANS / 2.0f) * cam.nearPlane;
        float t = r * (float(cam.height) / cam.width);

        deltaX *= r / (cam.width * 0.5f);
        deltaY *= t / (cam.height * 0.5f);

        yaw         = ae::math::atan2(deltaX, cam.nearPlane);
        float pitch = ae::math::atan2(deltaY, cam.nearPlane);

        float lerpT = 0.9f;
        cam.angularVelocity =
            ae::math::vec3_t(-pitch, yaw, 0.0f) * cameraSensitivity * (1 - lerpT) + cam.angularVelocity * (lerpT);
        cam.trans.eulerAngles += cam.angularVelocity;
    }

    // clamp camera pitch
    float pitchClamp = PI / 2.0f - 0.01f;
    if (cam.trans.eulerAngles.x < -pitchClamp) cam.trans.eulerAngles.x = -pitchClamp;
    if (cam.trans.eulerAngles.x > pitchClamp) cam.trans.eulerAngles.x = pitchClamp;

    float movementSpeed = 5.f;
    float linearStep    = movementSpeed;

    // NOTE: so there is this idea where if we consider the exact motion that the player would
    // take as the camera rotates, we realize that it's not a straight line. its instead an arc.
    // however, since this update is called quite a bit, we can safely approximate that arc with
    // a set of straight lines.
    ae::math::mat3_t camBasisBegin =
        ae::math::mat3_t(ae::math::buildRotMat4(ae::math::vec3_t(0.0f, beginEulerAngles.y, 0.0f)));

    {
        ae::math::vec3_t movDir = ae::math::vec3_t();
        
        if (userInput.keyDown[ae::GAME_KEY_W]) {
            movDir += camBasisBegin * ae::math::vec3_t(0.0f, 0.0f, -1);
        }

        if (userInput.keyDown[ae::GAME_KEY_S]) {
            movDir += camBasisBegin * ae::math::vec3_t(0.0f, 0.0f, 1);
        }
        
        if (userInput.keyDown[ae::GAME_KEY_A]) {
            movDir += camBasisBegin * ae::math::vec3_t(-1, 0.0f, 0.0f);
        }

        if (userInput.keyDown[ae::GAME_KEY_D]) {
            movDir += camBasisBegin * ae::math::vec3_t(1, 0.0f, 0.0f);
        }

        if (userInput.keyDown[ae::GAME_KEY_SHIFT]) {
            movDir += ae::math::vec3_t(0.0f, -1, 0.0f);
        }

        if (userInput.keyDown[ae::GAME_KEY_SPACE]) {
            movDir += ae::math::vec3_t(0.0f, 1, 0.0f);
        }

        auto movDirNorm = ae::math::normalize(movDir);

        float lerpT = 0.9f;
        cam.velocity = movDirNorm * linearStep * (1-lerpT) + cam.velocity * lerpT;
        cam.trans.pos += cam.velocity * userInput.packetLiveTime;
    }
}

static void MonkeyDemoUpdate(ae::game_memory_t *gameMemory)
{
    game_state_t *gd = getGameState(gameMemory);

    if (!gameMemory->getInitialized()) return;

    // TODO: again, need to be using std::atomic here.

    // sample game for rendering.
    ae::math::camera_t cam_Snapshot = gd->cam;
    bool optInFirstPersonCam_Snapshot = gd->optInFirstPersonCam;

    ae::engine_memory_t *   EM        = gameMemory->pEngineMemory;
    auto                    winInfo   = EM->pfn.getWindowInfo(false);

    bool             &bSpin               = gd->bSpin;
    bool             &lockCamYaw          = gd->lockCamYaw;
    bool             &lockCamPitch        = gd->lockCamPitch;
    float            &ambientStrength     = gd->ambientStrength;
    float            &specularStrength    = gd->specularStrength;
    ae::math::vec4_t &lightColor          = gd->lightColor;
    ae::math::vec3_t &lightPos            = gd->lightPos;
    float            &cameraSensitivity   = gd->cameraSensitivity;

    bool bRenderImGui = EM->bCanRenderImGui;

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)

    if (bRenderImGui) {
        ImGui::Begin(AUTOMATA_ENGINE_PROJECT_NAME);

        ImGui::Text(
            "---CONTROLS---\n"
            "F5 to toggle the GUI\n"
            "WASD to move\n"
            "Right click to enter first person cam.\n"
            "ESC to exit first person cam.\n"
            "SPACE to fly up\n"
            "SHIFT to fly down\n\n");

        // inputs.
#if !defined(AUTOMATA_ENGINE_VK_BACKEND)
        ImGui::Checkbox("debugRenderDepth", &gd->debugRenderDepthFlag);
#endif

        ImGui::Text("---CAMERA---\n");
        ImGui::Checkbox("lock yaw", &lockCamYaw);
        ImGui::Checkbox("lock pitch", &lockCamPitch);
        ImGui::SliderFloat("camera sensitivity", &cameraSensitivity, 1, 10);

        ImGui::Text("\n---SCENE---\n");
        ImGui::InputFloat3("sun position", &lightPos[0]);
        ImGui::ColorPicker4("sun color", &lightColor[0]);
        ImGui::SliderFloat("ambient light", &ambientStrength, 0.0f, 1.0f, "%.3f");

        ImGui::Text("\n---MONKEY---\n");
        ImGui::Checkbox("make it spin", &bSpin);
        ImGui::SliderFloat("make it shiny", &specularStrength, 0.0f, 1.0f, "%.3f");

        ImGui::End();
    }
#endif

    if (optInFirstPersonCam_Snapshot) {
        // clamp mouse cursor.
        EM->pfn.setMousePos((int)(winInfo.width / 2.0f), (int)(winInfo.height / 2.0f));
    }

    // NOTE: the monkey spinning is not game state that needs to be updated at a high granularity. therefore we can update it here
    // in the main update loop. and indeed, it's correct here to use lastFrameVisibleTime for the timing.
    if (bSpin) gd->suzanneTransform.eulerAngles += ae::math::vec3_t(0.0f, 2.0f * EM->timing.lastFrameVisibleTime, 0.0f);

    // TODO: look into the depth testing stuff more deeply on the hardware side of things.
    // what is something that we can only do because we really get it?

    ae::super::updateAndRender(gameMemory);

    MonkeyDemoRender(gameMemory);
}

// TODO: so for the monkey demo and before ship, what is left?

// want to get rid of the white stuff at the beginning.
// and maybe we want to fix the bug where things crash on window minimize.
// also fix the fact that we fix things to 60hz, but someone can have a different refresh rate monitor (e.g. Dan, he can playtest :>)
//

// this is all that I can think of for now.

