project ( CoffeeImGui )
cmake_minimum_required(VERSION 3.1)

include( toolchain/cmake/Modules/SubprojectFunctions.cmake )
init_subproject()


set ( IMGUI_DIR
    ${CMAKE_SOURCE_DIR}/src/libs/imgui
    )

if(BUILD_LIBRARIES)
    find_package ( StandardLibs REQUIRED )

    if(TARGET CoffeeSDL2)
        find_package ( SDL2 REQUIRED )
    endif()

    import_coffee_lib ( Coffee REQUIRED )
    
    add_subdirectory(src/imgui)

    if(BUILD_BINARIES AND BUILD_EXAMPLES)
        add_subdirectory(examples/basic)
    endif()
endif()

add_subdirectory(toolchain/docs-gen)

End_Subproject()
