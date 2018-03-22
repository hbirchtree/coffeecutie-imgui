#include <CoffeeDef.h>
#include <coffee/core/CApplication>
#include <coffee/core/CDebug>
#include <coffee/core/CFiles>

#include <coffee/windowing/renderer/renderer.h>
#include <coffee/graphics/apis/CGLeamRHI>
#include <coffee/core/platform_data.h>
#include <coffee/core/input/eventhandlers.h>

#include <imgui.h>
#include <coffee/imgui/imgui_binding.h>
#include <coffee/imgui/graphics_widgets.h>

#include <coffee/core/coffee.h>

using namespace Coffee;

#if defined(COFFEE_IMGUI_USE_GLEAM)
using GFX = RHI::GLEAM::GLEAM_API;
#else
using GFX = RHI::NullAPI;
#endif

struct RData
{
    GFX::API_CONTEXT load_api;
    CString input;
    CImGui::Widgets::FrameCounter counter;
    CImGui::Widgets::EventHandlerList elist;
    CImGui::Widgets::RendererViewer rviewer;

    bool open = false;
    bool marked = false;
    bool display_gui = true;

    u8 padding[5];
};
using R = Display::CSDL2Renderer;
using Vis = Display::CDProperties;

using ELD = Display::EventLoopData<R,RData>;

void setup(R& r, RData* data)
{
    cDebug("Running setup");
    data->load_api = GFX::GetLoadAPI();

    if(!data->load_api(PlatformData::IsDebug()))
        r.closeWindow();

    if(!CImGui::Init(r,r))
    {
        cWarning("Failed to init ImGui");
        r.closeWindow();
    }
    data->input.resize(100);

    data->display_gui = true;

    data->counter = CImGui::Widgets::GetFramerateStats(1.);
    data->elist = CImGui::Widgets::GetEventHandlerList();
    data->rviewer = CImGui::Widgets::GetRendererViewer<GFX>();
}

void loop(R& r, RData* data)
{
    GFX::DefaultFramebuffer().clear(0, {0.2f, 0.2f, 0.3f, 1.0});

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
        for(float i=0;i<1;i+=0.2)
            ImGui::ProgressBar(CMath::fmod(r.contextTime()+i, 1.0), {-1, 1}, "HAHA");

        ImGuiInputTextFlags text_flags = 0;
        ImGui::BeginChild("Test child", {300,100}, true, ImGuiWindowFlags_AlwaysAutoResize);
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
    data->elist = nullptr;
    data->rviewer = nullptr;

    CImGui::Shutdown();


    data->load_api = nullptr;
    GFX::UnloadAPI();
}

int32 coffeeimgui_main(int32, cstring_w*)
{
    CResources::FileResourcePrefix("coffeeimgui/");
    ApplicationData();

    cDebug("Hello World!");

    auto disable_imgui = [](void* u, CIEvent const& ev, c_cptr data)
    {
        if(ev.type == CIEvent::Keyboard)
        {
            auto e = *(CIKeyEvent const*)data;
            auto render_data = (RData*)u;
            if(e.key == CK_F9 && e.mod & CIKeyEvent::PressedModifier)
                render_data->display_gui = !render_data->display_gui;
        }
    };

    Vis visual = Display::GetDefaultVisual<GFX>();

    visual.gl.flags |= Display::GLProperties::GLDebug;

    ELD* eld_data = new ELD{Display::CreateRendererUq(),
            MkUq<RData>(),
            setup, loop, cleanup,
            0, {}};

    R& renderer = *eld_data->renderer.get();

    renderer.installEventHandler(
    {
                    Display::EventHandlers::WindowManagerCloseWindow<R>,
                    "Window manager closing window",
                    &renderer
                });
    renderer.installEventHandler(
    {
                    Display::EventHandlers::EscapeCloseWindow<R>,
                    "Escape key closing window",
                    &renderer
                });
    renderer.installEventHandler(
    {
                    Display::EventHandlers::ResizeWindowUniversal<GFX>,
                    "Window resizing",
                    nullptr
                });

    renderer.installEventHandler(
    {
                   Display::EventHandlers::WindowManagerFullscreen<R>,
                    "Window fullscreen trigger on F11 and Alt-Enter",
                    &renderer
                });
    renderer.installEventHandler(
    {
                    disable_imgui,
                    "ImGui toggle switch on F9",
                    eld_data->data.get()
                });

    CString err_s;
    R::execEventLoop(*eld_data, visual, err_s);

    cDebug("Error: {0}", err_s);

    return 0;
}

COFFEE_APPLICATION_MAIN(coffeeimgui_main)
