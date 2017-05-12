// ImGui SDL2 binding with OpenGL3
// In this binding, ImTextureID is used to store an OpenGL 'GLuint' texture identifier. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include <coffee/imgui/imgui_binding.h>
#include <coffee/graphics/apis/CGLeamRHI>
#include <coffee/core/CDebug>

using namespace Coffee;
using namespace Display;

using GFX = RHI::GLEAM::GLEAM_API;

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
        vertices(ResourceAccess::Streaming|ResourceAccess::WriteOnly, 0),
        elements(ResourceAccess::Streaming|ResourceAccess::WriteOnly, 0),
        fonts(PixFmt::RGBA8)
    {
        fonts_sampler.attach(&fonts);
    }
    ~ImGuiData()
    {
        cDebug("Deconstructing!");
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
                view_.m_scissor[0] = {(int)cmd->ClipRect.x,
                                      (int)(fb_height - cmd->ClipRect.w),
                                      (int)(cmd->ClipRect.z - cmd->ClipRect.x),
                                      (int)(cmd->ClipRect.w - cmd->ClipRect.y)
                                     };
                GFX::SetViewportState(view_);
                handle.texture = ExtractIntegerPtr<u32>(cmd->TextureId);
                /* TODO: Improve this by using batching structure, D_DATA arrays */
                GFX::Draw(im_data->pipeline, pipstate, im_data->attributes, dc, dd);
            }
            dd.m_eoff += cmd->ElemCount;
        }
    }

    view_.m_scissor[0] = {0, 0, fb_width, fb_height};
    GFX::SetViewportState(view_);

    /*

    // Backup GL state
    GLint last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, &last_active_texture);
    glActiveTexture(GL_TEXTURE0);
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    GLint last_element_array_buffer; glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
    GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
    GLint last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, &last_blend_src_rgb);
    GLint last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, &last_blend_dst_rgb);
    GLint last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, &last_blend_src_alpha);
    GLint last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, &last_blend_dst_alpha);
    GLint last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, &last_blend_equation_rgb);
    GLint last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &last_blend_equation_alpha);
    GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
    GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
    GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
    GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    // Setup viewport, orthographic projection matrix
    glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);

    glUseProgram(g_ShaderHandle);
    glUniform1i(g_AttribLocationTex, 0);
    glUniformMatrix4fv(g_AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
    glBindVertexArray(g_VaoHandle);

    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawIdx* idx_buffer_offset = 0;

        glBindBuffer(GL_ARRAY_BUFFER, g_VboHandle);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ElementsHandle);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
                glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
            }
            idx_buffer_offset += pcmd->ElemCount;
        }
    }

    // Restore modified GL state
    glUseProgram(last_program);
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glActiveTexture(last_active_texture);
    glBindVertexArray(last_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
    glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
    glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
    if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
    */
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

    io.Fonts->TexID = FitIntegerInPtr(s.m_handle);

    /*
    // Upload texture to graphics system
    GLint last_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGenTextures(1, &g_FontTexture);
    glBindTexture(GL_TEXTURE_2D, g_FontTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Store our identifier
    io.Fonts->TexID = (void *)(intptr_t)g_FontTexture;

    // Restore state
    glBindTexture(GL_TEXTURE_2D, last_texture);
    */
}

bool Coffee::CImGui::CreateDeviceObjects()
{
    constexpr cstring vertex_shader =
        "#version 330\n"
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
        "#version 330\n"
        "uniform sampler2D Texture;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "out vec4 Out_Color;\n"
        "void main()\n"
        "{\n"
        "	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
        "}\n";

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
        vert.compile(ShaderStage::Vertex, vd);

        auto fd = Bytes::CreateString(fragment_shader);
        frag.compile(ShaderStage::Fragment, fd);

        pip.attach(vert, ShaderStage::Vertex);
        pip.attach(frag, ShaderStage::Fragment);

        pip.assemble();

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

        pos.m_idx = 0;
        tex.m_idx = 1;
        col.m_idx = 2;

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
    /*
    // Backup GL state
    GLint last_texture, last_array_buffer, last_vertex_array;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

    g_ShaderHandle = glCreateProgram();
    g_VertHandle = glCreateShader(GL_VERTEX_SHADER);
    g_FragHandle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(g_VertHandle, 1, &vertex_shader, 0);
    glShaderSource(g_FragHandle, 1, &fragment_shader, 0);
    glCompileShader(g_VertHandle);
    glCompileShader(g_FragHandle);
    glAttachShader(g_ShaderHandle, g_VertHandle);
    glAttachShader(g_ShaderHandle, g_FragHandle);
    glLinkProgram(g_ShaderHandle);

    g_AttribLocationTex = glGetUniformLocation(g_ShaderHandle, "Texture");
    g_AttribLocationProjMtx = glGetUniformLocation(g_ShaderHandle, "ProjMtx");
    g_AttribLocationPosition = glGetAttribLocation(g_ShaderHandle, "Position");
    g_AttribLocationUV = glGetAttribLocation(g_ShaderHandle, "UV");
    g_AttribLocationColor = glGetAttribLocation(g_ShaderHandle, "Color");

    glGenBuffers(1, &g_VboHandle);
    glGenBuffers(1, &g_ElementsHandle);

    glGenVertexArrays(1, &g_VaoHandle);
    glBindVertexArray(g_VaoHandle);
    glBindBuffer(GL_ARRAY_BUFFER, g_VboHandle);
    glEnableVertexAttribArray(g_AttribLocationPosition);
    glEnableVertexAttribArray(g_AttribLocationUV);
    glEnableVertexAttribArray(g_AttribLocationColor);

#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
    glVertexAttribPointer(g_AttribLocationPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(g_AttribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer(g_AttribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
#undef OFFSETOF

    ImGui_ImplSdlGL3_CreateFontsTexture();

    // Restore modified GL state
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindVertexArray(last_vertex_array);

    return true;
    */
}

