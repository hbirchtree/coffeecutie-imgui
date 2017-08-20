project ( coffeeimgui )
cmake_minimum_required(VERSION 3.1)

set ( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin )

set ( COFFEE_DESKTOP_DIRECTORY "${CMAKE_SOURCE_DIR}/desktop" )
set ( CMAKE_PACKAGED_OUTPUT_PREFIX packaged )
set ( COFFEE_PACKAGE_DIRECTORY "${PROJECT_BINARY_DIR}/${CMAKE_PACKAGED_OUTPUT_PREFIX}" )
set ( COFFEE_DEPLOY_DIRECTORY "${PROJECT_BINARY_DIR}/deploy" )
set ( COFFEE_SOURCE_TEMPLATE_DIRECTORY "${CMAKE_SOURCE_DIR}/internal/templates" )

if(ANDROID)
    set ( CMAKE_MODULE_PATH
        ${CMAKE_SOURCE_DIR}/cmake/Find_Android
        ${CMAKE_MODULE_PATH}
        )
elseif(WIN32)
    set ( CMAKE_MODULE_PATH
        ${CMAKE_SOURCE_DIR}/cmake/Find_Windows
        ${CMAKE_MODULE_PATH}
        )
endif()

set ( CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${CMAKE_SOURCE_DIR}/cmake/Find
    ${CMAKE_SOURCE_DIR}/cmake/Modules
    ${CMAKE_SOURCE_DIR}/cmake/Packaging
    )

if(NOT WIN32)
    set ( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib )
else()
    set ( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin )
endif()

include ( BuildFlags )
include ( BuildOptions )
include ( BuildFunctions )
include ( PackagingOptions )
include ( TestingOptions )

option ( BUILD_TESTS "Build unit tests" ON )

find_package ( Coffee REQUIRED )
find_package ( StandardLibs REQUIRED )
find_package ( SDL2 REQUIRED )

set ( IMGUI_DIR
    ${CMAKE_SOURCE_DIR}/libs/imgui
    )

set ( SRCS
    src/imgui_binding.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    )

set ( LIBS
    ${COFFEE_GLEAM_COMMON_LIBRARY};
    ${COFFEE_CORE_LIBRARY};
    ${COFFEE_ANDROID_LIBRARY};
    ${STANDARDLIBS_LIBRARIES};
    ${SDL2_LIBRARY};${SDL2_LIBRARIES}
    )

set ( PERMS
    OPENGL;AUDIO;ENVIRONMENT_SENSORS;NETWORK_CONNECT;JOYSTICK
    )

include_directories (
    ${COFFEE_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/imgui
    ${SDL2_INCLUDE_DIR}
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

coffee_add_application_longerer(
    CoffeeImGuiTest
    "Coffee ImGui Test" "Coffee" "1"
    "???" "???"
    "src/main.cpp" "CoffeeImGui"
    "" "" ""
    "${PERMS}" ""
    )

install (
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/coffee"
    DESTINATION include/
    )

install (
    FILES
    "${CMAKE_SOURCE_DIR}/libs/imgui/imgui.h"
    "${CMAKE_SOURCE_DIR}/libs/imgui/imconfig.h"

    DESTINATION include/
    )
install (
    FILES
    "${CMAKE_SOURCE_DIR}/FindCoffeeImGui.cmake"
    DESTINATION share/cmake/Modules
    )
