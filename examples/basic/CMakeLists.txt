coffee_application (
    TARGET ImGuiTest

    TITLE "Coffee ImGui Test"
    COMPANY "Coffee"
    VERSION_CODE "1"

    SOURCES main.cpp

    LIBRARIES ImGui Coffee::RendererDefault

    PERMISSIONS
    OPENGL
    AUDIO
    ENVIRONMENT_SENSORS
    NETWORK_CONNECT
    JOYSTICK
    )
