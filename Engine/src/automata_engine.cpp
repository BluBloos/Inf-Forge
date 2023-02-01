#include <automata_engine.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" 

#include <windows.h>
#include <win32_engine.h>

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
#include "imgui.h"
#endif

namespace automata_engine {

    bool super::g_renderImGui = true;

    // if both are UINT32_MAX, window maximizes. else it uses the OS defined default
    // dim if UINT32_MAX.
    int32_t defaultWidth = UINT32_MAX;
    int32_t defaultHeight = UINT32_MAX;

    game_window_profile_t defaultWinProfile = AUTOMATA_ENGINE_WINPROFILE_RESIZE;
    const char *defaultWindowName = "Automata Engine";

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
    void ImGuiRenderVec3(char *vecName, math::vec3_t vec) {
        ImGui::Text(vecName);
        if (ImGui::BeginTable(vecName, 3)) {
            float vals[] = {vec.x, vec.y, vec.z};
            for (int i = 0; i < 3; i++) {
                ImGui::TableNextColumn();
                ImGui::Value("", vals[i]);    
            }
            ImGui::EndTable();
        }
    }

    void ImGuiRenderMat4(char *matName, math::mat4_t mat) {
        ImGui::Text(matName);
        if (ImGui::BeginTable(matName, 4)) {
            // Diaply the matrix back in ImGUI.
            // NOTE(Noah): We remark that we are using column-major order.
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    ImGui::TableNextColumn();
                    ImGui::Value("", mat.mat[x][y]);
                }
            }
            ImGui::EndTable();
        }
    }
#endif

    static update_model_t updateModel;
    void setUpdateModel(update_model_t newModel) {
        updateModel = newModel;
    }
    loaded_image_t platform::stbImageLoad(char *fileName) {
        loaded_image_t myImage = {};
        int x, y, n;
        // NOTE(Noah): For now, let's avoid .jpg.
        // seems stb image loader has troubles with a subset of .jpg,
        // and I would rather not put any effort into determining precisely
        // which .jpg I have.
        unsigned char *data = stbi_load(fileName, &x, &y, &n, 0);
        if (data != NULL) {
            myImage.pixelPointer = (uint32_t *)data;
            myImage.width = x;
            myImage.height = y;   
        } else {
            // TODO(Noah): Do something intelligent in the case of failure here.
            PlatformLoggerError("automata_engine::platform::stbImageLoad failed");
            assert(false);
        }
        return myImage;
    }
    void setGlobalRunning(bool newVal) {
        platform::GLOBAL_RUNNING = newVal;
    }
    void setFatalExit() {
        platform::GLOBAL_PROGRAM_RESULT = -1;
        platform::GLOBAL_RUNNING = false;
    }
    game_state_t *getGameState(game_memory_t *gameMemory) {
        return (game_state_t *)gameMemory->data;
    }
    // TODO(Noah): Swap this appTable out for something more performant
    // like a hash table. OR, could do the other design pattern where we map
    // some index to names. We just O(1) lookup...
    //
    // it's also probably the case that strcmp will return true if we have a
    // substring match? Please ... fix this code.
    typedef void (*f_ptr_t)(game_memory_t *); 
    static uint32_t _currentApp = 0;
    typedef struct {
        f_ptr_t updateFunc;
        f_ptr_t transitionInto;
        f_ptr_t transitionOut;
    } bifrost_app_t;
    static bifrost_app_t *appTable_func = nullptr;
    static const char **appTable_name = nullptr;
    void bifrost::registerApp(
        const char *appName, f_ptr_t callback,
        f_ptr_t transitionInto, f_ptr_t transitionOut
    ) {
        bifrost_app_t app = {callback, transitionInto, transitionOut};
        StretchyBufferPush(appTable_func, app);
        StretchyBufferPush(appTable_name, appName);
    }
    void bifrost::updateApp(game_memory_t * gameMemory, const char *appname) {
        for (uint32_t i = 0; i < StretchyBufferCount(appTable_name); i++) {
            if (strcmp(appTable_name[i], appname) == 0) {
                if (appTable_func[_currentApp].transitionOut != nullptr)
                    appTable_func[_currentApp].transitionOut(gameMemory);
                if (appTable_func[i].transitionInto != nullptr)
                    appTable_func[i].transitionInto(gameMemory);
                _currentApp = i;
            }
        }
    }
    std::function<void(game_memory_t *)> bifrost::getCurrentApp() {
        // TODO(Noah): This can crash if _currentApp gets corrupted or something silly.
        return (appTable_func == nullptr) ? nullptr : 
            appTable_func[_currentApp].updateFunc;
    }
    void super::updateAndRender(game_memory_t * gameMemory) {
#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
        // Present the ImGui stuff to allow user to switch apps.
        if (super::g_renderImGui) {
            ImGui::Begin("AutomataEngine");
            int item_current = _currentApp;
            ImGui::Combo("App", &item_current, appTable_name, StretchyBufferCount(appTable_name));
            if (item_current != _currentApp) { bifrost::updateApp(gameMemory, appTable_name[item_current]); }
            ImGui::Text("lastFrameTimeCPU: %.3f ms", 1000.0f * platform::lastFrameTime);
            ImGui::Text("lastFrameTimeTotal: %.3f ms (%.1f FPS)",
                1000.0f * platform::lastFrameTimeTotal, 1.0f / platform::lastFrameTimeTotal);
            ImGui::Text("updateModel: %s", updateModelToString(platform::GLOBAL_UPDATE_MODEL));
            bool vsync = platform::GLOBAL_VSYNC;
            ImGui::Checkbox("vsync", &vsync);
            if (platform::GLOBAL_VSYNC != vsync) {
                platform::setVsync(vsync);
            }
            ImGui::End();
        }
#endif
    }
    void super::close() {
        StretchyBufferFree(appTable_func);
        StretchyBufferFree(appTable_name);
    }
    void super::init() {
        stbi_set_flip_vertically_on_load(true);
        stbi_flip_vertically_on_write(true);
    }
    const char *updateModelToString(update_model_t updateModel) {
        const char *names[] = {
            "AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC",
            "AUTOMATA_ENGINE_UPDATE_MODEL_FRAME_BUFFERING",
            "AUTOMATA_ENGINE_UPDATE_MODEL_ONE_LATENT_FRAME"
        };
        static_assert(_countof(names) == AUTOMATA_ENGINE_UPDATE_MODEL_COUNT,
            "updateModelToString needs update.");
        int idx = updateModel;
        if (idx >= 0 && idx < _countof(names)) {
            return names[idx];
        } else {
            return "UNKNOWN";
        }
    }
};