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
#include <coffee/graphics/apis/CGLeamRHI>
#include <coffee/imgui/imgui_binding.h>
#include <coffee/interfaces/cgraphics_util.h>
#include <peripherals/libc/memory_ops.h>

#include <coffee/graphics/apis/gleam/levels/all_levels.h>

#include <coffee/strings/libc_types.h>

#include <coffee/core/CDebug>

#define IM_API "ImGui::"

using namespace Coffee;
using namespace Display;
using namespace Input;

#if defined(COFFEE_IMGUI_USE_GLEAM)
using GFX = RHI::GLEAM::GLEAM_API;
#else
using GFX = RHI::NullAPI;
#endif

static const Vector<Pair<ImGuiKey_, u16>> ImKeyMap = {
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
};

C_UNUSED(STATICINLINE ImGuiKey CfToImKey(u32 k))
{
    auto it = std::find_if(
        ImKeyMap.begin(), ImKeyMap.end(), [k](Pair<ImGuiKey_, u16> const& p) {
            return p.second == k;
        });
    if(it != ImKeyMap.end())
        return it->first;
    return 0;
}

C_UNUSED(STATICINLINE u32 ImToCfKey(ImGuiKey k))
{
    auto it = std::find_if(
        ImKeyMap.begin(), ImKeyMap.end(), [k](Pair<ImGuiKey_, u16> const& p) {
            return p.first == k;
        });
    if(it != ImKeyMap.end())
        return it->second;
    return 0;
}

struct ImGuiData : State::GlobalState
{
    ImGuiData() :
        attributes(), pipeline(MkShared<GFX::PIP>()),
        vertices(RSCA::Streaming | RSCA::WriteOnly, 0),
        elements(RSCA::Streaming | RSCA::WriteOnly, 0), shader_view(pipeline),
        fonts(PixFmt::RGBA8)
    {
        fonts_sampler.attach(&fonts);
    }
    ~ImGuiData();

    GFX::V_DESC     attributes;
    ShPtr<GFX::PIP> pipeline;
    GFX::BUF_A      vertices;
    GFX::BUF_E      elements;

    RHI::shader_param_view<GFX> shader_view;

    GFX::SM_2D fonts_sampler;
    GFX::S_2D  fonts;

    Matf4 projection_matrix;

    f32 time;
    f32 scroll;

    u32 _pad;
};

ImGuiData::~ImGuiData()
{
    GFX::ERROR ec;
    vertices.dealloc();
    elements.dealloc();
    attributes.dealloc();
    pipeline->dealloc(ec);
    fonts.dealloc();
    fonts_sampler.dealloc();
}