void    Coffee::CImGui::InvalidateDeviceObjects()
{
    im_data->vertices.dealloc();
    im_data->elements.dealloc();
    im_data->attributes.dealloc();
    /*
    if (g_VaoHandle) glDeleteVertexArrays(1, &g_VaoHandle);
    if (g_VboHandle) glDeleteBuffers(1, &g_VboHandle);
    if (g_ElementsHandle) glDeleteBuffers(1, &g_ElementsHandle);
    g_VaoHandle = g_VboHandle = g_ElementsHandle = 0;

    if (g_ShaderHandle && g_VertHandle) glDetachShader(g_ShaderHandle, g_VertHandle);
    if (g_VertHandle) glDeleteShader(g_VertHandle);
    g_VertHandle = 0;

    if (g_ShaderHandle && g_FragHandle) glDetachShader(g_ShaderHandle, g_FragHandle);
    if (g_FragHandle) glDeleteShader(g_FragHandle);
    g_FragHandle = 0;

    if (g_ShaderHandle) glDeleteProgram(g_ShaderHandle);
    g_ShaderHandle = 0;

    if (g_FontTexture)
    {
        glDeleteTextures(1, &g_FontTexture);
        ImGui::GetIO().Fonts->TexID = 0;
        g_FontTexture = 0;
    }
    */
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
            if(ev.key >= CK_a && ev.key <= CK_z)
                io->AddInputCharacter(ev.key);

            io->KeysDown[ev.key] = ev.mod & CIKeyEvent::PressedModifier;
            io->KeyAlt = ev.mod & CIKeyEvent::LAltModifier;
            io->KeyCtrl = ev.mod & CIKeyEvent::LCtrlModifier;
            io->KeyShift = ev.mod & CIKeyEvent::LShiftModifier;
            io->KeySuper = ev.mod & CIKeyEvent::SuperModifier;
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
    io.KeyMap[ImGuiKey_Tab] = CK_HTab;
    io.KeyMap[ImGuiKey_LeftArrow] = CK_Left;
    io.KeyMap[ImGuiKey_RightArrow] = CK_Right;
    io.KeyMap[ImGuiKey_UpArrow] = CK_Up;
    io.KeyMap[ImGuiKey_DownArrow] = CK_Down;
    io.KeyMap[ImGuiKey_PageUp] = CK_PgUp;
    io.KeyMap[ImGuiKey_PageDown] = CK_PgDn;
    io.KeyMap[ImGuiKey_Home] = CK_Home;
    io.KeyMap[ImGuiKey_End] = CK_End;
    io.KeyMap[ImGuiKey_Delete] = CK_Delete;
    io.KeyMap[ImGuiKey_Backspace] = CK_BackSpace;
    io.KeyMap[ImGuiKey_Enter] = CK_EnterNL;
    io.KeyMap[ImGuiKey_Escape] = CK_Escape;
    io.KeyMap[ImGuiKey_A] = CK_a;
    io.KeyMap[ImGuiKey_C] = CK_c;
    io.KeyMap[ImGuiKey_V] = CK_v;
    io.KeyMap[ImGuiKey_X] = CK_x;
    io.KeyMap[ImGuiKey_Y] = CK_y;
    io.KeyMap[ImGuiKey_Z] = CK_z;

    io.RenderDrawListsFn = ImGui_ImplSdlGL3_RenderDrawLists;   // Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.
    io.SetClipboardTextFn = ImGui_ImplSdlGL3_SetClipboardText;
    io.GetClipboardTextFn = ImGui_ImplSdlGL3_GetClipboardText;
    io.ClipboardUserData = NULL;

    return true;
}

void Coffee::CImGui::Shutdown()
{
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
