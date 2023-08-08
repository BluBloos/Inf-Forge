// backend GFX API specific versions.
void MonkeyDemoRender(
    ae::game_memory_t *gameMemory);
void MonkeyDemoUpdate(ae::game_memory_t *gameMemory);
void MonkeyDemoHotload(ae::game_memory_t *gameMemory);

static game_state_t *getGameState(ae::game_memory_t *gameMemory) { return (game_state_t *)gameMemory->data; }

typedef unsigned int u32;

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
    auto                 winInfo = EM->pfn.getWindowInfo(false);

    // default initialize.
    game_state_t *gd = new (gameMemory->data) game_state_t();

    ae::math::camera_t initCamData = {};
    initCamData.trans.scale        = ae::math::vec3_t(1.0f, 1.0f, 1.0f);
    initCamData.fov                = 90.0f;
    initCamData.nearPlane          = 0.01f;
    initCamData.farPlane           = 100.0f;
    initCamData.width              = winInfo.width;
    initCamData.height             = winInfo.height;

    gd->cam     = (initCamData);

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
    gd->cameraSensitivity   = 1.f;
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

    // ImGui panel settings (we just read these, but they are written to from the other thread).
    const bool  lockCamYaw        = gd->lockCamYaw.load();
    const bool  lockCamPitch      = gd->lockCamPitch.load();
    const float cameraSensitivity = gd->cameraSensitivity.load();

    std::atomic<bool>               &optInFirstPersonCam = gd->optInFirstPersonCam;

    bool               local_optInFirstPersonCam = optInFirstPersonCam.load();

    // modified by only this thread.
    bool &bFocusedLastFrame = gd->bFocusedLastFrame;

    auto winInfo = EM->pfn.getWindowInfo(true);

    // NOTE: the user input to handle is stored within the engine memory.
    // whenever this is called from the engine, the input is "dirty" and we ought to
    // handle it.
    const ae::user_input_t &userInput = EM->userInput;

    // TODO: can we call showMouse less often?

    auto enterGui = [&]() {
        EM->pfn.showMouse(true);
        local_optInFirstPersonCam = false;
    };

    auto exitGui = [&]() {
        EM->pfn.showMouse(false);
        local_optInFirstPersonCam = true;
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

    //NOTE: there would be a concern where requestClearTransitionCounts will be set to false,
    //causing us to miss a reset. but that case is silly. this thread is going very fast,
    //while the other thread is going much slower in comparison. therefore, we'll handle
    //the reset faster than the reset requests can come in. 
    if (gd->requestClearTransitionCounts)
    {
        gd->halfTransitionCount_W = 0;
        gd->halfTransitionCount_A = 0;
        gd->halfTransitionCount_S = 0;
        gd->halfTransitionCount_D = 0;
        gd->halfTransitionCount_shift = 0;
        gd->halfTransitionCount_space = 0;

        gd->requestClearTransitionCounts = false;

        gd->deltaMouseX.store(0.f);
        gd->deltaMouseY.store(0.f);
    }

    // accumulate the raw mouse input.
    float deltaX = userInput.rawDeltaMouseX + gd->deltaMouseX.load();
    float deltaY = userInput.rawDeltaMouseY + gd->deltaMouseY.load();

    gd->deltaMouseX.store(deltaX);
    gd->deltaMouseY.store(deltaY);

    // TODO: maybe many games want to do this so we can move this to the Inf-Forge side.
    // check for the user input keys.
    if ( gd->state_W != userInput.keyDown[ae::GAME_KEY_W] ) 
    {
        gd->halfTransitionCount_W++;
        gd->state_W = userInput.keyDown[ae::GAME_KEY_W];
    }

    if ( gd->state_A != userInput.keyDown[ae::GAME_KEY_A] ) 
    {
        gd->halfTransitionCount_A++;
        gd->state_A = userInput.keyDown[ae::GAME_KEY_A];
    }

    if ( gd->state_S != userInput.keyDown[ae::GAME_KEY_S] ) 
    {
        gd->halfTransitionCount_S++;
        gd->state_S = userInput.keyDown[ae::GAME_KEY_S];
    }

    if ( gd->state_D != userInput.keyDown[ae::GAME_KEY_D] ) 
    {
        gd->halfTransitionCount_D++;
        gd->state_D = userInput.keyDown[ae::GAME_KEY_D];
    }

    if ( gd->state_shift != userInput.keyDown[ae::GAME_KEY_SHIFT] ) 
    {
        gd->halfTransitionCount_shift++;
        gd->state_shift = userInput.keyDown[ae::GAME_KEY_SHIFT];
    }

    if ( gd->state_space != userInput.keyDown[ae::GAME_KEY_SPACE] ) 
    {
        gd->halfTransitionCount_space++;
        gd->state_space = userInput.keyDown[ae::GAME_KEY_SPACE];
    }

    optInFirstPersonCam.store(local_optInFirstPersonCam);
}

