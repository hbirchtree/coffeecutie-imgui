#include <coffee/core/CApplication>
#include <coffee/core/CDebug>
#include <coffee/core/CFiles>

using namespace Coffee;

int32 coffeeimgui_main(int32, cstring_w*)
{
    CResources::FileResourcePrefix("coffeeimgui/");

    cDebug("Hello World!");

    return 0;
}

COFFEE_APPLICATION_MAIN(coffeeimgui_main)
