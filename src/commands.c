/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 SuiYe_TR (https://github.com/Suiye-18)
 *
 * 【本文件】全部 58 个子命令的实现与命令表 CMD_TABLE：先定义参数/路径校验等
 * 内部助手，再按功能分组实现各 cmd_*，最后由 CMD_TABLE 把命令名接到处理函数。
 *
 * All 58 subcommand implementations and the CMD_TABLE registry: internal
 * validation helpers first, cmd_* handlers grouped by feature, then the
 * table mapping each command name to its handler.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "commands.h"
#include "port.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

/* ================================================================== */
/* 内部小助手 / Small internal helpers                                  */
/* ================================================================== */
int g_dellog_cleared = 0;

/* 最少参数个数检查：不足时打印本地化用法并返回 1。
 * Minimal-arity check: prints localized usage and returns 1 on shortage. */
static int need(const LangPack* L, int argc, int n, const char* usage) {
    if (argc < n) { printf(L->err_args, usage); printf("\n"); return 1; }
    return 0;
}

/* 拒绝超出固定个数 n 的尾随参数（argv[n] 即多余项）。
 * Reject trailing arguments beyond the fixed count n (argv[n] is the culprit). */
static int check_extra(const LangPack* L, int argc, int n, char** argv) {
    if (argc > n) { printf(L->err_extra_args, argv[n]); printf("\n"); return 1; }
    return 0;
}

/* ---- 严格数值参数解析（不静默退回 0）/ Strict numeric parsing (no silent 0 fallback) ---- */

/* 解析非负整数：拒绝空串、符号、字母、小数、空白与溢出；成功返回 0。
 * Parse a non-negative integer. Rejects empty string, sign, letters,
 * decimals, surrounding whitespace and overflow. Returns 0 on success. */
static int parse_int_nonneg(const LangPack* L, const char* s, long long* out) {
    if (!s || !*s) { printf(L->err_int_bad, s ? s : ""); printf("\n"); return 1; }
    const char* p = s;
    if (*p == '+') p++;
    else if (*p == '-') { printf(L->err_negative, s); printf("\n"); return 1; }
    for (const char* q = p; *q; q++)
        if (!isdigit((unsigned char)*q)) { printf(L->err_int_bad, s); printf("\n"); return 1; }
    errno = 0;
    char* end = NULL;
    long long v = strtoll(p, &end, 10);
    if (errno == ERANGE || !end || *end != '\0') { printf(L->err_int_bad, s); printf("\n"); return 1; }
    *out = v;
    return 0;
}

/* 解析非负小数：拒绝空、非数字、负数、NaN、Inf、前导空白与溢出；成功返回 0。
 * Parse a non-negative decimal number. Rejects empty, non-numeric, negative,
 * NaN, Inf, leading whitespace and overflow. Returns 0 on success. */
static int parse_num_nonneg(const LangPack* L, const char* s, double* out) {
    if (!s || !*s) { printf(L->err_num_bad, s ? s : ""); printf("\n"); return 1; }
    if (isspace((unsigned char)s[0])) { printf(L->err_num_bad, s); printf("\n"); return 1; }
    errno = 0;
    char* end = NULL;
    double v = strtod(s, &end);
    if (errno == ERANGE || !end || *end != '\0' || isnan(v) || isinf(v)) {
        printf(L->err_num_bad, s); printf("\n"); return 1;
    }
    if (v < 0.0) { printf(L->err_negative, s); printf("\n"); return 1; }
    *out = v;
    return 0;
}

/* ---- 路径角色校验 / Path role checks ---- */

/* 目录命令：路径必须存在且是目录；区分"路径不存在"与"给了文件却要目录"。
 * Directory command: path must exist and be a directory. Distinguishes a
 * plain missing path from "a file was passed where a directory was wanted". */
static int check_dir(const LangPack* L, const char* p) {
    if (ft_is_dir(p)) return 0;
    if (ft_is_file(p)) { printf(L->err_need_dir, p); printf("\n"); return 1; }
    printf(L->err_dirmissing, p); printf("\n"); return 1;
}

/* 文件命令：路径必须存在且是常规文件；区分"路径不存在"与"给了目录却要文件"。
 * File command: path must exist and be a regular file. Distinguishes a plain
 * missing path from "a directory was passed where a file was wanted". */
static int check_file(const LangPack* L, const char* p) {
    if (ft_is_file(p)) return 0;
    if (ft_is_dir(p)) { printf(L->err_need_file, p); printf("\n"); return 1; }
    printf(L->err_notfound, p); printf("\n"); return 1;
}

/* 角色校验后以读方式打开；可读但被拒绝的文件给出本地化"无法读取"而非笼统
 * 的"不存在"。
 * Open a file for reading after role validation; a readable-but-denied file
 * yields the localized access error rather than a generic not-found. */
static FILE* open_read(const LangPack* L, const char* p) {
    if (check_file(L, p)) return NULL;
    FILE* f = ft_fopen(p, "rb");
    if (!f) { printf(L->err_access, p); printf("\n"); return NULL; }
    return f;
}

/* 执行遍历；遍历器本身进不去目录时给出友好的"无法读取"，而不是静默输出空结果。
 * Run a traversal; report a friendly access error instead of a silent empty
 * result when the walker itself cannot enter the tree. */
static int walk_check(const LangPack* L, const char* root, int recursive, ft_walk_fn cb, void* u) {
    if (ft_walk(root, recursive, cb, u) != 0) {
        printf(L->err_access, root); printf("\n");
        return 1;
    }
    return 0;
}

/* 去掉行尾的 \r、空格、制表符（兼容 CRLF 与尾随空白）。
 * Strip trailing CR, spaces and tabs (CRLF tolerance + padding). */
static void trim_cr(char* s) {
    size_t l = strlen(s);
    while (l > 0 && (s[l-1] == '\r' || s[l-1] == ' ' || s[l-1] == '\t'))
        s[--l] = '\0';
}

/* 释放 read_lines 返回的行数组：逐行 free 后再释放数组本身。
 * Free the line array from read_lines: each line, then the array itself. */
static void free_lines(char** a, int n) {
    if (!a) return;
    for (int i = 0; i < n; i++) free(a[i]);
    free(a);
}

/* 把文件读成行数组，调用方用 free_lines 释放。每次分配都检查；失败时先清理
 * 半成品数组与流再返回 NULL。
 * read a file into an array of lines; caller frees with free_lines.
 * Every allocation is checked; on failure the partially built array and the
 * stream are cleaned up before returning NULL. */
static char** read_lines(const char* path, int* out_n) {
    *out_n = 0;
    FILE* f = ft_fopen(path, "rb");
    if (!f) return NULL;
    size_t cap = 64, n = 0;
    char** arr = (char**)calloc(cap, sizeof(char*));
    if (!arr) { fclose(f); return NULL; }
    size_t bl = 0, bcap = 256;
    char* line = (char*)malloc(bcap);
    if (!line) { free(arr); fclose(f); return NULL; }
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') {
            line[bl] = '\0';
            if (n == cap) {
                size_t nc = cap * 2;
                char** na = (char**)realloc(arr, nc * sizeof(char*));
                if (!na) { free_lines(arr, (int)n); free(line); fclose(f); return NULL; }
                arr = na; cap = nc;
            }
            arr[n++] = line;
            bl = 0; bcap = 256;
            line = (char*)malloc(bcap);
            if (!line) { free_lines(arr, (int)n); fclose(f); return NULL; }
        } else {
            if (bl + 1 >= bcap) {
                bcap *= 2;
                char* nl = (char*)realloc(line, bcap);
                if (!nl) { free_lines(arr, (int)n); free(line); fclose(f); return NULL; }
                line = nl;
            }
            line[bl++] = (char)c;
        }
    }
    if (bl > 0) {
        line[bl] = '\0';
        if (n == cap) {
            size_t nc = cap * 2;
            char** na = (char**)realloc(arr, nc * sizeof(char*));
            if (!na) { free_lines(arr, (int)n); free(line); fclose(f); return NULL; }
            arr = na; cap = nc;
        }
        arr[n++] = line;
    } else free(line);
    fclose(f);
    /* strip a leading UTF-8 BOM from the first line */
    if (n > 0 && arr[0][0] == '\xEF' && arr[0][1] == '\xBB' && arr[0][2] == '\xBF')
        memmove(arr[0], arr[0] + 3, strlen(arr[0]) - 2);
    *out_n = (int)n;
    return arr;
}

/* ================================================================== */
/* help / version / langs / log / dellog                                */
/* 帮助、版本、语言列表，以及自身运行日志的查看与删除                      */
/* Help, version, language list, and own run-log view / delete.         */
/* ================================================================== */
int cmd_help(const LangPack* L, int argc, char** argv) {
    (void)argc; (void)argv;
    printf("%s  v%s\n", L->banner, FILETGIE_VERSION);
    printf("%s\n\n", L->usage);
    printf("%s\n", L->help_title);
    for (int i = 0; i < CMD_COUNT; i++) {
        const char* desc = *(const char**)((const char*)L + CMD_TABLE[i].help_off);
        printf("  %-14s  %s\n", CMD_TABLE[i].name, desc);
    }
    printf("\n--lang cn|en|ja|ru  %s\n", L->lang_now);
    return 0;
}

static int cmd_version(const LangPack* L, int argc, char** argv) {
    if (check_extra(L, argc, 1, argv)) return 1;
    printf(L->ver_line, FILETGIE_VERSION);
    printf("\n");
    return 0;
}

