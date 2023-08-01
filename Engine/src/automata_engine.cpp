
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

    void initModuleGlobals()
    {
        // NOTE: this is a setting that needs to be set again each time that we hot-load the game
        // DLL. we can't just rely on the memory stored in game_memory.
        stbi_set_flip_vertically_on_load(true);
        stbi_flip_vertically_on_write(true);
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
#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
        // Present the ImGui stuff to allow user to switch apps.
        if (EM->g_renderImGui) {
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
                    "the amount of time the CPU portion of the frame took.");

            ImGui::Text("CPU + GPU frame time: %.3f ms",
                1000.0f * timing::getTimeElapsed(EM->timing.lastFrameBeginTime, EM->timing.lastFrameGpuEndTime));

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "the total time to render the frame.\n"
                    "this is the CPU portion plus the GPU render time.");

            float collectPeriod = EM->timing.lastFrameVisibleTime / 2.0f;
            
            // TODO:
            //ImGui::Text("input latency: %.3f s", collectPeriod);
            //if (ImGui::IsItemHovered()) ImGui::SetTooltip("the phase shift of the input signal to the game signal.");

            float gameSimulateFrameTime;  // time of second poll + 1/2 poll period.
            ImGui::Text("present latency: %.3f s",
                timing::getTimeElapsed(EM->timing.lastFrameBeginTime, EM->timing.lastFrameMaybeVblankTime) -
                    collectPeriod / 2.f);
            
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "the phase shift of the game signal to the vertical blank signal.\n"
                    "if this is negative, that implies the vblank leads the game signal.");

            ImGui::Text("frames per second: %.3f FPS", 1.f / EM->timing.lastFrameVisibleTime);
            // ImGui::Text("update model: %s", updateModelToString(EM->g_updateModel));           
            // TODO: for now VSYNC is always on.
            ImGui::Text("VSync: ON");

            ImGui::Checkbox("show ImGui demo window", &bifrost.bShowDemoWindow);
            ImGui::End();

            if (bifrost.bShowDemoWindow) ImGui::ShowDemoWindow();
        }
#endif
    }
    void shutdownModuleGlobals() {
#if defined(AUTOMATA_ENGINE_DX12_BACKEND) || defined(AUTOMATA_ENGINE_VK_BACKEND)
        ae::HLSL::_close();
#endif
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
