#include <coffee/core/CApplication>
#include <coffee/core/CDebug>
#include <coffee/core/CFiles>

#include <coffee/sdl2/CSDL2GLRenderer>
#include <coffee/graphics/apis/CGLeamRHI>
#include <coffee/core/platform_data.h>
#include <coffee/core/input/eventhandlers.h>

#include <imgui.h>
#include <coffee/imgui/imgui_binding.h>

using namespace Coffee;

using GFX = RHI::GLEAM::GLEAM_API;

struct RData
{
    GFX::API_CONTEXT load_api;
    bool open = false;
    bool marked = false;
    CString input;

    bool display_gui = true;
};
using R = Display::CSDL2Renderer;
using Vis = Display::CDProperties;

using ELD = Display::EventLoopData<R,RData>;

void setup(R& r, RData* data)
{
    data->load_api = GFX::GetLoadAPI();

    if(!data->load_api(PlatformData::IsDebug()))
        r.closeWindow();

    CImGui::Init(r,r);
    data->input.resize(100);
}


void loop(R& r, RData* data)
{
    GFX::DefaultFramebuffer().clear(0, {0.1f, 0.1f, 0.1f, 1.0});

    if(data->display_gui)
    {
        CImGui::NewFrame(r, r);

        ImGui::Begin("Hello!", &data->open, 0);

        ImGui::BeginMainMenuBar();

        ImGui::EndMainMenuBar();

        ImGui::Text("Hello, world!");
        for(float i=0;i<1;i+=0.2)
            ImGui::ProgressBar(CMath::fmod(r.contextTime()+i, 1.0), {0, 1}, "HAHA");

        ImGuiInputTextFlags text_flags = 0;
        ImGui::BeginChild("Test child", {100,100}, true, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Checkbox("Test", &data->marked);
        if(!data->marked)
            text_flags = ImGuiInputTextFlags_ReadOnly;
        ImGui::InputText("Text", &data->input[0], data->input.size(),
                text_flags, nullptr, nullptr);

        ImGui::EndChild();

        ImGui::End();

        ImGui::Begin("Event handlers");
        for(auto const& e : *r.getEventHandlersI())
            ImGui::Text("%s", e.name);
        for(auto const& e : *r.getEventHandlersD())
            ImGui::Text("%s", e.name);
        ImGui::End();
    }

    r.pollEvents();

    if(data->display_gui)
        CImGui::EndFrame();

    r.swapBuffers();
}

void cleanup(R&, RData*)
{
    CImGui::Shutdown();
}

int32 coffeeimgui_main(int32, cstring_w*)
{
    CResources::FileResourcePrefix("coffeeimgui/");

    cDebug("Hello World!");

    R renderer;

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


    Vis visual = Display::GetDefaultVisual<GFX>();
    RData render_data;

    renderer.installEventHandler(
    {
                    disable_imgui,
                    "ImGui toggle switch on F9",
                    &render_data
                });

    ELD eld_data;
    eld_data.setup = setup;
    eld_data.loop = loop;
    eld_data.cleanup = cleanup;
    eld_data.renderer = &renderer;
    eld_data.data = &render_data;

    CString err_s;
    R::execEventLoop(eld_data, visual, err_s);

    cDebug("Error: {0}", err_s);

    return 0;
}

COFFEE_APPLICATION_MAIN(coffeeimgui_main)