static int cmd_langs(const LangPack* L, int argc, char** argv) {
    if (check_extra(L, argc, 1, argv)) return 1;
    (void)L;
    lang_list_all();
    return 0;
}

/* 打印可执行文件旁的自身运行日志；缺失/为空都友好提示，不报错。
 * Print FileTgie's own run log (next to the executable). Missing / empty are
 * friendly, non-fatal. */
static int cmd_log(const LangPack* L, int argc, char** argv) {
    if (check_extra(L, argc, 1, argv)) return 1;
    char lp[4096];
    ft_log_path(lp, sizeof(lp));
    FILE* f = ft_fopen(lp, "rb");
    if (!f) { printf("%s\n", L->log_missing); return 0; }
    int any = 0, last = '\n', c;
    while ((c = fgetc(f)) != EOF) { putchar(c); any = 1; last = c; }
    fclose(f);
    if (!any) printf("%s\n", L->log_empty);
    else if (last != '\n') printf("\n");
    return 0;
}

/* 只删除 FileTgie 自身日志文件，绝不触碰其他任何文件。
 * Delete only FileTgie's own log file; never touches anything else. */
static int cmd_dellog(const LangPack* L, int argc, char** argv) {
    if (check_extra(L, argc, 1, argv)) return 1;
    g_dellog_cleared = 1; /* never log the dellog invocation itself */
    char lp[4096];
    ft_log_path(lp, sizeof(lp));
    if (!ft_path_exists(lp)) { printf("%s\n", L->log_missing); return 0; }
    if (ft_remove_file(lp) == 0) printf("%s\n", L->log_deleted);
    else printf("%s\n", L->log_missing);
    return 0;
}

/* ================================================================== */
/* stat / count / lines / linesdir / size / du                         */
/* 基础统计：详情、文件/目录计数、行数、大小与目录汇总                    */
/* Basics: detail, file/dir counts, line counts, size and directory sum */
/* ================================================================== */
static int cmd_stat(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "stat <path>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    ft_stat_t st;
    if (ft_stat(argv[1], &st) != 0) { printf(L->err_notfound, argv[1]); printf("\n"); return 1; }
    char hsize[32], mt[32], ct[32], at[32];
    human_size(st.size, hsize, sizeof(hsize));
    ft_format_time(st.mtime, mt, sizeof(mt));
    ft_format_time(st.ctime, ct, sizeof(ct));
    ft_format_time(st.atime, at, sizeof(at));
    printf("%s: %s\n", L->lbl_path, argv[1]);
    printf("  %s: %s\n", L->lbl_type, st.is_dir ? L->lbl_dir : L->lbl_file);
    printf("  %s: %s (%lld %s)\n", L->lbl_size, hsize, st.size, L->lbl_bytes);
    printf("  %s: %s\n", L->lbl_mtime, mt);
    printf("  %s: %s\n", L->lbl_ctime, ct);
    printf("  %s: %s\n", L->lbl_atime, at);
    return 0;
}

typedef struct { long long files, dirs; } CountCtx;
static void cb_count(const ft_entry_t* e, void* u) {
    CountCtx* c = (CountCtx*)u;
    if (e->is_dir) c->dirs++; else c->files++;
}
static int cmd_count(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "count <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    CountCtx c = {0,0};
    if (walk_check(L, argv[1], 1, cb_count, &c)) return 1;
    printf("%s: %lld %s\n", L->lbl_file, c.files, L->lbl_count);
    printf("%s: %lld %s\n", L->lbl_dir, c.dirs, L->lbl_count);
    printf("%s: %lld\n", L->lbl_total, c.files + c.dirs);
    return 0;
}

static int cmd_lines(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "lines <file>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    FILE* f = open_read(L, argv[1]);
    if (!f) return 1;
    long long lines = 0; int c, prev = '\n';
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') lines++;
        prev = c;
    }
    if (prev != '\n' && prev != EOF) lines++;
    fclose(f);
    printf("%s: %lld\n", L->lbl_lines, lines);
    return 0;
}

typedef struct { long long total; const LangPack* L; } LinesDirCtx;
static void cb_linesdir(const ft_entry_t* e, void* u) {
    LinesDirCtx* c = (LinesDirCtx*)u;
    if (e->is_dir || !looks_text(e->full)) return;
    FILE* f = ft_fopen(e->full, "rb");
    if (!f) return;
    int ch, prev = '\n';
    while ((ch = fgetc(f)) != EOF) { if (ch == '\n') c->total++; prev = ch; }
    if (prev != '\n' && prev != EOF) c->total++;
    fclose(f);
}
static int cmd_linesdir(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "linesdir <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    LinesDirCtx c = {0, L};
    if (walk_check(L, argv[1], 1, cb_linesdir, &c)) return 1;
    printf("%s: %lld\n", L->lbl_lines, c.total);
    return 0;
}

static int cmd_size(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "size <file>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_file(L, argv[1])) return 1;
    ft_stat_t st;
    if (ft_stat(argv[1], &st) != 0) { printf(L->err_notfound, argv[1]); printf("\n"); return 1; }
    char h[32]; human_size(st.size, h, sizeof(h));
    printf("%s: %s (%lld %s)\n", L->lbl_size, h, st.size, L->lbl_bytes);
    return 0;
}

typedef struct { long long bytes; } DuCtx;
static void cb_du(const ft_entry_t* e, void* u) {
    DuCtx* c = (DuCtx*)u;
    if (!e->is_dir) c->bytes += e->size;
}
static int cmd_du(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "du <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    DuCtx c = {0};
    if (walk_check(L, argv[1], 1, cb_du, &c)) return 1;
    char h[32]; human_size(c.bytes, h, sizeof(h));
    printf("%s: %s (%lld %s)\n", L->lbl_total, h, c.bytes, L->lbl_bytes);
    return 0;
}

/* Directory comparison accumulator: counts, bytes, text files/lines and
 * hidden files. It only reads metadata and text bytes; it never writes. */
typedef struct {
    long long files;
    long long dirs;
    long long bytes;
    long long text_files;
    long long text_lines;
    long long hidden;
} DirCompareStat;

static void cb_dirstat(const ft_entry_t* e, void* u) {
    DirCompareStat* s = (DirCompareStat*)u;
    if (e->is_dir) { s->dirs++; return; }
    s->files++;
    s->bytes += e->size;
    if (e->hidden) s->hidden++;
    if (!looks_text(e->full)) return;
    s->text_files++;
    FILE* f = ft_fopen(e->full, "rb");
    if (!f) return;
    int ch, prev = '\n';
    while ((ch = fgetc(f)) != EOF) {
        if (ch == '\n') s->text_lines++;
        prev = ch;
    }
    if (prev != '\n' && prev != EOF) s->text_lines++;
    fclose(f);
}

static int collect_dirstat(const LangPack* L, const char* path, DirCompareStat* s) {
    memset(s, 0, sizeof(*s));
    if (check_dir(L, path)) return 1;
    return walk_check(L, path, 1, cb_dirstat, s);
}

/* Format a signed count for the comparison table; "=0" marks an unchanged row. */
static void fmt_signed_long(long long v, char* out, size_t n) {
    if (v == 0) snprintf(out, n, "=0");
    else snprintf(out, n, "%+lld", v);
}

/* Format a signed human-readable size; "=0 B" marks an unchanged row. */
static void fmt_signed_size(long long v, char* out, size_t n) {
    char h[32];
    human_size(v < 0 ? -v : v, h, sizeof(h));
    if (v == 0) snprintf(out, n, "=%s", h);
    else snprintf(out, n, "%c%s", v < 0 ? '-' : '+', h);
}

static int cmd_dirdiff(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 3, "dirdiff <dirA> <dirB>")) return 1;
    if (check_extra(L, argc, 3, argv)) return 1;
    DirCompareStat a, b;
    if (collect_dirstat(L, argv[1], &a)) return 1;
    if (collect_dirstat(L, argv[2], &b)) return 1;

    char sa[32], sb[32];
    human_size(a.bytes, sa, sizeof(sa));
    human_size(b.bytes, sb, sizeof(sb));

    long long avg_a = a.files ? a.bytes / a.files : 0;
    long long avg_b = b.files ? b.bytes / b.files : 0;
    char aa[32], ab[32];
    human_size(avg_a, aa, sizeof(aa));
    human_size(avg_b, ab, sizeof(ab));

    char d_files[16], d_dirs[16], d_size[64], d_bytes[16];
    char d_text[16], d_lines[16], d_hidden[16], d_avg[64];
    fmt_signed_long(b.files - a.files, d_files, sizeof(d_files));
    fmt_signed_long(b.dirs - a.dirs, d_dirs, sizeof(d_dirs));
    fmt_signed_size(b.bytes - a.bytes, d_size, sizeof(d_size));
    fmt_signed_long(b.bytes - a.bytes, d_bytes, sizeof(d_bytes));
    fmt_signed_long(b.text_files - a.text_files, d_text, sizeof(d_text));
    fmt_signed_long(b.text_lines - a.text_lines, d_lines, sizeof(d_lines));
    fmt_signed_long(b.hidden - a.hidden, d_hidden, sizeof(d_hidden));
    fmt_signed_size(avg_b - avg_a, d_avg, sizeof(d_avg));

    printf("%s\n", L->cmp_title);
    printf("A: %s\n", argv[1]);
    printf("B: %s\n\n", argv[2]);
    printf("%-20s %14s %14s %12s\n", L->cmp_metric, "A", "B", L->cmp_col_delta);
    printf("----------------------------------------------------------------\n");

    printf("%-20s %14lld %14lld %12s\n", L->lbl_file, a.files, b.files, d_files);
    printf("%-20s %14lld %14lld %12s\n", L->lbl_dir, a.dirs, b.dirs, d_dirs);

    printf("\n");
    printf("%-20s %14s %14s %12s\n", L->lbl_size, sa, sb, d_size);
    printf("%-20s %14lld %14lld %12s\n", L->lbl_bytes, a.bytes, b.bytes, d_bytes);
    printf("%-20s %14s %14s %12s\n", L->cmp_avg_size, aa, ab, d_avg);

    printf("\n");
    printf("%-20s %14lld %14lld %12s\n", L->cmp_text_files, a.text_files, b.text_files, d_text);
    printf("%-20s %14lld %14lld %12s\n", L->lbl_lines, a.text_lines, b.text_lines, d_lines);

    printf("\n");
    printf("%-20s %14lld %14lld %12s\n", L->cmp_hidden, a.hidden, b.hidden, d_hidden);
    return 0;
}

