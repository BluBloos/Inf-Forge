#include <automata_engine.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" 

#include <windows.h>
#include <win32_engine.h>
#include "imgui.h"

namespace automata_engine {
    
    int32_t defaultWidth = 1280;
    int32_t defaultHeight = 720;
    game_window_profile_t defaultWinProfile = AUTOMATA_ENGINE_WINPROFILE_RESIZE;
    const char *defaultWindowName = "Automata Engine";

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

    static update_model_t updateModel;
    void setUpdateModel(update_model_t newModel) {
        updateModel = newModel;
    }
    loaded_image_t platform::stbImageLoad(char *fileName) {
        loaded_image myImage = {};
        int x, y, n;
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
    static f_ptr_t *appTable_func = nullptr;
    static const char **appTable_name = nullptr;
    void bifrost::registerApp(const char *appName, f_ptr_t callback) {
        StretchyBufferPush(appTable_func, callback);
        StretchyBufferPush(appTable_name, appName);
    }
    void bifrost::updateApp(const char *appname) {
        for (uint32_t i = 0; i < StretchyBufferCount(appTable_name); i++) {
            if (strcmp(appTable_name[i], appname) == 0) {
                _currentApp = i;
            }
        }
    }
    std::function<void(game_memory_t *)> bifrost::getCurrentApp() {
        // TODO(Noah): This can crash if _currentApp gets corrupted or something silly.
        return (appTable_func == nullptr) ? nullptr : 
            appTable_func[_currentApp];
    }
    void super::updateAndRender() {
        // Present the ImGui stuff to allow user to switch apps.
#ifndef RELEASE
        ImGui::Begin("AutomataEngine Devtools");
        static int item_current = 0;
        ImGui::Combo("App", &item_current, appTable_name, StretchyBufferCount(appTable_name));
        if (item_current != _currentApp) { _currentApp = item_current; }
        ImGui::End();
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
};