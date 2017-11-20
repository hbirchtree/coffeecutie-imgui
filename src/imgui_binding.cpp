// ImGui SDL2 binding with OpenGL3
// In this binding, ImTextureID is used to store an OpenGL 'GLuint' texture identifier. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include <CoffeeDef.h>
#include <coffee/imgui/imgui_binding.h>
#include <coffee/graphics/apis/CGLeamRHI>
#include <coffee/core/CDebug>

using namespace Coffee;
using namespace Display;

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

STATICINLINE ImGuiKey CfToImKey(u32 k)
{
    auto it = std::find_if(ImKeyMap.begin(), ImKeyMap.end(),
                 [k](Pair<ImGuiKey_,u16> const& p)
    {
        return p.second == k;
    });
    if(it != ImKeyMap.end())
        return it->first;
    return 0;
}

STATICINLINE u32 ImToCfKey(ImGuiKey k)
{
    auto it = std::find_if(ImKeyMap.begin(), ImKeyMap.end(),
                           [k](Pair<ImGuiKey_,u16> const& p)
    {
        return p.first == k;
    });
    if(it != ImKeyMap.end())
        return it->second;
    return 0;
}

// Data
static double       g_Time = 0.0;
//static bool         g_MousePressed[3] = { false, false, false };
static float        g_MouseWheel = 0.0f;
static GLuint       g_FontTexture = 0;
//static int          g_ShaderHandle = 0, g_VertHandle = 0, g_FragHandle = 0;
//static int          g_AttribLocationTex = 0, g_AttribLocationProjMtx = 0;
//static int          g_AttribLocationPosition = 0, g_AttribLocationUV = 0, g_AttribLocationColor = 0;
//static unsigned int g_VboHandle = 0, g_VaoHandle = 0, g_ElementsHandle = 0;

struct ImGuiData
{
    ImGuiData():
        attributes(),
        pipeline(),
        vertices(ResourceAccess::Streaming|ResourceAccess::WriteOnly, 0),
        elements(ResourceAccess::Streaming|ResourceAccess::WriteOnly, 0),
        fonts(PixFmt::RGBA8)
    {
        fonts_sampler.attach(&fonts);
    }
    ~ImGuiData()
    {
    }

    GFX::V_DESC attributes;
    GFX::PIP pipeline;
    GFX::BUF_A vertices;
    GFX::BUF_E elements;

    GFX::UNIFDESC u_tex;
    GFX::UNIFDESC u_xf;

    GFX::S_2D fonts;
    GFX::SM_2D fonts_sampler;
};

static UqPtr<ImGuiData> im_data = nullptr;

// This is the main rendering function that you have to implement and provide to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
// If text or lines are blurry when integrating ImGui in your engine:
// - in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
static void ImGui_ImplSdlGL3_RenderDrawLists(ImDrawData* draw_data)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO& io = ImGui::GetIO();
    int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fb_width == 0 || fb_height == 0)
        return;
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

    GFX::BLNDSTATE blend;
    blend.m_doBlend = true;
    GFX::RASTSTATE raster;
    raster.m_culling = 0;
    GFX::DEPTSTATE depth;
    depth.m_test = false;
    GFX::VIEWSTATE view_(1);

    view_.m_view.clear();
    view_.m_depth.clear();

    GFX::SetBlendState(blend);
    GFX::SetRasterizerState(raster);
    GFX::SetDepthState(depth);

    GFX::USTATE v_state;
    GFX::USTATE f_state;

    const float ortho_projection[4][4] =
    {
        { 2.0f/io.DisplaySize.x, 0.0f,                   0.0f, 0.0f },
        { 0.0f,                  2.0f/-io.DisplaySize.y, 0.0f, 0.0f },
        { 0.0f,                  0.0f,                  -1.0f, 0.0f },
        {-1.0f,                  1.0f,                   0.0f, 1.0f },
    };

    Bytes xf_data = {C_FCAST<u8*>(ortho_projection), sizeof(ortho_projection), 0};
    GFX::UNIFVAL xf_value;
    auto handle = im_data->fonts_sampler.handle();
    xf_value.data = &xf_data;

    v_state.setUniform(im_data->u_xf, &xf_value);
    f_state.setSampler(im_data->u_tex, &handle);

    GFX::PSTATE pipstate =
    {
        {ShaderStage::Vertex, v_state},
        {ShaderStage::Fragment, f_state}
    };

    glBlendEquation(GL_FUNC_ADD);

    GFX::D_CALL dc;
    dc.m_idxd = true;
    dc.m_inst = false;
    GFX::D_DATA dd;
    dd.m_eltype = (sizeof(ImDrawIdx) == 2) ? TypeEnum::UShort : TypeEnum::UInt;

    for(int n=0;n<draw_data->CmdListsCount;n++)
    {
        auto cmd_list = draw_data->CmdLists[n];
        dd.m_eoff = 0;

        im_data->vertices.commit(cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), cmd_list->VtxBuffer.Data);
        im_data->elements.commit(cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), cmd_list->IdxBuffer.Data);

        for(int cmd_i=0;cmd_i<cmd_list->CmdBuffer.Size;cmd_i++)
        {
            auto cmd = &cmd_list->CmdBuffer[cmd_i];
            dd.m_elems = cmd->ElemCount;
            if(cmd->UserCallback)
                cmd->UserCallback(cmd_list, cmd);
            else
            {
                view_.m_scissor[0] =
                {
                    (int)cmd->ClipRect.x,
                    (int)(fb_height - cmd->ClipRect.w),
                    (int)(cmd->ClipRect.z - cmd->ClipRect.x),
                    (int)(cmd->ClipRect.w - cmd->ClipRect.y)
                };
                GFX::SetViewportState(view_);
                handle.glTexHandle() =
                        ExtractIntegerPtr<u32>(cmd->TextureId);
                /* TODO: Improve this by using batching structure,
                 *  D_DATA arrays */
                GFX::Draw(im_data->pipeline, pipstate,
                          im_data->attributes, dc, dd);
            }
            dd.m_eoff += cmd->ElemCount;
        }
    }

    view_.m_scissor[0] = {0, 0, fb_width, fb_height};
    GFX::SetViewportState(view_);
}