/* ================================================================== */
/* tree / list                                                         */
/* 目录树打印与单层列表；树需手工递归以画出正确的连接前缀                 */
/* Tree printing and flat listing; the tree walks manually to draw the  */
/* correct connector prefixes.                                          */
/* ================================================================== */
/* 手工递归遍历以便画出正确的树形前缀（├─ / └─）。
 * Manual recursive traversal so we can draw correct tree prefixes. */
static void tree_print(const char* path, int depth, int max_depth, const char* prefix) {
    if (depth > max_depth) return;
#ifdef _WIN32
    wchar_t* wp = NULL;
    {
        int n = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
        if (n <= 0) return;
        wp = (wchar_t*)malloc((size_t)n * sizeof(wchar_t));
        if (!wp) return;
        MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, n);
    }
    char pat[4200];
    snprintf(pat, sizeof(pat), "%s\\*", path);
    wchar_t* wpat = NULL;
    {
        int n = MultiByteToWideChar(CP_UTF8, 0, pat, -1, NULL, 0);
        if (n <= 0) { free(wp); return; }
        wpat = (wchar_t*)malloc((size_t)n * sizeof(wchar_t));
        if (!wpat) { free(wp); return; }
        MultiByteToWideChar(CP_UTF8, 0, pat, -1, wpat, n);
    }
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(wpat, &fd);
    free(wpat); free(wp);
    if (h == INVALID_HANDLE_VALUE) return;
    int total = 0;
    /* collect children to know the last entry */
    WIN32_FIND_DATAW* all = NULL; int cap = 16;
    all = (WIN32_FIND_DATAW*)calloc((size_t)cap, sizeof(WIN32_FIND_DATAW));
    if (!all) { FindClose(h); return; }
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if (total == cap) {
            cap *= 2;
            WIN32_FIND_DATAW* na = (WIN32_FIND_DATAW*)realloc(all, (size_t)cap * sizeof(WIN32_FIND_DATAW));
            if (!na) break;
            all = na;
        }
        all[total++] = fd;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    for (int i = 0; i < total; i++) {
        WIN32_FIND_DATAW* d = &all[i];
        int isdir = (d->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        char name[1024];
        WideCharToMultiByte(CP_UTF8, 0, d->cFileName, -1, name, sizeof(name), NULL, NULL);
        int last = (i == total - 1);
        printf("%s%s%s %s\n", prefix, last ? "└─ " : "├─ ", name, isdir ? "/" : "");
        if (isdir && depth + 1 <= max_depth) {
            char child[4200];
            snprintf(child, sizeof(child), "%s\\%s", path, name);
            char np[4200];
            snprintf(np, sizeof(np), "%s%s", prefix, last ? "    " : "│   ");
            tree_print(child, depth + 1, max_depth, np);
        }
    }
    free(all);
#else
    DIR* d = opendir(path);
    if (!d) return;
    struct dirent* de;
    int cap = 16, total = 0;
    struct dirent** all = (struct dirent**)calloc((size_t)cap, sizeof(struct dirent*));
    if (!all) { closedir(d); return; }
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        if (total == cap) {
            cap *= 2;
            struct dirent** na = (struct dirent**)realloc(all, (size_t)cap * sizeof(struct dirent*));
            if (!na) break;
            all = na;
        }
        all[total++] = de;
    }
    closedir(d);
    for (int i = 0; i < total; i++) {
        char child[4200];
        snprintf(child, sizeof(child), "%s/%s", path, all[i]->d_name);
        struct stat s;
        int isdir = (lstat(child, &s) == 0 && S_ISDIR(s.st_mode));
        int last = (i == total - 1);
        printf("%s%s%s %s\n", prefix, last ? "└─ " : "├─ ", all[i]->d_name, isdir ? "/" : "");
        if (isdir && depth + 1 <= max_depth) {
            char np[4200];
            snprintf(np, sizeof(np), "%s%s", prefix, last ? "    " : "│   ");
            tree_print(child, depth + 1, max_depth, np);
        }
    }
    free(all);
#endif
}

static int cmd_tree(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "tree <dir> [depth]")) return 1;
    if (argc > 3) { printf(L->err_extra_args, argv[3]); printf("\n"); return 1; }
    if (check_dir(L, argv[1])) return 1;
    long long depth = 20;
    if (argc >= 3 && parse_int_nonneg(L, argv[2], &depth)) return 1;
    int maxd = (int)depth;
    printf("%s/\n", argv[1]);
    tree_print(argv[1], 0, maxd, "");
    return 0;
}

static void cb_list(const ft_entry_t* e, void* u) {
    (void)u;
    char t[32], mt[32];
    human_size(e->size, t, sizeof(t));
    ft_format_time(e->mtime, mt, sizeof(mt));
    printf("%-4s %12s  %s  %s\n", e->is_dir ? "[D]" : "[F]", t, mt, e->name);
}
static int cmd_list(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "list <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    printf("%-4s %12s  %s  %s\n", L->hdr_type, L->hdr_size, L->lbl_mtime, L->hdr_name);
    if (walk_check(L, argv[1], 0, cb_list, NULL)) return 1;
    return 0;
}

