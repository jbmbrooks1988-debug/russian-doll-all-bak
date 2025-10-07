#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <unistd.h> // for usleep
#include <stdbool.h>

// --- Globals ---

FT_Library ft;
FT_Face face; // For text
FT_Face emoji_face; // For emojis

Display *x_display = NULL;
Window x_window;
Atom clipboard_atom;
Atom utf8_atom;
Atom targets_atom;
Atom xa_string_atom;

char *copied_text = NULL;

#define MAX_LINES 1000
#define MAX_LINE_LENGTH 1024
char text_buffer[MAX_LINES][MAX_LINE_LENGTH];
int num_lines = 1;

int cursor_line = 0;
int cursor_col = 0;

int selection_start_line = -1;
int selection_start_col = -1;
int selection_end_line = -1;
int selection_end_col = -1;
int is_selecting = 0;

int window_width = 800;
int window_height = 600;

int font_size = 16;
float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {0.1f, 0.1f, 0.1f, 1.0f};

char status_message[256] = "";
time_t status_message_expiry = 0;

typedef enum {
    STATE_EDITING,
    STATE_FILE_MENU,
    STATE_LOAD_BROWSER,
    STATE_SAVE_PROMPT,
    STATE_CONTEXT_MENU,
    STATE_EMOJI_PICKER
} UIState;

UIState current_state = STATE_EDITING;
float context_menu_x = 0;
float context_menu_y = 0;

const char *emojis[] = {"😀", "😂", "😍", "😎", "😢", "😊", "👍", "❤️"};
#define NUM_EMOJIS 8

// --- Function Prototypes ---

void initFreeType();
void render_text(const char* str, float x, float y);
void render_emoji(unsigned int codepoint, float x, float y);
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void special(int key, int x, int y);
void mouse(int button, int state, int x, int y);
void motion(int x, int y);
void init();
void idle();
void draw_rect(float x, float y, float w, float h, float color[3]);
void display_context_menu();
void display_emoji_picker();
void copy_selection_to_clipboard();
void paste_from_clipboard();
void insert_emoji(const char *emoji);
char* get_selected_text();
void delete_selection();
void set_status_message(const char* msg, int duration_seconds);

// --- UTF8 & Font Helpers ---

int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) {
        *codepoint = str[0];
        return 1;
    }
    if ((str[0] & 0xE0) == 0xC0) {
        if ((str[1] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
            return 2;
        }
    }
    if ((str[0] & 0xF0) == 0xE0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
            return 3;
        }
    }
    if ((str[0] & 0xF8) == 0xF0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
            return 4;
        }
    }
    *codepoint = '?';
    return 1;
}

float get_char_width(unsigned int codepoint) {
    int is_emoji = (codepoint >= 0x1F300 && codepoint <= 0x1FAD6);
    FT_Face current_face = is_emoji ? emoji_face : face;
    if (!current_face) return font_size; // Fallback
    if (FT_Load_Char(current_face, codepoint, is_emoji ? (FT_LOAD_RENDER | FT_LOAD_COLOR) : FT_LOAD_RENDER)) {
        return font_size; // Fallback width
    }
    if (is_emoji) {
        return (emoji_face->glyph->advance.x >> 6) * emoji_scale;
    } else {
        return (face->glyph->advance.x >> 6);
    }
}

float get_string_width(const char* str, int bytes_to_measure) {
    float total_width = 0;
    const unsigned char* p = (const unsigned char*)str;
    const unsigned char* end = p + bytes_to_measure;
    while (p < end) {
        unsigned int codepoint;
        int bytes = decode_utf8(p, &codepoint);
        if (p + bytes > end) break;
        total_width += get_char_width(codepoint);
        p += bytes;
    }
    return total_width;
}

void screen_to_text_coords(int screen_x, int screen_y, int* out_line, int* out_col) {
    float line_height = font_size * 1.2f;
    float y_offset = window_height - font_size - 10.0f;
    
    *out_line = (y_offset - (window_height - screen_y) + font_size) / line_height;
    if (*out_line < 0) *out_line = 0;
    if (*out_line >= num_lines) *out_line = num_lines - 1;

    float current_x = 10.0f;
    const unsigned char* p = (const unsigned char*)text_buffer[*out_line];
    int byte_pos = 0;
    while (*p) {
        unsigned int codepoint;
        int bytes = decode_utf8(p, &codepoint);
        float char_width = get_char_width(codepoint);
        if (screen_x < current_x + char_width / 2) {
            *out_col = byte_pos;
            return;
        }
        current_x += char_width;
        byte_pos += bytes;
        p += bytes;
    }
    *out_col = byte_pos;
}

