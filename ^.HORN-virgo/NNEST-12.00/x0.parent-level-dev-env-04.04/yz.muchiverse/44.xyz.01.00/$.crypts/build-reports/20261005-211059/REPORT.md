# House-wide compile report — 2026-10-05 21:11:01

**Summary: 2 scripts — PASS=1 FAIL=1 TIMEOUT=0**

| Status | Script | Log |
|---|---|---|
| PASS | _.monads/_.livedesk-taskbar/ops/build_db_hq_manager.sh | $.crypts/build-reports/20261005-211059/_.monads__.livedesk-taskbar_ops_build_db_hq_manager.sh.log |
| FAIL (exit 1) | _.monads/_.livedesk-taskbar/ops/build_db_hq.sh | $.crypts/build-reports/20261005-211059/_.monads__.livedesk-taskbar_ops_build_db_hq.sh.log |

## Failure log tails

### _.monads/_.livedesk-taskbar/ops/build_db_hq.sh — FAIL (exit 1)
```
   54 |         out->has_bg_color = 1; snprintf(out->bg_color, sizeof(out->bg_color), "%s", v);
      |                                                                                ^~   ~
In file included from /usr/include/stdio.h:894,
                 from khtpm_css_parser.c:3:
/usr/include/x86_64-linux-gnu/bits/stdio2.h:71:10: note: ‘__builtin_snprintf’ output between 1 and 256 bytes into a destination of size 32
   71 |   return __builtin___snprintf_chk (__s, __n, __USE_FORTIFY_LEVEL - 1,
      |          ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   72 |                                    __glibc_objsize (__s), __fmt,
      |                                    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   73 |                                    __va_arg_pack ());
      |                                    ~~~~~~~~~~~~~~~~~
khtpm_taskbar_manager.c:38:10: fatal error: kh_proc_registry.h: No such file or directory
   38 | #include "kh_proc_registry.h"
      |          ^~~~~~~~~~~~~~~~~~~~
compilation terminated.
```

