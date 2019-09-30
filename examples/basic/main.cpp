#include <coffee/core/CApplication>
#include <coffee/core/CFiles>

#include <coffee/comp_app/app_wrap.h>
#include <coffee/comp_app/bundle.h>
#include <coffee/comp_app/subsystems.h>
#include <coffee/core/input/eventhandlers.h>
#include <coffee/core/platform_data.h>
#include <coffee/graphics/apis/CGLeamRHI>

#include <coffee/strings/libc_types.h>

#include <coffee/core/CDebug>
#include <coffee/imgui/graphics_widgets.h>
#include <coffee/imgui/imgui_binding.h>
#include <imgui.h>

#if defined(FEATURE_ENABLE_ASIO)
#include <coffee/asio/net_profiling.h>
#endif

using namespace Coffee;

#if defined(COFFEE_IMGUI_USE_GLEAM)
using GFX = RHI::GLEAM::GLEAM_API;
#else
using GFX = RHI::NullAPI;
#endif

struct RData
{
    GFX::API_CONTEXT load_api;
    CString          input;
    //    CImGui::Widgets::FrameCounter     counter;
    //    CImGui::Widgets::EventHandlerList elist;
    CImGui::Widgets::RendererViewer rviewer;

    bool open        = false;
    bool marked      = false;
    bool display_gui = true;

    u8 padding[5];
};

void setup(
    Components::EntityContainer& r, RData& data, Components::time_point const&)
{
    cDebug("Running setup");
    data.load_api = GFX::GetLoadAPI();

    if(!data.load_api(PlatformData::IsDebug()))
        r.service<comp_app::Windowing>()->close();

    data.input.resize(100);

    data.display_gui = true;

    //    data.counter = CImGui::Widgets::GetFramerateStats(1.);
    //    data.elist   = CImGui::Widgets::GetEventHandlerList();
    //    data.rviewer = CImGui::Widgets::GetRendererViewer<GFX>();
}

void loop(
    Components::EntityContainer&  r,
    RData&                        data,
    Components::time_point const& time,
    Components::duration const&   delta)
{
    GFX::DefaultFramebuffer()->use(RHI::FramebufferT::All);
    GFX::DefaultFramebuffer()->clear(0, {0.2f, 0.2f, 0.3f, 1.0});

    if(data.display_gui)
    {
        //        data.rviewer(GFX::GraphicsContext(), GFX::GraphicsDevice());

        ImGui::Begin("Hello!", &data.open);

        //        data->counter(r);

        ImGui::Text("Hello, world!");
        for(float i = 0; i < 1; i += 0.2)
            ImGui::ProgressBar(
                CMath::fmod(time.time_since_epoch().count() + i, 1.0),
                {-1, 1},
                "HAHA");

        ImGuiInputTextFlags text_flags = 0;
        ImGui::BeginChild(
            "Test child", {300, 100}, true, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Checkbox("Test", &data.marked);
        if(!data.marked)
            text_flags = ImGuiInputTextFlags_ReadOnly;
        ImGui::InputText("Text", &data.input[0], data.input.size());

        ImGui::EndChild();

        ImGui::End();

#if !defined(NDEBUG)
        ImGui::ShowTestWindow();
#endif
    }
}

void cleanup(
    Components::EntityContainer&, RData& data, Components::time_point const&)
{
    data.input.resize(0);

    CImGui::Shutdown();

    data.load_api = nullptr;
    GFX::UnloadAPI();
}

int32 coffeeimgui_main(int32, cstring_w*)
{
    using namespace Coffee::Input;

#if defined(FEATURE_ENABLE_ASIO)
    Net::RegisterProfiling();
#endif

    RuntimeQueue::CreateNewQueue("ImGui");

    auto& container = comp_app::createContainer();

    auto& loader = comp_app::AppLoader::register_service(container);
    comp_app::configureDefaults(loader);

    using namespace Display;
    using namespace EventHandlers;
    using namespace Input;

    comp_app::app_error ec;
    comp_app::addDefaults(container, loader, ec);

    auto& ibus = *container.service<comp_app::BasicEventBus<CIEvent>>();
    auto& wbus = *container.service<comp_app::BasicEventBus<Event>>();

    wbus.addEventHandler(10, WindowResize<GFX>());
    ibus.addEventHandler(
        10,
        ExitOn<OnKey<Input::CK_Escape>>(
            container.service_ref<comp_app::Windowing>()));
    ibus.addEventHandler(
        10, ExitOn<OnQuit>(container.service_ref<comp_app::Windowing>()));
    ibus.addEventHandler(
        10,
        FullscreenOn<AnyIKey<
            KeyCombo<CK_EnterNL, CIKeyEvent::KeyModifiers::RAltModifier>,
            KeyCombo<CK_F11>>>(container.service_ref<comp_app::Windowing>()));

    comp_app::AppContainer<RData>::addTo(container, setup, loop, cleanup);

    auto& imgui = container.register_subsystem_inplace<
        CImGui::ImGuiTag,
        CImGui::ImGuiSystem>();
    imgui.load(container, ec);
    imgui.addWidget(CImGui::Widgets::StatsMenu());

    return comp_app::ExecLoop<comp_app::BundleData>::exec(container);
}

COFFEE_APPLICATION_MAIN(coffeeimgui_main)
