project ( CoffeeImGui VERSION 0.9 )
cmake_minimum_required(VERSION 3.1)

include( toolchain/cmake/Modules/SubprojectFunctions.cmake )
init_subproject()

include (GitDependencies)

dependency_get (
    REQUIRED
    SOURCE hbirchtree/coffeecutie
    TAG rel-4.4.2.10.7
    NAMES
        libraries=Coffee
    SIDELOAD
        ${Coffee_LOCAL_BUILD}
    )

set ( IMGUI_DIR
    ${CMAKE_SOURCE_DIR}/src/libs/imgui
    )

if(BUILD_LIBRARIES)
    find_package ( Coffee REQUIRED )
    dependency_resolve ( Coffee::ComponentBundleSetup Coffee::CoreApplication )

    add_subdirectory(src/imgui)

    if(BUILD_BINARIES AND BUILD_EXAMPLES)
        if(TARGET Coffee::ASIO)
            dependency_resolve ( Coffee::ASIO )
        endif()

        add_subdirectory(examples/basic)
    endif()
endif()

add_subdirectory(toolchain/docs-gen)

End_Subproject()
