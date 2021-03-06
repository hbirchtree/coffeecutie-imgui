coffee_library (
    TARGET ImGui
    SOURCES

    imgui_binding.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_demo.cpp

    LIBRARIES
    Coffee::Core
    Coffee::ComponentBundleSetup
    Coffee::GLeamRHI

    HEADER_DIRS
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/libs/imgui>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/include>
    $<INSTALL_INTERFACE:include>
    )

target_compile_definitions ( ImGui PUBLIC
    -DCOFFEE_IMGUI_VERSION="${COFFEE_BUILD_STRING}"
    -DCOFFEE_IMGUI_USE_GLEAM
    -DIMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCS
    -DIMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCS
    )

coffee_bundle (
    HEADER_DIRECTORIES
    ${PROJECT_SOURCE_DIR}/src/include

    PUBLIC_HEADERS
    ${PROJECT_SOURCE_DIR}/src/libs/imgui/imgui.h
    ${PROJECT_SOURCE_DIR}/src/libs/imgui/imconfig.h
    )

dependency_link (
    TARGET ImGui
    DEPENDENCIES libraries=Coffee
    )
