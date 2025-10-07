#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <locale.h>

// Helper: Write a Unicode codepoint as UTF-8 to file
void write_utf8_codepoint(FILE *f, uint32_t cp) {
    if (cp <= 0x7F) {
        fputc(cp, f);
    } else if (cp <= 0x7FF) {
        fputc(0xC0 | (cp >> 6), f);
        fputc(0x80 | (cp & 0x3F), f);
    } else if (cp <= 0xFFFF) {
        fputc(0xE0 | (cp >> 12), f);
        fputc(0x80 | ((cp >> 6) & 0x3F), f);
        fputc(0x80 | (cp & 0x3F), f);
    } else if (cp <= 0x10FFFF) {
        fputc(0xF0 | (cp >> 18), f);
        fputc(0x80 | ((cp >> 12) & 0x3F), f);
        fputc(0x80 | ((cp >> 6) & 0x3F), f);
        fputc(0x80 | (cp & 0x3F), f);
    }
}

// Helper: Write a sequence of codepoints as UTF-8 string + newline
void write_emoji_sequence(FILE *f, uint32_t *seq, size_t len) {
    for (size_t i = 0; i < len; i++) {
        write_utf8_codepoint(f, seq[i]);
    }
    fputc('\n', f);
}

int main() {
    setlocale(LC_ALL, "");

    FILE *out = fopen("emojis_all.txt", "wb"); // Open in binary to avoid encoding issues
    if (!out) {
        perror("Cannot open emojis_all.txt");
        return 1;
    }

    // --- Single Unicode Emoji Codepoints ---
    // Based on Unicode 15.1 Emoji List (in ranges for compactness)
    // Source: https://unicode.org/emoji/charts/emoji-list.html
    // We'll cover Basic Emoji (single codepoints)

    struct range {
        uint32_t start;
        uint32_t end;
    };

    struct range emoji_ranges[] = {
        {0x231A, 0x231B},    // ⌚ ⌛
        {0x23E9, 0x23EC},    // ⏩ ⏪ ⏫ ⏬
        {0x23F0, 0x23F0},    // ⏰
        {0x23F3, 0x23F3},    // ⏳
        {0x2600, 0x2604},    // ☀ ☁ ☂ ☃ ☄
        {0x260E, 0x260E},    // ☎
        {0x2611, 0x2611},    // ☑
        {0x2614, 0x2615},    // ☔ ☕
        {0x2618, 0x2618},    // ☘
        {0x261D, 0x261D},    // ☝
        {0x2620, 0x2620},    // ☠
        {0x2622, 0x2623},    // ☢ ☣
        {0x2626, 0x2626},    // ☦
        {0x262A, 0x262A},    // ☪
        {0x262E, 0x262F},    // ☮ ☯
        {0x2638, 0x263A},    // ☸ ☹ ☺
        {0x2648, 0x2653},    // ♈-♓
        {0x265F, 0x265F},    // ♟
        {0x2660, 0x2660},    // ♠
        {0x2663, 0x2663},    // ♣
        {0x2665, 0x2666},    // ♥ ♦
        {0x2668, 0x2668},    // ♨
        {0x267B, 0x267B},    // ♻
        {0x267E, 0x267F},    // ♾ ♿
        {0x2692, 0x2697},    // ⚒ ⚓ ⚔ ⚕ ⚖ ⚗
        {0x2699, 0x2699},    // ⚙
        {0x269B, 0x269C},    // ⚛ ⚜
        {0x26A0, 0x26A1},    // ⚠ ⚡
        {0x26AA, 0x26AB},    // ⚪ ⚫
        {0x26B0, 0x26B1},    // ⚰ ⚱
        {0x26BD, 0x26BE},    // ⚽ ⚾
        {0x26C4, 0x26C5},    // ⛄ ⚝
        {0x26C8, 0x26C8},    // ⛈
        {0x26CE, 0x26CE},    // ⛎
        {0x26CF, 0x26D1},    // ⛏ ⛐ ⛑
        {0x26D3, 0x26D4},    // ⛓ ⛔
        {0x26E9, 0x26EA},    // ⛩ ⛪
        {0x26F0, 0x26F5},    // ⛰ ⛱ ⛲ ⛳ ⛴
        {0x26F7, 0x26FA},    // ⛷ ⛸ ⛹ ⛺
        {0x26FD, 0x26FD},    // ⛽
        {0x2702, 0x2702},    // ✂
        {0x2705, 0x2705},    // ✅
        {0x2708, 0x2709},    // ✈ ✉
        {0x270A, 0x270B},    // ✊ ✋
        {0x270C, 0x270D},    // ✌ ✍
        {0x270F, 0x270F},    // ✏
        {0x2712, 0x2712},    // ✒
        {0x2714, 0x2714},    // ✔
        {0x2716, 0x2716},    // ✖
        {0x271D, 0x271D},    // ✝
        {0x2721, 0x2721},    // ✡
        {0x2728, 0x2728},    // ✨
        {0x2733, 0x2734},    // ✳ ✴
        {0x2744, 0x2744},    // ❄
        {0x2747, 0x2747},    // ❇
        {0x274C, 0x274C},    // ❌
        {0x274E, 0x274E},    // ❎
        {0x2753, 0x2755},    // ❓ ❔ ❕
        {0x2757, 0x2757},    // ❗
        {0x2763, 0x2764},    // ❣ ❤
        {0x2795, 0x2797},    // ➕ ➖ ➗
        {0x27A1, 0x27A1},    // ➡
        {0x27B0, 0x27B0},    // ➰
        {0x27BF, 0x27BF},    // ➿
        {0x2B05, 0x2B07},    // ⬅ ⬇
        {0x2B1B, 0x2B1C},    // ⬛ ⬜
        {0x2B50, 0x2B50},    // ⭐
        {0x2B55, 0x2B55},    // ⭕
        {0x1F004, 0x1F004},  // 🀄
        {0x1F0CF, 0x1F0CF},  // 🃏
        {0x1F170, 0x1F171},  // 🅰 🅱
        {0x1F17E, 0x1F17F},  // 🅾 🅿
        {0x1F18E, 0x1F18E},  // 🆎
        {0x1F191, 0x1F19A},  // 🆑- 🆚
        {0x1F1E6, 0x1F1FF},  // Regional indicators A-Z (flags base)
        {0x1F201, 0x1F202},  // 🈁 🈂
        {0x1F21A, 0x1F21A},  // 🈚
        {0x1F22F, 0x1F22F},  // 🈯
        {0x1F232, 0x1F23A},  // 🈲- 🈺
        {0x1F250, 0x1F251},  // 🉐 🉑
        {0x1F300, 0x1F321},  // 🌀 - 🌡
        {0x1F324, 0x1F393},  // 🌤 - 🎓
        {0x1F396, 0x1F397},  // 🎖 🎗
        {0x1F399, 0x1F39B},  // 🎙 🎚 🎛
        {0x1F39E, 0x1F3F0},  // 🎞 🎟 🎠 🎡 🎢 🎣 🎤 🎥 🎦 🎧 🎨 🎩 🎪 🎭 🎮 🎯 🎰 🎱 🎲 🎳 🎴 🎵 🎶 🎷 🎸 🎹 🎺 🎻 🎼 🎽 🎾 🎿 🏀 🏁 🏂 🏃 🏄 🏅 🏆 🏇 🏈 🏉 🏊 🏋 🏌 🏍 🏎 🏏 🏐 🏑 🏒 🏓 🏔 🏕 🏖 🏗 🏘 🏙 🏚 🏛 🏜 🏝 🏞 🏟 🏠 🏡 🏢 🏣 🏤 🏥 🏦 🏧 🏨 🏩 🏪 🏫 🏬 🏭 🏮 🏯 🏰 🏱 🏲 🏳 🏴
        {0x1F3F3, 0x1F3F5},  // 🏳 🏴 🏵
        {0x1F3F7, 0x1F3FA},  // 🏷 🏸 🏹 🏺
        {0x1F400, 0x1F4FD},  // 🐀 - 📽
        {0x1F4FF, 0x1F53D},  // 📿 - 🔽
        {0x1F549, 0x1F54E},  // 🕉 - 🕎
        {0x1F550, 0x1F567},  // 🕐 - 🕧
        {0x1F57A, 0x1F57A},  // 🕺
        {0x1F595, 0x1F596},  // 🖕 🖖
        {0x1F5A4, 0x1F5A4},  // 🖤
        {0x1F5FB, 0x1F64F},  // 🗻 - 🙏
        {0x1F680, 0x1F6C5},  // 🚀 - 🛅
        {0x1F6CC, 0x1F6CC},  // 🛌
        {0x1F6D0, 0x1F6D2},  // 🛐 🛑 🛒
        {0x1F6D5, 0x1F6D7},  // 🛕 🛖 🛗
        {0x1F6DC, 0x1F6DF},  // 🛜 🛝 🛞 🛟
        {0x1F6EB, 0x1F6EC},  // 🛫 🛬
        {0x1F6F4, 0x1F6FC},  // 🛴 - 🛼
        {0x1F7E0, 0x1F7EB},  // Large color squares 🟥🟦🟧🟨🟩🟪🟫⬛⬜
        {0x1F90C, 0x1F93A},  // 🤌 - 🤺
        {0x1F93C, 0x1F945},  // 🤼 - 🤥
        {0x1F947, 0x1F978},  // 🤧 - 🥸
        {0x1F97A, 0x1F9CB},  // 🥺 - 🧋
        {0x1F9CD, 0x1F9FF},  // 🧍 - 🧿
        {0x1FA70, 0x1FA74},  // 🩰 - 🩴
        {0x1FA78, 0x1FA7A},  // 🩸 - 🩺
        {0x1FA80, 0x1FA86},  // 🪀 - 🪆
        {0x1FA90, 0x1FAA8},  // 🪐 - 🪨
        {0x1FAB0, 0x1FAB6},  // 🪰 - 🪶
        {0x1FAC0, 0x1FAC2},  // 🫀 - 🫂
        {0x1FAD0, 0x1FAD6},  // 🫐 - 🫖
        {0x1FAE0, 0x1FAE7},  // 🫠 - 🫧
        {0x1FAF0, 0x1FAF6},  // 🫰 - 🫶
    };

    size_t num_ranges = sizeof(emoji_ranges) / sizeof(emoji_ranges[0]);

    // Write single codepoint emojis
    for (size_t r = 0; r < num_ranges; r++) {
        for (uint32_t cp = emoji_ranges[r].start; cp <= emoji_ranges[r].end; cp++) {
            write_utf8_codepoint(out, cp);
            fputc('\n', out);
        }
    }

    // --- Emoji with Zero Width Joiner (ZWJ) Sequences ---
    // Common sequences: families, professions, flags, gender/skin tone variants

    // Example: Family sequences
    uint32_t family1[] = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F466}; // 👨‍👩‍👦
    uint32_t family2[] = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467}; // 👨‍👩‍👧
    uint32_t family3[] = {0x1F468, 0x200D, 0x1F468, 0x200D, 0x1F466, 0x200D, 0x1F466}; // 👨‍👨‍👦‍👦
    uint32_t rainbow_flag[] = {0x1F3F3, 0xFE0F, 0x200D, 0x1F308}; // 🏳️‍🌈
    uint32_t transgender_flag[] = {0x1F3F3, 0xFE0F, 0x200D, 0x26A7, 0xFE0F}; // 🏳️‍⚧️
    uint32_t man_mechanic[] = {0x1F468, 0x200D, 0x1F527}; // 👨‍🔧
    uint32_t woman_scientist[] = {0x1F469, 0x200D, 0x1F52C}; // 👩‍🔬
    uint32_t woman_firefighter_dark[] = {0x1F469, 0x1F3FF, 0x200D, 0x1F692}; // 👩🏿‍🚒

    uint32_t *sequences[] = {
        family1, family2, family3, rainbow_flag, transgender_flag,
        man_mechanic, woman_scientist, woman_firefighter_dark
    };
    size_t sequence_lengths[] = {
        5, 5, 7, 4, 5, 3, 3, 4
    };
    size_t num_sequences = sizeof(sequences) / sizeof(sequences[0]);

    for (size_t i = 0; i < num_sequences; i++) {
        write_emoji_sequence(out, sequences[i], sequence_lengths[i]);
    }

    // --- Flags (using regional indicator pairs) ---
    // A = U+1F1E6, Z = U+1F1FF
    // Write all 26x26 possible flag combinations (many are not real, but valid emoji sequences)
    for (uint32_t c1 = 0x1F1E6; c1 <= 0x1F1FF; c1++) {
        for (uint32_t c2 = 0x1F1E6; c2 <= 0x1F1FF; c2++) {
            write_utf8_codepoint(out, c1);
            write_utf8_codepoint(out, c2);
            fputc('\n', out);
        }
    }

    fclose(out);
    printf("✅ Wrote all standard Unicode emojis to 'emojis_all.txt'\n");
    return 0;
}