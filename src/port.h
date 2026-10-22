/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 SuiYe_TR (https://github.com/Suiye-18)
 *
 * 【本文件】可移植抽象层接口：把 Windows（宽字符 Win32 API，正确处理中日俄文件名）
 * 与 Linux（POSIX）差异收敛到同一套小 C API。对外路径一律 UTF-8。
 *
 * FILETGIE - portable abstraction layer. Hides Windows / Linux differences
 * behind one small C API. All public paths are UTF-8 (on Windows they are
 * converted to wide strings before calling the Win32 API, so non-ASCII
 * names work).
 */
#ifndef FILETGIE_PORT_H
#define FILETGIE_PORT_H

#include <stdio.h>
#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ft_stat 返回的文件/目录元数据（字节大小、三个时间戳、类型与只读位）。
 * File / directory metadata returned by ft_stat */
typedef struct {
    long long size;        /* bytes                              */
    long long mtime;      /* last modification time (epoch s)  */
    long long ctime;      /* creation / status change time      */
    long long atime;      /* last access time                  */
    int       is_dir;
    int       is_file;
    int       is_readonly;
} ft_stat_t;

/* 遍历回调收到的条目：仅名称、完整 UTF-8 路径、大小、修改时间、目录/隐藏标志。
 * Entry handed to the walker callback */
typedef struct {
    const char* name;     /* file / dir name only (no path)     */
    const char* full;     /* full UTF-8 path                    */
    long long   size;
    long long   mtime;
    int         is_dir;
    int         hidden;    /* Windows hidden attr / Linux dotfile */
} ft_entry_t;

typedef void (*ft_walk_fn)(const ft_entry_t* e, void* user);

/* ---- 路径辅助 / Path helpers ---- */
int  ft_path_exists(const char* path);
int  ft_is_dir(const char* path);
int  ft_is_file(const char* path);
int  ft_stat(const char* path, ft_stat_t* out);

/* 跨平台打开接受 UTF-8 路径的 FILE*（Windows 内部转宽字符）。
 * Open a FILE* that accepts a UTF-8 path on every platform. */
FILE* ft_fopen(const char* path, const char* mode);

/* 目录遍历：recursive=0 仅列本层，recursive=1 递归子目录；成功返回 0。
 * Directory walk: recursive=0 lists only the folder itself,
 * recursive=1 descends into sub-folders. Returns 0 on success. */
int  ft_walk(const char* root, int recursive, ft_walk_fn cb, void* user);

/* 把 epoch 秒格式化为本地时间 "YYYY-MM-DD HH:MM:SS"。
 * Format an epoch second into "YYYY-MM-DD HH:MM:SS" in local time. */
void ft_format_time(long long t, char* buf, size_t len);

/* 查询路径所在磁盘的总容量与剩余字节；成功返回 0。
 * Disk free space for a path; bytes out. Returns 0 on success. */
int  ft_disk_free(const char* path, long long* total, long long* free_bytes);

/* 列出逻辑驱动器（Windows）/挂载点（Linux）到调用方缓冲区；返回数量。
 * List logical drives (Windows) / mount points (Linux) into caller buffer.
 * out: array of pointers owned by caller; returns count. */
int  ft_list_roots(const char** out, int max);

/* Windows 上把 argv 转成 UTF-8；Linux 原样返回。返回数组须用 ft_free_argv 释放。
 * Convert argv to UTF-8 on Windows; on Linux returns argv unchanged.
 * Caller must free() the returned array with ft_free_argv. */
char** ft_argv_utf8(int* argc_out, char** argv);
void   ft_free_argv(char** argv);

/* ASCII 大小写不敏感比较。
 * Case-insensitive compare (ASCII) */
int ft_strcase_eq(const char* a, const char* b);

/* ---- 全局遍历跳过钩子（排除过滤器）/ Global walk skip hook (exclude filter) ----
 * 返回 1 跳过该条目：不调用回调；若为目录则同时剪枝不再下钻。置 NULL 关闭。
 * Return 1 to skip this entry: the callback is not invoked and, when the
 * entry is a directory, descent into it is pruned. Set to NULL to disable. */
typedef int (*ft_skip_fn)(const char* name, int is_dir);
void ft_set_skip(ft_skip_fn fn);

/* ---- 自身日志文件 / Own log file ----
 * 解析可执行文件旁固定的 filetgie.log；失败回退当前目录，写出 UTF-8 路径。
 * Resolve the fixed log file next to the executable (filetgie.log);
 * on failure fall back to the current directory. Writes UTF-8 path. */
void ft_log_path(char* buf, size_t len);
/* 按 UTF-8 路径删除文件（Windows 走宽字符）；成功返回 0。
 * Delete a file by UTF-8 path (wide-char on Windows). 0 on success. */
int  ft_remove_file(const char* path);

#ifdef __cplusplus
}
#endif
#endif