// --- FreeType and Rendering ---

void initFreeType() {
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init FreeType Library\n");
        exit(1);
    }

    // Load text face
    const char* font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
    FT_Error err = FT_New_Face(ft, font_path, 0, &face);
    if (err) {
        font_path = "/usr/share/fonts/liberation/LiberationMono-Regular.ttf";
        err = FT_New_Face(ft, font_path, 0, &face);
        if (err) {
            fprintf(stderr, "Could not open text font. Please check paths.\n");
            exit(1);
        }
    }
    if (FT_Set_Pixel_Sizes(face, 0, font_size)) {
        fprintf(stderr, "Could not set pixel size for text font\n");
        exit(1);
    }

    // Load emoji face, copying from 18.th
    const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    err = FT_New_Face(ft, emoji_font_path, 0, &emoji_face);
    if (err) {
        fprintf(stderr, "Error: Could not load emoji font at %s, error code: %d\n", emoji_font_path, err);
        emoji_face = NULL;
        return;
    }
    if (FT_IS_SCALABLE(emoji_face)) {
        err = FT_Set_Pixel_Sizes(emoji_face, 0, 24);
        if (err) {
            fprintf(stderr, "Error: Could not set pixel size to 24 for emoji font, error code: %d\n", err);
            FT_Done_Face(emoji_face);
            emoji_face = NULL;
            return;
        }
    } else if (emoji_face->num_fixed_sizes > 0) {
        err = FT_Select_Size(emoji_face, 0);
        if (err) {
            fprintf(stderr, "Error: Could not select size 0 for emoji font, error code: %d\n", err);
            FT_Done_Face(emoji_face);
            emoji_face = NULL;
            return;
        }
    } else {
        fprintf(stderr, "Error: No fixed sizes available in emoji font\n");
        FT_Done_Face(emoji_face);
        emoji_face = NULL;
        return;
    }

    // Calculate emoji scale
    int loaded_emoji_size = emoji_face->size->metrics.y_ppem;
    emoji_scale = (float)font_size / (float)loaded_emoji_size;
    fprintf(stderr, "Emoji font loaded, loaded size: %d, scale: %f\n", loaded_emoji_size, emoji_scale);
}

void render_emoji(unsigned int codepoint, float x, float y) {
    FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err) {
        fprintf(stderr, "Error: Could not load glyph for codepoint U+%04X, error code: %d\n", codepoint, err);
        return;
    }

    FT_GlyphSlot slot = emoji_face->glyph;
    if (!slot->bitmap.buffer) {
        fprintf(stderr, "Error: No bitmap for glyph U+%04X\n", codepoint);
        return;
    }
    if (slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) {
        fprintf(stderr, "Error: Incorrect pixel mode for glyph U+%04X\n", codepoint);
        return;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float scale_factor = emoji_scale;
    float w = slot->bitmap.width * scale_factor;
    float h = slot->bitmap.rows * scale_factor;
    float x2 = x + slot->bitmap_left * scale_factor;
    float y2 = y - (slot->bitmap.rows - slot->bitmap_top) * scale_factor;

    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 1.0); glVertex2f(x2, y2);
    glTexCoord2f(1.0, 1.0); glVertex2f(x2 + w, y2);
    glTexCoord2f(1.0, 0.0); glVertex2f(x2 + w, y2 + h);
    glTexCoord2f(0.0, 0.0); glVertex2f(x2, y2 + h);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &texture);
}