static const char* ImGui_ImplSdlGL3_GetClipboardText(void*)
{
    return nullptr;
}

static void ImGui_ImplSdlGL3_SetClipboardText(void*, const char* text)
{
//    SDL_SetClipboardText(text);
}

void ImGui_ImplSdlGL3_CreateFontsTexture()
{
    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bits for OpenGL3 demo because it is more likely to be compatible with user's existing shader.

    auto& s = im_data->fonts;
    auto& sm = im_data->fonts_sampler;

    s.allocate({width, height}, PixCmp::RGBA);
    s.upload(BitFmt::UByte, PixCmp::RGBA, {width, height}, pixels);

    sm.alloc();
    sm.setFiltering(Filtering::Linear, Filtering::Linear);

    io.Fonts->TexID = FitIntegerInPtr(s.glTexHandle());
}

bool Coffee::CImGui::CreateDeviceObjects()
{
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
            "}\n"
            ;

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
            "}\n"
            ;

    if(im_data)
        return true;

    im_data = UqPtr<ImGuiData>(new ImGuiData);

    u32 attr_idx[3] = {};

    im_data->attributes.alloc();
    im_data->vertices.alloc();
    im_data->elements.alloc();

    {
        GFX::SHD vert;
        GFX::SHD frag;
        auto& pip = im_data->pipeline;

        auto vd = Bytes::CreateString(vertex_shader);
        if(!vert.compile(ShaderStage::Vertex, vd))
            cDebug("Failed to compile vertex shader, using: \n{0}",
                   vertex_shader);

        auto fd = Bytes::CreateString(fragment_shader);
        if(!frag.compile(ShaderStage::Fragment, fd))
            cDebug("Failed to compile fragment shader, using: \n{0}",
                   fragment_shader);

        auto& vert_owned = pip.storeShader(std::move(vert));
        auto& frag_owned = pip.storeShader(std::move(frag));

        if(!pip.attach(vert_owned, ShaderStage::Vertex))
            cDebug("Failed to attach vertex shader");
        if(!pip.attach(frag_owned, ShaderStage::Fragment))
            cDebug("Failed to attach fragment shader");

        if(!pip.assemble())
            cDebug("Failed to assemble shader pipeline");

        cDebug("Shader pipeline is assembled");

        fprintf(stderr, "Shaders in use: \nVertex:\n%s\n\nFragment:\n%s",
               vertex_shader, fragment_shader);

//        vert.dealloc();
//        frag.dealloc();

        Vector<GFX::UNIFDESC> unifs;
        Vector<GFX::PPARAM> params;
        GFX::GetShaderUniformState(pip, &unifs, &params);

        for(auto const& unif : unifs)
        {
            if(unif.m_name == "Texture")
                im_data->u_tex = unif;
            if(unif.m_name == "ProjMtx")
                im_data->u_xf = unif;
        }

        for(auto const& attr : params)
        {
            if(attr.m_name == "Position")
                attr_idx[0] = attr.m_idx;
            if(attr.m_name == "UV")
                attr_idx[1] = attr.m_idx;
            if(attr.m_name == "Color")
                attr_idx[2] = attr.m_idx;
        }
    }

    {
        GFX::V_ATTR pos;
        GFX::V_ATTR tex;
        GFX::V_ATTR col;
        auto& a = im_data->attributes;

        pos.m_idx = attr_idx[0];
        tex.m_idx = attr_idx[1];
        col.m_idx = attr_idx[2];

        pos.m_size = tex.m_size = 2;
        col.m_size = 4;

        pos.m_stride = tex.m_stride = col.m_stride = sizeof(ImDrawVert);
        pos.m_off = offsetof(ImDrawVert,pos);
        tex.m_off = offsetof(ImDrawVert,uv);
        col.m_off = offsetof(ImDrawVert,col);
        col.m_type = TypeEnum::UByte;
        col.m_flags = GFX::AttributePacked | GFX::AttributeNormalization;

        a.addAttribute(pos);
        a.addAttribute(tex);
        a.addAttribute(col);

        a.bindBuffer(0, im_data->vertices);
        a.setIndexBuffer(&im_data->elements);
    }

    ImGui_ImplSdlGL3_CreateFontsTexture();

    return true;
}

