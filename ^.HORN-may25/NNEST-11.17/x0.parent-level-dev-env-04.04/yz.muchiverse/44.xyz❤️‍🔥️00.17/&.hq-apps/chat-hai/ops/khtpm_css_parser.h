/* khtpm_css_parser.h — minimal CSS subset for the HQML styling layer
 * (au11-hq/HQML-DESIGN+PLANS.md Phase 1), first proof for db-hq.
 *
 * Scope is deliberately small (au11-hq/rpg-maker-database.html's
 * load-bearing subset only, per the design doc's own "start small" note):
 * selectors = element, .class, .class.class, #id, :hover: properties =
 * background-color, color, border(-color/-width), position/top/left,
 * width/height (px or %), padding, font-family/size/weight, z-index.
 * No flex/grid/animations - out of scope until a section needs them. */
#ifndef KHTPM_CSS_PARSER_H
#define KHTPM_CSS_PARSER_H

#define CSS_MAX_RULES 256
#define CSS_MAX_CLASSES 8

typedef struct {
    int has_bg_color;      char bg_color[32];
    int has_fg_color;      char fg_color[32];
    int has_border_color;  char border_color[32];
    int has_border_width;  int border_width;
    int has_position;      int position_absolute; /* 0=relative/static, 1=absolute */
    int has_top;            int top;    /* px, signed */
    int has_left;           int left;   /* px, signed */
    int has_width;           int width;   int width_is_pct;
    int has_height;          int height;  int height_is_pct;
    int has_padding;         int padding;
    int has_font_family;     char font_family[64];
    int has_font_size;       int font_size;
    int has_font_weight;     int font_weight_bold;
    int has_z_index;         int z_index;
} CssStyle;

typedef struct {
    char selector[128];   /* raw selector text, e.g. ".tab.active", "#sidebar", "button:hover" */
    CssStyle style;
} CssRule;

typedef struct {
    CssRule rules[CSS_MAX_RULES];
    int n_rules;
} CssSheet;

void css_style_init(CssStyle *s);
/* loads path, appends parsed rules into sheet (sheet must be zero-inited by caller first) */
int css_load(const char *path, CssSheet *sheet);
/* computes the cascaded style for an element (tag/id/classes) into *out.
 * hover: pass 1 if the element is currently hovered (activates :hover rules). */
void css_compute_style(const CssSheet *sheet, const char *tag, const char *id,
                        char classes[][32], int n_classes, int hover, CssStyle *out);

#endif