void render_text(const char* str, float x, float y) {
    const unsigned char* p = (const unsigned char*)str;
    while (*p) {
        unsigned int codepoint;
        int bytes = decode_utf8(p, &codepoint);

        if (codepoint >= 0x1F300 && codepoint <= 0x1FAD6 && emoji_face) {
            render_emoji(codepoint, x, y);
            x += get_char_width(codepoint);
        } else {
            if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER)) {
                p += bytes;
                continue;
            }

            FT_GlyphSlot slot = face->glyph;

            GLuint texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, slot->bitmap.width, slot->bitmap.rows, 0, GL_ALPHA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);

            float x2 = x + slot->bitmap_left;
            float y2 = y - (slot->bitmap.rows - slot->bitmap_top);
            float w = slot->bitmap.width;
            float h = slot->bitmap.rows;

            glEnable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor3fv(font_color);
            glBegin(GL_QUADS);
            glTexCoord2f(0, 1); glVertex2f(x2, y2);
            glTexCoord2f(1, 1); glVertex2f(x2 + w, y2);
            glTexCoord2f(1, 0); glVertex2f(x2 + w, y2 + h);
            glTexCoord2f(0, 0); glVertex2f(x2, y2 + h);
            glEnd();
            glDisable(GL_TEXTURE_2D);
            glDeleteTextures(1, &texture);

            x += (slot->advance.x >> 6);
        }
        p += bytes;
    }
}

// --- Helper Functions ---

void set_status_message(const char* msg, int duration_seconds) {
    strncpy(status_message, msg, sizeof(status_message) - 1);
    status_message[sizeof(status_message) - 1] = '\0';
    status_message_expiry = time(NULL) + duration_seconds;
}

typedef struct {
    float x, y, w, h;
} Rect;

int is_point_in_rect(float x, float y, Rect r) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

