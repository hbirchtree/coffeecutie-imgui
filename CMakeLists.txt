project ( coffeeimgui )
cmake_minimum_required(VERSION 3.1)

set ( CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/cmake/Modules" )
include( SubprojectFunctions )
Init_Subproject()

find_package ( StandardLibs REQUIRED )
if(COFFEE_BUILD_SDL2)
    find_package ( SDL2 REQUIRED )
endif()
import_coffee_lib ( Coffee REQUIRED )

set ( IMGUI_DIR
    ${CMAKE_SOURCE_DIR}/libs/imgui
    )

set ( SRCS
    src/imgui_binding.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    )

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list ( APPEND SRCS
        ${IMGUI_DIR}/imgui_demo.cpp
        )
endif()

set ( LIBS
    CoffeeCore; CoffeeRenderer_Default;
    CoffeeGLeamRHI;
    )

set ( PERMS
    OPENGL;AUDIO;ENVIRONMENT_SENSORS;NETWORK_CONNECT;JOYSTICK
    )

include_directories (
    ${Coffee_INCLUDE_DIRS}
    )

add_definitions(
	-DCOFFEE_IMGUI_USE_GLEAM
	-DIMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCS
	-DIMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCS
	)

coffee_add_library ( CoffeeImGui
    "${SRCS}" "${LIBS}"
    "${CMAKE_SOURCE_DIR}/include/coffee/imgui"
    )

target_include_directories ( CoffeeImGui
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/imgui/>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/>
    )

coffee_application (
    TARGET CoffeeImGuiTest
    TITLE "Coffee ImGui Test"
    COMPANY "Coffee"
    VERSIONC_DE "1"
    SOURCES src/main.cpp
    LIBRARIES CoffeeImGui
    PERMISSIONS ${PERMS}
    )

coffee_bundle_includes( ${CMAKE_SOURCE_DIR}/include )
coffee_bundle_header ( ${CMAKE_SOURCE_DIR}/libs/imgui/imgui.h )
coffee_bundle_header ( ${CMAKE_SOURCE_DIR}/libs/imgui/imconfig.h )

End_Subproject()
