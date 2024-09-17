#include "fuujin.h"

int main(int argc, const char** argv) {
    ZoneScoped;
    return fuujin::Application::Run(argc, argv);
}