void draw_rect(float x, float y, float w, float h, float color[3]) {
    glColor3fv(color);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

// --- File Operations ---

#define MAX_FILES 256
#define MAX_FILENAME 256
char file_list[MAX_FILES][MAX_FILENAME];
int file_count = 0;
char save_filename_buffer[MAX_FILENAME] = "";

void save_file(const char* filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return;
    for (int i = 0; i < num_lines; i++) {
        fprintf(f, "%s\n", text_buffer[i]);
    }
    fclose(f);
    current_state = STATE_EDITING;
    glutPostRedisplay();
}

void list_directory(const char *path) {
    DIR *d;
    struct dirent *dir;
    file_count = 0;
    d = opendir(path);
    if (d) {
        while ((dir = readdir(d)) != NULL && file_count < MAX_FILES) {
            if (strcmp(dir->d_name, "." ) == 0 || strcmp(dir->d_name, "..") == 0) {
                continue;
            }
            if (dir->d_type == DT_DIR) {
                if (strlen(dir->d_name) < MAX_FILENAME - 2) {
                    snprintf(file_list[file_count], MAX_FILENAME, "%s/", dir->d_name);
                    file_count++;
                }
            } else {
                if (strlen(dir->d_name) < MAX_FILENAME) {
                    snprintf(file_list[file_count], MAX_FILENAME, "%s", dir->d_name);
                    file_count++;
                }
            }
        }
        closedir(d);
    }
}

void load_file(const char* filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return;

    for(int i = 0; i < MAX_LINES; ++i) memset(text_buffer[i], 0, MAX_LINE_LENGTH);
    num_lines = 0;

    while (fgets(text_buffer[num_lines], MAX_LINE_LENGTH, f) && num_lines < MAX_LINES -1) {
        text_buffer[num_lines][strcspn(text_buffer[num_lines], "\r\n")] = 0;
        num_lines++;
    }
    if (num_lines == 0) num_lines = 1;

    fclose(f);
    cursor_line = 0;
    cursor_col = 0;
    current_state = STATE_EDITING;
    glutPostRedisplay();
}

// --- Clipboard and Selection ---

void copy_selection_to_clipboard() {
    char* selected_text = get_selected_text();
    if (!selected_text) return;

    if (copied_text) free(copied_text);
    copied_text = selected_text;

    XSetSelectionOwner(x_display, clipboard_atom, x_window, CurrentTime);
    XFlush(x_display);

    set_status_message("Text copied to clipboard", 5);
}

void paste_from_clipboard() {
    if (!x_display) return;

    Atom property = XInternAtom(x_display, "PASTE_PROPERTY", False);

    XConvertSelection(x_display, clipboard_atom, utf8_atom, property, x_window, CurrentTime);
    XFlush(x_display);

    time_t start = time(NULL);
    char *text = NULL;

    while (true) {
        if (XPending(x_display) > 0) {
            XEvent event;
            XNextEvent(x_display, &event);
            if (event.type == SelectionNotify && event.xselection.selection == clipboard_atom) {
                if (event.xselection.property == None) {
                    set_status_message("No clipboard data", 5);
                } else {
                    Atom type;
                    int format;
                    unsigned long num_items, bytes_after;
                    unsigned char *data = NULL;
                    XGetWindowProperty(x_display, x_window, property, 0, LONG_MAX/4, True, AnyPropertyType, &type, &format, &num_items, &bytes_after, &data);
                    if (type == utf8_atom) {
                        text = (char*)data;
                    } else if (type == xa_string_atom) {
                        text = (char*)data;
                    } else {
                        XFree(data);
                    }
                }
                break;
            }
        }

        if (time(NULL) - start > 2) {
            set_status_message("Clipboard timeout", 5);
            break;
        }

        usleep(10000);
    }

    if (text) {
        if (selection_start_line != -1) {
            delete_selection();
        }

        int len = strlen(text_buffer[cursor_line]);
        int paste_len = strlen(text);
        if (len + paste_len < MAX_LINE_LENGTH) {
            memmove(&text_buffer[cursor_line][cursor_col + paste_len], &text_buffer[cursor_line][cursor_col], len - cursor_col + 1);
            memcpy(&text_buffer[cursor_line][cursor_col], text, paste_len);
            cursor_col += paste_len;
        }

        XFree(text);
    }

    glutPostRedisplay();
}

void idle() {
    if (XPending(x_display) > 0) {
        XEvent event;
        XNextEvent(x_display, &event);

        if (event.type == SelectionClear) {
            if (event.xselectionclear.selection == clipboard_atom) {
                // Optional: free copied_text if needed
            }
        } else if (event.type == SelectionRequest) {
            XSelectionRequestEvent *request = &event.xselectionrequest;
            if (request->selection == clipboard_atom) {
                XSelectionEvent reply = {0};
                reply.type = SelectionNotify;
                reply.display = request->display;
                reply.requestor = request->requestor;
                reply.selection = request->selection;
                reply.target = request->target;
                reply.time = request->time;
                reply.property = request->property;

                if (request->target == targets_atom) {
                    Atom targets[] = {targets_atom, utf8_atom, xa_string_atom};
                    XChangeProperty(x_display, request->requestor, request->property, XA_ATOM, 32, PropModeReplace, (unsigned char*)targets, sizeof(targets)/sizeof(Atom));
                } else if (request->target == utf8_atom || request->target == xa_string_atom) {
                    if (copied_text) {
                        XChangeProperty(x_display, request->requestor, request->property, request->target, 8, PropModeReplace, (unsigned char*)copied_text, strlen(copied_text));
                    } else {
                        reply.property = None;
                    }
                } else {
                    reply.property = None;
                }

                XSendEvent(x_display, request->requestor, False, 0, (XEvent*)&reply);
            }
        }
    }
}

char* get_selected_text() {
    if (selection_start_line == -1) return NULL;

    int from_line, from_col, to_line, to_col;
    if (selection_start_line < selection_end_line || (selection_start_line == selection_end_line && selection_start_col < selection_end_col)) {
        from_line = selection_start_line; from_col = selection_start_col;
        to_line = selection_end_line; to_col = selection_end_col;
    } else {
        from_line = selection_end_line; from_col = selection_end_col;
        to_line = selection_start_line; to_col = selection_start_col;
    }

    if (from_line == to_line && from_col == to_col) return NULL;

    size_t total_len = 0;
    for (int i = from_line; i <= to_line; i++) {
        total_len += strlen(text_buffer[i]) + 1; // +1 for newline
    }

    char* result = (char*)malloc(total_len);
    if (!result) return NULL;
    result[0] = '\0';

    if (from_line == to_line) {
        strncat(result, text_buffer[from_line] + from_col, to_col - from_col);
    } else {
        strcat(result, text_buffer[from_line] + from_col);
        strcat(result, "\n");
        for (int i = from_line + 1; i < to_line; i++) {
            strcat(result, text_buffer[i]);
            strcat(result, "\n");
        }
        strncat(result, text_buffer[to_line], to_col);
    }
    return result;
}

void delete_selection() {
    if (selection_start_line == -1) return; 
    
    int from_line, from_col, to_line, to_col;
    if (selection_start_line < selection_end_line || (selection_start_line == selection_end_line && selection_start_col < selection_end_col)) {
        from_line = selection_start_line; from_col = selection_start_col;
        to_line = selection_end_line; to_col = selection_end_col;
    } else {
        from_line = selection_end_line; from_col = selection_end_col;
        to_line = selection_start_line; to_col = selection_start_col;
    }

    if (from_line == to_line) {
        int len = strlen(text_buffer[from_line]);
        memmove(&text_buffer[from_line][from_col], &text_buffer[from_line][to_col], len - to_col + 1);
    } else {
        strcpy(&text_buffer[from_line][from_col], &text_buffer[to_line][to_col]);
        if (to_line > from_line) {
            memmove(&text_buffer[from_line + 1], &text_buffer[to_line + 1], (num_lines - to_line -1) * MAX_LINE_LENGTH);
            num_lines -= (to_line - from_line);
        }
    }
    cursor_line = from_line;
    cursor_col = from_col;
    selection_start_line = -1;
    glutPostRedisplay();
}

void insert_emoji(const char *emoji) {
    int len = strlen(text_buffer[cursor_line]);
    if (len + strlen(emoji) < MAX_LINE_LENGTH - 1) {
        memmove(&text_buffer[cursor_line][cursor_col + strlen(emoji)],
                &text_buffer[cursor_line][cursor_col],
                len - cursor_col + 1);
        strncpy(&text_buffer[cursor_line][cursor_col], emoji, strlen(emoji));
        cursor_col += strlen(emoji);
    }
    glutPostRedisplay();
}

// --- Display Functions ---

void display_editor() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    float x = 10.0f;
    float y = window_height - font_size - 10.0f;
    float line_height = font_size * 1.2f;

    if (selection_start_line != -1) {
        float highlight_color[3] = {0.2f, 0.2f, 0.8f};
        int from_line, from_col, to_line, to_col;
        if (selection_start_line < selection_end_line || (selection_start_line == selection_end_line && selection_start_col < selection_end_col)) {
            from_line = selection_start_line; from_col = selection_start_col;
            to_line = selection_end_line; to_col = selection_end_col;
        } else {
            from_line = selection_end_line; from_col = selection_end_col;
            to_line = selection_start_line; to_col = selection_start_col;
        }

        for (int i = from_line; i <= to_line; i++) {
            float line_y = y - i * line_height;
            float start_x = x;
            if (i == from_line) {
                start_x += get_string_width(text_buffer[i], from_col);
            }
            float end_x = x;
            if (i == to_line) {
                end_x += get_string_width(text_buffer[i], to_col);
            } else {
                end_x += get_string_width(text_buffer[i], strlen(text_buffer[i]));
            }
            draw_rect(start_x, line_y - font_size + 4, end_x - start_x, font_size, highlight_color);
        }
    }

    glColor3fv(font_color);
    for (int i = 0; i < num_lines; i++) {
        render_text(text_buffer[i], x, y - i * line_height);
    }

    float cursor_x = x + get_string_width(text_buffer[cursor_line], cursor_col);
    float cursor_y = y - cursor_line * line_height;

    glBegin(GL_QUADS);
    glVertex2f(cursor_x, cursor_y);
    glVertex2f(cursor_x + 2, cursor_y);
    glVertex2f(cursor_x + 2, cursor_y - font_size);
    glVertex2f(cursor_x, cursor_y - font_size);
    glEnd();

    if (time(NULL) < status_message_expiry) {
        glColor3f(1.0f, 1.0f, 0.0f);
        render_text(status_message, 10, 10);
    }

    Rect file_button_rect = {5, window_height - 25, 40, 20};
    float button_color[3] = {0.3f, 0.3f, 0.3f};
    draw_rect(file_button_rect.x, file_button_rect.y, file_button_rect.w, file_button_rect.h, button_color);
    glColor3fv(font_color);
    render_text("File", 10, window_height - 20);
}

