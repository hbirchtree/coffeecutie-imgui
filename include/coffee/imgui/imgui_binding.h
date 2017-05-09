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

}
}
