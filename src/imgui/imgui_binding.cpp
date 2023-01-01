// ImGui SDL2 binding with OpenGL3
// In this binding, ImTextureID is used to store an OpenGL 'GLuint' texture
// identifier. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See
// main.cpp for an example of using this. If you use this binding you'll need to
// call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(),
// ImGui::Render() and ImGui_ImplXXXX_Shutdown(). If you are new to ImGui, see
// examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include <coffee/core/CProfiling>
#include <coffee/core/base.h>
#include <coffee/core/base_state.h>
#include <coffee/core/platform_data.h>
#include <coffee/core/types/chunk.h>
#include <coffee/core/types/display/event.h>
#include <coffee/core/types/input/event_types.h>
#include <coffee/core/types/input/keymap.h>
#include <coffee/imgui/imgui_binding.h>
#include <coffee/interfaces/cgraphics_pixops.h>
#include <peripherals/libc/memory_ops.h>
#include <peripherals/typing/vectors/vectors.h>

#include <coffee/graphics/apis/gleam/rhi.h>

#include <coffee/strings/libc_types.h>

#include <coffee/core/CDebug>

#define IM_API "ImGui::"

using namespace imgui::detail;

using Coffee::DProfContext;
using Coffee::Chrono::duration_cast;
using Coffee::Chrono::seconds_float;
using libc_types::f32;
using libc_types::f64;
using libc_types::u16;
using libc_types::u32;
using semantic::mem_chunk;
using semantic::RSCA;
using stl_types::Range;
using stl_types::ShPtr;
using typing::geometry::size_2d;
using typing::geometry::size_3d;

using namespace typing::vector_types;
using namespace Coffee::Input;

using Coffee::cDebug;
using Coffee::cWarning;

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

static constexpr std::array<std::pair<ImGuiKey_, u16>, 19> ImKeyMap = {{
    {ImGuiKey_Tab, CK_HTab},
    {ImGuiKey_LeftArrow, CK_Left},
    {ImGuiKey_RightArrow, CK_Right},
    {ImGuiKey_UpArrow, CK_Up},
    {ImGuiKey_DownArrow, CK_Down},
    {ImGuiKey_PageUp, CK_PgUp},
    {ImGuiKey_PageDown, CK_PgDn},
    {ImGuiKey_Home, CK_Home},
    {ImGuiKey_End, CK_End},
    {ImGuiKey_Backspace, CK_BackSpace},
    {ImGuiKey_Delete, CK_Delete},
    {ImGuiKey_Enter, CK_EnterNL},
    {ImGuiKey_Escape, CK_Escape},
    {ImGuiKey_A, CK_a},
    {ImGuiKey_C, CK_c},
    {ImGuiKey_V, CK_v},
    {ImGuiKey_X, CK_x},
    {ImGuiKey_Y, CK_y},
    {ImGuiKey_Z, CK_z},
}};

C_UNUSED(STATICINLINE ImGuiKey CfToImKey(u32 k))
{
    auto it = std::find_if(
        ImKeyMap.begin(),
        ImKeyMap.end(),
        [k](std::pair<ImGuiKey_, u16> const& p) { return p.second == k; });
    if(it != ImKeyMap.end())
        return it->first;
    return 0;
}

C_UNUSED(STATICINLINE u32 ImToCfKey(ImGuiKey k))
{
    auto it = std::find_if(
        ImKeyMap.begin(),
        ImKeyMap.end(),
        [k](std::pair<ImGuiKey_, u16> const& p) { return p.first == k; });
    if(it != ImKeyMap.end())
        return it->second;
    return 0;
}

namespace imgui::detail {

struct ImGuiData
{
    ImGuiData(gfx::api& api) :
        vao(api.alloc_vertex_array()), pipeline(api.alloc_program()),
        vertices(api.alloc_buffer(
            gfx::buffers::vertex, RSCA::Streaming | RSCA::WriteOnly)),
        elements(api.alloc_buffer(
            gfx::buffers::vertex, RSCA::Streaming | RSCA::WriteOnly)),
        font_atlas(api.alloc_texture(
            gfx::textures::d2,
            typing::pixels::PixDesc(typing::pixels::PixFmt::RGBA8),
            1)),
        font_sampler(font_atlas->sampler())
    {
    }