void display_file_menu() {
    Rect menu_rect = {5, window_height - 70, 60, 45};
    float menu_color[3] = {0.2f, 0.2f, 0.2f};
    draw_rect(menu_rect.x, menu_rect.y, menu_rect.w, menu_rect.h, menu_color);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3fv(font_color);
    render_text("Save", 10, window_height - 45);
    render_text("Load", 10, window_height - 65);
}

void display() {
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (current_state == STATE_EDITING || current_state == STATE_CONTEXT_MENU || current_state == STATE_EMOJI_PICKER) {
        display_editor();
    } else if (current_state == STATE_LOAD_BROWSER) {
        display_load_browser();
    } else if (current_state == STATE_SAVE_PROMPT) {
        display_editor();
        display_save_prompt();
    } else if (current_state == STATE_FILE_MENU) {
        display_editor();
        display_file_menu();
    }

    if (current_state == STATE_CONTEXT_MENU) {
        display_context_menu();
    } else if (current_state == STATE_EMOJI_PICKER) {
        display_emoji_picker();
    }

    glutSwapBuffers();
}

void display_save_prompt() {
    Rect prompt_rect = {100, window_height / 2 - 50, window_width - 200, 100};
    float bg_color[3] = {0.2f, 0.2f, 0.2f};
    draw_rect(prompt_rect.x, prompt_rect.y, prompt_rect.w, prompt_rect.h, bg_color);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3fv(font_color);
    render_text("Save As:", 110, window_height / 2 + 20);
    render_text(save_filename_buffer, 110, window_height / 2);
}