/* ================================================================== */
/* find commands                                                       */
/* 查找族：按名通配、扩展名、内容、空文件、大小阈值、新旧时间            */
/* Search family: name wildcard, extension, content, empty, size        */
/* threshold, old/new by mtime.                                        */
/* ================================================================== */
typedef struct { const char* pattern; const LangPack* L; int hits; } NameCtx;
static void cb_findname(const ft_entry_t* e, void* u) {
    NameCtx* c = (NameCtx*)u;
    if (!e->is_dir && wildcard_match(c->pattern, e->name)) {
        printf("%s\n", e->full);
        c->hits++;
    }
}
static int cmd_findname(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 3, "findname <dir> <wildcard>")) return 1;
    if (check_extra(L, argc, 3, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    NameCtx c = { argv[2], L, 0 };
    if (walk_check(L, argv[1], 1, cb_findname, &c)) return 1;
    if (c.hits == 0) printf("%s\n", L->msg_no_match);
    return 0;
}

typedef struct { const char* ext; const LangPack* L; int hits; } ExtCtx;
static void cb_findext(const ft_entry_t* e, void* u) {
    ExtCtx* c = (ExtCtx*)u;
    if (e->is_dir) return;
    char ex[64]; file_extension(e->name, ex, sizeof(ex));
    if (ft_strcase_eq(ex, c->ext)) { printf("%s\n", e->full); c->hits++; }
}
static int cmd_findext(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 3, "findext <dir> <ext>")) return 1;
    if (check_extra(L, argc, 3, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    ExtCtx c = { argv[2], L, 0 };
    if (walk_check(L, argv[1], 1, cb_findext, &c)) return 1;
    if (c.hits == 0) printf("%s\n", L->msg_no_match);
    return 0;
}

typedef struct { const char* text; long long hits; } ContentCtx;
static void cb_findcontent(const ft_entry_t* e, void* u) {
    ContentCtx* c = (ContentCtx*)u;
    if (e->is_dir || !looks_text(e->full)) return;
    int n; char** lines = read_lines(e->full, &n);
    if (!lines) return;
    for (int i = 0; i < n; i++) {
        if (strstr(lines[i], c->text) != NULL) {
            trim_cr(lines[i]);
            if (strlen(lines[i]) > 120) lines[i][120] = '\0';
            printf("%s:%d: %s\n", e->full, i + 1, lines[i]);
            c->hits++;
        }
    }
    free_lines(lines, n);
}
static int cmd_findcontent(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 3, "findcontent <dir> <text>")) return 1;
    if (check_extra(L, argc, 3, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    ContentCtx c = { argv[2], 0 };
    if (walk_check(L, argv[1], 1, cb_findcontent, &c)) return 1;
    printf("%s: %lld\n", L->lbl_matches, c.hits);
    return 0;
}

/* findempty 上下文：命中计数、递归收集到的子目录路径（用于二轮非递归判空）。
 * findempty context: hit counter and directory paths collected during the
 * recursive pass (used by the second, non-recursive emptiness check). */
typedef struct { const LangPack* L; int hits; char** dirs; int dn, dcap; } EmptyCtx;
static void cb_empty_file(const ft_entry_t* e, void* u) {
    EmptyCtx* c = (EmptyCtx*)u;
    if (!e->is_dir && e->size == 0) { printf("%s\n", e->full); c->hits++; }
    if (e->is_dir) {
        if (c->dn == c->dcap) {
            c->dcap = c->dcap ? c->dcap * 2 : 32;
            char** na = (char**)realloc(c->dirs, (size_t)c->dcap * sizeof(char*));
            if (!na) return;
            c->dirs = na;
        }
        c->dirs[c->dn++] = strdup(e->full);
    }
}
/* 非递归计数回调：统计一个目录顶层条目数（0 即空目录）。
 * Non-recursive counter: count top-level entries of one folder (0 = empty). */
static void cb_count_one(const ft_entry_t* e, void* u) { (void)e; (*(int*)u)++; }
static int cmd_findempty(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "findempty <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    EmptyCtx c = { L, 0, NULL, 0, 0 };
    if (walk_check(L, argv[1], 1, cb_empty_file, &c)) {
        for (int i = 0; i < c.dn; i++) free(c.dirs[i]);
        free(c.dirs);
        return 1;
    }
    /* 二轮：对收集到的每个子目录做非递归计数，0 条即空目录；根目录单独判空。
     * Second pass: non-recursive count per collected subfolder; zero entries
     * means empty. The root folder itself is checked separately. */
    for (int i = 0; i < c.dn; i++) {
        int n = 0;
        ft_walk(c.dirs[i], 0, cb_count_one, &n);
        if (n == 0) { printf("%s\n", c.dirs[i]); c.hits++; }
        free(c.dirs[i]);
    }
    free(c.dirs);
    int root_n = 0;
    ft_walk(argv[1], 0, cb_count_one, &root_n);
    if (root_n == 0) { printf("%s\n", argv[1]); c.hits++; }
    if (c.hits == 0) printf("%s\n", L->msg_no_match);
    return 0;
}

typedef struct { char path[4096]; long long size; } LargeItem;
static int large_cmp(const void* a, const void* b) {
    long long d = ((const LargeItem*)b)->size - ((const LargeItem*)a)->size;
    return d > 0 ? 1 : (d < 0 ? -1 : 0);
}
typedef struct { long long threshold; LargeItem* arr; int n, cap, oom; } LargeCtx;
static void cb_findlarge(const ft_entry_t* e, void* u) {
    LargeCtx* c = (LargeCtx*)u;
    if (c->oom || e->is_dir || e->size <= c->threshold) return;
    if (c->n == c->cap) {
        c->cap *= 2;
        LargeItem* na = (LargeItem*)realloc(c->arr, (size_t)c->cap * sizeof(LargeItem));
        if (!na) { c->oom = 1; return; }
        c->arr = na;
    }
    snprintf(c->arr[c->n].path, sizeof(c->arr[c->n].path), "%s", e->full);
    c->arr[c->n].size = e->size;
    c->n++;
}
static int cmd_findlarge(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 3, "findlarge <dir> <MB>")) return 1;
    if (check_extra(L, argc, 3, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    double mb;
    if (parse_num_nonneg(L, argv[2], &mb)) return 1;
    LargeCtx c = { (long long)(mb * 1048576.0),
                   (LargeItem*)calloc(256, sizeof(LargeItem)), 0, 256, 0 };
    if (!c.arr) { printf(L->err_generic, "out of memory"); printf("\n"); return 1; }
    if (walk_check(L, argv[1], 1, cb_findlarge, &c)) { free(c.arr); return 1; }
    if (c.oom) { free(c.arr); printf(L->err_generic, "out of memory"); printf("\n"); return 1; }
    qsort(c.arr, (size_t)c.n, sizeof(LargeItem), large_cmp);
    for (int i = 0; i < c.n; i++) {
        char h[32]; human_size(c.arr[i].size, h, sizeof(h));
        printf("%12s  %s\n", h, c.arr[i].path);
    }
    if (c.n == 0) printf("%s\n", L->msg_no_match);
    free(c.arr);
    return 0;
}

typedef struct { long long cutoff; int older; int hits; } AgeCtx;
static void cb_age(const ft_entry_t* e, void* u) {
    AgeCtx* c = (AgeCtx*)u;
    if (e->is_dir) return;
    long long age = time(NULL) - e->mtime;
    if (c->older ? (age > c->cutoff) : (age <= c->cutoff)) {
        printf("%s\n", e->full);
        c->hits++;
    }
}
static int cmd_findold(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 3, "findold <dir> <days>")) return 1;
    if (check_extra(L, argc, 3, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    double days;
    if (parse_num_nonneg(L, argv[2], &days)) return 1;
    AgeCtx c = { (long long)(days * 86400.0), 1, 0 };
    if (walk_check(L, argv[1], 1, cb_age, &c)) return 1;
    if (c.hits == 0) printf("%s\n", L->msg_no_match);
    return 0;
}
static int cmd_findnew(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 3, "findnew <dir> <days>")) return 1;
    if (check_extra(L, argc, 3, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    double days;
    if (parse_num_nonneg(L, argv[2], &days)) return 1;
    AgeCtx c = { (long long)(days * 86400.0), 0, 0 };
    if (walk_check(L, argv[1], 1, cb_age, &c)) return 1;
    if (c.hits == 0) printf("%s\n", L->msg_no_match);
    return 0;
}

/* ================================================================== */
/* hash / duplicates / checksum                                        */
/* 校验族：单文件 CRC32、按大小+CRC32 查重、全目录 CRC32 清单            */
/* Checksums: single-file CRC32, size+CRC32 duplicates, full CRC32 list */
/* ================================================================== */
static int cmd_hash(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "hash <file>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_file(L, argv[1])) return 1;
    unsigned long crc;
    if (file_crc32(argv[1], &crc) != 0) { printf(L->err_access, argv[1]); printf("\n"); return 1; }
    ft_stat_t st; ft_stat(argv[1], &st);
    printf("%s %08lX   %lld %s   %s\n", L->lbl_crc32, crc, st.size, L->lbl_bytes, argv[1]);
    return 0;
}

typedef struct { char* path; long long size; unsigned long crc; } DupEntry;
typedef struct { DupEntry* list; int n; int cap; int oom; } DupCx;
static void collector(const ft_entry_t* e, void* u) {
    DupCx* cx = (DupCx*)u;
    if (cx->oom || e->is_dir || e->size == 0) return;
    if (cx->n == cx->cap) {
        cx->cap *= 2;
        DupEntry* na = (DupEntry*)realloc(cx->list, (size_t)cx->cap * sizeof(DupEntry));
        if (!na) { cx->oom = 1; return; }
        cx->list = na;
    }
    DupEntry* d = &cx->list[cx->n];
    d->path = (char*)malloc(strlen(e->full) + 1);
    if (!d->path) { cx->oom = 1; return; }
    strcpy(d->path, e->full);
    d->size = e->size;
    file_crc32(e->full, &d->crc);
    cx->n++;
}
static int cmd_duplicates(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "duplicates <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    DupCx cx = { (DupEntry*)calloc(256, sizeof(DupEntry)), 0, 256, 0 };
    if (!cx.list) { printf(L->err_generic, "out of memory"); printf("\n"); return 1; }
    if (walk_check(L, argv[1], 1, collector, &cx)) {
        for (int i = 0; i < cx.n; i++) free(cx.list[i].path);
        free(cx.list);
        return 1;
    }
    if (cx.oom) {
        for (int i = 0; i < cx.n; i++) free(cx.list[i].path);
        free(cx.list);
        printf(L->err_generic, "out of memory"); printf("\n");
        return 1;
    }
    int printed = 0;
    for (int i = 0; i < cx.n; i++)
        for (int j = i + 1; j < cx.n; j++)
            if (cx.list[i].size > 0 && cx.list[i].size == cx.list[j].size &&
                cx.list[i].crc == cx.list[j].crc) {
                if (!printed) printf("%s\n", L->sep_duplicates);
                printf("  %s\n  %s\n\n", cx.list[i].path, cx.list[j].path);
                printed = 1;
            }
    if (!printed) printf("%s\n", L->msg_no_match);
    for (int i = 0; i < cx.n; i++) free(cx.list[i].path);
    free(cx.list);
    return 0;
}

typedef struct { int hits; } CkCtx;
static void cb_checksum(const ft_entry_t* e, void* u) {
    CkCtx* c = (CkCtx*)u;
    if (e->is_dir) return;
    unsigned long crc;
    if (file_crc32(e->full, &crc) != 0) return;
    printf("%08lX  %12lld  %s\n", crc, e->size, e->full);
    c->hits++;
}
static int cmd_checksum(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "checksum <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    CkCtx c = {0};
    if (walk_check(L, argv[1], 1, cb_checksum, &c)) return 1;
    if (c.hits == 0) printf("%s\n", L->msg_no_match);
    return 0;
}

/* ================================================================== */
/* encoding / type / istext / exists                                  */
/* 属性探测：编码、扩展名类型、文本/二进制、路径存在性                    */
/* Probing: encoding, extension type, text/binary, path existence.     */
/* ================================================================== */
static int cmd_encoding(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "encoding <file>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_file(L, argv[1])) return 1;
    char buf[64];
    if (detect_encoding(argv[1], buf, sizeof(buf)) != 0) {
        printf(L->err_access, argv[1]); printf("\n"); return 1;
    }
    printf("%s: %s\n", L->lbl_encoding, buf);
    return 0;
}
static int cmd_type(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "type <file>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_file(L, argv[1])) return 1;
    char buf[64];
    guess_type(argv[1], buf, sizeof(buf));
    printf("%s: %s\n", L->lbl_type, buf);
    return 0;
}
static int cmd_istext(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "istext <file>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_file(L, argv[1])) return 1;
    printf("%s\n", looks_text(argv[1]) ? L->lbl_text : L->lbl_binary);
    return 0;
}
static int cmd_exists(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "exists <path>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (ft_is_dir(argv[1])) printf("%s (%s)\n", argv[1], L->lbl_dir);
    else if (ft_is_file(argv[1])) printf("%s (%s)\n", argv[1], L->lbl_file);
    else { printf("%s", L->lbl_none); printf(": %s\n", argv[1]); return 1; }
    return 0;
}

/* ================================================================== */
/* compare / head / tail / wc / chars / longest / blank                */
/* 文本查看与统计：逐字节比较、头尾行、行/词/字符、最长行、空行            */
/* Text view & stats: byte compare, head/tail, wc, chars, longest, blank */
/* ================================================================== */
static int cmd_compare(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 3, "compare <fileA> <fileB>")) return 1;
    if (check_extra(L, argc, 3, argv)) return 1;
    if (check_file(L, argv[1])) return 1;
    if (check_file(L, argv[2])) return 1;
    FILE* a = ft_fopen(argv[1], "rb");
    FILE* b = ft_fopen(argv[2], "rb");
    if (!a || !b) {
        printf(L->err_access, !a ? argv[1] : argv[2]); printf("\n");
        if (a) fclose(a);
        if (b) fclose(b);
        return 1;
    }
    long long off = 0; int same = 1;
    int ca, cb;
    while ((ca = fgetc(a)) != EOF) {
        cb = fgetc(b);
        off++;
        if (cb != ca) { same = 0; break; }
    }
    if (same && fgetc(b) != EOF) same = 0;
    fclose(a); fclose(b);
    if (same) printf("%s\n", L->lbl_equal);
    else printf("%s @ %lld %s\n", L->lbl_diff, off, L->lbl_bytes);
    return same ? 0 : 1;
}

