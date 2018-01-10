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
using MainMenuWidget = Function<void(bigscalar, u32)>;

template<size_t values>
inline MainMenuWidget GetFrametimeGraph(
        bigscalar interval
        )
{
    return [=](bigscalar ms, u32 frames)
    {
        static Array<scalar, values> m_values = {};
        static szptr m_valuePtr = 0;

        m_values[m_valuePtr] = ms;

        scalar mx = 0.f;

        for(scalar v : m_values)
            mx = CMath::max(v, mx);

        ImGui::PlotLines("",
                         m_values.data(),
                         m_values.size(),
                         0, "",
                         0.f, mx, {1280, 32});

        m_valuePtr = (++m_valuePtr) % m_values.size();
    };
}

inline FrameCounter GetFramerateStats(
        bigscalar interval,
        Function<void(bigscalar ms,
                      u32 frames
                      )> const& mainMenu = [](bigscalar,u32){}
        )
{
    return [=](EventApplication const& event)
    {
        static bigscalar next_time;
        static bigscalar prev_time_always;
        static bigscalar prev_ms;
        static u32 frame_count;
        static u32 frame_count_display;

        prev_ms = event.contextTime() - prev_time_always;
        prev_time_always = event.contextTime();

        ImGui::BeginMainMenuBar();
        ImGui::Text("frametime=%.1f ms, framerate=%u FPS",
                    prev_ms * 1000., frame_count_display);
        ImGui::EndMainMenuBar();

        auto& io = ImGui::GetIO();

        scalar graph_height = 50;
        {
                    ImGui::SetNextWindowSize({io.DisplaySize.x,
                                              graph_height});
            ImGui::SetNextWindowPos({0,
                                     io.DisplaySize.y - graph_height});
            ImGui::Begin(
                        "Frametime graph", nullptr,
                        ImGuiWindowFlags_NoInputs|
                        ImGuiWindowFlags_NoResize|
                        ImGuiWindowFlags_NoTitleBar|
                        ImGuiWindowFlags_NoMove);

            GetFrametimeGraph<50>(interval)
                    (prev_ms * 1000., 0);

            ImGui::End();
        }

#define FRAMERATE_FRAC(ms, fps) (ms < (1. / fps)) ? 1.0 : (1. / fps) / ms

        ImGui::SetNextWindowSize({150, 100});
        ImGui::SetNextWindowPos({4,
                                 io.DisplaySize.y - 100 - 4
                                 - graph_height});

        ImGui::Begin("Framerate goals");
        ImGui::ProgressBar(FRAMERATE_FRAC(prev_ms, 30.), {-1, 14}, "30");
        ImGui::ProgressBar(FRAMERATE_FRAC(prev_ms, 60.), {-1, 14}, "60");
        ImGui::ProgressBar(FRAMERATE_FRAC(prev_ms, 120.), {-1, 14}, "120");
        ImGui::ProgressBar(FRAMERATE_FRAC(prev_ms, 144.), {-1, 14}, "144");
        ImGui::End();

#undef FRAMERATE_FRAC

        frame_count ++;
        if(event.contextTime() >= next_time)
        {
            frame_count_display = frame_count;
            frame_count = 0;
            next_time = event.contextTime() + interval;
        }
    };
}

inline EventHandlerList GetEventHandlerList(
        )
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
