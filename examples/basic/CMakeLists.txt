coffee_application (
    TARGET ImGuiTest

    TITLE "ImGui Test"
    COMPANY "Birchtrees"
    VERSION_CODE "1"

    SOURCES main.cpp

    LIBRARIES ImGui Coffee::ComponentBundleSetup

    PERMISSIONS
    OPENGL
    AUDIO
    ENVIRONMENT_SENSORS
    NETWORK_CONNECT
    JOYSTICK
    )

if(TARGET Coffee::ASIO)
    target_link_libraries ( ImGuiTest PRIVATE
        Coffee::ASIO
        )
endif()
