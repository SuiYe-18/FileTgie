/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 SuiYe_TR (https://github.com/Suiye-18)
 *
 * 【本文件】平台无关工具实现：人类可读大小、标准 CRC32 查表、*? 通配符、扩展名
 * 提取、BOM/UTF-8/UTF-16/ANSI 编码检测、文本启发式、类型表，以及全局排除过滤器。
 *
 * Platform-independent helpers: human size, table-driven CRC32, *? wildcard,
 * extension extraction, BOM/UTF-8/UTF-16/ANSI encoding detection, text
 * heuristic, type table, and the global exclude filter.
 */
#include "util.h"
#include "port.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* 人类可读大小 / Human readable size                                   */
/* ------------------------------------------------------------------ */
void human_size(long long bytes, char* buf, size_t len) {
    double v = (double)bytes;
    const char* units[] = { "B", "KB", "MB", "GB", "TB", "PB" };
    int u = 0;
    /* strict binary 1024 standard: B -> KB -> MB ... only escalate at 1024 */
    while (v >= 1024.0 && u < 5) { v /= 1024.0; u++; }
    if (u == 0) snprintf(buf, len, "%lld B", bytes);
    else snprintf(buf, len, "%.2f %s", v, units[u]);
}

/* ------------------------------------------------------------------ */
/* CRC32（标准多项式 0xEDB88320 查表实现，惰性初始化一次）              */
/* CRC32: standard 0xEDB88320 table, built once on first use            */
/* ------------------------------------------------------------------ */
static unsigned long crc_table[256];
static int crc_ready = 0;

/* 首次调用时生成 256 项查表；之后直接复用。
 * Build the 256-entry lookup table on first use; reused afterwards. */
static void crc_init(void) {
    for (unsigned int i = 0; i < 256; i++) {
        unsigned long c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        crc_table[i] = c;
    }
    crc_ready = 1;
}

int file_crc32(const char* path, unsigned long* out) {
    if (!crc_ready) crc_init();
    FILE* f = ft_fopen(path, "rb");
    if (!f) return -1;
    unsigned long crc = 0xFFFFFFFFUL;
    unsigned char chunk[65536];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0)
        for (size_t i = 0; i < n; i++)
            crc = crc_table[(crc ^ chunk[i]) & 0xFF] ^ (crc >> 8);
    fclose(f);
    *out = crc ^ 0xFFFFFFFFUL;
    return 0;
}

/* ------------------------------------------------------------------ */
/* 通配符 * ?（ASCII 大小写不敏感）/ Wildcard * ? (ASCII case-insensitive) */
/* ------------------------------------------------------------------ */
/* 单字符小写化辅助。Lowercase one ASCII character. */
static int wi(char c) { return tolower((unsigned char)c); }

int wildcard_match(const char* pattern, const char* name) {
    const char* p = pattern;
    const char* n = name;
    const char* star = NULL;
    const char* mark = NULL;
    while (*n) {
        if (*p == '*') {
            star = p++;
            mark = n;
        } else if (*p == '?' || (wi(*p) == wi(*n))) {
            p++; n++;
        } else if (star) {
            p = star + 1;
            n = ++mark;
        } else {
            return 0;
        }
    }
    while (*p == '*') p++;
    return *p == '\0';
}

/* ------------------------------------------------------------------ */
/* 扩展名提取 / Extension extraction                                     */
/* ------------------------------------------------------------------ */
void file_extension(const char* path, char* ext, size_t len) {
    const char* dot = strrchr(path, '.');
    const char* slash = strrchr(path, '/');
#ifdef _WIN32
    const char* bslash = strrchr(path, '\\');
    if (bslash > slash) slash = bslash;
#endif
    ext[0] = '\0';
    if (!dot || (slash && dot < slash)) return;
    size_t i = 0;
    for (const char* s = dot + 1; *s && i + 1 < len; s++, i++)
        ext[i] = (char)tolower((unsigned char)*s);
    ext[i] = '\0';
}

/* ------------------------------------------------------------------ */
/* 编码检测（先看 BOM，再按 UTF-8 合法性，否则归为 ANSI 本地码页）        */
/* Encoding detection: BOM first, then UTF-8 validity, else ANSI/codepage */
/* ------------------------------------------------------------------ */
/* 校验一段字节是否为合法 UTF-8 序列（检查多字节引导位与续接位）。
 * Validate a byte run as well-formed UTF-8 (lead byte + continuation bytes). */
static int utf8_valid(const unsigned char* b, size_t n) {
    size_t i = 0;
    while (i < n) {
        unsigned char c = b[i];
        if (c < 0x80) { i++; continue; }
        int need;
        if ((c & 0xE0) == 0xC0) need = 1;
        else if ((c & 0xF0) == 0xE0) need = 2;
        else if ((c & 0xF8) == 0xF0) need = 3;
        else return 0;
        if (i + need >= n) return 0;
        for (int k = 1; k <= need; k++)
            if ((b[i + k] & 0xC0) != 0x80) return 0;
        i += need + 1;
    }
    return 1;
}

