/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 SuiYe_TR (https://github.com/Suiye-18)
 *
 * 【本文件】程序入口：解析 --lang / --exclude / --exclude-ext 全局参数（可在
 * 命令前后出现，排除项可重复），按命令名在 CMD_TABLE 中分发，并在每次调用后
 * 向自身日志追加一条记录。
 *
 * FILETGIE - entry point. Parses global flags (may appear before or after
 * the command, repeatable for excludes), dispatches <command> to CMD_TABLE,
 * and appends one own run record after the call.
 *
 * Global flags:
 *   --lang cn|en|ja|ru | -lang cn|en|ja|ru | -l cn
 *   --lang=cn | -lang=cn | -l=cn        (case-insensitive code)
 *   --exclude <wildcard> | --exclude=<wildcard>
 *   --exclude-ext <ext>  | --exclude-ext=<ext>   (dot optional, case-insens.)
 * Everything else is <command> [args...].
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "i18n.h"
#include "commands.h"
#include "port.h"
#include "util.h"

#ifdef _WIN32
#include <windows.h>
#endif

/* 向自身日志文件追加一条运行记录（时间戳、语言码、命令、参数、退出码）；
 * 写失败被静默忽略，保证日志永不导致崩溃或改变命令结果。
 * Append one run record to the own log file; failure is silently ignored so
 * logging can never crash or alter a command's result. */
static void log_run(const char* lang_code, int n, char** rest, int rc) {
    char lp[4096];
    ft_log_path(lp, sizeof(lp));
    FILE* f = ft_fopen(lp, "a");
    if (!f) return;
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "[%s] lang=%s cmd=", ts, lang_code ? lang_code : "cn");
    if (n > 0) fprintf(f, "%s", rest[0]);
    else fprintf(f, "-");
    for (int i = 1; i < n; i++) fprintf(f, " %s", rest[i]);
    fprintf(f, " rc=%d\n", rc);
    fclose(f);
}

/* 形似负数的令牌（-3、-0.5）是位置参数而非选项，应原样放行给命令自行校验；
 * 真正的选项以字母或 '--' 开头。
 * A token shaped like a negative number (-3, -0.5) is a positional argument,
 * not an option; genuine flags start with letters or '--'. */
