#include <windows.h>
#include <cstring>
#include "Application.h"
#include "SingleInstance.h"
#ifdef ELIOTOPY_AUTO_UPDATE
#include "StealthName.h"
#include "Updater.h"
#endif

int main(int argc, char** argv) {
    bool controlledRelaunch = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "/u") == 0)
            controlledRelaunch = true;

    if (!controlledRelaunch && !SingleInstance::acquire()) {
        MessageBoxA(nullptr, "Eliotopy is already running.", "Eliotopy", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

#ifdef ELIOTOPY_AUTO_UPDATE
    if (!controlledRelaunch)
        StealthName::enforce();
    Updater::cleanupOld();
    Updater::run();
#endif

    Application app;
    app.init();
    return 0;
}