int detect_encoding(const char* path, char* buf, size_t len) {
    FILE* f = ft_fopen(path, "rb");
    if (!f) return -1;
    unsigned char head[4096];
    size_t n = fread(head, 1, sizeof(head), f);
    fclose(f);
    if (n >= 3 && head[0] == 0xEF && head[1] == 0xBB && head[2] == 0xBF) {
        snprintf(buf, len, "UTF-8 BOM");
    } else if (n >= 2 && head[0] == 0xFF && head[1] == 0xFE) {
        snprintf(buf, len, "UTF-16 LE");
    } else if (n >= 2 && head[0] == 0xFE && head[1] == 0xFF) {
        snprintf(buf, len, "UTF-16 BE");
    } else if (utf8_valid(head, n)) {
        snprintf(buf, len, "UTF-8 / ASCII");
    } else {
        snprintf(buf, len, "ANSI / local codepage (e.g. GBK)");
    }
    return 0;
}

int looks_text(const char* path) {
    FILE* f = ft_fopen(path, "rb");
    if (!f) return 0;
    unsigned char head[8192];
    size_t n = fread(head, 1, sizeof(head), f);
    fclose(f);
    for (size_t i = 0; i < n; i++)
        if (head[i] == 0) return 0;
    return 1;
}

long long count_utf8_chars(const char* path) {
    FILE* f = ft_fopen(path, "rb");
    if (!f) return -1;
    long long count = 0;
    int expect = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        unsigned char b = (unsigned char)c;
        if (expect > 0) { expect--; continue; }
        count++;
        if ((b & 0x80) == 0) continue;
        if ((b & 0xE0) == 0xC0) expect = 1;
        else if ((b & 0xF0) == 0xE0) expect = 2;
        else if ((b & 0xF8) == 0xF0) expect = 3;
    }
    fclose(f);
    return count;
}

/* ------------------------------------------------------------------ */
/* 类型猜测（按扩展名查表）/ Type guess (extension lookup table)          */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* 全局排除过滤器（--exclude / --exclude-ext）/ Global exclude filter    */
/* ------------------------------------------------------------------ */
#define EXCL_SLOTS 128
static const char* g_excl_pat[EXCL_SLOTS];
static int g_excl_npat = 0;
static const char* g_excl_ext[EXCL_SLOTS];
static int g_excl_next = 0;

void excl_clear(void) { g_excl_npat = 0; g_excl_next = 0; }

void excl_add_pattern(const char* pat) {
    if (pat && *pat && g_excl_npat < EXCL_SLOTS) g_excl_pat[g_excl_npat++] = pat;
}

void excl_add_ext(const char* ext) {
    if (ext && *ext && g_excl_next < EXCL_SLOTS) g_excl_ext[g_excl_next++] = ext;
}

/* 规范化扩展名：去掉前导点并转小写，使 ".c" 与 "c" 等价。
 * Normalize an extension: strip leading dots and lowercase, so ".c"=="c". */
static void norm_ext(const char* raw, char* out, size_t len) {
    size_t i = 0;
    const char* s = raw;
    while (*s == '.') s++;
    for (; *s && i + 1 < len; s++, i++) out[i] = (char)tolower((unsigned char)*s);
    out[i] = '\0';
}

int excl_skip(const char* name, int is_dir) {
    if (!name) return 0;
    for (int i = 0; i < g_excl_npat; i++)
        if (wildcard_match(g_excl_pat[i], name)) return 1;
    if (!is_dir) {
        char have[64];
        file_extension(name, have, sizeof(have));
        for (int i = 0; i < g_excl_next; i++) {
            char want[64];
            norm_ext(g_excl_ext[i], want, sizeof(want));
            if (ft_strcase_eq(want, have)) return 1;
        }
    }
    return 0;
}

void guess_type(const char* path, char* buf, size_t len) {
    char e[64];
    file_extension(path, e, sizeof(e));
    static const struct { const char* ext; const char* label; } table[] = {
        {"txt","Text"}, {"md","Markdown"}, {"c","C source"}, {"h","C header"},
        {"cpp","C++ source"}, {"hpp","C++ header"}, {"py","Python"}, {"js","JavaScript"},
        {"json","JSON"}, {"xml","XML"}, {"html","HTML"}, {"css","CSS"},
        {"pdf","PDF"}, {"doc","Word"}, {"docx","Word"}, {"xls","Excel"},
        {"xlsx","Excel"}, {"ppt","PowerPoint"}, {"pptx","PowerPoint"},
        {"jpg","JPEG image"}, {"jpeg","JPEG image"}, {"png","PNG image"},
        {"gif","GIF image"}, {"bmp","Bitmap"}, {"svg","SVG vector"},
        {"mp3","Audio"}, {"wav","Audio"}, {"flac","Audio"},
        {"mp4","Video"}, {"mkv","Video"}, {"avi","Video"},
        {"zip","Archive"}, {"rar","Archive"}, {"7z","Archive"}, {"tar","Archive"},
        {"gz","Archive"}, {"exe","Windows program"}, {"dll","Windows library"},
        {"so","Linux library"}, {"sh","Shell script"}, {"bat","Batch script"}
    };
    for (size_t i = 0; i < sizeof(table)/sizeof(table[0]); i++) {
        if (strcmp(e, table[i].ext) == 0) {
            snprintf(buf, len, "%s", table[i].label);
            return;
        }
    }
    snprintf(buf, len, "%s", e[0] ? e : "—");
}