static int cmd_head(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "head <file> [N]")) return 1;
    if (argc > 3) { printf(L->err_extra_args, argv[3]); printf("\n"); return 1; }
    if (check_file(L, argv[1])) return 1;
    long long limit = 10;
    if (argc >= 3 && parse_int_nonneg(L, argv[2], &limit)) return 1;
    int n; char** lines = read_lines(argv[1], &n);
    if (!lines) { printf(L->err_access, argv[1]); printf("\n"); return 1; }
    for (long long i = 0; i < n && i < limit; i++) { trim_cr(lines[i]); puts(lines[i]); }
    free_lines(lines, n);
    return 0;
}
static int cmd_tail(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "tail <file> [N]")) return 1;
    if (argc > 3) { printf(L->err_extra_args, argv[3]); printf("\n"); return 1; }
    if (check_file(L, argv[1])) return 1;
    long long limit = 10;
    if (argc >= 3 && parse_int_nonneg(L, argv[2], &limit)) return 1;
    int n; char** lines = read_lines(argv[1], &n);
    if (!lines) { printf(L->err_access, argv[1]); printf("\n"); return 1; }
    long long start = n - limit; if (start < 0) start = 0;
    for (long long i = start; i < n; i++) { trim_cr(lines[i]); puts(lines[i]); }
    free_lines(lines, n);
    return 0;
}

static int cmd_wc(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "wc <file>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_file(L, argv[1])) return 1;
    int n; char** lines = read_lines(argv[1], &n);
    if (!lines) { printf(L->err_access, argv[1]); printf("\n"); return 1; }
    long long words = 0, chars = 0;
    for (int i = 0; i < n; i++) {
        trim_cr(lines[i]);
        chars += (long long)strlen(lines[i]) + 1;
        int inw = 0;
        for (const char* s = lines[i]; *s; s++) {
            if (isspace((unsigned char)*s)) inw = 0;
            else if (!inw) { inw = 1; words++; }
        }
    }
    printf("%s: %d\n", L->lbl_lines, n);
    printf("%s: %lld\n", L->lbl_words, words);
    printf("%s: %lld\n", L->lbl_chars, chars);
    free_lines(lines, n);
    return 0;
}

static int cmd_chars(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "chars <file>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_file(L, argv[1])) return 1;
    long long c = count_utf8_chars(argv[1]);
    if (c < 0) { printf(L->err_access, argv[1]); printf("\n"); return 1; }
    printf("%s: %lld\n", L->lbl_chars, c);
    return 0;
}

static int cmd_longest(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "longest <file>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_file(L, argv[1])) return 1;
    int n; char** lines = read_lines(argv[1], &n);
    if (!lines) { printf(L->err_access, argv[1]); printf("\n"); return 1; }
    size_t mx = 0;
    for (int i = 0; i < n; i++) { trim_cr(lines[i]); if (strlen(lines[i]) > mx) mx = strlen(lines[i]); }
    printf(L->msg_maxline, mx);
    printf("\n");
    free_lines(lines, n);
    return 0;
}

static int cmd_blank(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "blank <file>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_file(L, argv[1])) return 1;
    int n; char** lines = read_lines(argv[1], &n);
    if (!lines) { printf(L->err_access, argv[1]); printf("\n"); return 1; }
    long long blank = 0;
    for (int i = 0; i < n; i++) {
        int ws = 1;
        for (const char* s = lines[i]; *s; s++) if (!isspace((unsigned char)*s)) { ws = 0; break; }
        if (ws) blank++;
    }
    printf("%s: %lld\n", L->lbl_empty, blank);
    free_lines(lines, n);
    return 0;
}

/* ================================================================== */
/* disk / recent / extstats / extsize / whereis                       */
/* 系统视图：磁盘容量、最近修改、按扩展名数量/大小统计、PATH 找程序        */
/* System view: disk space, recent files, per-ext count/size, whereis.  */
/* ================================================================== */
static int cmd_disk(const LangPack* L, int argc, char** argv) {
    if (check_extra(L, argc, 1, argv)) return 1;
    const char* roots[26];
    int k = ft_list_roots(roots, 26);
    for (int i = 0; i < k; i++) {
        long long total, freeb;
        if (ft_disk_free(roots[i], &total, &freeb) != 0) continue;
        long long used = total - freeb;
        char ht[32], hf[32], hu[32];
        human_size(total, ht, sizeof(ht));
        human_size(freeb, hf, sizeof(hf));
        human_size(used, hu, sizeof(hu));
        printf("%s %s | %s: %s / %s (%s: %s)\n",
               L->lbl_drive, roots[i], L->lbl_free, hf, ht, L->lbl_used, hu);
    }
    return 0;
}

typedef struct { char path[4096]; long long mtime; long long size; } Recent;
static int recent_cmp(const void* a, const void* b) {
    long long d = ((const Recent*)b)->mtime - ((const Recent*)a)->mtime;
    return d > 0 ? 1 : (d < 0 ? -1 : 0);
}
static void recent_collect(const ft_entry_t* e, void* u);
typedef struct { Recent* arr; int n; int cap; int oom; } Rx;
static int cmd_recent(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "recent <dir> [N]")) return 1;
    if (argc > 3) { printf(L->err_extra_args, argv[3]); printf("\n"); return 1; }
    if (check_dir(L, argv[1])) return 1;
    long long limit = 10;
    if (argc >= 3 && parse_int_nonneg(L, argv[2], &limit)) return 1;
    Rx rx = { (Recent*)calloc(256, sizeof(Recent)), 0, 256, 0 };
    if (!rx.arr) { printf(L->err_generic, "out of memory"); printf("\n"); return 1; }
    if (walk_check(L, argv[1], 1, recent_collect, &rx)) { free(rx.arr); return 1; }
    if (rx.oom) { free(rx.arr); printf(L->err_generic, "out of memory"); printf("\n"); return 1; }
    qsort(rx.arr, (size_t)rx.n, sizeof(Recent), recent_cmp);
    for (int i = 0; i < rx.n && i < limit; i++) {
        char mt[32]; ft_format_time(rx.arr[i].mtime, mt, sizeof(mt));
        printf("%s  %s\n", mt, rx.arr[i].path);
    }
    if (rx.n == 0) printf("%s\n", L->msg_no_match);
    free(rx.arr);
    return 0;
}
static void recent_collect(const ft_entry_t* e, void* u) {
    Rx* rx = (Rx*)u;
    if (rx->oom || e->is_dir) return;
    if (rx->n == rx->cap) {
        rx->cap *= 2;
        Recent* na = (Recent*)realloc(rx->arr, (size_t)rx->cap * sizeof(Recent));
        if (!na) { rx->oom = 1; return; }
        rx->arr = na;
    }
    Recent* r = &rx->arr[rx->n];
    snprintf(r->path, sizeof(r->path), "%s", e->full);
    r->mtime = e->mtime; r->size = e->size;
    rx->n++;
}

