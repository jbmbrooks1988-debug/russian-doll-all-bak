/* khtpm_taskbar_main.c — shared entry; one toolbar logic (khtpm_taskbar_core). */
#include "khtpm_taskbar_core.h"
#include "khtpm_taskbar_plat.h"
#include <stdio.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  define ktb_pid() ((int)GetCurrentProcessId())
#else
#  include <unistd.h>
#  define ktb_pid() ((int)getpid())
#endif

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: tp_taskbar <house_root>\n");
        return 1;
    }
    KtbState st;
    ktb_init(&st, argv[1]);
    ktb_write_pidfile(&st, ktb_pid());
    ktb_reload(&st);
    int rc = ktb_plat_run(&st);
    ktb_unlink_pidfile(&st);
    return rc;
}
