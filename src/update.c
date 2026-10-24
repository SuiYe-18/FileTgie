/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 SuiYe_TR (https://github.com/Suiye-18)
 *
 * 【本文件】手动更新检测与更新：checkupdate 只查询 GitHub Release 并比较版本；
 * update 在 Windows 优先启动同目录独立更新器（检查 GitHub/官网、下载安装包、静默
 * 覆盖），失败时回退到 WinHTTP 直接下载；Linux 不替换二进制，只给出手动升级说明。
 * 本命令由用户显式触发，不做启动时后台自动检查。
 *
 * Manual update check / update. checkupdate only queries GitHub Release and
 * compares versions. On Windows update prefers the bundled standalone updater
 * (GitHub/website check, installer download, silent overwrite) and falls back
 * to WinHTTP direct download; Linux prints manual upgrade steps and never
 * replaces its own binary. These commands are explicit only; no startup check
 * is performed.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "update.h"
#include "commands.h"
#include "port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#else
#include <sys/wait.h>
#endif

#define FT_API_URL "https://api.github.com/repos/SuiYe-18/FileTgie/releases/latest"
#define FT_RELEASE_URL "https://github.com/SuiYe-18/FileTgie/releases/latest"

/* Small dynamic byte buffer for HTTP responses. */
typedef struct {
    char*  data;
    size_t len;
    size_t cap;
} ft_buf_t;

static int buf_reserve(ft_buf_t* b, size_t need) {
    if (b->len + need + 1 <= b->cap) return 0;
    size_t cap = b->cap ? b->cap : 4096;
    while (cap < b->len + need + 1) cap *= 2;
    char* p = (char*)realloc(b->data, cap);
    if (!p) return -1;
    b->data = p;
    b->cap = cap;
    return 0;
}

static int buf_append(ft_buf_t* b, const void* ptr, size_t n) {
    if (buf_reserve(b, n) != 0) return -1;
    memcpy(b->data + b->len, ptr, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

/* Parse "major.minor.patch" from an optional leading v/V. Returns 0 on success. */
static int parse_semver(const char* text, int v[3]) {
    while (*text == 'v' || *text == 'V') text++;
    return sscanf(text, "%d.%d.%d", &v[0], &v[1], &v[2]) == 3;
}

static int compare_semver(const int a[3], const int b[3]) {
    for (int i = 0; i < 3; i++) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

/* Copy a JSON string value starting just after the opening quote. */
static void json_copy_string(const char* start, char* out, size_t len) {
    out[0] = '\0';
    if (!start || len == 0) return;
    size_t n = 0;
    while (start[n] && start[n] != '"' && n + 1 < len) n++;
    memcpy(out, start, n);
    out[n] = '\0';
}

/* Locate `"key" : "`, tolerating spaces around the colon; returns text after quote. */
static const char* json_value_start(const char* body, const char* key) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(body, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (isspace((unsigned char)*p)) p++;
    if (*p != ':') return NULL;
    p++;
    while (isspace((unsigned char)*p)) p++;
    if (*p != '"') return NULL;
    return p + 1;
}

static void json_field(const char* body, const char* key, char* out, size_t len) {
    json_copy_string(json_value_start(body, key), out, len);
}

/* Find the first download URL whose name contains both windows and suffix. */
static int extract_asset(const char* body, const char* suffix, char* out, size_t len) {
    const char* p = body;
    out[0] = '\0';
    while ((p = json_value_start(p, "browser_download_url")) != NULL) {
        char url[2048];
        json_copy_string(p, url, sizeof(url));
        if (url[0] && strstr(url, "windows") && strstr(url, suffix)) {
            snprintf(out, len, "%s", url);
            return 0;
        }
        p += strlen(url);
    }
    return -1;
}

static int check_no_extra(const LangPack* L, int argc, char** argv) {
    if (argc > 1) {
        printf(L->err_extra_args, argv[1]);
        printf("\n");
        return 1;
    }
    return 0;
}

#ifdef _WIN32
/* Split https://host/path?query into host and path buffers. */
static int split_https_url(const char* url, char* host, size_t hlen, char* path, size_t plen) {
    const char* p = strstr(url, "://");
    if (!p) return -1;
    p += 3;
    const char* slash = strchr(p, '/');
    if (!slash) {
        snprintf(host, hlen, "%s", p);
        snprintf(path, plen, "/");
        return 0;
    }
    size_t n = (size_t)(slash - p);
    if (n >= hlen) return -1;
    memcpy(host, p, n);
    host[n] = '\0';
    snprintf(path, plen, "%s", slash);
    return 0;
}

static int to_wide(const char* utf8, wchar_t** out) {
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (n <= 0) return -1;
    *out = (wchar_t*)calloc((size_t)n, sizeof(wchar_t));
    if (!*out) return -1;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, *out, n);
    return 0;
}

/* Follow HTTP redirects and return 200 on success. Text body is appended to out. */
static int win_http_text(const char* url_in, ft_buf_t* out) {
    char url[2048];
    snprintf(url, sizeof(url), "%s", url_in);

    for (int hop = 0; hop < 5; hop++) {
        char host[256], path[2048];
        if (split_https_url(url, host, sizeof(host), path, sizeof(path)) != 0) return 0;

        HINTERNET session = WinHttpOpen(L"FileTgie/0.2",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) return 0;

        wchar_t* whost = NULL;
        wchar_t* wpath = NULL;
        int ok = to_wide(host, &whost) == 0 && to_wide(path, &wpath) == 0;
        HINTERNET hConnect = ok ? WinHttpConnect(session, whost, INTERNET_DEFAULT_HTTPS_PORT, 0) : NULL;
        HINTERNET request = hConnect ? WinHttpOpenRequest(hConnect, L"GET", wpath, NULL,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;

        DWORD status = 0;
        if (request) {
            const wchar_t* headers =
                L"Accept: application/vnd.github+json\r\n"
                L"User-Agent: FileTgie\r\n";
            if (WinHttpSendRequest(request, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(request, NULL)) {
                DWORD size = sizeof(status);
                WinHttpQueryHeaders(request,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
            }
        }

        if (status == 200 && request) {
            char chunk[16384];
            DWORD n = 0;
            while (WinHttpReadData(request, chunk, (DWORD)sizeof(chunk), &n) && n > 0) {
                if (buf_append(out, chunk, n) != 0) {
                    status = 0;
                    break;
                }
            }
        } else if (status >= 301 && status <= 308 && request) {
            DWORD size = 0;
            WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, L"Location",
                NULL, &size, WINHTTP_NO_HEADER_INDEX);
            if (size > 0) {
                wchar_t* loc = (wchar_t*)calloc(size / sizeof(wchar_t) + 1, sizeof(wchar_t));
                if (loc && WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION,
                    L"Location", loc, &size, WINHTTP_NO_HEADER_INDEX)) {
                    char next[2048];
                    WideCharToMultiByte(CP_UTF8, 0, loc, -1, next, sizeof(next), NULL, NULL);
                    snprintf(url, sizeof(url), "%s", next);
                }
                free(loc);
            }
        }

        if (request) WinHttpCloseHandle(request);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (session) WinHttpCloseHandle(session);
        free(whost);
        free(wpath);

        if (status == 200) return 200;
        if (status < 301 || status > 308) return (int)status;
    }
    return 0;
}

static int win_http_download(const char* url_in, const char* dest) {
    char url[2048];
    snprintf(url, sizeof(url), "%s", url_in);
    FILE* f = ft_fopen(dest, "wb");
    if (!f) return 0;

    int ok = 0;
    for (int hop = 0; hop < 5; hop++) {
        char host[256], path[2048];
        if (split_https_url(url, host, sizeof(host), path, sizeof(path)) != 0) break;

        HINTERNET session = WinHttpOpen(L"FileTgie/0.2",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        wchar_t* whost = NULL;
        wchar_t* wpath = NULL;
        int ready = session && to_wide(host, &whost) == 0 && to_wide(path, &wpath) == 0;
        HINTERNET hConnect = ready ? WinHttpConnect(session, whost, INTERNET_DEFAULT_HTTPS_PORT, 0) : NULL;
        HINTERNET request = hConnect ? WinHttpOpenRequest(hConnect, L"GET", wpath, NULL,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
        DWORD status = 0;

        if (request && WinHttpSendRequest(request, L"User-Agent: FileTgie\r\n",
            (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(request, NULL)) {
            DWORD size = sizeof(status);
            WinHttpQueryHeaders(request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        }

        if (status == 200 && request) {
            char chunk[16384];
            DWORD n = 0;
            while (WinHttpReadData(request, chunk, (DWORD)sizeof(chunk), &n) && n > 0)
                if (fwrite(chunk, 1, n, f) != n) { status = 0; break; }
            ok = (status == 200);
        } else if (status >= 301 && status <= 308 && request) {
            DWORD size = 0;
            WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, L"Location",
                NULL, &size, WINHTTP_NO_HEADER_INDEX);
            if (size > 0) {
                wchar_t* loc = (wchar_t*)calloc(size / sizeof(wchar_t) + 1, sizeof(wchar_t));
                if (loc && WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION,
                    L"Location", loc, &size, WINHTTP_NO_HEADER_INDEX)) {
                    char next[2048];
                    WideCharToMultiByte(CP_UTF8, 0, loc, -1, next, sizeof(next), NULL, NULL);
                    snprintf(url, sizeof(url), "%s", next);
                }
                free(loc);
            }
        }

        if (request) WinHttpCloseHandle(request);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (session) WinHttpCloseHandle(session);
        free(whost);
        free(wpath);
        if (ok || (status < 301 || status > 308)) break;
    }
    fclose(f);
    if (!ok) ft_remove_file(dest);
    return ok;
}
#else
static int linux_curl_available(void) {
    FILE* f = popen("command -v curl >/dev/null 2>&1", "r");
    if (!f) return 0;
    int rc = pclose(f);
    return WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
}

static int linux_http_text(ft_buf_t* out) {
    if (!linux_curl_available()) return 2;
    FILE* f = popen(
        "curl -fsSL --max-time 20 -H 'Accept: application/vnd.github+json' "
        "https://api.github.com/repos/SuiYe-18/FileTgie/releases/latest", "r");
    if (!f) return 0;
    char chunk[16384];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0)
        if (buf_append(out, chunk, n) != 0) { pclose(f); return 0; }
    int rc = pclose(f);
    return WIFEXITED(rc) && WEXITSTATUS(rc) == 0 ? 200 : 0;
}
#endif

/* Shared release fetch + compare. Returns 1 on hard failure, 0 otherwise. */
static int fetch_release(const LangPack* L, char* tag, size_t tag_len, char* setup, size_t setup_len) {
    ft_buf_t body = { NULL, 0, 0 };
#ifdef _WIN32
    int status = win_http_text(FT_API_URL, &body);
#else
    int status = linux_http_text(&body);
    if (status == 2) {
        printf(L->upd_curl_missing);
        printf("\n");
        free(body.data);
        return 1;
    }
#endif
    if (status != 200 || !body.data || body.len == 0) {
        printf(L->upd_network);
        printf("\n");
        free(body.data);
        return 1;
    }

    json_field(body.data, "tag_name", tag, tag_len);
    if (tag[0] == 'v' || tag[0] == 'V')
        memmove(tag, tag + 1, strlen(tag));
    if (!tag[0] || parse_semver(tag, (int[3]){0,0,0}) == 0) {
        printf(L->upd_parse);
        printf("\n");
        free(body.data);
        return 1;
    }
#ifdef _WIN32
    extract_asset(body.data, ".setup.exe", setup, setup_len);
#endif
    free(body.data);
    return 0;
}

static int report_versions(const LangPack* L, const char* latest_tag, int* newer) {
    int cur[3] = {0,0,0}, lat[3] = {0,0,0};
    parse_semver(FILETGIE_VERSION, cur);
    parse_semver(latest_tag, lat);
    *newer = compare_semver(cur, lat) < 0;
    printf(L->upd_current, FILETGIE_VERSION);
    printf("\n");
    printf(L->upd_latest, latest_tag);
    printf("\n");
    if (*newer) {
        printf(L->upd_newer, latest_tag);
        printf("\n");
        printf(L->upd_release_url, FT_RELEASE_URL);
        printf("\n");
    } else {
        printf(L->upd_uptodate);
        printf("\n");
    }
    return 0;
}

int cmd_checkupdate(const LangPack* L, int argc, char** argv) {
    if (check_no_extra(L, argc, argv)) return 1;
    char tag[64], setup[2048];
    setup[0] = '\0';
    if (fetch_release(L, tag, sizeof(tag), setup, sizeof(setup))) return 1;
    int newer = 0;
    return report_versions(L, tag, &newer);
}

int cmd_update(const LangPack* L, int argc, char** argv) {
    if (check_no_extra(L, argc, argv)) return 1;
    char tag[64], setup[2048];
    setup[0] = '\0';
    if (fetch_release(L, tag, sizeof(tag), setup, sizeof(setup))) return 1;

    int newer = 0;
    report_versions(L, tag, &newer);
    if (!newer) return 0;

#ifdef _WIN32
    if (!setup[0]) {
        printf(L->upd_no_asset);
        printf("\n");
        return 1;
    }

    /* Prefer the bundled standalone updater when it sits next to FileTgie.exe.
     * It checks GitHub and the website again, downloads the installer, and runs
     * the silent overwrite install in the background. */
    char self[MAX_PATH], updater[MAX_PATH + 32];
    if (GetModuleFileNameA(NULL, self, MAX_PATH) > 0) {
        char* slash = strrchr(self, '\\');
        if (slash) *slash = '\0';
        snprintf(updater, sizeof(updater), "%s\\FileTgieUpdater.exe", self);
        if (GetFileAttributesA(updater) != INVALID_FILE_ATTRIBUTES) {
            char args[128];
            snprintf(args, sizeof(args), "--current=%s --background", FILETGIE_VERSION);
            HINSTANCE uh = ShellExecuteA(NULL, "open", updater, args, NULL, SW_HIDE);
            if ((INT_PTR)uh > 32) {
                printf(L->upd_launch);
                printf("\n");
                printf(L->upd_done);
                printf("\n");
                return 0;
            }
        }
    }

    char tmp[MAX_PATH];
    DWORD n = GetTempPathA(MAX_PATH, tmp);
    if (n == 0 || n >= MAX_PATH) {
        printf(L->upd_network);
        printf("\n");
        return 1;
    }
    char dest[MAX_PATH + 64];
    snprintf(dest, sizeof(dest), "%sfiletgie-update-setup.exe", tmp);
    ft_remove_file(dest);

    printf(L->upd_download);
    printf("\n");
    if (!win_http_download(setup, dest)) {
        printf(L->upd_network);
        printf("\n");
        return 1;
    }
    printf(L->upd_downloaded, dest);
    printf("\n");
    printf(L->upd_launch);
    printf("\n");

    HINSTANCE h = ShellExecuteA(NULL, "runas", dest,
        "/VERYSILENT /NORESTART /SUPPRESSMSGBOXES /CLOSEAPPLICATIONS /FORCECLOSEAPPLICATIONS /MERGETASKS=overwrite",
        NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32) {
        printf(L->upd_network);
        printf("\n");
        return 1;
    }
    printf(L->upd_done);
    printf("\n");
    return 0;
#else
    printf(L->upd_linux_manual);
    printf("\n");
    printf(L->upd_release_url, FT_RELEASE_URL);
    printf("\n");
    return 0;
#endif
}

#ifdef _WIN32
/* Run a short scheduler command and return its exit code. The task name is a
 * fixed FileTgie-owned user-level task; no administrator rights are required. */
static int reminder_run(const char* cmdline) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    const char* schtasks = "C:\\Windows\\System32\\schtasks.exe";
    if (!CreateProcessA(schtasks, (LPSTR)cmdline, NULL, NULL, FALSE, 0,
                        NULL, NULL, &si, &pi))
        return 1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

/* Prefer the bundled read-only updater, which checks GitHub and the website;
 * fall back to this executable's GitHub-only checkupdate when the updater is
 * absent (for example in a slim portable layout). */
static int reminder_action(char* exe, size_t exe_len, char* args, size_t args_len) {
    char self[MAX_PATH], updater[MAX_PATH + 32];
    if (GetModuleFileNameA(NULL, self, MAX_PATH) == 0) return 0;
    char* slash = strrchr(self, '\\');
    if (slash) *slash = '\0';
    snprintf(updater, sizeof(updater), "%s\\FileTgieUpdater.exe", self);
    if (GetFileAttributesA(updater) != INVALID_FILE_ATTRIBUTES) {
        snprintf(exe, exe_len, "%s", updater);
        snprintf(args, args_len, "--check");
        return 1;
    }
    snprintf(exe, exe_len, "%s", self);
    snprintf(args, args_len, "checkupdate");
    return GetFileAttributesA(exe) != INVALID_FILE_ATTRIBUTES;
}

static int reminder_task_exists(void) {
    FILE* f = _popen("schtasks.exe /Query /TN FileTgieReminder /FO LIST 2>NUL", "r");
    if (!f) return 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {}
    int code = _pclose(f);
    return code == 0;
}

static int reminder_valid_time(const char* s) {
    int hh = -1, mm = -1;
    if (strlen(s) != 5 || s[2] != ':') return 0;
    if (sscanf(s, "%d:%d", &hh, &mm) != 2) return 0;
    return hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59;
}
#endif

int cmd_reminder(const LangPack* L, int argc, char** argv) {
#ifndef _WIN32
    (void)argc; (void)argv;
    printf(L->rem_linux_manual);
    printf("\n");
    return 0;
#else
    if (argc < 2) {
        printf(L->rem_usage);
        printf("\n");
        return 1;
    }

    const char* action = argv[1];
    if (ft_strcase_eq(action, "off")) {
        if (reminder_task_exists()) {
            int rc = reminder_run("schtasks.exe /Delete /TN FileTgieReminder /F");
            if (rc != 0) {
                printf(L->rem_scheduler_failed);
                printf("\n");
                return 1;
            }
        }
        printf(L->rem_off);
        printf("\n");
        return 0;
    }

    if (ft_strcase_eq(action, "status")) {
        printf(reminder_task_exists() ? L->rem_status_on : L->rem_status_off);
        printf("\n");
        return 0;
    }

    if (!ft_strcase_eq(action, "on")) {
        printf(L->rem_bad_schedule);
        printf("\n");
        return 1;
    }

    const char* mode = argc >= 3 ? argv[2] : "daily";
    const char* time = argc >= 4 ? argv[3] : "09:00";
    if (argc >= 5) {
        printf(L->rem_bad_schedule);
        printf("\n");
        return 1;
    }

    char exe[MAX_PATH + 64], args[64];
    if (!reminder_action(exe, sizeof(exe), args, sizeof(args))) {
        printf(L->rem_scheduler_failed);
        printf("\n");
        return 1;
    }

    char cmdline[8192];
    int built = 0;
    if (ft_strcase_eq(mode, "daily")) {
        if (!reminder_valid_time(time)) {
            printf(L->rem_bad_schedule);
            printf("\n");
            return 1;
        }
        built = snprintf(cmdline, sizeof(cmdline),
            "schtasks.exe /Create /TN FileTgieReminder "
            "/TR \"\\\"%s\\\" %s\" /SC DAILY /ST %s /F", exe, args, time) > 0;
    } else if (ft_strcase_eq(mode, "hourly")) {
        built = snprintf(cmdline, sizeof(cmdline),
            "schtasks.exe /Create /TN FileTgieReminder "
            "/TR \"\\\"%s\\\" %s\" /SC HOURLY /MO 1 /F", exe, args) > 0;
    } else {
        printf(L->rem_bad_schedule);
        printf("\n");
        return 1;
    }

    if (!built || reminder_run(cmdline) != 0) {
        printf(L->rem_scheduler_failed);
        printf("\n");
        return 1;
    }

    if (ft_strcase_eq(mode, "daily")) printf(L->rem_on_daily, time);
    else printf(L->rem_on_hourly);
    printf("\n");
    return 0;
#endif
}
