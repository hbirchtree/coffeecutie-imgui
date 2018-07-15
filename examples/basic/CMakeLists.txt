coffee_application (
    TARGET CoffeeImGuiTest

    TITLE "Coffee ImGui Test"
    COMPANY "Coffee"
    VERSION_CODE "1"

    SOURCES main.cpp

    LIBRARIES CoffeeImGui

    PERMISSIONS
    OPENGL
    AUDIO
    ENVIRONMENT_SENSORS
    NETWORK_CONNECT
    JOYSTICK
    )