typedef struct { char ext[64]; long long count; long long bytes; } Agg;
typedef struct { Agg* items; int n; int cap; int by_size; const LangPack* L; int oom; } AggCtx;
static void cb_agg(const ft_entry_t* e, void* u) {
    AggCtx* c = (AggCtx*)u;
    if (c->oom || e->is_dir) return;
    char ex[64]; file_extension(e->name, ex, sizeof(ex));
    for (int i = 0; i < c->n; i++)
        if (strcmp(c->items[i].ext, ex) == 0) {
            c->items[i].count++;
            c->items[i].bytes += e->size;
            return;
        }
    if (c->n == c->cap) {
        c->cap *= 2;
        Agg* na = (Agg*)realloc(c->items, (size_t)c->cap * sizeof(Agg));
        if (!na) { c->oom = 1; return; }
        c->items = na;
    }
    Agg* a = &c->items[c->n++];
    snprintf(a->ext, sizeof(a->ext), "%s", ex);
    a->count = 1; a->bytes = e->size;
}
static int cmd_extstats(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "extstats <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    AggCtx c = { (Agg*)calloc(64, sizeof(Agg)), 0, 64, 0, L, 0 };
    if (!c.items) { printf(L->err_generic, "out of memory"); printf("\n"); return 1; }
    if (walk_check(L, argv[1], 1, cb_agg, &c)) { free(c.items); return 1; }
    if (c.oom) { free(c.items); printf(L->err_generic, "out of memory"); printf("\n"); return 1; }
    for (int i = 0; i < c.n; i++) {
        if (c.items[i].ext[0])
            printf("  .%-10s  %lld %s\n", c.items[i].ext, c.items[i].count, L->lbl_count);
        else
            printf("  %-10s  %lld %s\n", L->ext_none, c.items[i].count, L->lbl_count);
    }
    if (c.n == 0) printf("%s\n", L->msg_no_match);
    free(c.items);
    return 0;
}
static int cmd_extsize(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "extsize <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    AggCtx c = { (Agg*)calloc(64, sizeof(Agg)), 0, 64, 1, L, 0 };
    if (!c.items) { printf(L->err_generic, "out of memory"); printf("\n"); return 1; }
    if (walk_check(L, argv[1], 1, cb_agg, &c)) { free(c.items); return 1; }
    if (c.oom) { free(c.items); printf(L->err_generic, "out of memory"); printf("\n"); return 1; }
    for (int i = 0; i < c.n; i++) {
        char h[32]; human_size(c.items[i].bytes, h, sizeof(h));
        if (c.items[i].ext[0])
            printf("  .%-10s  %s\n", c.items[i].ext, h);
        else
            printf("  %-10s  %s\n", L->ext_none, h);
    }
    if (c.n == 0) printf("%s\n", L->msg_no_match);
    free(c.items);
    return 0;
}

static int cmd_whereis(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "whereis <program>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    const char* path = getenv("PATH");
    if (!path) return 1;
    char buf[8192];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    const char* sep = ";";
#ifndef _WIN32
    sep = ":";
#endif
    char* save = NULL;
    int found = 0;
    for (char* tok = strtok_r(buf, sep, &save); tok; tok = strtok_r(NULL, sep, &save)) {
        char cand[4096];
#ifdef _WIN32
        snprintf(cand, sizeof(cand), "%s\\%s", tok, argv[1]);
        if (ft_is_file(cand)) { printf("%s\n", cand); found = 1; continue; }
        snprintf(cand, sizeof(cand), "%s\\%s.exe", tok, argv[1]);
#else
        snprintf(cand, sizeof(cand), "%s/%s", tok, argv[1]);
#endif
        if (ft_is_file(cand)) { printf("%s\n", cand); found = 1; }
    }
    if (!found) printf("%s\n", L->msg_no_match);
    return 0;
}

/* ================================================================== */
/* expanded helpers                                                    */
/* 扩充命令共用的小助手（大小写不敏感子串包含等）                          */
/* Shared helpers for the expanded commands (case-insensitive contains). */
/* ================================================================== */
/* 大小写不敏感子串包含判断（供 findpath 等使用）。
 * Case-insensitive substring containment (used by findpath and friends). */
static int ci_contains(const char* hay, const char* needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0) return 1;
    for (; *hay; hay++) {
        size_t i = 0;
        while (i < nlen && tolower((unsigned char)hay[i]) == tolower((unsigned char)needle[i])) i++;
        if (i == nlen) return 1;
    }
    return 0;
}

/* ================================================================== */
/* expanded: statistics                                                */
/* 扩充统计：最新/最旧、平均大小、大小排序、大类计数、隐藏、只读          */
/* Expanded stats: newest/oldest, average, size sort, broad-type count,  */
/* hidden, readonly.                                                     */
/* ================================================================== */
/* 遍历过程中记录极值（最新/最旧）的临时状态。
 * Per-traversal state for tracking the newest / oldest extremum. */
typedef struct { char path[4096]; long long mtime; long long size; int found; } Extremum;
static void cb_newest(const ft_entry_t* e, void* u) {
    Extremum* x = u;
    if (e->is_dir) return;
    if (!x->found || e->mtime > x->mtime) {
        x->found = 1; x->mtime = e->mtime; x->size = e->size;
        snprintf(x->path, sizeof(x->path), "%s", e->full);
    }
}
static void cb_oldest(const ft_entry_t* e, void* u) {
    Extremum* x = u;
    if (e->is_dir) return;
    if (!x->found || e->mtime < x->mtime) {
        x->found = 1; x->mtime = e->mtime; x->size = e->size;
        snprintf(x->path, sizeof(x->path), "%s", e->full);
    }
}
static int print_extremum(const LangPack* L, const char* dir, int oldest) {
    if (check_dir(L, dir)) return 1;
    Extremum x; memset(&x, 0, sizeof(x));
    if (walk_check(L, dir, 1, oldest ? cb_oldest : cb_newest, &x)) return 1;
    if (!x.found) { printf("%s\n", L->msg_no_match); return 0; }
    char mt[32], hs[32];
    ft_format_time(x.mtime, mt, sizeof(mt));
    human_size(x.size, hs, sizeof(hs));
    printf("%s  %s  %s\n", mt, hs, x.path);
    return 0;
}
static int cmd_newest(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "newest <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    return print_extremum(L, argv[1], 0);
}
static int cmd_oldest(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "oldest <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    return print_extremum(L, argv[1], 1);
}

typedef struct { long long bytes, count; } AvgCtx;
static void cb_avg(const ft_entry_t* e, void* u) {
    AvgCtx* c = u;
    if (!e->is_dir) { c->bytes += e->size; c->count++; }
}
static int cmd_avgsize(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "avgsize <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    AvgCtx c = {0,0};
    if (walk_check(L, argv[1], 1, cb_avg, &c)) return 1;
    if (c.count == 0) { printf("%s\n", L->msg_no_match); return 0; }
    char h[32]; human_size(c.bytes / c.count, h, sizeof(h));
    printf(L->msg_avgsize, c.count, c.bytes, L->lbl_bytes, h);
    printf("\n");
    return 0;
}

typedef struct { char path[4096]; long long size; } SizeItem;
static int sizeitem_cmp(const void* a, const void* b) {
    long long d = ((const SizeItem*)b)->size - ((const SizeItem*)a)->size;
    return d > 0 ? 1 : (d < 0 ? -1 : 0);
}
typedef struct { SizeItem* arr; int n; int cap; int oom; } SxCx;
static void sizesort_collect(const ft_entry_t* e, void* u) {
    SxCx* sx = u;
    if (sx->oom || e->is_dir) return;
    if (sx->n == sx->cap) {
        sx->cap *= 2;
        SizeItem* na = (SizeItem*)realloc(sx->arr, (size_t)sx->cap * sizeof(SizeItem));
        if (!na) { sx->oom = 1; return; }
        sx->arr = na;
    }
    snprintf(sx->arr[sx->n].path, sizeof(sx->arr[sx->n].path), "%s", e->full);
    sx->arr[sx->n].size = e->size;
    sx->n++;
}
static int cmd_sizesort(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "sizesort <dir> [N]")) return 1;
    if (argc > 3) { printf(L->err_extra_args, argv[3]); printf("\n"); return 1; }
    if (check_dir(L, argv[1])) return 1;
    long long limit = 20;
    if (argc >= 3 && parse_int_nonneg(L, argv[2], &limit)) return 1;
    SxCx sx = { (SizeItem*)calloc(256, sizeof(SizeItem)), 0, 256, 0 };
    if (!sx.arr) { printf(L->err_generic, "out of memory"); printf("\n"); return 1; }
    if (walk_check(L, argv[1], 1, sizesort_collect, &sx)) { free(sx.arr); return 1; }
    if (sx.oom) { free(sx.arr); printf(L->err_generic, "out of memory"); printf("\n"); return 1; }
    qsort(sx.arr, (size_t)sx.n, sizeof(SizeItem), sizeitem_cmp);
    for (int i = 0; i < sx.n && i < limit; i++) {
        char h[32]; human_size(sx.arr[i].size, h, sizeof(h));
        printf("%12s  %s\n", h, sx.arr[i].path);
    }
    if (sx.n == 0) printf("%s\n", L->msg_no_match);
    free(sx.arr);
    return 0;
}

