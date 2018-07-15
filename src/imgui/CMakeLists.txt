coffee_library (
    TARGET CoffeeImGui
    SOURCES

    imgui_binding.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_demo.cpp

    LIBRARIES
    CoffeeCore
    CoffeeRenderer_Default
    CoffeeGLeamRHI

    HEADER_DIRS
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/libs/imgui>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/include>
    $<INSTALL_INTERFACE:include>
    )

target_compile_definitions ( CoffeeImGui PUBLIC
    -DCOFFEE_IMGUI_VERSION="${COFFEE_BUILD_STRING}"
    -DCOFFEE_IMGUI_USE_GLEAM
    -DIMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCS
    -DIMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCS
    )

coffee_bundle_includes ( ${CMAKE_SOURCE_DIR}/src/include )
coffee_bundle_header ( ${CMAKE_SOURCE_DIR}/src/libs/imgui/imgui.h )
coffee_bundle_header ( ${CMAKE_SOURCE_DIR}/src/libs/imgui/imconfig.h )
