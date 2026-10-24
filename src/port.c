/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 SuiYe_TR (https://github.com/Suiye-18)
 *
 * 【本文件】可移植抽象层实现。Windows 分支把 UTF-8 转宽字符后调用 Win32 API，
 * 保证中文/日文/俄文文件名正确解析；Linux 分支直接使用等价 POSIX 接口。
 *
 * FILETGIE - portable abstraction layer implementation.
 * Windows branch uses wide-char Win32 APIs converted from UTF-8 so that
 * Chinese / Japanese / Russian file names resolve correctly.
 * Linux branch uses the POSIX equivalents directly.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "port.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#define SEP "\\"
#else
#define SEP "/"
#include <unistd.h>
#include <strings.h>
#endif

/* ------------------------------------------------------------------ */
/* 平台共用的小工具 / Shared small helpers                              */
/* ------------------------------------------------------------------ */
/* ASCII 大小写不敏感比较：Windows 用 _stricmp，Linux 用 strcasecmp。
 * Case-insensitive compare: _stricmp on Windows, strcasecmp on Linux. */
int ft_strcase_eq(const char* a, const char* b) {
    if (!a || !b) return 0;
#ifdef _WIN32
    return _stricmp(a, b) == 0;
#else
    return strcasecmp(a, b) == 0;
#endif
}

/* 全局排除钩子，每次 ft_walk 遍历都会咨询；由 main.c 安装。
 * Global exclude hook consulted by every ft_walk traversal */
static ft_skip_fn g_skip = NULL;
void ft_set_skip(ft_skip_fn fn) { g_skip = fn; }

/* 拼接 base 与 name，按平台补一个分隔符（base 末尾已带分隔符则不重复）。
 * Join base and name, inserting one platform separator unless base ends
 * with one already. */
static void path_join(char* out, size_t n, const char* base, const char* name) {
    size_t bl = strlen(base);
    int need_sep = (bl > 0 && base[bl - 1] != '\\' && base[bl - 1] != '/');
    snprintf(out, n, "%s%s%s", base, need_sep ? SEP : "", name);
}

/* ------------------------------------------------------------------ */
#ifdef _WIN32
/* ------------------------------ WINDOWS --------------------------- */
/* Windows 分支：UTF-8 ↔ 宽字符互转 + 宽字符 Win32 API。
 * Windows branch: UTF-8 <-> wide conversion plus wide-char Win32 API. */
#include <sys/stat.h>

/* UTF-8 → 宽字符（调用方 free）；转换失败返回 NULL。
 * UTF-8 to wide string (caller frees); NULL on failure. */
static wchar_t* to_wide(const char* utf8) {
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t* w = (wchar_t*)malloc((size_t)n * sizeof(wchar_t));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, n);
    return w;
}

/* 宽字符 → UTF-8（调用方 free）；转换失败返回 NULL。
 * Wide string to UTF-8 (caller frees); NULL on failure. */
static char* to_utf8(const wchar_t* w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return NULL;
    char* s = (char*)malloc((size_t)n);
    if (!s) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    return s;
}

/* 把 Windows FILETIME（1601 起的 100ns 计数）换算为 epoch 秒。
 * Convert a Windows FILETIME (100ns ticks since 1601) to epoch seconds. */
