project ( CoffeeImGui )
cmake_minimum_required(VERSION 3.1)

include( toolchain/cmake/Modules/SubprojectFunctions.cmake )
init_subproject()

include (GitDependencies)

dependency_get (
    REQUIRED
    SOURCE hbirchtree/coffeecutie
    TAG automated-release-4.4.1.1.5
    NAMES
        libraries
    )
dependency_get (
    SOURCE hbirchtree/native-library-bundle
    TAG rel-0.3.1.0.14
    NAMES
        sdl2
    )

set ( IMGUI_DIR
    ${CMAKE_SOURCE_DIR}/src/libs/imgui
    )

if(BUILD_LIBRARIES)
    find_package( Coffee REQUIRED )

    if(TARGET Coffee::SDL2Backend)
        find_package ( SDL2 REQUIRED )
    endif()
    
    add_subdirectory(src/imgui)

    if(BUILD_BINARIES AND BUILD_EXAMPLES)
        add_subdirectory(examples/basic)
    endif()
endif()

add_subdirectory(toolchain/docs-gen)

End_Subproject()