static void MonkeyDemoUpdate(ae::game_memory_t *gameMemory)
{
    game_state_t *gd = getGameState(gameMemory);

    if (!gameMemory->getInitialized()) return;

    // NOTE: these are read by us and written from the handle input thread.
    bool optInFirstPersonCam_Snapshot = gd->optInFirstPersonCam.load();

    ae::engine_memory_t *EM      = gameMemory->pEngineMemory;
    auto                 winInfo = EM->pfn.getWindowInfo(false);

    // NOTE: these are thread local variables.
    bool             &bSpin            = gd->bSpin;
    float            &ambientStrength  = gd->ambientStrength;
    float            &specularStrength = gd->specularStrength;
    ae::math::vec4_t &lightColor       = gd->lightColor;
    ae::math::vec3_t &lightPos         = gd->lightPos;

    // NOTE: these are written by us and read by the handle input thread.
    std::atomic<bool>  &lockCamYaw        = gd->lockCamYaw;
    std::atomic<bool>  &lockCamPitch      = gd->lockCamPitch;
    std::atomic<float> &cameraSensitivity = gd->cameraSensitivity;

    bool bRenderImGui = EM->bCanRenderImGui;

    constexpr float drag = 10.f;
    constexpr float movementSpeed = 80.f;

    constexpr float angularDrag = 0.5f;
    constexpr float angularSpeed = 0.05f;

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)

    if (bRenderImGui) {
        float local_cameraSensitivity = cameraSensitivity.load();
        bool  local_lockCamYaw        = lockCamYaw.load();
        bool  local_lockCamPitch      = lockCamPitch.load();

        ImGui::Begin(AUTOMATA_ENGINE_PROJECT_NAME);

        ImGui::Text(
            "---CONTROLS---\n"
            "F5 to toggle the GUI.\n"
            "WASD to move.\n"
            "Right click to enter first person cam.\n"
            "ESC to exit first person cam.\n"
            "SPACE to fly up.\n"
            "SHIFT to fly down.\n\n");

        // inputs.
#if !defined(AUTOMATA_ENGINE_VK_BACKEND)
        ImGui::Checkbox("debugRenderDepth", &gd->debugRenderDepthFlag);
#endif

        ImGui::Text("---CAMERA---\n");
        ImGui::Checkbox("lock yaw", &local_lockCamYaw);
        ImGui::Checkbox("lock pitch", &local_lockCamPitch);
        ImGui::SliderFloat("camera sensitivity", &local_cameraSensitivity, 1, 10);

        ImGui::Text("\n---SCENE---\n");
        ImGui::InputFloat3("sun position", &lightPos[0]);
        ImGui::ColorPicker4("sun color", &lightColor[0]);
        ImGui::SliderFloat("ambient light", &ambientStrength, 0.0f, 1.0f, "%.3f");

        ImGui::Text("\n---MONKEY---\n");
        ImGui::Text("model name: %s", gd->suzanne.modelName);
        ImGui::Checkbox("make it spin", &bSpin);
        ImGui::SliderFloat("make it shiny", &specularStrength, 0.0f, 1.0f, "%.3f");

        ImGui::End();

        cameraSensitivity.store(local_cameraSensitivity);
        lockCamYaw.store(local_lockCamYaw);
        lockCamPitch.store(local_lockCamPitch);
    }
