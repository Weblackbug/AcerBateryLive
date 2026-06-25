#include "App.hpp"

#include <Windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    return App::Instance().Run(instance);
}