    ShPtr<gfx::vertex_array_t> vao;
    ShPtr<gfx::program_t>      pipeline;
    ShPtr<gfx::buffer_t>       vertices;
    ShPtr<gfx::buffer_t>       elements;

    ShPtr<gfx::texture_2d_t> font_atlas;
    ShPtr<gfx::sampler_t>    font_sampler;

    Matf4 projection_matrix;

    f64 time;
    f32 scroll;

    u32 _pad;
};

template<typename T>
static auto to_bytes(ImVector<T>& data)
{
    return semantic::mem_chunk<byte_t>::ofBytes(data.Data, data.Size).view;
}

// This is the main rendering function that you have to implement and provide to
// ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure) If text
// or lines are blurry when integrating ImGui in your engine:
// - in your Render function, try translating your projection matrix by
// (0.5f,0.5f) or (0.375f,0.375f)
static void draw_items(ImGuiData* im_data, ImDrawData* draw_data, gfx::api& api)
{
    using namespace std::string_view_literals;

    // Avoid rendering when minimized, scale coordinates for retina displays
    // (screen coordinates != framebuffer coordinates)
    auto         draw_scope = api.debug().scope(IM_API "ImGui render");
    DProfContext _(IM_API "Rendering draw lists");

    ImGuiIO& io        = ImGui::GetIO();
    int      fb_width  = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int      fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if(fb_width == 0 || fb_height == 0)
        return;
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

    //    GFX::BLNDSTATE prev_blnd;
    //    GFX::VIEWSTATE prev_view;
    //    GFX::RASTSTATE prev_rast;
    //    GFX::DEPTSTATE prev_dept;

    //    GFX::GetBlendState(prev_blnd);
    //    GFX::GetViewportState(prev_view);
    //    GFX::GetRasterizerState(prev_rast);
    //    GFX::GetDepthState(prev_dept);

    //    GFX::BLNDSTATE blend;
    //    blend.m_doBlend = true;
    //    GFX::RASTSTATE raster;
    //    raster.m_culling = C_CAST<u32>(RHI::Datatypes::Face::Front);
    //    GFX::DEPTSTATE depth;
    //    depth.m_test = false;
    //    GFX::VIEWSTATE view_(1);

    //    view_.m_view.clear();
    //    view_.m_depth.clear();

    //    GFX::SetBlendState(blend);
    //    GFX::SetRasterizerState(raster);
    //    GFX::SetDepthState(depth);

    im_data->projection_matrix.d[0] = Vecf4{2.f / io.DisplaySize.x, 0, 0, 0};
    im_data->projection_matrix.d[1] = Vecf4{0, 2.f / -io.DisplaySize.y, 0, 0};
    im_data->projection_matrix.d[2] = Vecf4{0, 0, -1.f, 0};
    im_data->projection_matrix.d[3] = Vecf4{-1.f, 1.f, 0, 1.f};

    auto const& ro_projection = im_data->projection_matrix;

    //    GFX::D_CALL dc(true, false);
    //    GFX::D_DATA dd;
    //    dd.m_eltype =
    //        (sizeof(ImDrawIdx) == 2) ? RHI::TypeEnum::UShort :
    //        RHI::TypeEnum::UInt;

    for(int n = 0; n < draw_data->CmdListsCount; n++)
    {
        auto draw_scope = api.debug().scope(IM_API "Command list");

        auto cmd_list = draw_data->CmdLists[n];
        //        dd.m_eoff     = 0;

        im_data->vertices->commit(to_bytes(cmd_list->VtxBuffer));
        im_data->elements->commit(to_bytes(cmd_list->IdxBuffer));

        for(auto const& cmd : cmd_list->CmdBuffer)
        {
            auto draw_scope = api.debug().scope(IM_API "Command buffer");
            if(cmd.UserCallback)
                cmd.UserCallback(cmd_list, &cmd);
            else
            {
                //                view_.m_scissor[0] = {
                //                    C_CAST<i32>(cmd->ClipRect.x),
                //                    C_CAST<i32>(fb_height - cmd->ClipRect.w),
                //                    C_CAST<i32>(cmd->ClipRect.z -
                //                    cmd->ClipRect.x),
                //                    C_CAST<i32>(cmd->ClipRect.w -
                //                    cmd->ClipRect.y)};

                //                GFX::SetViewportState(view_);
                api.submit(
                    {
                        .program  = im_data->pipeline,
                        .vertices = im_data->vao,
                        .call =
                            {
                                .indexed = true,
                            },
                        .data =
                            {
                                {
                                    .elements =
                                        {
                                            .count  = cmd.ElemCount,
                                            .offset = cmd.IdxOffset,
                                            .type =
                                                sizeof(ImDrawIdx) == 2
                                                    ? semantic::TypeEnum::UShort
                                                    : semantic::TypeEnum::UInt,
                                        },
                                },
                            },
                    },
                    gfx::make_sampler_list(gfx::sampler_definition_t{
                        typing::graphics::ShaderStage::Fragment,
                        {"Texture"sv, 0},
                        im_data->font_sampler}),
                    gfx::make_uniform_list(
                        typing::graphics::ShaderStage::Vertex,
                        gfx::uniform_pair<const Matf4>{
                            {"ProjMtx"sv, 1},
                            semantic::SpanOne(ro_projection)}),
                    gfx::view_state{
                        .scissor = Veci4(
                            cmd.ClipRect.x,
                            fb_height - cmd.ClipRect.w,
                            cmd.ClipRect.z - cmd.ClipRect.x,
                            cmd.ClipRect.w - cmd.ClipRect.y),
                    },
                    gfx::cull_state{},
                    gfx::blend_state{}
                    );
            }
        }
    }

    //    view_.m_scissor[0] = {0, 0, fb_width, fb_height};
    //    GFX::SetViewportState(view_);

    //    GFX::SetViewportState(prev_view);
    //    GFX::SetBlendState(prev_blnd);
    //    GFX::SetRasterizerState(prev_rast);
    //    GFX::SetDepthState(prev_dept);
}

static void CreateFontsTexture(ImGuiData* im_data, gfx::api& api)
{
    DProfContext _(IM_API "Creating font atlas");
    auto         __ = api.debug().scope(IM_API "Create font atlas");

    // Build texture atlas
    ImGuiIO&       io = ImGui::GetIO();
    unsigned char* pixels;
    int            width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    using typing::PixCmp;
    using typing::pixels::BitFmt;
    auto pixelDataSize = Coffee::GetPixSize(
        BitFmt::UByte, PixCmp::RGBA, C_FCAST<u32>(width * height));

    auto surface_size = size_2d<i32>{width, height}.convert<u32>();

    im_data->font_atlas->alloc(size_3d<u32>{surface_size.w, surface_size.h, 1});
    im_data->font_atlas->upload(
        mem_chunk<byte_t const>::ofBytes(pixels, pixelDataSize).view,
        Veci2(0, 0),
        size_2d<i32>{width, height});

    im_data->font_sampler->alloc();
    im_data->font_sampler->set_filtering(
        typing::Filtering::Linear, typing::Filtering::Nearest);
}

static std::optional<ImError> CreateDeviceObjects(
    ImGuiData* im_data, gfx::api& api)
{
    DProfContext _(IM_API "Creating device data");

    constexpr std::string_view vertex_shader =
#ifdef GLEAM_USE_CORE
        "#version 330\n"
#else
        "#version 300 es\n"
#endif
        "#extension GL_ARB_explicit_uniform_location : enable\n"
        "layout(location=1) uniform mat4 ProjMtx;\n"
        "layout(location=0) in vec2 Position;\n"
        "layout(location=1) in vec2 UV;\n"
        "layout(location=2) in vec4 Color;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main()\n"
        "{\n"
        "	Frag_UV = UV;\n"
        "	Frag_Color = Color;\n"
        "	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
        "}\n";

    constexpr std::string_view fragment_shader =
#ifdef GLEAM_USE_CORE
        "#version 330\n"
#else
        "#version 300 es\n"
#endif
        "#extension GL_ARB_explicit_uniform_location : enable\n"
        "layout(location=0) uniform sampler2D Texture;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "out vec4 OutColor;\n"
        "void main()\n"
        "{\n"
        "	OutColor = Frag_Color * texture( Texture, Frag_UV.st);\n"
        "}\n";

    auto a        = api.debug().scope(IM_API "Creating device data");
    im_data->time = 0.f;

    do
    {
        DProfContext _(IM_API "Compiling shaders");

        auto& pip = im_data->pipeline;

        pip->add(
            gfx::program_t::stage_t::Vertex,
            api.alloc_shader(
                mem_chunk<byte_t const>::ofContainer(vertex_shader)));
        pip->add(
            gfx::program_t::stage_t::Fragment,
            api.alloc_shader(
                mem_chunk<byte_t const>::ofContainer(fragment_shader)));
        if(auto res = pip->compile(); res.has_error())
            cWarning("Shader compile error: {0}", res.error());
        else
            cDebug("Shader compile info: {0}", res.value());
    } while(false);

    do
    {
        using attribute_flags = gfx::vertex_attribute::attribute_flags;
        DProfContext _(IM_API "Creating vertex array object");
        auto&        a = im_data->vao;

        im_data->vertices->alloc();
        im_data->elements->alloc();

        a->alloc();
        a->add(
            {.index = 0,
             .value =
                 {
                     .offset = offsetof(ImDrawVert, pos),
                     .stride = sizeof(ImDrawVert),
                     .count  = 2,
                 },
             .buffer = {
                 .offset = 0,
                 .id     = 0,
             }});
        a->add(
            {.index = 1,
             .value =
                 {
                     .offset = offsetof(ImDrawVert, uv),
                     .stride = sizeof(ImDrawVert),
                     .count  = 2,
                 },
             .buffer = {
                 .offset = 0,
                 .id     = 0,
             }});
        a->add(
            {.index = 2,
             .value =
                 {
                     .offset = offsetof(ImDrawVert, col),
                     .stride = sizeof(ImDrawVert),
                     .count  = 4,
                     .type   = semantic::TypeEnum::UByte,
                     .flags =
                         attribute_flags::normalized | attribute_flags::packed,
                 },
             .buffer = {
                 .offset = 0,
                 .id     = 0,
             }});
        a->set_buffer(gfx::buffers::vertex, im_data->vertices, 0);
        a->set_buffer(gfx::buffers::element, im_data->elements);
    } while(false);

    CreateFontsTexture(im_data, api);

    return std::nullopt;
}

static void InvalidateDeviceObjects(ImGuiData* im_data, gfx::api& api)
{
    DProfContext _(IM_API "Invalidating device objects");
    auto         __ = api.debug().scope(IM_API "Invalidating device objects");
    im_data->vertices->dealloc();
    im_data->elements->dealloc();
    im_data->pipeline->dealloc();
    im_data->font_atlas->dealloc();
    im_data->font_sampler->dealloc();
}

// bool Init(EntityContainer& container)
//{
//     DProfContext _(IM_API "Initializing state");

//    ImGui::CreateContext();
//    ImGuiIO& io = ImGui::GetIO();

//    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
//    io.ConfigDockingWithShift    = true;
//    io.ConfigDockingAlwaysTabBar = true;

//    /* io is statically allocated, this is safe */
//    container.service<comp_app::BasicEventBus<CIEvent>>()->addEventData(
//        {100, ImGui_InputHandle});

//    for(auto const& p : ImKeyMap)
//    {
//        io.KeyMap[p.first] = p.second;
//    }

//    SetStyle();

//    return true;
//}

// void Shutdown()
//{
//     DProfContext _(IM_API "Shutting down");

//    InvalidateDeviceObjects();
//    ImGui::DestroyContext();
//}

template<typename T>
inline T const& C(c_cptr d)
{
    return *(C_FCAST<T const*>(d));
}

void ImGui_InputHandle(CIEvent& ev, c_ptr data)
{
    //    auto io      = &ImGui::GetIO();
    //    bool handled = true;

    //    switch(ev.type)
    //    {
    //    case CIEvent::TouchPan: {
    //        CIMTouchMotionEvent const& pan = C<CIMTouchMotionEvent>(data);

    //        io->MousePos = {
    //            (pan.origin.x + pan.translation.x) /
    //            io->DisplayFramebufferScale.x, (pan.origin.y +
    //            pan.translation.y) / io->DisplayFramebufferScale.y};

    //        auto btn = CIMouseButtonEvent::LeftButton;

    //        io->MouseDown[btn - 1]       = true;
    //        io->MouseClickedPos[btn - 1] = io->MousePos;

    //        break;
    //    }
    //    case CIEvent::TouchTap: {
    //        CITouchTapEvent const& tap = C<CITouchTapEvent>(data);

    //        io->MouseDown[CIMouseButtonEvent::LeftButton - 1]       =
    //        tap.pressed; io->MouseClickedPos[CIMouseButtonEvent::LeftButton -
    //        1] = {
    //            tap.pos.x / io->DisplayFramebufferScale.x,
    //            tap.pos.y / io->DisplayFramebufferScale.y};

    //        break;
    //    }
    //    case CIEvent::MouseButton: {
    //        auto ev = C<CIMouseButtonEvent>(data);
    //        if(ev.btn < 5 && ev.btn > 0)
    //        {
    //            bool flag                 = ev.mod &
    //            CIMouseButtonEvent::Pressed; io->MouseDown[ev.btn - 1] = flag;
    //            io->MouseClickedPos[ev.btn - 1] = {ev.pos.x, ev.pos.y};
    //        }
    //        break;
    //    }
    //    case CIEvent::MouseMove: {
    //        auto ev      = C<CIMouseMoveEvent>(data);
    //        io->MousePos = {
    //            (ev.origin.x + ev.delta.x) / io->DisplayFramebufferScale.x,
    //            (ev.origin.y + ev.delta.y) / io->DisplayFramebufferScale.y};

    //        break;
    //    }
    //    case CIEvent::Scroll: {
    //        auto ev        = C<CIScrollEvent>(data);
    //        io->MouseWheel = ev.delta.y;
    //        break;
    //    }
    //    case CIEvent::Keyboard: {
    //        auto ev = C<CIKeyEvent>(data);
    //        if(ev.key < 512)
    //        {
    //            if(((ev.key >= CK_a && ev.key <= CK_z) ||
    //                (ev.key >= CK_A && ev.key <= CK_Z) ||
    //                (ev.key >= CK_0 && ev.key <= CK_9)) &&
    //               (ev.mod & CIKeyEvent::RepeatedModifier ||
    //                ev.mod & CIKeyEvent::PressedModifier))
    //                io->AddInputCharacter(C_CAST<ImWchar>(ev.key));

    //            io->KeysDown[ev.key] = ev.mod & CIKeyEvent::PressedModifier;

    //            //            io->KeyAlt   = ev.mod &
    //            CIKeyEvent::LAltModifier;
    //            //            io->KeyCtrl  = ev.mod &
    //            CIKeyEvent::LCtrlModifier;
    //            //            io->KeyShift = ev.mod &
    //            CIKeyEvent::LShiftModifier;
    //            //            io->KeySuper = ev.mod &
    //            CIKeyEvent::SuperModifier;

    //            switch(ev.key)
    //            {
    //            case CK_LShift:
    //            case CK_RShift:
    //                io->KeyShift = true;
    //                break;
    //            case CK_LCtrl:
    //            case CK_RCtrl:
    //                io->KeyCtrl = true;
    //                break;
    //            case CK_AltGr:
    //            case CK_LAlt:
    //                io->KeyAlt = true;
    //                break;
    //            case CK_LSuper:
    //            case CK_RSuper:
    //                io->KeyShift = true;
    //                break;
    //            }
    //        }
    //        break;
    //    }
    //    case CIEvent::TextInput: {
    //        auto ev = C<CIWriteEvent>(data);
    //        io->AddInputCharactersUTF8(ev.text);
    //        break;
    //    }
    //    case CIEvent::TextEdit: {
    //        auto ev = C<CIWEditEvent>(data);
    //        io->AddInputCharactersUTF8(ev.text);
    //        break;
    //    }
    //    default:
    //        handled = false;
    //        break;
    //    }

    //    if(handled && (io->WantCaptureMouse || io->WantCaptureKeyboard))
    //    {
    //        /* Discard the event for all other handlers */
    //        ev.type = CIEvent::NoneType;
    //    }
}

static void SetStyle()
{
    DProfContext _(IM_API "Applying custom style");
    ImGuiStyle&  style      = ImGui::GetStyle();
    style.WindowRounding    = 3.f;
    style.FrameRounding     = 3.f;
    style.ScrollbarRounding = 0;
    style.FramePadding      = {3.f, 3.f};

    style.Colors[ImGuiCol_PlotHistogram]        = ImVec4(.3f, .3f, .9f, 1.f);
    style.Colors[ImGuiCol_PlotLines]            = ImVec4(.3f, .3f, .9f, 1.f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(.2f, .2f, .9f, 1.f);
    style.Colors[ImGuiCol_PlotLinesHovered]     = ImVec4(.2f, .2f, .9f, 1.f);

    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(.2f, .2f, .9f, 1.f);
    style.Colors[ImGuiCol_SliderGrabActive]    = ImVec4(.2f, .2f, .9f, 1.f);

    style.Colors[ImGuiCol_Button]        = ImVec4(.2f, .2f, .9f, 1.f);
    style.Colors[ImGuiCol_ButtonActive]  = ImVec4(.3f, .3f, .9f, 1.f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(.5f, .5f, .9f, 1.f);

    style.Colors[ImGuiCol_FrameBg]        = ImVec4(.2f, .2f, .5f, 1.f);
    style.Colors[ImGuiCol_FrameBgActive]  = ImVec4(.3f, .3f, .5f, 1.f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(.4f, .4f, .5f, 1.f);

    return;

    style.Colors[ImGuiCol_Text]         = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
    style.Colors[ImGuiCol_WindowBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.98f);
    //   style.Colors[ImGuiCol_ChildWindowBg]  = ImVec4(1.00f, 1.00f, 1.00f,
    //   0.01f);
    style.Colors[ImGuiCol_PopupBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.99f);
    style.Colors[ImGuiCol_Border]         = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    style.Colors[ImGuiCol_BorderShadow]   = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]        = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_FrameBgActive]  = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
    style.Colors[ImGuiCol_TitleBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.99f);
    style.Colors[ImGuiCol_TitleBgCollapsed]
        = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.00f, 0.00f, 0.99f);
    style.Colors[ImGuiCol_MenuBarBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.98f);
    style.Colors[ImGuiCol_ScrollbarBg]   = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]
        = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]
        = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
    //    style.Colors[ImGuiCol_ComboBg]    =
    //    ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]  = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive]
        = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_Button]        = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    style.Colors[ImGuiCol_ButtonActive]  = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_Header]        = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.25f);
    style.Colors[ImGuiCol_HeaderActive]  = ImVec4(1.00f, 1.00f, 1.00f, 0.15f);
    //    style.Colors[ImGuiCol_Column]        = ImVec4(1.00f, 1.00f, 1.00f,
    //    0.00f); style.Colors[ImGuiCol_ColumnHovered] =
    //    ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    //    style.Colors[ImGuiCol_ColumnActive]  = ImVec4(1.00f, 1.00f, 1.00f,
    //    0.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
    style.Colors[ImGuiCol_ResizeGripHovered]
        = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_ResizeGripActive]
        = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    //    style.Colors[ImGuiCol_CloseButton] = ImVec4(1.00f, 1.00f, 1.00f,
    //    0.20f); style.Colors[ImGuiCol_CloseButtonHovered] =
    //        ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
    //    style.Colors[ImGuiCol_CloseButtonActive] =
    //        ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]
        = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    style.Colors[ImGuiCol_PlotHistogramHovered]
        = ImVec4(1.00f, 1.00f, 1.00f, 0.80f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_ModalWindowDarkening]
        = ImVec4(0.00f, 0.00f, 0.00f, 0.80f);

    for(auto i : Range<>(ImGuiCol_COUNT))
    {
        auto alpha = style.Colors[i].w;

        auto col = ImVec4(0.f, 0.f, 0.f, alpha);

        if(style.Colors[i].x > 0.5f)
            col = ImVec4(0.3f, 0.3f, 1.f, alpha);

        style.Colors[i] = col;
    }

    style.Colors[ImGuiCol_ResizeGrip]        = ImVec4(0.2f, 0.2f, 1.f, 1.f);
    style.Colors[ImGuiCol_ResizeGripActive]  = ImVec4(0.4f, 0.4f, 1.f, 1.f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.3f, 0.3f, 1.f, 1.f);

    style.Colors[ImGuiCol_PlotHistogram]        = ImVec4(.9f, .9f, .9f, 1.f);
    style.Colors[ImGuiCol_PlotLines]            = ImVec4(.9f, .9f, .9f, 1.f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.f, 1.f, 1.f, 1.f);
    style.Colors[ImGuiCol_PlotLinesHovered]     = ImVec4(1.f, 1.f, 1.f, 1.f);

    //    style.Colors[ImGuiCol_ComboBg] = ImVec4(.0f, .0f, .0f, 1.00f);

    style.Colors[ImGuiCol_Border] = ImVec4(.9f, .9f, .9f, 1.f);
}

