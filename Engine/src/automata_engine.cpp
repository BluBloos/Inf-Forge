
#include <automata_engine.hpp>
#include "stb_image.h"
#include "stb_image_write.h"

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
#include "imgui.h"
#endif

namespace automata_engine {

    namespace timing {
        float getTimeElapsed(uint64_t begin, uint64_t end)
        {
            return float(end - begin) / EM->pfn.getTimerFrequency();
        }
    }

    void setEngineContext(engine_memory_t *pEM) { EM = pEM; }

    engine_memory_t *EM;

    static gpu_info_t userGpuInfo;

    void initModuleGlobals()
    {
        // NOTE: this is a setting that needs to be set again each time that we hot-load the game
        // DLL. we can't just rely on the memory stored in game_memory.
        stbi_set_flip_vertically_on_load(true);
        stbi_flip_vertically_on_write(true);

        ae::EM->pfn.getGpuInfos(&userGpuInfo, 1);
    }

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
    void ImGuiRenderVec3(const char *vecName, math::vec3_t vec) {
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

    void ImGuiRenderMat4(const char *matName, math::mat4_t mat) {
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

    loaded_image_t platform::stbImageLoad(const char *fileName) {
        int x, y, n;
        int desired_channels=4;
        loaded_image_t myImage = {};
        loaded_file_t myFile = EM->pfn.readEntireFile(fileName);
        if (myFile.contents) {
            myImage.parentFile = myFile;
            // NOTE(Noah): For now, let's avoid .jpg.
            // seems stb image loader has troubles with a subset of .jpg,
            // and I would rather not put any effort into determining precisely
            // which .jpg I have.
            
            //unsigned char *data = stbi_load(fileName, &x, &y, &n, 0);
            unsigned char *data = stbi_load_from_memory((stbi_uc*)myFile.contents, myFile.contentSize, &x,
                                      &y, &n, desired_channels);
            if (data != NULL) {
              myImage.pixelPointer = (uint32_t *)data;
              myImage.width = x;
              myImage.height = y;
              // assert(n == 4);
            } else {
              // TODO(Noah): Do something intelligent in the case of failure
              // here.
              //               AELoggerError(EM, "ae::platform::stbImageLoad failed");
              assert(false);
            }
        }
        return myImage;
    }

    namespace bifrost {

        void clearAppTable(game_memory_t *gameMemory)
        {
            // TODO: the stretchy buffer free call really ought to set these to null. 
            StretchyBufferFree(gameMemory->bifrost.appTable_funcs);
            gameMemory->bifrost.appTable_funcs = nullptr;
            StretchyBufferFree(gameMemory->bifrost.appTable_names);
            gameMemory->bifrost.appTable_names = nullptr;
        }

        void registerApp(
            game_memory_t *gameMemory,
            const char *appName,
            PFN_GameFunctionKind callback,
            PFN_GameFunctionKind        transitionInto,
            PFN_GameFunctionKind        transitionOut
        ) {
            auto &bifrost = gameMemory->bifrost;
            bifrost_app_t app = {.updateFunc = callback, .transitionInto = transitionInto, .transitionOut = transitionOut};
            StretchyBufferPush(bifrost.appTable_funcs, app);
            StretchyBufferPush(bifrost.appTable_names, appName);
        }

        void updateApp(game_memory_t * gameMemory, const char *appname) {
            auto &bifrost = gameMemory->bifrost;
            for (uint32_t i = 0; i < StretchyBufferCount(bifrost.appTable_names); i++) {
                if (strcmp(bifrost.appTable_names[i], appname) == 0) {
                    auto transitionOut = bifrost.appTable_funcs[bifrost.currentAppIndex].transitionOut; 
                    if ( transitionOut != nullptr )
                        transitionOut(gameMemory);
                    auto transitionInto = bifrost.appTable_funcs[i].transitionInto; 
                    if ( transitionInto != nullptr)
                        transitionInto(gameMemory);
                    bifrost.currentAppIndex = i;
                }
            }
        }

        PFN_GameFunctionKind getCurrentApp(game_memory_t *gameMemory) {
            auto &bifrost = gameMemory->bifrost;
            // TODO(Noah): This can crash if _currentApp gets corrupted or something silly.
            return (bifrost.appTable_funcs == nullptr) ? nullptr : 
                bifrost.appTable_funcs[bifrost.currentAppIndex].updateFunc;
        }

    }

    void super::updateAndRender(game_memory_t * gameMemory) {
        engine_memory_t *EM = gameMemory->pEngineMemory;
        auto &bifrost = gameMemory->bifrost;

        auto winInfo = EM->pfn.getWindowInfo(false);

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
        if (EM->bCanRenderImGui) {
            ImGui::Begin(AUTOMATA_ENGINE_NAME_STRING);

            ImGui::Text("engine version: %s", AUTOMATA_ENGINE_VERSION_STRING);

            int item_current = bifrost.currentAppIndex;
            ImGui::Combo("App", &item_current, bifrost.appTable_names, StretchyBufferCount(bifrost.appTable_names));
            if (item_current != bifrost.currentAppIndex) { 
                bifrost::updateApp(gameMemory, bifrost.appTable_names[item_current]);
            }

            ImGui::Text("CPU frame time: %.3f ms",
                1000.0f * timing::getTimeElapsed(EM->timing.lastFrameBeginTime, EM->timing.lastFrameUpdateEndTime));

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "the amount of time the CPU logic update portion of the frame took.");

            ImGui::Text("CPU + GPU frame time: %.3f ms",
                1000.0f * timing::getTimeElapsed(EM->timing.lastFrameBeginTime, EM->timing.lastFrameGpuEndTime));

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "the total time to render the frame.\n"
                    "this is the CPU logic update portion plus the GPU render time.");

            ImGui::Text("frames displayed per second: %.3f FPS", 1.f / EM->timing.lastFrameVisibleTime);

            float presentLatency = timing::getTimeElapsed(EM->timing.lastFrameBeginTime, EM->timing.lastFrameMaybeVblankTime); // - ;
            
            ImGui::Text("input latency: %.4f s", presentLatency);

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "the time it takes for the result of user input to be displayed on the screen.");

            ImGui::Text("render resolution: %u x %u", winInfo.width, winInfo.height);
            ImGui::Text("display resolution: %u x %u", winInfo.width, winInfo.height);

            ImGui::Checkbox("show ImGui demo window", &bifrost.bShowDemoWindow);

#define README_WINDOW_TITLE AUTOMATA_ENGINE_NAME_STRING " engine README.txt"

            ImGui::Checkbox("show " README_WINDOW_TITLE, &bifrost.bShowEngineReadme);

            ImGui::End();

            if (bifrost.bShowDemoWindow) ImGui::ShowDemoWindow();

            if (bifrost.bShowEngineReadme) {

                ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize;

                // NOTE: we ignore the .ini since if the system scale changes between app launch,
                // we're gonna spawn this window with the wrong size.
                ImGui::SetNextWindowSize(ImVec2(667 * winInfo.systemScale, 600), ImGuiCond_Always);

                // Main body of the Demo window starts here.
                if (!ImGui::Begin(README_WINDOW_TITLE, nullptr, window_flags))
                {
                    // Early out if the window is collapsed, as an optimization.
                    ImGui::End();
                    // return;
                }

                // clang-format off
                ImGui::TextWrapped(
"Thanks for playing!\n"
"\n"
"If you encounter any issues at all, please read the listing of engine\n"
"limitations before making a bug report.\n"
"\n"
"The following is a listing of " AUTOMATA_ENGINE_NAME_STRING " engine facts:\n"
"- Minimum supported OS: Windows Vista\n" // the reasoning here is that we put PNG in the .ICO for .EXE icon. windows vista added support for PNG in the icons.
"- Frames are paced to be rendered just before each monitor vertical refresh\n"
"\n"
"The following is a listing of " AUTOMATA_ENGINE_NAME_STRING " engine limitations:\n"
"- The system is not designed to handle:\n"
"    - when monitors are hot-swapped\n"
"    - when the system text/app scale setting is changed at runtime\n"
"- Due to the windows compositor, the reported input latency is likely\n"
"  inaccurate\n"
"- Frame pacing is accomplished via a fixed upper-bound frame time estimate\n"
"  and therefore may not function correctly on all systems"
                );
                // clang-format on

                ImGui::End();
            }

#undef README_WINDOW_TITLE
        }
#endif
    }
    void shutdownModuleGlobals() {
#if defined(AUTOMATA_ENGINE_DX12_BACKEND) || defined(AUTOMATA_ENGINE_VK_BACKEND)
        ae::HLSL::_close();
#endif
        ae::EM->pfn.freeGpuInfos(&userGpuInfo, 1);
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
};  // namespace automata_engine
