cmake_minimum_required(VERSION 3.1)
project ( CoffeeImGui VERSION 0.9 )
cmake_policy ( SET CMP0069 NEW )

include( toolchain/cmake/Modules/SubprojectFunctions.cmake )
init_subproject()

set ( IMGUI_DIR
    ${PROJECT_SOURCE_DIR}/src/libs/imgui
    )

if(BUILD_LIBRARIES)
    add_subdirectory(src/imgui)
endif()

add_subdirectory(toolchain/docs-gen)

End_Subproject()
