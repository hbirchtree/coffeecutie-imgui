#pragma once

#include <coffee/comp_app/services.h>
#include <coffee/comp_app/subsystems.h>
#include <coffee/core/libc_types.h>
#include <coffee/core/stl_types.h>
#include <peripherals/stl/string_ops.h>
#include <platforms/process.h>
#include <platforms/sysinfo.h>

#include <imgui.h>

namespace Coffee {
namespace CImGui {

struct imgui_error_category : error_category
{
    virtual const char* name() const noexcept override;
    virtual std::string message(int) const override;
};

enum class ImError
{
    /* Graphics errors */
    ShaderCompilation,
    ShaderAttach,

    /* State errors */
    AlreadyLoaded,
    AlreadyUnloaded,
    InvalidDisplaySize,

    GlobalStateFailure,
};

using imgui_error_code = domain_error_code<ImError, imgui_error_category>;

using namespace Display;

IMGUI_API bool Init(Components::EntityContainer& container);
IMGUI_API void Shutdown();
IMGUI_API void NewFrame(Components::EntityContainer& container);
IMGUI_API void EndFrame();

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void InvalidateDeviceObjects(imgui_error_code& ec);
IMGUI_API bool CreateDeviceObjects(imgui_error_code& ec);

using ImGuiWidget = Function<void(
    Components::EntityContainer&,
    Components::time_point const&,
    Components::duration const&)>;

struct ImGuiSystem;

using ImGuiTag = Components::TagType<ImGuiSystem>;

struct ImGuiSystem : Components::RestrictedSubsystem<
                         ImGuiTag,
                         type_safety::empty_list_t,
                         type_safety::empty_list_t>,
                     comp_app::AppLoadableService
{
    virtual const ImGuiSystem& get() const final
    {
        return *this;
    }
    virtual ImGuiSystem& get() final
    {
        return *this;
    }

    virtual void load(entity_container& e, comp_app::app_error& ec) final;
    virtual void unload(entity_container& e, comp_app::app_error& ec) final;
    virtual void start_restricted(
        Proxy& p, Components::time_point const& t) final;
    virtual void end_restricted(Proxy&, Components::time_point const&) final;

    ImGuiSystem& addWidget(ImGuiWidget&& widget);

  private:
    time_point m_previousTime;
    Vector<ImGuiWidget> m_widgets;
};

namespace Widgets {

extern ImGuiWidget StatsMenu();

} // namespace Widgets

// namespace Widgets {

// using FrameCounter   = Function<void(Components::EntityContainer const&)>;
// using MainMenuWidget = Function<void(bigscalar, u32, scalar, scalar)>;

// template<size_t values>
// inline MainMenuWidget GetFrametimeGraph(bigscalar interval)
//{
//    return [=](bigscalar ms, u32 frames, scalar width, scalar height = 24) {
//        static Array<scalar, values> m_values   = {};
//        static szptr                 m_valuePtr = 0;

//        m_values[m_valuePtr] = ms;

//        scalar mx = 0.f;

//        for(scalar v : m_values)
//            mx = math::max(v, mx);

//        ImGui::PlotHistogram(
//            "",
//            m_values.data(),
//            m_values.size(),
//            0,
//            "",
//            0.f,
//            mx,
//            {width, height});

//        m_valuePtr = (++m_valuePtr) % m_values.size();
//    };
//}

// inline FrameCounter GetFramerateStats(
//    bigscalar                                       interval,
//    Function<void(bigscalar ms, u32 frames)> const& mainMenu = [](bigscalar,
//                                                                  u32) {})
//{
//    return [=](Components::EntityContainer const& event) {
//        using namespace ::platform;

//        static bigscalar next_time;
//        static bigscalar prev_time_always;
//        static bigscalar prev_ms;
//        static u32       frame_count;
//        static u32       frame_count_display;

//        prev_ms          = event.contextTime() - prev_time_always;
//        prev_time_always = event.contextTime();

//        ImGui::BeginMainMenuBar();
//        ImGui::Columns(4, "Menu");
//        ImGui::Text(
//            "ft=%.1f ms,"
//            " fps=%u"
//            " SF=%ldMB",
//            prev_ms * 1000.,
//            frame_count_display,

//            ProcessProperty::Mem(0) / 1_kB);
//        ImGui::NextColumn();
//        ImGui::Text(
//            "GPU=%.1fC" /*"|%.1fC"*/
//            "/%lluMB/%lluMB",

//            0.0, /*0.0,*/
//            0ULL,
//            0ULL);
//        ImGui::NextColumn();
//        auto      temp   = PowerInfo::CpuTemperature();
//        bigscalar mused  = SysInfo::MemTotal() - SysInfo::MemAvailable();
//        bigscalar mtotal = SysInfo::MemTotal();
//        ImGui::Text(
//            "CPU=%.1fC" /*"|%.1fC"*/
//            "/M=%.1fGB/%.1fGB",
//            temp.current,
//            //                    temp.trip_point,

//            mused / 1_GB,
//            mtotal / 1_GB);
//        ImGui::NextColumn();
//        GetFrametimeGraph<50>(interval)(
//            prev_ms * 1000., 0, ImGui::GetWindowSize().x / 4.f, 20.f);
//        ImGui::NextColumn();
//        ImGui::EndMainMenuBar();

//        auto& io = ImGui::GetIO();

//#define FRAMERATE_FRAC(ms, fps) (ms < (1. / fps)) ? 1.0 : (1. / fps) / ms

//        ImGui::SetNextWindowSize({150, 100});
//        ImGui::SetNextWindowPos({4, io.DisplaySize.y - 100 - 4});

//        ImGui::Begin("Framerate goals");
//        ImGui::ProgressBar(FRAMERATE_FRAC(prev_ms, 30.), {-1, 14}, "30");
//        ImGui::ProgressBar(FRAMERATE_FRAC(prev_ms, 60.), {-1, 14}, "60");
//        ImGui::ProgressBar(FRAMERATE_FRAC(prev_ms, 120.), {-1, 14}, "120");
//        ImGui::ProgressBar(FRAMERATE_FRAC(prev_ms, 144.), {-1, 14}, "144");
//        ImGui::End();

//#undef FRAMERATE_FRAC

//        frame_count++;
//        if(event.contextTime() >= next_time)
//        {
//            frame_count_display = frame_count;
//            frame_count         = 0;
//            next_time           = event.contextTime() + interval;
//        }
//    };
//}

//} // namespace Widgets

} // namespace CImGui
} // namespace Coffee