static int neg_number_like(const char* s) {
    if (s[0] != '-' || s[1] == '\0') return 0;
    int dot = 0;
    if (!isdigit((unsigned char)s[1])) return 0;
    for (int i = 1; s[i]; i++) {
        if (s[i] == '.') { if (dot) return 0; dot = 1; }
        else if (!isdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}

/* A bare *.ftg path is dispatched to run without typing the run command.
 * 直接给出 *.ftg 路径时，自动交给 run 执行。 */
static int ends_with_ftg(const char* s) {
    size_t l = strlen(s);
    return l >= 4 && ft_strcase_eq(s + l - 4, ".ftg");
}

int main(int argc_raw, char** argv_raw) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    (void)argc_raw;  /* argc is rebuilt by ft_argv_utf8 after wide-char conversion. */
    int argc = 0;
    char** argv = ft_argv_utf8(&argc, argv_raw);

    const char* lang_code = "cn";
    excl_clear();

    char** rest = (char**)calloc((size_t)argc + 1, sizeof(char*));
    if (!rest) { ft_free_argv(argv); return 1; }
    int n = 0;
    int parse_err = 0;

    /* ---- 参数解析 / Argument parsing ----
     * 逐令牌扫描：识别 --lang/-l、--exclude、--exclude-ext（等号与空格两种写法），
     * 其余非选项令牌原样收进 rest[] 交给命令分发。
     * Token-by-token scan: recognize --lang/-l, --exclude, --exclude-ext
     * (equals and space forms); every other non-option token goes into rest[]. */

    /* Tentative pack used only to localize errors found while parsing; it is
     * refined whenever a valid --lang token is seen.
     * 暂定先用 cn 语言包本地化解析期报错，每见到一个合法 --lang 令牌就刷新。 */
    const LangPack* L = lang_get("cn");

    for (int i = 1; i < argc && !parse_err; i++) {
        const char* a = argv[i];
        const char* eq = NULL;
        const char* take = NULL;
        const char* opt = NULL; /* option name used in error messages */

        if (strncmp(a, "--lang=", 7) == 0)       { eq = a + 7; take = "lang"; opt = "--lang"; }
        else if (strncmp(a, "-lang=", 6) == 0)  { eq = a + 6; take = "lang"; opt = "-lang"; }
        else if (strncmp(a, "-l=", 3) == 0)     { eq = a + 3; take = "lang"; opt = "-l"; }
        else if (strcmp(a, "--lang") == 0 ||
                 strcmp(a, "-lang") == 0 ||
                 strcmp(a, "-l") == 0)          { take = "lang"; opt = a; }
        else if (strncmp(a, "--exclude-ext=", 14) == 0) { eq = a + 14; take = "exte"; opt = "--exclude-ext"; }
        else if (strncmp(a, "--exclude=", 10) == 0)     { eq = a + 10; take = "pat";  opt = "--exclude"; }
        else if (strcmp(a, "--exclude-ext") == 0)      { take = "exte"; opt = "--exclude-ext"; }
        else if (strcmp(a, "--exclude") == 0)          { take = "pat";  opt = "--exclude"; }

        if (eq) {
            if (*eq == '\0') { printf(L->err_opt_empty, opt); printf("\n"); parse_err = 1; break; }
            if (!strcmp(take, "lang")) { lang_code = eq; L = lang_get(lang_code); }
            else if (!strcmp(take, "exte")) excl_add_ext(eq);
            else excl_add_pattern(eq);
            continue;
        }
        if (take) {
            if (i + 1 >= argc) { printf(L->err_opt_value, opt); printf("\n"); parse_err = 1; break; }
            const char* v = argv[++i];
            if (!*v) { printf(L->err_opt_empty, opt); printf("\n"); parse_err = 1; break; }
            if (!strcmp(take, "lang")) { lang_code = v; L = lang_get(lang_code); }
            else if (!strcmp(take, "exte")) excl_add_ext(v);
            else excl_add_pattern(v);
            continue;
        }

        /* An unrecognized token beginning with '-' is a bad option, not a
         * command; reject it here instead of falling through to
         * "unknown command". A bare '-' or a negative number literal is
         * passed through for the command's own validation. */
        if (a[0] == '-' && a[1] != '\0' && !neg_number_like(a)) {
            printf(L->err_bad_option, a);
            printf("\n");
            parse_err = 1;
            break;
        }
        rest[n++] = (char*)a;
    }
    rest[n] = NULL;

    /* Central exclude hook: every ft_walk traversal skips matches; explicit
     * single-file commands do not use ft_walk and are unaffected.
     * 全局排除钩子：所有 ft_walk 遍历都会跳过命中项；直接指定单文件的命令不走
     * ft_walk，因而不受 --exclude 影响。 */
    ft_set_skip(excl_skip);

    const LangPack* Lfinal = lang_get(lang_code);
    /* Unknown language code: warn and fall back (non-regression), keep going. */
    if (lang_code && !ft_strcase_eq(Lfinal->code, lang_code))
        printf(Lfinal->warn_lang, lang_code);

    /* ---- 命令分发 / Dispatch ----
     * 解析错误直接 rc=1；无参数时打印帮助；否则按大小写不敏感匹配 CMD_TABLE，
     * 未匹配到则报"未知命令"。
     * Parse error -> rc=1; no args -> help; otherwise match CMD_TABLE
     * case-insensitively, reporting "unknown command" on no match. */
    int rc;
    if (parse_err) {
        rc = 1;
    } else if (n == 0) {
        cmd_help(Lfinal, 0, NULL);
        rc = 0;
    } else {
        char* auto_args[3] = { NULL, NULL, NULL };
        char** dispatch = rest;
        int dn = n;
        if (n > 0 && ends_with_ftg(rest[0])) {
            auto_args[0] = (char*)"run";
            auto_args[1] = rest[0];
            dispatch = auto_args;
            dn = 2;
        }
        rc = -1;
        for (int i = 0; i < CMD_COUNT; i++) {
            if (ft_strcase_eq(dispatch[0], CMD_TABLE[i].name)) {
                rc = CMD_TABLE[i].fn(Lfinal, dn, dispatch);
                break;
            }
        }
        /* help-<command> pseudo-command: a token starting with "help-"
         * (case-insensitive) routes to the help- entry, which strips the
         * prefix and looks the target up itself. */
        if (rc == -1 && strlen(dispatch[0]) >= 5) {
            int is_help_prefix = 1;
            static const char hpref[] = "help-";
            for (int k = 0; k < 5; k++)
                if (tolower((unsigned char)dispatch[0][k]) != tolower((unsigned char)hpref[k])) {
                    is_help_prefix = 0;
                    break;
                }
            if (is_help_prefix) {
                for (int i = 0; i < CMD_COUNT; i++) {
                    if (ft_strcase_eq(CMD_TABLE[i].name, "help-")) {
                        rc = CMD_TABLE[i].fn(Lfinal, dn, dispatch);
                        break;
                    }
                }
            }
        }
        if (rc == -1) {
            printf(Lfinal->err_unknown, dispatch[0]);
            printf("\n");
            rc = 1;
        }
    }

    /* ---- 运行日志收尾 / Logging tail ----
     * 非 dellog 调用都追加一条记录；dellog 自身被 g_dellog_cleared 跳过。
     * Append a record for every run except dellog itself (skipped via flag). */

    if (!g_dellog_cleared) log_run(lang_code, n, rest, rc);
    ft_free_argv(argv);
    free(rest);
    return rc;
}
