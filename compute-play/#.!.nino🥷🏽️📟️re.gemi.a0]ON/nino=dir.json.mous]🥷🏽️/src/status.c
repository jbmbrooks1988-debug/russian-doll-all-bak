#include "status.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "highlight.h"
#include "unicode.h"
#include "utils.h"
#include "version.h"

void editorSetStatusMsg(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gEditor.status_msg[0], sizeof(gEditor.status_msg[0]), fmt, ap);
    va_end(ap);
}

void editorSetRStatusMsg(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gEditor.status_msg[1], sizeof(gEditor.status_msg[1]), fmt, ap);
    va_end(ap);
}

void editorDrawTopStatusBar(abuf* ab) {
    const char* right_buf = "  nino v" EDITOR_VERSION " ";
    bool has_more_files = false;
    int rlen = strlen(right_buf);
    int len = gEditor.explorer.width;

    gotoXY(ab, 1, gEditor.explorer.width + 1);

    setColor(ab, gEditor.color_cfg.top_status[0], 0);
    setColor(ab, gEditor.color_cfg.top_status[1], 1);

    if (gEditor.tab_offset != 0) {
        abufAppendN(ab, "<", 1);
        len++;
    }

    gEditor.tab_displayed = 0;
    if (gEditor.loading) {
        const char* loading_text = "Loading...";
        int loading_text_len = strlen(loading_text);
        abufAppendN(ab, loading_text, loading_text_len);
        len = loading_text_len;
    } else {
        for (int i = 0; i < gEditor.file_count; i++) {
            if (i < gEditor.tab_offset)
                continue;

            const EditorFile* file = &gEditor.files[i];

            bool is_current = (file == gCurFile);
            if (is_current) {
                setColor(ab, gEditor.color_cfg.top_status[4], 0);
                setColor(ab, gEditor.color_cfg.top_status[5], 1);
            } else {
                setColor(ab, gEditor.color_cfg.top_status[2], 0);
                setColor(ab, gEditor.color_cfg.top_status[3], 1);
            }

            char buf[EDITOR_PATH_MAX] = {0};
            const char* filename =
                file->filename ? getBaseName(file->filename) : "Untitled";
            int buf_len = snprintf(buf, sizeof(buf), " %s%s ",
                                   file->dirty ? "*" : "", filename);
            int tab_width = strUTF8Width(buf);

            if (gEditor.screen_cols - len < tab_width ||
                (i != gEditor.file_count - 1 &&
                 gEditor.screen_cols - len == tab_width)) {
                has_more_files = true;
                if (gEditor.tab_displayed == 0) {
                    // Display at least one tab
                    // TODO: This is wrong
                    tab_width = gEditor.screen_cols - len - 1;
                    buf_len = gEditor.screen_cols - len - 1;
                } else {
                    break;
                }
            }

            // Not enough space to even show one tab
            if (tab_width < 0)
                break;

            abufAppendN(ab, buf, buf_len);
            len += tab_width;
            gEditor.tab_displayed++;
        }
    }

    setColor(ab, gEditor.color_cfg.top_status[0], 0);
    setColor(ab, gEditor.color_cfg.top_status[1], 1);

    if (has_more_files) {
        abufAppendN(ab, ">", 1);
        len++;
    }

    while (len < gEditor.screen_cols) {
        if (gEditor.screen_cols - len == rlen) {
            abufAppendN(ab, right_buf, rlen);
            break;
        } else {
            abufAppend(ab, " ");
            len++;
        }
    }
}

static void drawLeftRightMsg(abuf* ab, const char* left, const char* right) {
    int len = strlen(left);
    int rlen = strlen(right);

    if (rlen > gEditor.screen_cols)
        rlen = 0;
    if (len + rlen > gEditor.screen_cols)
        len = gEditor.screen_cols - rlen;

    abufAppendN(ab, left, len);

    while (len < gEditor.screen_cols) {
        if (gEditor.screen_cols - len == rlen) {
            abufAppendN(ab, right, rlen);
            break;
        } else {
            abufAppend(ab, " ");
            len++;
        }
    }
}

void editorDrawPrompt(abuf* ab) {
    gotoXY(ab, gEditor.screen_rows - 1, 0);

    setColor(ab, gEditor.color_cfg.prompt[0], 0);
    setColor(ab, gEditor.color_cfg.prompt[1], 1);

    drawLeftRightMsg(ab, gEditor.status_msg[0], gEditor.status_msg[1]);
}

void editorDrawStatusBar(abuf* ab) {
    gotoXY(ab, gEditor.screen_rows, 0);

    setColor(ab, gEditor.color_cfg.status[0], 0);
    setColor(ab, gEditor.color_cfg.status[1], 1);

    const char* help_str = "";
    const char* help_info[] = {
        " ^Q: Quit  ^O: Open  ^P: Prompt  ^S: Save  ^F: Find  ^G: Goto",
        " ^Q: Quit  ^O: Open  ^P: Prompt",
        " ^Q: Cancel  Up: back  Down: Next",
        " ^Q: Cancel",
        " ^Q: Cancel",
        " ^Q: Cancel",
        " ^Q: Cancel",
    };
    if (CONVAR_GETINT(helpinfo))
        help_str = help_info[gEditor.state];

    char lang[16];
    char pos[64];
    int len = strlen(help_str);
    int lang_len, pos_len;
    int rlen;
    if (gEditor.file_count == 0) {
        lang_len = 0;
        pos_len = 0;
    } else {
        const char* file_type =
            gCurFile->syntax ? gCurFile->syntax->file_type : "Plain Text";
        int row = gCurFile->cursor.y + 1;
        int col = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                  gCurFile->cursor.x) +
                  1;
        float line_percent = 0.0f;
        const char* nl_type = (gCurFile->newline == NL_UNIX) ? "LF" : "CRLF";
        if (gCurFile->num_rows - 1 > 0) {
            line_percent =
                (float)gCurFile->row_offset / (gCurFile->num_rows - 1) * 100.0f;
        }

        lang_len = snprintf(lang, sizeof(lang), "  %s  ", file_type);
        pos_len = snprintf(pos, sizeof(pos), " %d:%d [%.f%%] <%s> ", row, col,
                           line_percent, nl_type);
    }

    rlen = lang_len + pos_len;

    if (rlen > gEditor.screen_cols)
        rlen = 0;
    if (len + rlen > gEditor.screen_cols)
        len = gEditor.screen_cols - rlen;

    abufAppendN(ab, help_str, len);

    while (len < gEditor.screen_cols) {
        if (gEditor.screen_cols - len == rlen) {
            setColor(ab, gEditor.color_cfg.status[2], 0);
            setColor(ab, gEditor.color_cfg.status[3], 1);
            abufAppendN(ab, lang, lang_len);
            setColor(ab, gEditor.color_cfg.status[4], 0);
            setColor(ab, gEditor.color_cfg.status[5], 1);
            abufAppendN(ab, pos, pos_len);
            break;
        } else {
            abufAppend(ab, " ");
            len++;
        }
    }
}