void ImGuiDataDeleter::operator()(ImGuiData* p)
{
    delete p;
}

ImGuiSystem::ImGuiSystem(gfx::api& api) : m_api(api)
{
}

void ImGuiSystem::load(entity_container& e, comp_app::app_error& ec)
{
    DProfContext _(IM_API "Initializing state");

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift    = true;
    io.ConfigDockingAlwaysTabBar = true;

    /* io is statically allocated, this is safe */
    //    e.service<comp_app::BasicEventBus<CIEvent>>()->addEventData(
    //        {100, ImGui_InputHandle});

    for(auto const& p : ImKeyMap)
    {
        io.KeyMap[p.first] = p.second;
    }

    SetStyle();

    m_textInputActive = false;

    e.register_component_inplace<ImGuiWidget>();

    auto ibus = e.service<comp_app::BasicEventBus<CIEvent>>();

    ibus->addEventFunction<CIMouseButtonEvent>(
        0, [](CIEvent& e, CIMouseButtonEvent* mouse) {
            ImGuiIO& io = ImGui::GetIO();

            u32 index = 0;
            switch(mouse->btn)
            {
            case CIMouseButtonEvent::LeftButton:
                index = 0;
                break;
            case CIMouseButtonEvent::MiddleButton:
                index = 1;
                break;
            case CIMouseButtonEvent::RightButton:
                index = 2;
                break;
            default:
                break;
            }

            bool pressed
                = static_cast<bool>(mouse->mod & CIMouseButtonEvent::Pressed);
            bool double_press = static_cast<bool>(
                mouse->mod & CIMouseButtonEvent::DoubleClick);

            io.MouseDown[index] = pressed;

            if(pressed)
            {
                io.MouseClicked[index]     = pressed;
                io.MouseClickedPos[index]  = io.MousePos;
                io.MouseClickedTime[index] = ImGui::GetTime();
                cDebug("Clicked {0}", index);
            }

            if(double_press)
            {
                io.MouseDoubleClicked[index] = double_press;
                io.MouseDoubleClickTime      = ImGui::GetTime();
            }
        });
}

