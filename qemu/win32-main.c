/*
 * UV-K5 V3 emulator: main.c with built-in Win32 GUI.
 *
 * On Windows, the GUI takes over the main thread and runs QEMU's main loop
 * on a background thread.  On other platforms, fall back to the default
 * behaviour.
 *
 * This file replaces QEMU's system/main.c during the build.
 *
 * This code is licensed under the GPL version 2 or later.
 */

#include "qemu/osdep.h"
#include "qemu-main.h"
#include "sysemu/replay.h"
#include "qemu/main-loop.h"
#include "qemu/thread.h"

#ifdef _WIN32
#include <windows.h>
/* Implemented in hw/arm/win32-gui.c */
extern int uvk5_win32_main(int argc, char **argv);
#endif

static void *qemu_default_main(void *opaque)
{
    int status;
    replay_mutex_lock();
    bql_lock();
    status = qemu_main_loop();
    qemu_cleanup(status);
    bql_unlock();
    replay_mutex_unlock();
#ifdef _WIN32
    ExitProcess(status);
#else
    exit(status);
#endif
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    return uvk5_win32_main(argc, argv);
#else
    qemu_init(argc, argv);
    bql_unlock();
    replay_mutex_unlock();
    qemu_default_main(NULL);
    return 0;
#endif
}
