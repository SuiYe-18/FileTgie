/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 SuiYe_TR (https://github.com/Suiye-18)
 *
 * 【本文件】手动更新检测/更新命令声明。checkupdate 只查询版本并报告；update 在
 * Windows 优先启动同目录独立更新器，由其检查 GitHub/官网并静默覆盖，失败时回退
 * 到直接下载安装包；Linux 只输出手动升级说明。
 *
 * Manual update-check / update command declarations. checkupdate only queries
 * and reports; on Windows update prefers the bundled standalone updater, which
 * checks GitHub/website and performs a silent overwrite, and falls back to a
 * direct installer download; Linux prints manual upgrade steps.
 */
#ifndef FILETGIE_UPDATE_H
#define FILETGIE_UPDATE_H

#include "i18n.h"

#ifdef __cplusplus
extern "C" {
#endif

int cmd_checkupdate(const LangPack* L, int argc, char** argv);
int cmd_update(const LangPack* L, int argc, char** argv);
int cmd_reminder(const LangPack* L, int argc, char** argv);

#ifdef __cplusplus
}
#endif
#endif
