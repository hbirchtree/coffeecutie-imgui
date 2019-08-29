#include <coffee/core/CApplication>
#include <coffee/core/CFiles>

#include <coffee/core/input/eventhandlers.h>
#include <coffee/core/platform_data.h>
#include <coffee/graphics/apis/CGLeamRHI>
#include <coffee/windowing/renderer/renderer.h>

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
    GFX::API_CONTEXT                  load_api;
    CString                           input;
    CImGui::Widgets::FrameCounter     counter;
    CImGui::Widgets::EventHandlerList elist;
    CImGui::Widgets::RendererViewer   rviewer;

    bool open        = false;
    bool marked      = false;
    bool display_gui = true;

    u8 padding[5];
};
using R   = Display::CSDL2Renderer;
using Vis = Display::Properties;

using ELD = Display::EventLoopData<R, RData>;

void setup(R& r, RData* data)
{
    cDebug("Running setup");
    data->load_api = GFX::GetLoadAPI();

    if(!data->load_api(PlatformData::IsDebug()))
        r.closeWindow();

    if(!CImGui::Init(r, r))
    {
        cWarning("Failed to init ImGui");
        r.closeWindow();
    }
    data->input.resize(100);

    data->display_gui = true;

    data->counter = CImGui::Widgets::GetFramerateStats(1.);
    data->elist   = CImGui::Widgets::GetEventHandlerList();
    data->rviewer = CImGui::Widgets::GetRendererViewer<GFX>();
}

void loop(R& r, RData* data)
{
    GFX::DefaultFramebuffer()->use(RHI::FramebufferT::All);
    GFX::DefaultFramebuffer()->clear(0, {0.2f, 0.2f, 0.3f, 1.0});

    bool enable_gui_now = data->display_gui;
    bool frame_prepared = false;

    if(enable_gui_now)
        CImGui::NewFrame(r, r);

    r.pollEvents();

    if(enable_gui_now)
    {
        data->rviewer(GFX::GraphicsContext(), GFX::GraphicsDevice());

        ImGui::Begin("Hello!", &data->open);

        data->counter(r);

        ImGui::Text("Hello, world!");
        for(float i = 0; i < 1; i += 0.2)
            ImGui::ProgressBar(
                CMath::fmod(r.contextTime() + i, 1.0), {-1, 1}, "HAHA");

        ImGuiInputTextFlags text_flags = 0;
        ImGui::BeginChild(
            "Test child", {300, 100}, true, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Checkbox("Test", &data->marked);
        if(!data->marked)
            text_flags = ImGuiInputTextFlags_ReadOnly;
        ImGui::InputText("Text", &data->input[0], data->input.size());

        ImGui::EndChild();

        ImGui::End();

#if !defined(NDEBUG)
        ImGui::ShowTestWindow();
#endif

        data->elist(r);
        frame_prepared = true;
    }

    if(enable_gui_now && frame_prepared)
        CImGui::EndFrame();

    r.swapBuffers();
}

void cleanup(R&, RData* data)
{
    data->input.resize(0);

    data->counter = nullptr;
    data->elist   = nullptr;
    data->rviewer = nullptr;

    CImGui::Shutdown();

    data->load_api = nullptr;
    GFX::UnloadAPI();
}

int32 coffeeimgui_main(int32, cstring_w*)
{
    using namespace Coffee::Input;

#if defined(FEATURE_ENABLE_ASIO)
    Net::RegisterProfiling();
#endif

    RuntimeQueue::CreateNewQueue("ImGui");

    Vis visual = Display::GetDefaultVisual<GFX>();

    visual.gl.flags |= Display::GL::Properties::GLDebug;

    auto eld_data = MkShared<ELD>(
        Display::CreateRendererUq(),
        MkUq<RData>(),
        setup,
        loop,
        cleanup,
        std::move(visual));

    using namespace Display;
    using namespace EventHandlers;
    using namespace Input;

    R& renderer = *eld_data->renderer.get();

    renderer.installEventHandler(
        EHandle<Event>::MkHandler(WindowResize<GFX>()));
    renderer.installEventHandler(EHandle<CIEvent>::MkHandler(
        ExitOn<OnKey<Input::CK_Escape>>(eld_data->renderer)));
    renderer.installEventHandler(
        EHandle<CIEvent>::MkHandler(ExitOn<OnQuit>(eld_data->renderer)));
    renderer.installEventHandler(EHandle<CIEvent>::MkHandler(
        FullscreenOn<AnyIKey<
            KeyCombo<CK_EnterNL, CIKeyEvent::KeyModifiers::RAltModifier>,
            KeyCombo<CK_F11>>>(eld_data->renderer)));

    auto render_data = eld_data->data;
    renderer.installEventHandler(EHandle<CIEvent>::MkFunc<CIKeyEvent>(
        [render_data](CIEvent const& ev, CIKeyEvent const* e) {
            if(e->key == CK_F9 && e->mod & CIKeyEvent::PressedModifier)
                render_data->display_gui = !render_data->display_gui;
        },
        "ImGui toggle switch on F9"));

    CString err_s;
    R::execEventLoop(std::move(eld_data), visual, err_s);

    cDebug("Error: {0}", err_s);

    return 0;
}

COFFEE_APPLICATION_MAIN(coffeeimgui_main)