void    Coffee::CImGui::InvalidateDeviceObjects()
{
    if(im_data)
    {
        im_data->vertices.dealloc();
        im_data->elements.dealloc();
        im_data->attributes.dealloc();
    }
}

template<typename T>
inline T const& C(c_cptr d)
{
    return *((T const*)d);
}

void ImGui_InputHandle(void* r, CIEvent const& ev, c_cptr data)
{
    ImGuiIO* io = (ImGuiIO*)r;

    switch(ev.type)
    {
    case CIEvent::MouseButton:
    {
        auto ev = C<CIMouseButtonEvent>(data);
        if(ev.btn < 5 && ev.btn > 0)
        {
            bool flag = ev.mod & CIMouseButtonEvent::Pressed;
            io->MouseDown[ev.btn - 1] = flag;
            io->MouseClickedPos[ev.btn - 1] = {ev.pos.x, ev.pos.y};
        }
        break;
    }
    case CIEvent::Keyboard:
    {
        auto ev = C<CIKeyEvent>(data);
        if(ev.key < 512)
        {
            if(
                    ((ev.key >= CK_a && ev.key <= CK_z)
                     ||(ev.key >= CK_A && ev.key <= CK_Z))
                    && (ev.mod & CIKeyEvent::RepeatedModifier
                        || ev.mod & CIKeyEvent::PressedModifier
                        )
                    )
                io->AddInputCharacter(C_CAST<ImWchar>(ev.key));

            io->KeysDown[ev.key] = ev.mod & CIKeyEvent::PressedModifier;

            io->KeyAlt = ev.mod & CIKeyEvent::LAltModifier;
            io->KeyCtrl = ev.mod & CIKeyEvent::LCtrlModifier;
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
    default:
        break;
    }
}

bool Coffee::CImGui::Init(WindowManagerClient& window, EventApplication &event)
{
    ImGuiIO& io = ImGui::GetIO();

    event.installEventHandler(
    {
                    ImGui_InputHandle,
                    "ImGui input handler",
                    &io
                });

    for(auto const& p : ImKeyMap)
    {
        io.KeyMap[p.first] = p.second;
    }

    io.RenderDrawListsFn = ImGui_ImplSdlGL3_RenderDrawLists;   // Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.
    io.SetClipboardTextFn = ImGui_ImplSdlGL3_SetClipboardText;
    io.GetClipboardTextFn = ImGui_ImplSdlGL3_GetClipboardText;
    io.ClipboardUserData = NULL;

    return true;
}

void Coffee::CImGui::Shutdown()
{
    im_data = nullptr;

    InvalidateDeviceObjects();
    ImGui::Shutdown();
}

void Coffee::CImGui::NewFrame(WindowManagerClient& window,
                               EventApplication& event)
{
    if (!g_FontTexture)
        CreateDeviceObjects();

    ImGuiIO& io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    auto s = window.windowSize();
    io.DisplaySize = ImVec2((float)s.w, (float)s.h);
    io.DisplayFramebufferScale = ImVec2(1, 1);

    // Setup time step
    auto time = event.contextTime();
    io.DeltaTime = g_Time > 0.0 ? (float)(time - g_Time) : (float)(1.0f / 60.0f);
    g_Time = time;

    // Setup inputs
    auto pos = event.mousePosition();
    io.MousePos = ImVec2(pos.x, pos.y);

    io.MouseWheel = g_MouseWheel;
    g_MouseWheel = 0.0f;

    // Start the frame
    ImGui::NewFrame();
}

void CImGui::EndFrame()
{
    ImGui::Render();
}