// This is the main rendering function that you have to implement and provide to
// ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure) If text
// or lines are blurry when integrating ImGui in your engine:
// - in your Render function, try translating your projection matrix by
// (0.5f,0.5f) or (0.375f,0.375f)
static void ImGui_ImplSdlGL3_RenderDrawLists(ImDrawData* draw_data)
{
    const auto im_data = C_DCAST<ImGuiData>(State::PeekState("im_data").get());

    // Avoid rendering when minimized, scale coordinates for retina displays
    // (screen coordinates != framebuffer coordinates)
    GFX::DBG::SCOPE a(IM_API "ImGui render");
    DProfContext    _(IM_API "Rendering draw lists");

    ImGuiIO& io        = ImGui::GetIO();
    int      fb_width  = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int      fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if(fb_width == 0 || fb_height == 0)
        return;
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

    GFX::BLNDSTATE prev_blnd;
    GFX::VIEWSTATE prev_view;
    GFX::RASTSTATE prev_rast;
    GFX::DEPTSTATE prev_dept;

    GFX::GetBlendState(prev_blnd);
    GFX::GetViewportState(prev_view);
    GFX::GetRasterizerState(prev_rast);
    GFX::GetDepthState(prev_dept);

    GFX::BLNDSTATE blend;
    blend.m_doBlend = true;
    GFX::RASTSTATE raster;
    raster.m_culling = C_CAST<u32>(RHI::Datatypes::Face::Front);
    GFX::DEPTSTATE depth;
    depth.m_test = false;
    GFX::VIEWSTATE view_(1);

    view_.m_view.clear();
    view_.m_depth.clear();

    GFX::SetBlendState(blend);
    GFX::SetRasterizerState(raster);
    GFX::SetDepthState(depth);

    const float ortho_projection[4][4] = {
        {2.0f / io.DisplaySize.x, 0.0f, 0.0f, 0.0f},
        {0.0f, 2.0f / -io.DisplaySize.y, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f, 1.0f},
    };

    auto target = Bytes::From(im_data->projection_matrix);
    auto source = Bytes::From(C_RCAST<const float*>(ortho_projection), 16);

    MemCpy(source, target);

    GFX::D_CALL dc(true, false);
    GFX::D_DATA dd;
    dd.m_eltype =
        (sizeof(ImDrawIdx) == 2) ? RHI::TypeEnum::UShort : RHI::TypeEnum::UInt;

    for(int n = 0; n < draw_data->CmdListsCount; n++)
    {
        GFX::DBG::SCOPE _(IM_API "Command list");

        auto cmd_list = draw_data->CmdLists[n];
        dd.m_eoff     = 0;

        im_data->vertices.commit(
            cmd_list->VtxBuffer.Size * sizeof(ImDrawVert),
            cmd_list->VtxBuffer.Data);
        im_data->elements.commit(
            cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx),
            cmd_list->IdxBuffer.Data);

        for(int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            GFX::DBG::SCOPE _(IM_API "Command buffer");

            auto cmd   = &cmd_list->CmdBuffer[cmd_i];
            dd.m_elems = cmd->ElemCount;
            if(cmd->UserCallback)
                cmd->UserCallback(cmd_list, cmd);
            else
            {
                view_.m_scissor[0] = {
                    C_CAST<i32>(cmd->ClipRect.x),
                    C_CAST<i32>(fb_height - cmd->ClipRect.w),
                    C_CAST<i32>(cmd->ClipRect.z - cmd->ClipRect.x),
                    C_CAST<i32>(cmd->ClipRect.w - cmd->ClipRect.y)};

                GFX::SetViewportState(view_);
                /* TODO: Improve this by using batching structure,
                 *  D_DATA arrays */
                GFX::Draw(
                    *im_data->pipeline,
                    im_data->shader_view.get_state(),
                    im_data->attributes,
                    dc,
                    dd);
            }
            dd.m_eoff += cmd->ElemCount;
        }
    }

    view_.m_scissor[0] = {0, 0, fb_width, fb_height};
    //    GFX::SetViewportState(view_);

    GFX::SetViewportState(prev_view);
    GFX::SetBlendState(prev_blnd);
    GFX::SetRasterizerState(prev_rast);
    GFX::SetDepthState(prev_dept);
}

static const char* ImGui_ImplSdlGL3_GetClipboardText(void*)
{
    return nullptr;
}

static void ImGui_ImplSdlGL3_SetClipboardText(void*, const char*)
{
    //    SDL_SetClipboardText(text);
}

void ImGui_ImplSdlGL3_CreateFontsTexture()
{
    const auto im_data = C_DCAST<ImGuiData>(State::PeekState("im_data").get());

    DProfContext    _(IM_API "Creating font atlas");
    GFX::DBG::SCOPE a(IM_API "Create font atlas");

    // Build texture atlas
    ImGuiIO&       io = ImGui::GetIO();
    unsigned char* pixels;
    int            width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    auto pixelDataSize =
        GetPixSize(BitFmt::UByte, PixCmp::RGBA, C_FCAST<u32>(width * height));

    auto& s  = im_data->fonts;
    auto& sm = im_data->fonts_sampler;

    auto surface_size = size_2d<i32>{width, height}.convert<u32>();

    s.allocate(surface_size, PixCmp::RGBA);
    s.upload(
        {s.m_pixfmt, BitFmt::UByte, PixCmp::RGBA},
        surface_size,
        Bytes::From(pixels, pixelDataSize));

    sm.alloc();
    sm.setFiltering(Filtering::Linear, Filtering::Linear);

    io.Fonts->TexID = libc::ptr::put_value(s.glTexHandle());
}

template<typename T>
inline T const& C(c_cptr d)
{
    return *(C_FCAST<T const*>(d));
}