void display_load_browser() {
    Rect browser_rect = {50, 50, window_width - 100, window_height - 100};
    float bg_color[3] = {0.2f, 0.2f, 0.2f};
    draw_rect(browser_rect.x, browser_rect.y, browser_rect.w, browser_rect.h, bg_color);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3fv(font_color);
    float y_offset = window_height - 70;
    for (int i = 0; i < file_count; i++) {
        render_text(file_list[i], 60, y_offset);
        y_offset -= 20;
    }

    Rect cancel_button_rect = {window_width - 150, 60, 60, 20};
    float button_color[3] = {0.3f, 0.3f, 0.3f};
    draw_rect(cancel_button_rect.x, cancel_button_rect.y, cancel_button_rect.w, cancel_button_rect.h, button_color);
    glColor3fv(font_color);
    render_text("Cancel", cancel_button_rect.x + 10, cancel_button_rect.y + 5);
}

void display_context_menu() {
    Rect menu_rect = {context_menu_x, context_menu_y - 75, 100, 75};
    float menu_color[3] = {0.2f, 0.2f, 0.2f};
    draw_rect(menu_rect.x, menu_rect.y, menu_rect.w, menu_rect.h, menu_color);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3fv(font_color);
    render_text("Copy", context_menu_x + 10, context_menu_y - 20);
    render_text("Paste", context_menu_x + 10, context_menu_y - 40);
    render_text("Insert Emoji", context_menu_x + 10, context_menu_y - 60);
}

void display_emoji_picker() {
    Rect picker_rect = {window_width/2 - 100, window_height/2 - 100, 200, 160};
    float bg_color[3] = {0.2f, 0.2f, 0.2f};
    draw_rect(picker_rect.x, picker_rect.y, picker_rect.w, picker_rect.h, bg_color);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3fv(font_color);
    float y_offset = window_height/2 + 50;
    for (int i = 0; i < NUM_EMOJIS; i++) {
        render_text(emojis[i], window_width/2 - 90, y_offset);
        y_offset -= 20;
    }
}