static const char* classify(const char* path) {
    char e[64]; file_extension(path, e, sizeof(e));
    if (!strcmp(e,"jpg")||!strcmp(e,"jpeg")||!strcmp(e,"png")||!strcmp(e,"gif")||!strcmp(e,"bmp")||!strcmp(e,"svg")||!strcmp(e,"webp")||!strcmp(e,"ico")) return "image";
    if (!strcmp(e,"mp4")||!strcmp(e,"mkv")||!strcmp(e,"avi")||!strcmp(e,"mov")||!strcmp(e,"wmv")||!strcmp(e,"flv")) return "video";
    if (!strcmp(e,"mp3")||!strcmp(e,"wav")||!strcmp(e,"flac")||!strcmp(e,"ogg")||!strcmp(e,"m4a")) return "audio";
    if (!strcmp(e,"txt")||!strcmp(e,"md")||!strcmp(e,"pdf")||!strcmp(e,"doc")||!strcmp(e,"docx")||!strcmp(e,"xls")||!strcmp(e,"xlsx")||!strcmp(e,"ppt")||!strcmp(e,"pptx")||!strcmp(e,"csv")||!strcmp(e,"rtf")) return "document";
    if (!strcmp(e,"zip")||!strcmp(e,"rar")||!strcmp(e,"7z")||!strcmp(e,"tar")||!strcmp(e,"gz")||!strcmp(e,"bz2")) return "archive";
    if (!strcmp(e,"c")||!strcmp(e,"h")||!strcmp(e,"cpp")||!strcmp(e,"hpp")||!strcmp(e,"py")||!strcmp(e,"js")||!strcmp(e,"ts")||!strcmp(e,"java")||!strcmp(e,"go")||!strcmp(e,"rs")||!strcmp(e,"json")||!strcmp(e,"html")||!strcmp(e,"css")||!strcmp(e,"xml")||!strcmp(e,"sh")||!strcmp(e,"bat")) return "code";
    return "other";
}
typedef struct { long long image,video,audio,document,archive,code,other; } TypeCx;
static void cb_typecount(const ft_entry_t* e, void* u) {
    TypeCx* t = u;
    if (e->is_dir) return;
    const char* c = classify(e->full);
    if (!strcmp(c,"image")) t->image++;
    else if (!strcmp(c,"video")) t->video++;
    else if (!strcmp(c,"audio")) t->audio++;
    else if (!strcmp(c,"document")) t->document++;
    else if (!strcmp(c,"archive")) t->archive++;
    else if (!strcmp(c,"code")) t->code++;
    else t->other++;
}
static int cmd_typecount(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "typecount <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    TypeCx t; memset(&t, 0, sizeof(t));
    if (walk_check(L, argv[1], 1, cb_typecount, &t)) return 1;
    printf("%s:    %lld\n%s:    %lld\n%s:    %lld\n%s: %lld\n%s:  %lld\n%s:     %lld\n%s:    %lld\n",
           L->cat_image, t.image, L->cat_video, t.video, L->cat_audio, t.audio,
           L->cat_document, t.document, L->cat_archive, t.archive,
           L->cat_code, t.code, L->cat_other, t.other);
    return 0;
}

typedef struct { int hits; } HdCtx;
static void cb_hidden(const ft_entry_t* e, void* u) {
    HdCtx* c = u;
    if (!e->is_dir && e->hidden) { printf("%s\n", e->full); c->hits++; }
}
static int cmd_hidden(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "hidden <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    HdCtx c = {0};
    if (walk_check(L, argv[1], 1, cb_hidden, &c)) return 1;
    if (c.hits == 0) printf("%s\n", L->msg_no_match);
    return 0;
}
static void cb_readonly(const ft_entry_t* e, void* u) {
    HdCtx* c = u;
    if (e->is_dir) return;
    ft_stat_t st;
    if (ft_stat(e->full, &st) == 0 && st.is_readonly) { printf("%s\n", e->full); c->hits++; }
}
static int cmd_readonly(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "readonly <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    HdCtx c = {0};
    if (walk_check(L, argv[1], 1, cb_readonly, &c)) return 1;
    if (c.hits == 0) printf("%s\n", L->msg_no_match);
    return 0;
}

/* ================================================================== */
/* expanded: lines                                                    */
/* 扩充行数：非空行、匹配行数、平均行长、含文本行号、CJK 字符             */
/* Expanded line stats: nonblank, matchlines, avg length, findline, CJK.  */
/* ================================================================== */
static int cmd_nonblank(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "nonblank <file>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_file(L, argv[1])) return 1;
    int n; char** lines = read_lines(argv[1], &n);
    if (!lines) { printf(L->err_access, argv[1]); printf("\n"); return 1; }
    long long nb = 0;
    for (int i = 0; i < n; i++) {
        int has = 0;
        for (const char* s = lines[i]; *s; s++) if (!isspace((unsigned char)*s)) { has = 1; break; }
        if (has) nb++;
    }
    printf(L->msg_nonblank, nb);
    printf("\n");
    free_lines(lines, n);
    return 0;
}
typedef struct { const char* text; long long hits; } MatchCx;
static void cb_matchlines(const ft_entry_t* e, void* u) {
    MatchCx* m = u;
    if (e->is_dir || !looks_text(e->full)) return;
    int n; char** lines = read_lines(e->full, &n);
    if (!lines) return;
    for (int i = 0; i < n; i++)
        if (strstr(lines[i], m->text) != NULL) m->hits++;
    free_lines(lines, n);
}
static int cmd_matchlines(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 3, "matchlines <dir> <text>")) return 1;
    if (check_extra(L, argc, 3, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    MatchCx m = { argv[2], 0 };
    if (walk_check(L, argv[1], 1, cb_matchlines, &m)) return 1;
    printf("%s: %lld\n", L->lbl_matches, m.hits);
    return 0;
}
static int cmd_avglen(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "avglen <file>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    FILE* f = open_read(L, argv[1]);
    if (!f) return 1;
    long long sum = 0, nb = 0, cur = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') { if (cur > 0) { sum += cur; nb++; } cur = 0; }
        else if (c != '\r') cur++;
    }
    if (cur > 0) { sum += cur; nb++; }
    fclose(f);
    printf(L->msg_avglen, nb ? (double)sum / (double)nb : 0.0);
    printf("\n");
    return 0;
}
static int cmd_findline(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 3, "findline <file> <text>")) return 1;
    if (check_extra(L, argc, 3, argv)) return 1;
    if (check_file(L, argv[1])) return 1;
    int n; char** lines = read_lines(argv[1], &n);
    if (!lines) { printf(L->err_access, argv[1]); printf("\n"); return 1; }
    for (int i = 0; i < n; i++)
        if (strstr(lines[i], argv[2]) != NULL) { trim_cr(lines[i]); printf(L->fmt_findline, i + 1, lines[i]); printf("\n"); }
    free_lines(lines, n);
    return 0;
}
static int cmd_cjk(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "cjk <file>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    FILE* f = open_read(L, argv[1]);
    if (!f) return 1;
    /* 按 UTF-8 逐码点解码，只统计 CJK 统一表意文字 U+4E00–U+9FFF；
     * 不再用首字节区间猜测，避免把续字节/其它文字误计。
     * Decode UTF-8 codepoints and count only CJK Unified Ideographs
     * U+4E00–U+9FFF; no lead-byte guessing, so continuation bytes and
     * other scripts are not miscounted. */
    long long cjk = 0;
    int need = 0;        /* remaining continuation bytes for current codepoint */
    unsigned cp = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        unsigned char b = (unsigned char)c;
        if ((b & 0x80) == 0) { need = 0; continue; }          /* ASCII */
        if (need == 0) {
            if ((b & 0xE0) == 0xC0)      { need = 1; cp = b & 0x1F; }
            else if ((b & 0xF0) == 0xE0)  { need = 2; cp = b & 0x0F; }
            else if ((b & 0xF8) == 0xF0)  { need = 3; cp = b & 0x07; }
            else { need = 0; }                            /* stray lead byte */
        } else if ((b & 0xC0) == 0x80) {
            cp = (cp << 6) | (b & 0x3F);
            if (--need == 0) {
                if (cp >= 0x4E00 && cp <= 0x9FFF) cjk++;
            }
        } else {
            need = 0;                        /* malformed: reset, retry this byte */
            if ((b & 0xE0) == 0xE0) { need = 2; cp = b & 0x0F; }
        }
    }
    fclose(f);
    printf(L->msg_cjk, cjk);
    printf("\n");
    return 0;
}

/* ================================================================== */
/* expanded: search                                                    */
/* 扩充查找：精确文件名、同名文件、今日修改、大小区间、多扩展名、路径子串 */
/* Expanded search: exact name, dupenames, today, size range, multi-ext,  */
/* path substring.                                                        */
/* ================================================================== */
/* findfile 回调上下文：目标精确名与命中计数。
 * Callback context for findfile: exact target name and hit counter. */
