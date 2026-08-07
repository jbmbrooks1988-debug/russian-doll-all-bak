#ifndef KHTPM_TASKBAR_PLAT_H
#define KHTPM_TASKBAR_PLAT_H
#include "khtpm_taskbar_core.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Run taskbar until quit. Uses shared KtbState only. */
int ktb_plat_run(KtbState *s);
/* Run a shortcut command string (portable path already preferred). */
void ktb_plat_run_command(const char *cmd, const char *house_root);
#ifdef __cplusplus
}
#endif
#endif