#endif

    if (optInFirstPersonCam_Snapshot) {
        // clamp mouse cursor.
        EM->pfn.setMousePos((int)(winInfo.width / 2.0f), (int)(winInfo.height / 2.0f));
    }

    // NOTE: the monkey spinning is not game state that needs to be updated at a high granularity. therefore we can update it here
    // in the main update loop. and indeed, it's correct here to use lastFrameVisibleTime for the timing.
    if (bSpin) gd->suzanneTransform.eulerAngles += ae::math::vec3_t(0.0f, 2.0f * EM->timing.lastFrameVisibleTime, 0.0f);

    // update the player camera.
    {
        auto beginEulerAngles = gd->cam.trans.eulerAngles;

        // TODO: consider once again updating the path of the camera to be smooth when there is also a
        // rotation of the camera occur?
        
        // update camera rotation.
        {
            // update position based on velocity.
            gd->cam.trans.eulerAngles += gd->cam.angularVelocity;

            // clamp camera pitch
            float pitchClamp = PI / 2.0f - 0.01f;
            if (gd->cam.trans.eulerAngles.x < -pitchClamp) gd->cam.trans.eulerAngles.x = -pitchClamp;
            if (gd->cam.trans.eulerAngles.x > pitchClamp) gd->cam.trans.eulerAngles.x = pitchClamp;

            // update velocity.
            // gd->cam.velocity += (movDirNorm * linearStep - gd->cam.velocity * drag) * dt;

            float deltaX = (optInFirstPersonCam_Snapshot) ? gd->deltaMouseX.load() : 0.f;
            float deltaY = (optInFirstPersonCam_Snapshot) ? gd->deltaMouseY.load() : 0.f;

            float rotationSpeed = cameraSensitivity * angularSpeed * DEGREES_TO_RADIANS;

            if (lockCamYaw) deltaX = 0.f;
            if (lockCamPitch) deltaY = 0.f;

            float yaw   = deltaX * rotationSpeed;
            float pitch = deltaY * rotationSpeed;

            gd->cam.angularVelocity += ae::math::vec3_t(-pitch, yaw, 0.0f) - gd->cam.angularVelocity * angularDrag;
        }

        float linearStep    = movementSpeed;

        u32 hw = gd->halfTransitionCount_W;
        u32 ha = gd->halfTransitionCount_A;
        u32 hs = gd->halfTransitionCount_S;
        u32 hd = gd->halfTransitionCount_D;
        u32 h_space = gd->halfTransitionCount_space;
        u32 h_shift = gd->halfTransitionCount_shift;

        gd->requestClearTransitionCounts = true;

        bool sw = gd->state_W;
        bool sa = gd->state_A;
        bool ss = gd->state_S;
        bool sd = gd->state_D;
        bool s_space = gd->state_space;
        bool s_shift = gd->state_shift;

        ae::math::mat3_t camBasisBegin =
            ae::math::mat3_t(ae::math::buildRotMat4(ae::math::vec3_t(0.0f, beginEulerAngles.y, 0.0f)));

        {
            ae::math::vec3_t movDir = ae::math::vec3_t();

            auto ratio_for_even_half_count = [](u32 h, u32 s) -> float {
                u32 top=h>>1;
                u32 bottom=h+1;
                if (s) top = bottom - top;
                return float(top)/float(bottom);
            };

            float w_factor = -1.f * ((hw % 2 != 0) ? 0.5f : ratio_for_even_half_count(hw, sw) );
            movDir += camBasisBegin * ae::math::vec3_t(0.0f, 0.0f, w_factor );

            float s_factor = 1.f * ((hs % 2 != 0) ? 0.5f : ratio_for_even_half_count(hs, ss) );
            movDir += camBasisBegin * ae::math::vec3_t(0.0f, 0.0f, s_factor);

            float a_factor = -1.f * ((ha % 2 != 0) ? 0.5f : ratio_for_even_half_count(ha, sa) );
            movDir += camBasisBegin * ae::math::vec3_t(a_factor, 0.0f, 0.f);

            float d_factor = 1.f * ((hd % 2 != 0) ? 0.5f : ratio_for_even_half_count(hd, sd) );
            movDir += camBasisBegin * ae::math::vec3_t(d_factor, 0.0f, 0.f);

            float shift_factor = -1.f * ((h_shift % 2 != 0) ? 0.5f : ratio_for_even_half_count(h_shift, s_shift) );
            movDir += camBasisBegin * ae::math::vec3_t(0.f, shift_factor, 0.f);

            float space_factor = 1.f * ((h_space % 2 != 0) ? 0.5f : ratio_for_even_half_count(h_space, s_space) );
            movDir += camBasisBegin * ae::math::vec3_t(0.f, space_factor, 0.f);

            auto movDirNorm = ae::math::normalize(movDir);

            float dt = EM->timing.lastFrameVisibleTime;

            // update position based on velocity.
            gd->cam.trans.pos += gd->cam.velocity * dt;

            // update velocity.
            gd->cam.velocity += (movDirNorm * linearStep - gd->cam.velocity * drag) * dt;
        }

    }


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

