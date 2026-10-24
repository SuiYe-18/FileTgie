/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 SuiYe_TR (https://github.com/Suiye-18)
 *
 * 【本文件】子命令注册表与入口声明：CMD_TABLE 将命令名映射到处理函数与帮助文案
 * 偏移；FILETGIE_VERSION 是全程序版本号的唯一来源。
 *
 * FILETGIE - subcommand table and entry points. CMD_TABLE maps each command
 * name to its handler and help-text offset; FILETGIE_VERSION is the single
 * source of the program version.
 */
#ifndef FILETGIE_COMMANDS_H
#define FILETGIE_COMMANDS_H

#include "i18n.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int cmd_help(const LangPack* L, int argc, char** argv);
int cmd_checkupdate(const LangPack* L, int argc, char** argv);
int cmd_update(const LangPack* L, int argc, char** argv);
int cmd_reminder(const LangPack* L, int argc, char** argv);

/* 命令表条目：命令名、处理函数指针，以及帮助文案在 LangPack 内的字段偏移。
 * Command entry: name, handler, and the h_xxx description offset inside
 * LangPack (used by help to print localized text without name lookups). */
typedef struct {
    const char* name;
    int (*fn)(const LangPack* L, int argc, char** argv);
    size_t help_off; /* offset of the h_xxx description inside LangPack */
} CommandEntry;

extern const CommandEntry CMD_TABLE[];
extern const int CMD_COUNT;

/* 由 cmd_dellog 每次调用置 1；main.c 据此跳过为 dellog 写日志，避免友好回复后
 * 又凭空造出一条日志。
 * Set to 1 by cmd_dellog on every invocation; main.c then skips appending the
 * dellog record so a missing log stays missing after a friendly reply. */
extern int g_dellog_cleared;

/* 版本号唯一来源，version 命令与帮助横幅都读取它，不在别处硬编码。
 * Single source of truth for the version; read by version and the help banner. */
#ifndef FILETGIE_VERSION
#define FILETGIE_VERSION "0.4.0"
#endif

#ifdef __cplusplus
}
#endif
#endif
