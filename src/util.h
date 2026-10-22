/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 SuiYe_TR (https://github.com/Suiye-18)
 *
 * 【本文件】平台无关的共享工具：CRC32 校验、通配符匹配、扩展名提取、编码检测、
 * 文本/二进制启发式判断、类型猜测，以及 --exclude 全局排除过滤器。
 *
 * FILETGIE - shared helpers: CRC32, wildcards, encoding, text heuristics,
 * type guessing, and the global --exclude filter.
 */
#ifndef FILETGIE_UTIL_H
#define FILETGIE_UTIL_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 把字节数格式化为人类可读大小（1024 二进制进位），如 1.52 MB。
 * Human readable size into buf, e.g. 1.52 MB */
void human_size(long long bytes, char* buf, size_t len);

/* 计算文件 CRC32；成功返回 0 并把校验值写入 out，出错返回 -1。
 * CRC32 of a file; returns 0 on success, checksum in out. -1 on error. */
int file_crc32(const char* path, unsigned long* out);

/* 简单通配符匹配（* 与 ?），ASCII 大小写不敏感。
 * Simple wildcard match with * and ?, ASCII case-insensitive. */
int wildcard_match(const char* pattern, const char* name);

/* 提取最后一个点之后的小写扩展名写入 ext；无扩展名写空串。
 * Lowercased extension (after last dot), written into ext buf. "" if none. */
void file_extension(const char* path, char* ext, size_t len);

/* 编码检测：把标签写入 buf（"UTF-8 BOM"、"UTF-16 LE" 等）；成功返回 0。
 * Encoding detection: writes label into buf ("UTF-8 BOM", "UTF-16LE", ...).
 * Returns 0 on success. */
int detect_encoding(const char* path, char* buf, size_t len);

/* 1=疑似文本，0=疑似二进制（按首块是否出现 NUL 判断）。
 * 1 if likely text, 0 if likely binary. */
int looks_text(const char* path);

/* 统计 UTF-8 码点数；出错返回 -1。
 * Count UTF-8 code points; -1 on error. */
long long count_utf8_chars(const char* path);

/* 按扩展名猜测友好类型名写入 buf。
 * Guess a friendly type name from extension into buf. */
void guess_type(const char* path, char* buf, size_t len);

/* ---- 全局排除过滤器（--exclude / --exclude-ext）/ Global exclude filter ----
 * 通配符匹配 basename（* ?，ASCII 大小写不敏感）；扩展名带不带前导点等价、
 * 大小写不敏感。excl_skip(name, is_dir) 返回 1 表示跳过（命中模式的目录被
 * 遍历器剪枝）。
 * Patterns match the basename (wildcard * ?, ASCII case-insensitive);
 * extensions match with or without a leading dot, case-insensitive.
 * excl_skip(name, is_dir) returns 1 when the entry must be skipped
 * (directories matching a pattern are pruned by the walker). */
void excl_clear(void);
void excl_add_pattern(const char* pat);
void excl_add_ext(const char* ext);
int  excl_skip(const char* name, int is_dir);

#ifdef __cplusplus
}
#endif
#endif