void ImGuiSystem::unload(entity_container& e, comp_app::app_error& ec)
{
    DProfContext _(IM_API "Shutting down");
    InvalidateDeviceObjects(m_im_data.get(), m_api);
}

void ImGuiSystem::start_restricted(Proxy& p, time_point const& t)
{
    DProfContext _(IM_API "Preparing frame data");

    if(!m_im_data)
    {
        m_im_data = std::unique_ptr<ImGuiData, ImGuiDataDeleter>(
            new ImGuiData{m_api});
        if(auto ec = CreateDeviceObjects(m_im_data.get(), m_api))
            return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    auto s = p.service<comp_app::Windowing>()->size();

    f32 uiScaling = 1.f;

    if(auto display = p.service<comp_app::DisplayInfo>())
        uiScaling = display->dpi(display->currentDisplay());

    io.DisplaySize             = ImVec2(s.w / uiScaling, s.h / uiScaling);
    io.DisplayFramebufferScale = ImVec2(uiScaling, uiScaling);

    // Setup time step
    f64 time        = ImGui::GetTime();
    io.DeltaTime    = time > 0.0 ? time - m_im_data->time : 1.0 / 60.0;
    m_im_data->time = time;

    // Setup inputs
    auto mouse      = p.service<comp_app::MouseInput>();
    auto pos        = mouse->position();
    io.MousePosPrev = io.MousePos;
    io.MousePos     = ImVec2(pos.x / uiScaling, pos.y / uiScaling);
    //    if(!io.MouseClicked[0])
    //        io.MouseClicked[0] = mouse->buttons() &
    //        CIMouseButtonEvent::LeftButton;
    //    if(!io.MouseClicked[1])
    //        io.MouseClicked[1]
    //            = mouse->buttons() & CIMouseButtonEvent::MiddleButton;
    //    if(!io.MouseClicked[2])
    //        io.MouseClicked[2] = mouse->buttons() &
    //        CIMouseButtonEvent::RightButton;
    //    for(auto i : Range<>{5})
    //        if(io.MouseClicked[i])
    //        {
    //            io.MouseDown[i]        = true;
    //            io.MouseClickedPos[i]  = io.MousePos;
    //            io.MouseClickedTime[i] = time;
    //        } else
    //        {
    //            io.MouseDown[i] = false;
    //        }

    io.MouseWheel     = m_im_data->scroll;
    m_im_data->scroll = 0.0f;

    // Start the frame
    DProfContext __(IM_API "Running ImGui::NewFrame()");
    ImGui::NewFrame();

    if(auto keyboard = p.service<comp_app::KeyboardInput>())
    {
        if(io.WantTextInput && !m_textInputActive)
        {
            m_textInputActive = true;
            keyboard->openVirtual();
        } else if(!io.WantTextInput && m_textInputActive)
        {
            m_textInputActive = false;
            keyboard->closeVirtual();
        }
    }

    auto delta = duration_cast<duration>(t - m_previousTime);

    {
        for(auto& widget : p.select<ImGuiWidget>())
            p.get<ImGuiWidget>(widget.id)->func(get_container(p), t, delta);
    }

    m_previousTime = t;
}

void ImGuiSystem::end_restricted(Proxy&, time_point const&)
{
    DProfContext _(IM_API "Rendering UI");

    ImGui::Render();
    draw_items(m_im_data.get(), ImGui::GetDrawData(), m_api);
}

} // namespace imgui::detail

namespace imgui::widgets {

ImGuiWidget StatsMenu()
{
    return {
        .name = "framerate stats",
        .func =
            [m_values = std::vector<f32>(), m_index = szptr(0)](
                EntityContainer&,
                time_point const&,
                duration const& delta) mutable {
                m_values.resize(50);

                const auto delta_ms
                    = duration_cast<seconds_float>(delta).count();

                ImGui::BeginMainMenuBar();
                ImGui::Columns(4);

                ImGui::TextColored({1, 1, 1, 1}, "ms=%f", delta_ms);

                ImGui::NextColumn();

                ImGui::NextColumn();

                ImGui::NextColumn();

                m_values.at(m_index) = delta_ms;
                ImGui::PlotHistogram(
                    "",
                    m_values.data(),
                    m_values.size(),
                    0,
                    "",
                    0.f,
                    0.1f,
                    {100, 24});
                m_index = (++m_index) % m_values.size();

                ImGui::NextColumn();
                ImGui::EndMainMenuBar();
            },
    };
}

} // namespace imgui::widgets