void ImGui_InputHandle(CIEvent const& ev, c_ptr data)
{
    auto io = &ImGui::GetIO();

    switch(ev.type)
    {
    case CIEvent::TouchPan:
    {
        CIMTouchMotionEvent const& pan = C<CIMTouchMotionEvent>(data);

        io->MousePos = {
            (pan.origin.x + pan.translation.x) / io->DisplayFramebufferScale.x,
            (pan.origin.y + pan.translation.y) / io->DisplayFramebufferScale.y};

        auto btn = CIMouseButtonEvent::LeftButton;

        io->MouseDown[btn - 1]       = true;
        io->MouseClickedPos[btn - 1] = io->MousePos;

        break;
    }
    case CIEvent::TouchTap:
    {
        CITouchTapEvent const& tap = C<CITouchTapEvent>(data);

        io->MouseDown[CIMouseButtonEvent::LeftButton - 1]       = tap.pressed;
        io->MouseClickedPos[CIMouseButtonEvent::LeftButton - 1] = {
            tap.pos.x / io->DisplayFramebufferScale.x,
            tap.pos.y / io->DisplayFramebufferScale.y};

        break;
    }
    case CIEvent::MouseButton:
    {
        auto ev = C<CIMouseButtonEvent>(data);
        if(ev.btn < 5 && ev.btn > 0)
        {
            bool flag                 = ev.mod & CIMouseButtonEvent::Pressed;
            io->MouseDown[ev.btn - 1] = flag;
            io->MouseClickedPos[ev.btn - 1] = {ev.pos.x, ev.pos.y};
        }
        break;
    }
    case CIEvent::MouseMove:
    {
        auto ev      = C<CIMouseMoveEvent>(data);
        io->MousePos = {
            (ev.origin.x + ev.delta.x) / io->DisplayFramebufferScale.x,
            (ev.origin.y + ev.delta.y) / io->DisplayFramebufferScale.y};

        break;
    }
    case CIEvent::Scroll:
    {
        auto ev        = C<CIScrollEvent>(data);
        io->MouseWheel = ev.delta.y;
        break;
    }
    case CIEvent::Keyboard:
    {
        auto ev = C<CIKeyEvent>(data);
        if(ev.key < 512)
        {
            if(((ev.key >= CK_a && ev.key <= CK_z) ||
                (ev.key >= CK_A && ev.key <= CK_Z) ||
                (ev.key >= CK_0 && ev.key <= CK_9)) &&
               (ev.mod & CIKeyEvent::RepeatedModifier ||
                ev.mod & CIKeyEvent::PressedModifier))
                io->AddInputCharacter(C_CAST<ImWchar>(ev.key));

            io->KeysDown[ev.key] = ev.mod & CIKeyEvent::PressedModifier;

            io->KeyAlt   = ev.mod & CIKeyEvent::LAltModifier;
            io->KeyCtrl  = ev.mod & CIKeyEvent::LCtrlModifier;
            io->KeyShift = ev.mod & CIKeyEvent::LShiftModifier;
            io->KeySuper = ev.mod & CIKeyEvent::SuperModifier;

            switch(ev.key)
            {
            case CK_LShift:
            case CK_RShift:
                io->KeyShift = true;
                break;
            case CK_LCtrl:
            case CK_RCtrl:
                io->KeyCtrl = true;
                break;
            case CK_AltGr:
            case CK_LAlt:
                io->KeyAlt = true;
                break;
            case CK_LSuper:
            case CK_RSuper:
                io->KeyShift = true;
                break;
            }
        }
        break;
    }
    case CIEvent::TextInput:
    {
        auto ev = C<CIWriteEvent>(data);
        io->AddInputCharactersUTF8(ev.text);
        break;
    }
    case CIEvent::TextEdit:
    {
        auto ev = C<CIWEditEvent>(data);
        io->AddInputCharactersUTF8(ev.text);
        break;
    }
    default:
        break;
    }
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

    style.Colors[ImGuiCol_Text]           = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]   = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
    style.Colors[ImGuiCol_WindowBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.98f);
    style.Colors[ImGuiCol_ChildWindowBg]  = ImVec4(1.00f, 1.00f, 1.00f, 0.01f);
    style.Colors[ImGuiCol_PopupBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.99f);
    style.Colors[ImGuiCol_Border]         = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    style.Colors[ImGuiCol_BorderShadow]   = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]        = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_FrameBgActive]  = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
    style.Colors[ImGuiCol_TitleBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.99f);
    style.Colors[ImGuiCol_TitleBgCollapsed] =
        ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.00f, 0.00f, 0.99f);
    style.Colors[ImGuiCol_MenuBarBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.98f);
    style.Colors[ImGuiCol_ScrollbarBg]   = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] =
        ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] =
        ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
    style.Colors[ImGuiCol_ComboBg]    = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]  = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] =
        ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_Button]        = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    style.Colors[ImGuiCol_ButtonActive]  = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_Header]        = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.25f);
    style.Colors[ImGuiCol_HeaderActive]  = ImVec4(1.00f, 1.00f, 1.00f, 0.15f);
    style.Colors[ImGuiCol_Column]        = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_ColumnHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_ColumnActive]  = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_ResizeGrip]    = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
    style.Colors[ImGuiCol_ResizeGripHovered] =
        ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_ResizeGripActive] =
        ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_CloseButton] = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    style.Colors[ImGuiCol_CloseButtonHovered] =
        ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
    style.Colors[ImGuiCol_CloseButtonActive] =
        ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] =
        ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    style.Colors[ImGuiCol_PlotHistogramHovered] =
        ImVec4(1.00f, 1.00f, 1.00f, 0.80f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_ModalWindowDarkening] =
        ImVec4(0.00f, 0.00f, 0.00f, 0.80f);

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

    style.Colors[ImGuiCol_ComboBg] = ImVec4(.0f, .0f, .0f, 1.00f);

    style.Colors[ImGuiCol_Border] = ImVec4(.9f, .9f, .9f, 1.f);
}