// --- GLUT Callbacks ---

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN && current_state == STATE_EDITING) {
        context_menu_x = x;
        context_menu_y = window_height - y;
        current_state = STATE_CONTEXT_MENU;
        glutPostRedisplay();
        return;
    }

    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            is_selecting = 1;
            screen_to_text_coords(x, y, &selection_start_line, &selection_start_col);
            selection_end_line = selection_start_line;
            selection_end_col = selection_start_col;
            cursor_line = selection_start_line;
            cursor_col = selection_start_col;
            glutPostRedisplay();
        } else if (state == GLUT_UP) {
            is_selecting = 0;
        }
    }

    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        float gl_x = x;
        float gl_y = window_height - y;

        if (current_state == STATE_CONTEXT_MENU) {
            Rect copy_rect = {context_menu_x, context_menu_y - 25, 80, 20};
            Rect paste_rect = {context_menu_x, context_menu_y - 45, 80, 20};
            Rect emoji_rect = {context_menu_x, context_menu_y - 65, 80, 20};
            if (is_point_in_rect(gl_x, gl_y, copy_rect)) {
                copy_selection_to_clipboard();
                current_state = STATE_EDITING;
            } else if (is_point_in_rect(gl_x, gl_y, paste_rect)) {
                paste_from_clipboard();
                current_state = STATE_EDITING;
            } else if (is_point_in_rect(gl_x, gl_y, emoji_rect)) {
                current_state = STATE_EMOJI_PICKER;
            } else {
                current_state = STATE_EDITING;
            }
            glutPostRedisplay();
            return;
        } else if (current_state == STATE_EMOJI_PICKER) {
            float y_offset = window_height/2 + 50;
            for (int i = 0; i < NUM_EMOJIS; i++) {
                Rect emoji_rect = {window_width/2 - 90, y_offset - 15, 180, 20};
                if (is_point_in_rect(gl_x, gl_y, emoji_rect)) {
                    insert_emoji(emojis[i]);
                    current_state = STATE_EDITING;
                    break;
                }
                y_offset -= 20;
            }
            if (!is_point_in_rect(gl_x, gl_y, (Rect){window_width/2 - 100, window_height/2 - 100, 200, 160})) {
                current_state = STATE_EDITING;
            }
            glutPostRedisplay();
            return;
        }

        Rect file_button_rect = {5, window_height - 25, 40, 20};
        if (is_point_in_rect(gl_x, gl_y, file_button_rect)) {
            if (current_state == STATE_EDITING) {
                current_state = STATE_FILE_MENU;
            } else {
                current_state = STATE_EDITING;
            }
            glutPostRedisplay();
            return;
        }

        if (current_state == STATE_FILE_MENU) {
            Rect save_button_rect = {10, window_height - 50, 40, 20};
            Rect load_button_rect = {10, window_height - 70, 40, 20};
            if (is_point_in_rect(gl_x, gl_y, save_button_rect)) {
                current_state = STATE_SAVE_PROMPT;
                save_filename_buffer[0] = '\0';
            } else if (is_point_in_rect(gl_x, gl_y, load_button_rect)) {
                list_directory(".");
                current_state = STATE_LOAD_BROWSER;
            }
            glutPostRedisplay();
        } else if (current_state == STATE_LOAD_BROWSER) {
            Rect cancel_button_rect = {window_width - 150, 60, 60, 20};
            if (is_point_in_rect(gl_x, gl_y, cancel_button_rect)) {
                current_state = STATE_EDITING;
                glutPostRedisplay();
                return;
            }

            float y_offset = window_height - 70;
            for (int i = 0; i < file_count; i++) {
                Rect file_rect = {60, y_offset - 15, window_width - 120, 20};
                if (is_point_in_rect(gl_x, gl_y, file_rect)) {
                    load_file(file_list[i]);
                    break;
                }
                y_offset -= 20;
            }
        }
    }
}

