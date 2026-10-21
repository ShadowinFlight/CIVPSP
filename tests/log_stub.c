#include <stdarg.h>

#include "log.h"

/* Host-side tests link the gameplay code without the PSP file system.  Keep
 * AI/gameplay diagnostics available on the target while making the same code
 * link cleanly on the host. */
void Log_Printf(const char *fmt, ...) {
    (void)fmt;
}