typedef struct { const char* name; int hits; } FfCtx;
static void cb_findfile(const ft_entry_t* e, void* u) {
    FfCtx* c = u;
    if (!e->is_dir && ft_strcase_eq(e->name, c->name)) { printf("%s\n", e->full); c->hits++; }
}
static int cmd_findfile(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 3, "findfile <dir> <exact-name>")) return 1;
    if (check_extra(L, argc, 3, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    FfCtx c = { argv[2], 0 };
    if (walk_check(L, argv[1], 1, cb_findfile, &c)) return 1;
    if (c.hits == 0) printf("%s\n", L->msg_no_match);
    return 0;
}
typedef struct { char name[128]; int count; } NameCount;
typedef struct { NameCount* items; int n; int cap; int oom; } DupNxCx;
static void cb_dupenames(const ft_entry_t* e, void* u) {
    DupNxCx* dx = u;
    if (dx->oom || e->is_dir) return;
    for (int i = 0; i < dx->n; i++)
        if (ft_strcase_eq(dx->items[i].name, e->name)) { dx->items[i].count++; return; }
    if (dx->n == dx->cap) {
        dx->cap *= 2;
        NameCount* na = (NameCount*)realloc(dx->items, (size_t)dx->cap * sizeof(NameCount));
        if (!na) { dx->oom = 1; return; }
        dx->items = na;
    }
    snprintf(dx->items[dx->n].name, sizeof(dx->items[dx->n].name), "%s", e->name);
    dx->items[dx->n].count = 1;
    dx->n++;
}
static int cmd_dupenames(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "dupenames <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    DupNxCx dx = { (NameCount*)calloc(64, sizeof(NameCount)), 0, 64, 0 };
    if (!dx.items) { printf(L->err_generic, "out of memory"); printf("\n"); return 1; }
    if (walk_check(L, argv[1], 1, cb_dupenames, &dx)) { free(dx.items); return 1; }
    if (dx.oom) { free(dx.items); printf(L->err_generic, "out of memory"); printf("\n"); return 1; }
    int printed = 0;
    for (int i = 0; i < dx.n; i++)
        if (dx.items[i].count > 1) { printf("%s  x%d\n", dx.items[i].name, dx.items[i].count); printed = 1; }
    if (!printed) printf("%s\n", L->msg_no_match);
    free(dx.items);
    (void)L;
    return 0;
}
static int same_day(long long mtime) {
    time_t now = time(NULL);
    time_t mt = (time_t)mtime;
    struct tm a, b;
#ifdef _WIN32
    localtime_s(&a, &now);
    localtime_s(&b, &mt);
#else
    localtime_r(&now, &a);
    localtime_r(&mt, &b);
#endif
    return a.tm_year == b.tm_year && a.tm_yday == b.tm_yday;
}
typedef struct { int hits; } TodayCtx;
static void cb_today(const ft_entry_t* e, void* u) {
    TodayCtx* c = u;
    if (!e->is_dir && same_day(e->mtime)) { printf("%s\n", e->full); c->hits++; }
}
static int cmd_today(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 2, "today <dir>")) return 1;
    if (check_extra(L, argc, 2, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    TodayCtx c = {0};
    if (walk_check(L, argv[1], 1, cb_today, &c)) return 1;
    if (c.hits == 0) printf("%s\n", L->msg_no_match);
    return 0;
}
typedef struct { long long lo, hi; } Rg;
typedef struct { Rg r; int hits; } RgCtx;
static void cb_range(const ft_entry_t* e, void* u) {
    RgCtx* c = u;
    if (e->is_dir) return;
    if (e->size >= c->r.lo && e->size <= c->r.hi) {
        char h[32]; human_size(e->size, h, sizeof(h));
        printf("%12s  %s\n", h, e->full);
        c->hits++;
    }
}
static int cmd_findrange(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 4, "findrange <dir> <minMB> <maxMB>")) return 1;
    if (check_extra(L, argc, 4, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    double lo, hi;
    if (parse_num_nonneg(L, argv[2], &lo)) return 1;
    if (parse_num_nonneg(L, argv[3], &hi)) return 1;
    if (lo > hi) { printf(L->err_range_bad, argv[2], argv[3]); printf("\n"); return 1; }
    RgCtx c = { { (long long)(lo * 1048576.0), (long long)(hi * 1048576.0) }, 0 };
    if (walk_check(L, argv[1], 1, cb_range, &c)) return 1;
    if (c.hits == 0) printf("%s\n", L->msg_no_match);
    return 0;
}
typedef struct { const char* list; int hits; } ExtsCtx;
static void cb_exts(const ft_entry_t* e, void* u) {
    ExtsCtx* c = u;
    if (e->is_dir) return;
    char ex[64]; file_extension(e->name, ex, sizeof(ex));
    char buf[256]; snprintf(buf, sizeof(buf), "%s", c->list);
    char* save = NULL;
    for (char* tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ') tok++;
        if (ft_strcase_eq(ex, tok)) { printf("%s\n", e->full); c->hits++; break; }
    }
}
static int cmd_findexts(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 3, "findexts <dir> <ext1,ext2,...>")) return 1;
    if (check_extra(L, argc, 3, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    ExtsCtx c = { argv[2], 0 };
    if (walk_check(L, argv[1], 1, cb_exts, &c)) return 1;
    if (c.hits == 0) printf("%s\n", L->msg_no_match);
    return 0;
}
typedef struct { const char* sub; int hits; } FpCtx;
static void cb_findpath(const ft_entry_t* e, void* u) {
    FpCtx* c = u;
    if (!e->is_dir && ci_contains(e->full, c->sub)) { printf("%s\n", e->full); c->hits++; }
}
static int cmd_findpath(const LangPack* L, int argc, char** argv) {
    if (need(L, argc, 3, "findpath <dir> <substring>")) return 1;
    if (check_extra(L, argc, 3, argv)) return 1;
    if (check_dir(L, argv[1])) return 1;
    FpCtx c = { argv[2], 0 };
    if (walk_check(L, argv[1], 1, cb_findpath, &c)) return 1;
    if (c.hits == 0) printf("%s\n", L->msg_no_match);
    return 0;
}

/* ================================================================== */
/* command table                                                       */
/* 命令注册表：命令名 → 处理函数 → 帮助文案在 LangPack 中的偏移；main.c 按名匹配。
 * Command registry: name -> handler -> help offset inside LangPack;
 * main.c matches by name and calls fn.                                 */
/* ================================================================== */
/* OFF(m)：取 LangPack 字段 m 的字节偏移，供 help 按偏移打印本地化描述。
 * OFF(m): byte offset of LangPack field m, used by help to print the
 * localized description without a name lookup. */
#define OFF(m) offsetof(LangPack, m)
const CommandEntry CMD_TABLE[] = {
    { "help",        cmd_help,        OFF(h_help) },
    { "version",     cmd_version,     OFF(h_version) },
    { "langs",       cmd_langs,      OFF(h_langs) },
    { "log",         cmd_log,        OFF(h_log) },
    { "dellog",      cmd_dellog,     OFF(h_dellog) },
    { "stat",        cmd_stat,       OFF(h_stat) },
    { "count",       cmd_count,       OFF(h_count) },
    { "lines",       cmd_lines,       OFF(h_lines) },
    { "linesdir",    cmd_linesdir,    OFF(h_linesdir) },
    { "size",        cmd_size,        OFF(h_size) },
    { "du",          cmd_du,          OFF(h_du) },
    { "dirdiff",     cmd_dirdiff,     OFF(h_dirdiff) },
    { "tree",        cmd_tree,        OFF(h_tree) },
    { "findname",    cmd_findname,    OFF(h_findname) },
    { "findext",     cmd_findext,    OFF(h_findext) },
    { "findcontent", cmd_findcontent, OFF(h_findcontent) },
    { "findempty",   cmd_findempty,   OFF(h_findempty) },
    { "findlarge",   cmd_findlarge,   OFF(h_findlarge) },
    { "findold",     cmd_findold,     OFF(h_findold) },
    { "findnew",     cmd_findnew,     OFF(h_findnew) },
    { "hash",        cmd_hash,        OFF(h_hash) },
    { "duplicates",  cmd_duplicates,  OFF(h_duplicates) },
    { "encoding",    cmd_encoding,    OFF(h_encoding) },
    { "type",        cmd_type,        OFF(h_type) },
    { "list",        cmd_list,        OFF(h_list) },
    { "compare",     cmd_compare,     OFF(h_compare) },
    { "head",        cmd_head,        OFF(h_head) },
    { "tail",        cmd_tail,        OFF(h_tail) },
    { "wc",          cmd_wc,          OFF(h_wc) },
    { "disk",        cmd_disk,        OFF(h_disk) },
    { "recent",      cmd_recent,      OFF(h_recent) },
    { "extstats",    cmd_extstats,    OFF(h_extstats) },
    { "extsize",     cmd_extsize,     OFF(h_extsize) },
    { "checksum",    cmd_checksum,    OFF(h_checksum) },
    { "exists",      cmd_exists,      OFF(h_exists) },
    { "istext",      cmd_istext,      OFF(h_istext) },
    { "longest",     cmd_longest,     OFF(h_longest) },
    { "blank",       cmd_blank,       OFF(h_blank) },
    { "chars",       cmd_chars,       OFF(h_chars) },
    { "whereis",     cmd_whereis,     OFF(h_whereis) },
    /* expanded: statistics */
    { "newest",      cmd_newest,      OFF(h_newest) },
    { "oldest",      cmd_oldest,      OFF(h_oldest) },
    { "avgsize",     cmd_avgsize,     OFF(h_avgsize) },
    { "sizesort",    cmd_sizesort,    OFF(h_sizesort) },
    { "typecount",   cmd_typecount,   OFF(h_typecount) },
    { "hidden",      cmd_hidden,      OFF(h_hidden) },
    { "readonly",    cmd_readonly,    OFF(h_readonly) },
    /* expanded: lines */
    { "nonblank",    cmd_nonblank,    OFF(h_nonblank) },
    { "matchlines",  cmd_matchlines,  OFF(h_matchlines) },
    { "avglen",      cmd_avglen,      OFF(h_avglen) },
    { "findline",    cmd_findline,    OFF(h_findline) },
    { "cjk",         cmd_cjk,         OFF(h_cjk) },
    /* expanded: search */
    { "findfile",    cmd_findfile,    OFF(h_findfile) },
    { "dupenames",   cmd_dupenames,   OFF(h_dupenames) },
    { "today",       cmd_today,       OFF(h_today) },
    { "findrange",   cmd_findrange,   OFF(h_findrange) },
    { "findexts",    cmd_findexts,    OFF(h_findexts) },
    { "findpath",    cmd_findpath,    OFF(h_findpath) },
};
const int CMD_COUNT = (int)(sizeof(CMD_TABLE)/sizeof(CMD_TABLE[0]));