namespace Coffee {
namespace CImGui {

bool CreateDeviceObjects(imgui_error_code& ec)
{
    DProfContext _(IM_API "Creating device data");

    constexpr cstring vertex_shader =
#if defined(COFFEE_GLEAM_DESKTOP)
        "#version 330\n"
#else
        "#version 300 es\n"
#endif
        "uniform mat4 ProjMtx;\n"
        "in vec2 Position;\n"
        "in vec2 UV;\n"
        "in vec4 Color;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main()\n"
        "{\n"
        "	Frag_UV = UV;\n"
        "	Frag_Color = Color;\n"
        "	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
        "}\n";

    constexpr cstring fragment_shader =
#if defined(COFFEE_GLEAM_DESKTOP)
        "#version 330\n"
#else
        "#version 300 es\n"
#endif
        "uniform sampler2D Texture;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
#if !defined(COFFEE_GLES20_MODE)
        "out vec4 OutColor;\n"
#endif
        "void main()\n"
        "{\n"
        "	OutColor = Frag_Color * texture( Texture, Frag_UV.st);\n"
        "}\n";

    auto im_data = C_DCAST<ImGuiData>(State::PeekState("im_data").get());

    if(im_data)
    {
        ec = ImError::AlreadyLoaded;
        return true;
    }

    GFX::DBG::SCOPE a(IM_API "Creating device data");

    State::SwapState("im_data", MkShared<ImGuiData>());
    im_data = C_DCAST<ImGuiData>(State::PeekState("im_data").get());

    if(!im_data)
    {
        ec = ImError::GlobalStateFailure;
        return false;
    }

    im_data->time = 0.f;

    i32 attr_idx[3] = {-1, -1, -1};

    do
    {
        DProfContext _(IM_API "Allocating vertex objects");
        im_data->attributes.alloc();
        im_data->vertices.alloc();
        im_data->elements.alloc();
    } while(false);

    do
    {
        DProfContext            _(IM_API "Compiling shaders");
        RHI::GLEAM::gleam_error gec;

        GFX::SHD vert;
        GFX::SHD frag;
        auto&    pip = im_data->pipeline;

        auto vd = Bytes::CreateString(vertex_shader);
        if(!vert.compile(RHI::ShaderStage::Vertex, vd, gec))
        {
            ec = ImError::ShaderCompilation;
            ec = gec.error_message;
            return false;
        }

        auto fd = Bytes::CreateString(fragment_shader);
        if(!frag.compile(RHI::ShaderStage::Fragment, fd, gec))
        {
            ec = ImError::ShaderCompilation;
            ec = gec.error_message;
            return false;
        }

        auto& vert_owned = pip->storeShader(std::move(vert));
        auto& frag_owned = pip->storeShader(std::move(frag));

        if(!pip->attach(vert_owned, RHI::ShaderStage::Vertex, gec))
        {
            ec = ImError::ShaderAttach;
            ec = gec.message();
            return false;
        }

        if(!pip->attach(frag_owned, RHI::ShaderStage::Fragment, gec))
        {
            ec = ImError::ShaderAttach;
            ec = gec.message();
            return false;
        }

        if(!pip->assemble(gec))
        {
            ec = ImError::ShaderAttach;
            ec = gec.message();
            return false;
        }

        Profiler::DeepPushContext(IM_API "Getting shader properties");
        im_data->shader_view.get_pipeline_params();
        Profiler::DeepPopContext();

        for(auto const& unif : im_data->shader_view.constants())
        {
            if(unif.m_name == "Texture")
                im_data->shader_view.set_sampler(
                    unif, im_data->fonts_sampler.handle());
            if(unif.m_name == "ProjMtx")
                im_data->shader_view.set_constant(
                    unif, Bytes::Create(im_data->projection_matrix));
        }

        im_data->shader_view.build_state();

        for(auto const& attr : im_data->shader_view.params())
        {
            if(attr.m_name == "Position")
                attr_idx[0] = attr.m_idx;
            if(attr.m_name == "UV")
                attr_idx[1] = attr.m_idx;
            if(attr.m_name == "Color")
                attr_idx[2] = attr.m_idx;
        }
    } while(false);

    do
    {
        DProfContext _(IM_API "Creating vertex array object");

        GFX::V_ATTR pos;
        GFX::V_ATTR tex;
        GFX::V_ATTR col;
        auto&       a = im_data->attributes;

        pos.m_idx = C_FCAST<u32>(attr_idx[0]);
        tex.m_idx = C_FCAST<u32>(attr_idx[1]);
        col.m_idx = C_FCAST<u32>(attr_idx[2]);

        pos.m_size = tex.m_size = 2;
        col.m_size              = 4;

        pos.m_stride = tex.m_stride = col.m_stride = sizeof(ImDrawVert);
        pos.m_off                                  = offsetof(ImDrawVert, pos);
        tex.m_off                                  = offsetof(ImDrawVert, uv);
        col.m_off                                  = offsetof(ImDrawVert, col);
        col.m_type                                 = RHI::TypeEnum::UByte;
        col.m_flags = GFX::AttributePacked | GFX::AttributeNormalization;

        a.addAttribute(pos);
        a.addAttribute(tex);
        a.addAttribute(col);

        a.bindBuffer(0, im_data->vertices);
        a.setIndexBuffer(&im_data->elements);
    } while(false);

    ImGui_ImplSdlGL3_CreateFontsTexture();

    return true;
}

void InvalidateDeviceObjects(imgui_error_code& ec)
{
    const auto im_data = C_DCAST<ImGuiData>(State::PeekState("im_data").get());

    if(im_data)
    {
        DProfContext    _(IM_API "Invalidating device objects");
        GFX::DBG::SCOPE a(IM_API "Invalidating device objects");
        im_data->vertices.dealloc();
        im_data->elements.dealloc();
        im_data->attributes.dealloc();
    } else
        ec = ImError::AlreadyUnloaded;
}

bool Init(Components::EntityContainer& container)
{
    DProfContext _(IM_API "Initializing state");

    ImGuiIO& io = ImGui::GetIO();

    /* io is statically allocated, this is safe */
    auto io_ptr = &io;
    container.service<comp_app::BasicEventBus<CIEvent>>()->addEventData(
        {100, ImGui_InputHandle});

    for(auto const& p : ImKeyMap)
    {
        io.KeyMap[p.first] = p.second;
    }

    io.RenderDrawListsFn =
        ImGui_ImplSdlGL3_RenderDrawLists; // Alternatively you can set this to
                                          // NULL and call ImGui::GetDrawData()
                                          // after ImGui::Render() to get the
                                          // same ImDrawData pointer.
    io.SetClipboardTextFn = ImGui_ImplSdlGL3_SetClipboardText;
    io.GetClipboardTextFn = ImGui_ImplSdlGL3_GetClipboardText;
    io.ClipboardUserData  = nullptr;

    SetStyle();

    return true;
}

void Shutdown()
{
    DProfContext _(IM_API "Shutting down");

    State::SwapState("im_data", {});

    imgui_error_code ec;

    InvalidateDeviceObjects(ec);
    ImGui::Shutdown();
}

void NewFrame(Components::EntityContainer& container)
{
    auto im_data = C_DCAST<ImGuiData>(State::PeekState("im_data").get());
    DProfContext _(IM_API "Preparing frame data");

    imgui_error_code ec;

    if(!im_data || !im_data->pipeline->pipelineHandle())
    {
        CreateDeviceObjects(ec);
        im_data = C_DCAST<ImGuiData>(State::PeekState("im_data").get());
    }

    C_ERROR_CHECK(ec);

    ImGuiIO& io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    auto s = container.service<comp_app::Windowing>()->size();

    scalar uiScaling = PlatformData::DeviceDPI();

    io.DisplaySize             = ImVec2(s.w / uiScaling, s.h / uiScaling);
    io.DisplayFramebufferScale = ImVec2(uiScaling, uiScaling);

    // Setup time step
    f32 time     = C_CAST<f32>(0);
    io.DeltaTime = im_data->time > 0.0f ? C_CAST<f32>(time - im_data->time)
                                        : C_CAST<f32>(1.0f / 60.0f);
    im_data->time = time;

    // Setup inputs
#if !defined(COFFEE_ANDROID) && !defined(COFFEE_APPLE_MOBILE)
    auto pos    = container.service<comp_app::MouseInput>()->position();
    io.MousePos = ImVec2(pos.x / uiScaling, pos.y / uiScaling);
#else
    io.MouseDown[CIMouseButtonEvent::LeftButton - 1] = false;
#endif

    io.MouseWheel   = im_data->scroll;
    im_data->scroll = 0.0f;

    // Start the frame
    DProfContext __(IM_API "Running ImGui::NewFrame()");
    ImGui::NewFrame();
}

void EndFrame()
{
    DProfContext _(IM_API "Rendering UI");

    ImGui::Render();
}

const char* imgui_error_category::name() const noexcept
{
    return "imgui_error_category";
}

std::string imgui_error_category::message(int code) const
{
    using E = ImError;

    E error = C_CAST<E>(code);

    switch(error)
    {
    case E::ShaderCompilation:
        return "Shader compilation failed";
    case E::ShaderAttach:
        return "Shader attach failed";
    case E::AlreadyLoaded:
        return "ImGui is already loaded";
    case E::AlreadyUnloaded:
        return "ImGui is already unloaded";
    case E::InvalidDisplaySize:
        return "Display size is 0x0";

    case E::GlobalStateFailure:
        return "Failed to initialize global state";
    }

    C_ERROR_CODE_OUT_OF_BOUNDS();
}

void ImGuiSystem::load(entity_container& e, comp_app::app_error& ec)
{
    Init(e);
    priority          = 512;
    m_textInputActive = false;
}

void ImGuiSystem::unload(entity_container& e, comp_app::app_error& ec)
{
    Shutdown();
}

void ImGuiSystem::start_restricted(Proxy& p, Components::time_point const& t)
{
    NewFrame(get_container(p));

    auto  keyboard = p.service<comp_app::KeyboardInput>();
    auto& io       = ImGui::GetIO();

    if(keyboard)
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

    auto delta = Chrono::duration_cast<duration>(t - m_previousTime);

    for(auto& widget : m_widgets)
        widget(get_container(p), t, delta);

    m_previousTime = t;
}

void ImGuiSystem::end_restricted(Proxy&, Components::time_point const&)
{
    EndFrame();
}

ImGuiSystem& ImGuiSystem::addWidget(ImGuiWidget&& widget)
{
    m_widgets.emplace_back(std::move(widget));
    return *this;
}

ImGuiWidget Widgets::StatsMenu()
{
    return [ m_values = Vector<scalar>(), m_index = szptr(0) ](
        Components::EntityContainer&,
        Components::time_point const&,
        Components::duration const& delta) mutable
    {
        m_values.resize(50);

        const auto delta_ms =
            Chrono::duration_cast<Chrono::seconds_float>(delta).count();

        ImGui::BeginMainMenuBar();
        ImGui::Columns(4);

        ImGui::TextColored({1, 1, 1, 1}, "ms=%f", delta_ms);

        ImGui::NextColumn();

        ImGui::NextColumn();

        ImGui::NextColumn();

        m_values.at(m_index) = delta_ms;
        ImGui::PlotHistogram(
            "", m_values.data(), m_values.size(), 0, "", 0.f, 0.1f, {100, 24});
        m_index = (++m_index) % m_values.size();

        ImGui::NextColumn();
        ImGui::EndMainMenuBar();
    };
}

} // namespace CImGui
} // namespace Coffee
