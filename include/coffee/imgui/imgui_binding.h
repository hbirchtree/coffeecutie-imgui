#pragma once

#include <coffee/core/types/tdef/integertypes.h>
#include <coffee/core/base/renderer/windowmanagerclient.h>
#include <coffee/core/base/renderer/eventapplication.h>

#include <imgui.h>

namespace Coffee{
namespace CImGui{

using namespace Display;

IMGUI_API bool        Init(WindowManagerClient& window,
                           EventApplication& event);
IMGUI_API void        Shutdown();
IMGUI_API void        NewFrame(WindowManagerClient& window,
                               EventApplication& event);
IMGUI_API void        EndFrame();

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void        InvalidateDeviceObjects();
IMGUI_API bool        CreateDeviceObjects();

namespace Widgets{

using FrameCounter = Function<void(EventApplication const&)>;
using EventHandlerList = Function<void(EventApplication&)>;

inline FrameCounter GetFramerateStats(bigscalar interval)
{
    return [=](EventApplication const& event)
    {
        static bigscalar next_time;
        static bigscalar prev_time_always;
        static bigscalar prev_ms;
        static u32 frame_count;
        static u32 frame_count_display;

        prev_ms = event.contextTime() - prev_time_always;

        ImGui::BeginMainMenuBar();
        ImGui::Text("frametime=%f ms, framerate=%u FPS",
                    prev_ms, frame_count_display);
        ImGui::EndMainMenuBar();

#define FRAMERATE_FRAC(ms, fps) (ms < (1. / fps)) ? 1.0 : (1. / fps) / ms

        auto& io = ImGui::GetIO();

        ImGui::SetNextWindowSize({150, 100});
        ImGui::SetNextWindowPos({4, io.DisplaySize.y - 100 - 4});

        ImGui::Begin("Framerate goals");
        ImGui::ProgressBar(FRAMERATE_FRAC(prev_ms, 30.), {-1, 14}, "30");
        ImGui::ProgressBar(FRAMERATE_FRAC(prev_ms, 60.), {-1, 14}, "60");
        ImGui::ProgressBar(FRAMERATE_FRAC(prev_ms, 120.), {-1, 14}, "120");
        ImGui::ProgressBar(FRAMERATE_FRAC(prev_ms, 144.), {-1, 14}, "144");
        ImGui::End();

#undef FRAMERATE_FRAC

        frame_count ++;

        prev_time_always = event.contextTime();
        if(event.contextTime() >= next_time)
        {
            frame_count_display = frame_count;
            frame_count = 0;
            next_time = event.contextTime() + interval;
        }
    };
}

inline EventHandlerList GetEventHandlerList()
{
    return [](EventApplication& r)
    {
        ImGui::Begin("Event handlers");

        for(auto const& e : *r.getEventHandlersI())
        {
            CString ptr = StrUtil::pointerify(e.user_ptr);
            ImGui::Text("%s : %s", e.name, ptr.c_str());
        }
        for(auto const& e : *r.getEventHandlersD())
        {
            CString ptr = StrUtil::pointerify(e.user_ptr);
            ImGui::Text("%s : %s", e.name, ptr.c_str());
        }

        ImGui::End();
    };
}

}

}
}
