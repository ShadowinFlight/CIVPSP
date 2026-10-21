#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <psppower.h>

#include <errno.h>
#include <sys/types.h>

#include "app.h"
#include "audio.h"
#include "game.h"
#include "input.h"
#include "log.h"
#include "render.h"
#include "rng.h"
#include "text.h"

PSP_MODULE_INFO("CIVPSP", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(8192);

/* Prevent libcglue from pulling sceNetInet into a game that never uses the
 * network.  On some CFW the network modules are not loaded and a static
 * import causes 8002013C at boot. */
int __socket_close(int s) { (void)s; return -1; }
ssize_t recv(int s, void *buf, size_t len, int flags) {
    (void)s; (void)buf; (void)len; (void)flags; return -1;
}
ssize_t send(int s, const void *buf, size_t len, int flags) {
    (void)s; (void)buf; (void)len; (void)flags; return -1;
}
int __set_errno(int psp_err) {
    if ((psp_err & 0x80010000) != 0x80010000) return psp_err;
    errno = EIO;
    return -1;
}

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void) {
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    setup_callbacks();
    Log_Init();
    Log_Printf("boot: callbacks ok");

    int clock_rc = scePowerSetClockFrequency(333, 333, 166);
    Log_Printf("boot: clock rc=%d", clock_rc);

    Render_Init();
    Log_Printf("boot: GU ok");

    Text_Init();
    Input_Init();
    Rng_SeedFromClock();
    App_Init();
    Audio_Init();
    Log_Printf("boot: app ok, rng=%08X", (unsigned int)Rng_GetState());

    while (!App_ShouldQuit()) {
        Input_Update();
        App_Update();

        if (App_ShouldQuit()) break;

        Render_BeginFrame();
        App_Render();
        Render_EndFrame();
        sceDisplayWaitVblankStart();
    }

    Log_Printf("shutdown");
    Audio_Shutdown();
    Log_Close();
    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
