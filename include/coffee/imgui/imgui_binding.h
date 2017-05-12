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

inline FrameCounter FramerateStats(bigscalar interval)
{
    return [=](EventApplication const& event)
    {
        static bigscalar next_time;
        static bigscalar prev_time_always;
        static bigscalar prev_ms;
        static u32 frame_count;
        static u32 frame_count_display;

        prev_ms = event.contextTime() - prev_time_always;

        ImGui::Text("frametime=%f ms, framerate=%u FPS",
                    prev_ms, frame_count_display);

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

}

}
}