void motion(int x, int y) {
    if (is_selecting) {
        screen_to_text_coords(x, y, &selection_end_line, &selection_end_col);
        cursor_line = selection_end_line;
        cursor_col = selection_end_col;
        glutPostRedisplay();
    }
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    if (current_state == STATE_SAVE_PROMPT) {
        int len = strlen(save_filename_buffer);
        if (isprint(key) && len < MAX_FILENAME - 1) {
            save_filename_buffer[len] = key;
            save_filename_buffer[len + 1] = '\0';
        } else if (key == 8 && len > 0) {
            save_filename_buffer[len - 1] = '\0';
        } else if (key == 13) {
            save_file(save_filename_buffer);
        } else if (key == 27) {
            current_state = STATE_EDITING;
        }
        glutPostRedisplay();
        return;
    }

    if (current_state != STATE_EDITING) return;

    if (key == 3) { // Ctrl+C
        copy_selection_to_clipboard();
        return;
    }
    if (key == 22) { // Ctrl+V
        paste_from_clipboard();
        return;
    }
    if (key == 24) { // Ctrl+X
        copy_selection_to_clipboard();
        delete_selection();
        return;
    }

    if (selection_start_line != -1) {
        delete_selection();
    }

    if (isprint(key)) {
        int len = strlen(text_buffer[cursor_line]);
        if (len < MAX_LINE_LENGTH - 1 && cursor_col < MAX_LINE_LENGTH - 1) {
            memmove(&text_buffer[cursor_line][cursor_col + 1],
                    &text_buffer[cursor_line][cursor_col],
                    len - cursor_col + 1);
            text_buffer[cursor_line][cursor_col] = key;
            cursor_col++;
        }
    } else if (key == 8) { // Backspace
        if (cursor_col > 0) {
            int len = strlen(text_buffer[cursor_line]);
            memmove(&text_buffer[cursor_line][cursor_col - 1],
                    &text_buffer[cursor_line][cursor_col],
                    len - cursor_col + 1);
            cursor_col--;
        } else if (cursor_line > 0) {
            int prev_line_len = strlen(text_buffer[cursor_line - 1]);
            int current_line_len = strlen(text_buffer[cursor_line]);
            if (prev_line_len + current_line_len < MAX_LINE_LENGTH) {
                strcat(text_buffer[cursor_line - 1], text_buffer[cursor_line]);
                memmove(&text_buffer[cursor_line], &text_buffer[cursor_line + 1], (num_lines - cursor_line - 1) * MAX_LINE_LENGTH);
                num_lines--;
                cursor_line--;
                cursor_col = prev_line_len;
            }
        }
    } else if (key == 13) { // Enter
        if (num_lines < MAX_LINES) {
            char temp_line[MAX_LINE_LENGTH];
            strcpy(temp_line, &text_buffer[cursor_line][cursor_col]);
            text_buffer[cursor_line][cursor_col] = '\0';
            
            memmove(&text_buffer[cursor_line + 2], &text_buffer[cursor_line + 1], (num_lines - (cursor_line + 1)) * MAX_LINE_LENGTH);
            strcpy(text_buffer[cursor_line + 1], temp_line);
            
            num_lines++;
            cursor_line++;
            cursor_col = 0;
        }
    }
    selection_start_line = -1; // Clear selection after typing
    glutPostRedisplay();
}

void special(int key, int x, int y) {
    selection_start_line = -1; // Clear selection on arrow key movement
    switch (key) {
        case GLUT_KEY_LEFT:
            if (cursor_col > 0) cursor_col--;
            else if (cursor_line > 0) {
                cursor_line--;
                cursor_col = strlen(text_buffer[cursor_line]);
            }
            break;
        case GLUT_KEY_RIGHT:
            if (cursor_col < strlen(text_buffer[cursor_line])) cursor_col++;
            else if (cursor_line < num_lines - 1) {
                cursor_line++;
                cursor_col = 0;
            }
            break;
        case GLUT_KEY_UP:
            if (cursor_line > 0) {
                cursor_line--;
                if (cursor_col > strlen(text_buffer[cursor_line])) {
                    cursor_col = strlen(text_buffer[cursor_line]);
                }
            }
            break;
        case GLUT_KEY_DOWN:
            if (cursor_line < num_lines - 1) {
                cursor_line++;
                if (cursor_col > strlen(text_buffer[cursor_line])) {
                    cursor_col = strlen(text_buffer[cursor_line]);
                }
            }
            break;
        case GLUT_KEY_HOME:
            cursor_col = 0;
            break;
        case GLUT_KEY_END:
            cursor_col = strlen(text_buffer[cursor_line]);
            break;
    }
    glutPostRedisplay();
}

// --- Main and Initialization ---

void init() {
    for(int i = 0; i < MAX_LINES; ++i) {
        memset(text_buffer[i], 0, MAX_LINE_LENGTH);
    }
    initFreeType();

    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();

    if (!x_display) {
        fprintf(stderr, "Could not get X display\n");
    } else {
        clipboard_atom = XInternAtom(x_display, "CLIPBOARD", False);
        utf8_atom = XInternAtom(x_display, "UTF8_STRING", False);
        targets_atom = XInternAtom(x_display, "TARGETS", False);
        xa_string_atom = XA_STRING;
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Gemini Text Editor");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(idle);

    init();

    glutMainLoop();

    FT_Done_Face(face);
    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);

    if (copied_text) free(copied_text);
    if (x_display) XCloseDisplay(x_display);

    return 0;
}
