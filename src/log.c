#include "log.h"

#include <pspkernel.h>
#include <stdarg.h>
#include <stdio.h>

#include "game.h"

static FILE *log_file = NULL;

void Log_Init(void) {
    /* Start with a clean file. Some Memory Stick/emulator implementations do
     * not reliably truncate an existing file when it is opened with "w". */
    (void)remove("ms0:/CIVPSP_LOG.txt");
    log_file = fopen("ms0:/CIVPSP_LOG.txt", "w");
    if (!log_file) return;
    fprintf(log_file, "CIVITAS PSP %s\n", VERSION_STRING);
    fprintf(log_file, "log opened, tick=%u\n", (unsigned int)sceKernelGetSystemTimeLow());
    fflush(log_file);
}

void Log_Printf(const char *fmt, ...) {
    if (!log_file) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(log_file, "[%u] %s\n", (unsigned int)sceKernelGetSystemTimeLow(), buf);
    fflush(log_file);
}

void Log_Close(void) {
    if (log_file) {
        Log_Printf("log closed");
        fclose(log_file);
        log_file = NULL;
    }
}