static long long ft_from_filetime(const FILETIME* ft) {
    ULARGE_INTEGER u;
    u.LowPart  = ft->dwLowDateTime;
    u.HighPart = ft->dwHighDateTime;
    return (long long)((u.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

int ft_path_exists(const char* path) {
    wchar_t* w = to_wide(path);
    if (!w) return 0;
    DWORD a = GetFileAttributesW(w);
    free(w);
    return a != INVALID_FILE_ATTRIBUTES;
}

int ft_is_dir(const char* path) {
    wchar_t* w = to_wide(path);
    if (!w) return 0;
    DWORD a = GetFileAttributesW(w);
    free(w);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

int ft_is_file(const char* path) {
    wchar_t* w = to_wide(path);
    if (!w) return 0;
    DWORD a = GetFileAttributesW(w);
    free(w);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

int ft_stat(const char* path, ft_stat_t* out) {
    memset(out, 0, sizeof(*out));
    wchar_t* w = to_wide(path);
    if (!w) return -1;
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(w, GetFileExInfoStandard, &data)) { free(w); return -1; }
    free(w);
    out->size  = (long long)data.nFileSizeHigh * 0x100000000LL + data.nFileSizeLow;
    out->mtime = ft_from_filetime(&data.ftLastWriteTime);
    out->ctime = ft_from_filetime(&data.ftCreationTime);
    out->atime = ft_from_filetime(&data.ftLastAccessTime);
    out->is_dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    out->is_file = !out->is_dir;
    out->is_readonly = (data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
    return 0;
}

FILE* ft_fopen(const char* path, const char* mode) {
    wchar_t* w = to_wide(path);
    if (!w) return NULL;
    wchar_t wm[16] = {0};
    for (int i = 0; mode[i] && i < 15; i++) wm[i] = (wchar_t)mode[i];
    FILE* f = _wfopen(w, wm);
    free(w);
    return f;
}

void ft_log_path(char* buf, size_t len) {
    wchar_t exe[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) { snprintf(buf, len, "filetgie.log"); return; }
    wchar_t* slash = wcsrchr(exe, L'\\');
    if (slash) *(slash + 1) = L'\0';
    else exe[0] = L'\0';
    wchar_t wlog[MAX_PATH + 24];
    swprintf(wlog, MAX_PATH + 23, L"%sfiletgie.log", exe);
    wlog[MAX_PATH + 23] = L'\0';
    char* s = to_utf8(wlog);
    if (!s) { snprintf(buf, len, "filetgie.log"); return; }
    snprintf(buf, len, "%s", s);
    free(s);
}

int ft_remove_file(const char* path) {
    wchar_t* w = to_wide(path);
    if (!w) return -1;
    int r = _wremove(w);
    free(w);
    return r;
}

/* Windows 递归遍历：枚举当前层，命中排除钩子则跳过并剪枝，否则回调并按需下钻。
 * Recursive Windows enumeration; entries matched by the skip hook are
 * skipped and pruned, others are reported to cb and descended into. */
static int walk_rec(const char* base, int recursive, ft_walk_fn cb, void* user) {
    char pat[4096];
    path_join(pat, sizeof(pat), base, "*");
    wchar_t* wp = to_wide(pat);
    if (!wp) return -1;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(wp, &fd);
    free(wp);
    if (h == INVALID_HANDLE_VALUE) return -1;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        char* name = to_utf8(fd.cFileName);
        char full[4096];
        path_join(full, sizeof(full), base, name ? name : "?");
        ft_entry_t e;
        memset(&e, 0, sizeof(e));
        e.name = name;
        e.full = full;
        e.is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        e.hidden = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
        e.size = e.is_dir ? 0
                 : ((long long)fd.nFileSizeHigh * 0x100000000LL + fd.nFileSizeLow);
        e.mtime = ft_from_filetime(&fd.ftLastWriteTime);
        int skipped = g_skip && g_skip(e.name, e.is_dir);
        if (!skipped) cb(&e, user);
        free(name);
        if (recursive && e.is_dir && !skipped) walk_rec(full, 1, cb, user);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return 0;
}

int ft_walk(const char* root, int recursive, ft_walk_fn cb, void* user) {
    if (!ft_is_dir(root)) return -1;
    return walk_rec(root, recursive, cb, user);
}

void ft_format_time(long long t, char* buf, size_t len) {
    time_t tt = (time_t)t;
    struct tm tmv;
    localtime_s(&tmv, &tt);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", &tmv);
}

int ft_disk_free(const char* path, long long* total, long long* free_bytes) {
    wchar_t* w = to_wide(path);
    if (!w) return -1;
    ULARGE_INTEGER t, f;
    int ok = GetDiskFreeSpaceExW(w, NULL, &t, &f);
    free(w);
    if (!ok) return -1;
    *total = (long long)t.QuadPart;
    *free_bytes = (long long)f.QuadPart;
    return 0;
}

static char g_roots[26][4];
int ft_list_roots(const char** out, int max) {
    DWORD mask = GetLogicalDrives();
    int n = 0;
    for (int i = 0; i < 26 && n < max; i++) {
        if (mask & (1u << i)) {
            snprintf(g_roots[n], sizeof(g_roots[n]), "%c:\\", 'A' + i);
            out[n] = g_roots[n];
            n++;
        }
    }
    return n;
}

char** ft_argv_utf8(int* argc_out, char** argv) {
    (void)argv; /* Windows parses the wide command line directly below. */
    int n = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &n);
    if (!wargv) { *argc_out = 0; return NULL; }
    char** out = (char**)calloc((size_t)n + 1, sizeof(char*));
    for (int i = 0; i < n; i++) out[i] = to_utf8(wargv[i]);
    out[n] = NULL;
    LocalFree(wargv);
    *argc_out = n;
    return out;
}

void ft_free_argv(char** argv) {
    if (!argv) return;
    for (int i = 0; argv[i]; i++) free(argv[i]);
    free(argv);
}

#else
/* ------------------------------- LINUX ---------------------------- */
/* Linux 分支：直接使用 POSIX（stat/opendir/readlink/statvfs），路径即 UTF-8。
 * Linux branch: plain POSIX (stat/opendir/readlink/statvfs); paths are
 * already UTF-8. */
int ft_path_exists(const char* path) { return access(path, F_OK) == 0; }
int ft_is_dir(const char* path) {
    struct stat s; return stat(path, &s) == 0 && S_ISDIR(s.st_mode);
}
int ft_is_file(const char* path) {
    struct stat s; return stat(path, &s) == 0 && S_ISREG(s.st_mode);
}
int ft_stat(const char* path, ft_stat_t* out) {
    struct stat s;
    if (stat(path, &s) != 0) return -1;
    memset(out, 0, sizeof(*out));
    out->size  = (long long)s.st_size;
    out->mtime = (long long)s.st_mtime;
    out->ctime = (long long)s.st_ctime;
    out->atime = (long long)s.st_atime;
    out->is_dir = S_ISDIR(s.st_mode);
    out->is_file = S_ISREG(s.st_mode);
    out->is_readonly = !(s.st_mode & S_IWUSR);
    return 0;
}
FILE* ft_fopen(const char* path, const char* mode) { return fopen(path, mode); }

void ft_log_path(char* buf, size_t len) {
    char self[4096];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n > 0) {
        self[n] = '\0';
        char* slash = strrchr(self, '/');
        if (slash) *(slash + 1) = '\0';
        else self[0] = '\0';
        snprintf(buf, len, "%sfiletgie.log", self);
    } else {
        snprintf(buf, len, "filetgie.log");
    }
}

int ft_remove_file(const char* path) { return remove(path); }

/* Linux 递归遍历：readdir + lstat，命中排除钩子则跳过并剪枝。
 * Recursive POSIX walk: readdir + lstat; skip-hit entries are pruned. */
static int walk_rec(const char* base, int recursive, ft_walk_fn cb, void* user) {
    DIR* d = opendir(base);
    if (!d) return -1;
    struct dirent* de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        char full[4096];
        path_join(full, sizeof(full), base, de->d_name);
        struct stat s;
        if (lstat(full, &s) != 0) continue;
        ft_entry_t e;
        memset(&e, 0, sizeof(e));
        e.name = de->d_name;
        e.full = full;
        e.is_dir = S_ISDIR(s.st_mode);
        e.hidden = (de->d_name[0] == '.');
        e.size = e.is_dir ? 0 : (long long)s.st_size;
        e.mtime = (long long)s.st_mtime;
        int skipped = g_skip && g_skip(e.name, e.is_dir);
        if (!skipped) cb(&e, user);
        if (recursive && e.is_dir && !skipped) walk_rec(full, 1, cb, user);
    }
    closedir(d);
    return 0;
}
int ft_walk(const char* root, int recursive, ft_walk_fn cb, void* user) {
    if (!ft_is_dir(root)) return -1;
    return walk_rec(root, recursive, cb, user);
}
void ft_format_time(long long t, char* buf, size_t len) {
    time_t tt = (time_t)t;
    struct tm tmv;
    localtime_r(&tt, &tmv);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", &tmv);
}
int ft_disk_free(const char* path, long long* total, long long* free_bytes) {
    struct statvfs v;
    if (statvfs(path, &v) != 0) return -1;
    *total = (long long)v.f_blocks * v.f_frsize;
    *free_bytes = (long long)v.f_bavail * v.f_frsize;
    return 0;
}
static const char* g_root = "/";
int ft_list_roots(const char** out, int max) {
    if (max > 0) { out[0] = g_root; return 1; }
    return 0;
}
char** ft_argv_utf8(int* argc_out, char** argv) {
    int n = 0;
    while (argv[n]) n++;
    *argc_out = n;
    return argv;
}
void ft_free_argv(char** argv) { (void)argv; }
#endif
