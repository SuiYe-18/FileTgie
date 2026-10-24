/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 SuiYe_TR (https://github.com/Suiye-18)
 *
 * 【本文件】独立 FileTgie 更新器（Windows）：同时检查 GitHub Release 与官网版本，
 * 下载新版 Windows 安装包，再以静默覆盖方式启动。--check 只检测；--background
 * 用于后台运行，不弹出交互式向导。
 *
 * Standalone FileTgie updater (Windows). It checks both the GitHub Release API
 * and the official website, downloads the newer Windows installer, and launches
 * a silent overwrite install. --check only reports; --background suppresses
 * interactive progress text.
 */
#ifndef _WIN32
#error "FileTgieUpdater is Windows-only; Linux upgrades use the manual source path."
#endif

#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define FT_GITHUB_API "https://api.github.com/repos/SuiYe-18/FileTgie/releases/latest"
#define FT_WEBSITE "https://suiye-18.github.io/filetgie-official-website/"
#define FT_RELEASE_URL "https://github.com/SuiYe-18/FileTgie/releases/latest"

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} Buf;

static int buf_reserve(Buf* b, size_t need) {
    if (b->len + need + 1 <= b->cap) return 0;
    size_t cap = b->cap ? b->cap : 4096;
    while (cap < b->len + need + 1) cap *= 2;
    char* p = (char*)realloc(b->data, cap);
    if (!p) return -1;
    b->data = p;
    b->cap = cap;
    return 0;
}

static int buf_append(Buf* b, const void* ptr, size_t n) {
    if (buf_reserve(b, n) != 0) return -1;
    memcpy(b->data + b->len, ptr, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

static void buf_free(Buf* b) {
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

static int parse_semver(const char* s, int v[3]) {
    while (s && (*s == 'v' || *s == 'V')) s++;
    return sscanf(s, "%d.%d.%d", &v[0], &v[1], &v[2]) == 3;
}

static int cmp_semver(const int a[3], const int b[3]) {
    for (int i = 0; i < 3; i++) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

static int split_https(const char* url, char* host, size_t hlen, char* path, size_t plen) {
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

static int to_wide(const char* s, wchar_t** out) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return -1;
    *out = (wchar_t*)calloc((size_t)n, sizeof(wchar_t));
    if (!*out) return -1;
    MultiByteToWideChar(CP_UTF8, 0, s, -1, *out, n);
    return 0;
}

static int http_get(const char* url_in, Buf* out) {
    char url[2048];
    snprintf(url, sizeof(url), "%s", url_in);
    for (int hop = 0; hop < 5; hop++) {
        char host[256], path[2048];
        if (split_https(url, host, sizeof(host), path, sizeof(path)) != 0) return 0;

        HINTERNET session = WinHttpOpen(L"FileTgieUpdater/0.2",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) return 0;

        wchar_t* whost = NULL;
        wchar_t* wpath = NULL;
        int ready = to_wide(host, &whost) == 0 && to_wide(path, &wpath) == 0;
        HINTERNET connect = ready ? WinHttpConnect(session, whost, INTERNET_DEFAULT_HTTPS_PORT, 0) : NULL;
        HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", wpath, NULL,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;

        DWORD status = 0;
        if (request && WinHttpSendRequest(request,
                L"Accept: */*\r\nUser-Agent: FileTgieUpdater\r\n",
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
                if (buf_append(out, chunk, n) != 0) status = 0;
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
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
        free(whost);
        free(wpath);

        if (status == 200) return 1;
        if (status < 301 || status > 308) return 0;
    }
    return 0;
}

static int http_download(const char* url_in, const char* dest) {
    char url[2048];
    snprintf(url, sizeof(url), "%s", url_in);
    FILE* f = fopen(dest, "wb");
    if (!f) return 0;
    int ok = 0;
    for (int hop = 0; hop < 5; hop++) {
        char host[256], path[2048];
        if (split_https(url, host, sizeof(host), path, sizeof(path)) != 0) break;
        HINTERNET session = WinHttpOpen(L"FileTgieUpdater/0.2",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        wchar_t* whost = NULL;
        wchar_t* wpath = NULL;
        int ready = session && to_wide(host, &whost) == 0 && to_wide(path, &wpath) == 0;
        HINTERNET connect = ready ? WinHttpConnect(session, whost, INTERNET_DEFAULT_HTTPS_PORT, 0) : NULL;
        HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", wpath, NULL,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
        DWORD status = 0;
        if (request && WinHttpSendRequest(request, L"User-Agent: FileTgieUpdater\r\n",
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
            ok = status == 200;
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
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
        free(whost);
        free(wpath);
        if (ok || (status < 301 || status > 308)) break;
    }
    fclose(f);
    if (!ok) DeleteFileA(dest);
    return ok;
}

static const char* json_value(const char* body, const char* key) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(body, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (isspace((unsigned char)*p)) p++;
    if (*p != ':') return NULL;
    p++;
    while (isspace((unsigned char)*p)) p++;
    return *p == '"' ? p + 1 : NULL;
}

static void json_copy(const char* start, char* out, size_t len) {
    out[0] = '\0';
    if (!start) return;
    size_t n = 0;
    while (start[n] && start[n] != '"' && n + 1 < len) n++;
    memcpy(out, start, n);
    out[n] = '\0';
}

static int website_version(const char* html, int v[3]) {
    const char* p = html;
    int best[3] = {-1,-1,-1};
    while ((p = strchr(p, 'v')) != NULL) {
        int cand[3];
        if (parse_semver(p, cand)) {
            if (best[0] < 0 || cmp_semver(cand, best) > 0) memcpy(best, cand, sizeof(best));
        }
        p++;
    }
    if (best[0] < 0) return 0;
    memcpy(v, best, sizeof(best));
    return 1;
}

static int current_from_sibling(int v[3]) {
    char self[MAX_PATH], exe[MAX_PATH + 32], cmd[4096];
    GetModuleFileNameA(NULL, self, sizeof(self));
    char* slash = strrchr(self, '\\');
    if (slash) *slash = '\0';
    snprintf(exe, sizeof(exe), "%s\\FileTgie.exe", self);
    if (GetFileAttributesA(exe) == INVALID_FILE_ATTRIBUTES) return 0;
    snprintf(cmd, sizeof(cmd), "\"%s\" version", exe);
    FILE* f = _popen(cmd, "r");
    if (!f) return 0;
    char line[512];
    int ok = 0;
    while (fgets(line, sizeof(line), f)) {
        const char* p = strchr(line, 'v');
        if (p && parse_semver(p, v)) { ok = 1; break; }
    }
    _pclose(f);
    return ok;
}

/* Keep one notification per discovered release so a one-minute task does not
 * toast on every wake. The state file is user-specific and never touches the
 * installation folder. */
static void notified_path(char* out, size_t len) {
    const char* dir = getenv("LOCALAPPDATA");
    if (dir && *dir) {
        snprintf(out, len, "%s\\FileTgie\\notified.txt", dir);
        return;
    }
    char tmp[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tmp) > 0)
        snprintf(out, len, "%sFileTgie-notified.txt", tmp);
    else
        snprintf(out, len, "FileTgie-notified.txt");
}

static int notified_already(const char* latest) {
    char path[MAX_PATH + 64], buf[32];
    notified_path(path, sizeof(path));
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    int seen = 0;
    if (fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        seen = strcmp(buf, latest) == 0;
    }
    fclose(f);
    return seen;
}

static void mark_notified(const char* latest) {
    char path[MAX_PATH + 64];
    notified_path(path, sizeof(path));
    char* slash = strrchr(path, '\\');
    if (slash) {
        *slash = '\0';
        CreateDirectoryA(path, NULL);
        *slash = '\\';
    }
    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s", latest);
        fclose(f);
    }
}

/* Use the built-in Windows PowerShell toast channel; PowerShell itself is part
 * of Windows, so this adds no third-party dependency. Failures are ignored:
 * the read-only check still returns the newer-version exit code. */
static void show_update_toast(const char* current, const char* latest) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"[Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType=WindowsRuntime]|Out-Null;"
        "[Windows.Data.Xml.Dom.XmlDocument, Windows.Data.Xml.Dom.XmlDocument, ContentType=WindowsRuntime]|Out-Null;"
        "$x=New-Object Windows.Data.Xml.Dom.XmlDocument;"
        "$x.LoadXml('<toast activationType='protocol' launch='https://github.com/SuiYe-18/FileTgie/releases/latest'>"
        "<visual><binding template='ToastText02'>"
        "<text id='1'>FileTgie update available</text>"
        "<text id='2'>%s is available; current %s. Click to open the release page.</text>"
        "</binding></visual></toast>');"
        "$t=[Windows.UI.Notifications.ToastNotification]::new($x);"
        "[Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier('{1AC14E77-02E7-4E5D-B744-2EB1AE5198B7}\\WindowsPowerShell\\v1.0\\powershell.exe').Show($t)\"",
        latest, current);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

int main(int argc, char** argv) {
    int check_only = 0, background = 0, notify = 0;
    int current[3] = {0,0,0};
    int have_current = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--check") == 0) check_only = 1;
        else if (strcmp(argv[i], "--background") == 0) background = 1;
        else if (strcmp(argv[i], "--notify") == 0) notify = 1;
        else if (strncmp(argv[i], "--current=", 10) == 0)
            have_current = parse_semver(argv[i] + 10, current);
    }
    if (!have_current) have_current = current_from_sibling(current);

    Buf gh = {0}, site = {0};
    int gh_ok = http_get(FT_GITHUB_API, &gh);
    int site_ok = http_get(FT_WEBSITE, &site);
    if (!gh_ok && !site_ok) {
        fprintf(stderr, "Update check failed: GitHub and website are unreachable.\n");
        return 1;
    }

    int latest[3] = {-1,-1,-1};
    char asset[2048] = "";
    if (gh_ok && gh.data) {
        char tag[64];
        json_copy(json_value(gh.data, "tag_name"), tag, sizeof(tag));
        int gv[3];
        if (parse_semver(tag, gv) && (latest[0] < 0 || cmp_semver(gv, latest) > 0)) memcpy(latest, gv, sizeof(latest));
        const char* p = gh.data;
        while ((p = json_value(p, "browser_download_url")) != NULL) {
            char url[2048];
            json_copy(p, url, sizeof(url));
            if (strstr(url, "windows") && strstr(url, ".setup.exe")) {
                snprintf(asset, sizeof(asset), "%s", url);
                break;
            }
            p += strlen(url);
        }
    }
    if (site_ok && site.data) {
        int sv[3];
        if (website_version(site.data, sv) && (latest[0] < 0 || cmp_semver(sv, latest) > 0)) memcpy(latest, sv, sizeof(latest));
    }

    if (latest[0] < 0) {
        fprintf(stderr, "Could not parse a version from GitHub or the website.\n");
        buf_free(&gh); buf_free(&site);
        return 1;
    }

    if (have_current && cmp_semver(current, latest) >= 0) {
        if (!background) printf("Current v%d.%d.%d is already latest (remote v%d.%d.%d).\n",
            current[0], current[1], current[2], latest[0], latest[1], latest[2]);
        buf_free(&gh); buf_free(&site);
        return 0;
    }

    if (!background) printf("New version v%d.%d.%d found on GitHub/website.\n", latest[0], latest[1], latest[2]);
    if (check_only) {
        if (notify) {
            char latest_s[32], current_s[32];
            snprintf(latest_s, sizeof(latest_s), "v%d.%d.%d", latest[0], latest[1], latest[2]);
            if (have_current)
                snprintf(current_s, sizeof(current_s), "v%d.%d.%d", current[0], current[1], current[2]);
            else
                snprintf(current_s, sizeof(current_s), "unknown");
            if (!notified_already(latest_s)) {
                show_update_toast(current_s, latest_s);
                mark_notified(latest_s);
            }
        }
        buf_free(&gh); buf_free(&site);
        return have_current ? 2 : 0;
    }

    if (!asset[0]) {
        fprintf(stderr, "No Windows installer asset was found in the GitHub release.\n");
        buf_free(&gh); buf_free(&site);
        return 1;
    }

    char tmp[MAX_PATH];
    DWORD n = GetTempPathA(MAX_PATH, tmp);
    if (n == 0) {
        fprintf(stderr, "Cannot locate the temporary directory.\n");
        buf_free(&gh); buf_free(&site);
        return 1;
    }
    char dest[MAX_PATH + 64];
    snprintf(dest, sizeof(dest), "%sfiletgie-update-setup.exe", tmp);
    DeleteFileA(dest);

    if (!background) printf("Downloading installer...\n");
    if (!http_download(asset, dest)) {
        fprintf(stderr, "Installer download failed.\n");
        buf_free(&gh); buf_free(&site);
        return 1;
    }

    if (!background) printf("Launching silent overwrite installer...\n");
    HINSTANCE h = ShellExecuteA(NULL, "runas", dest,
        "/VERYSILENT /NORESTART /SUPPRESSMSGBOXES /CLOSEAPPLICATIONS /FORCECLOSEAPPLICATIONS /MERGETASKS=overwrite",
        NULL, SW_HIDE);
    buf_free(&gh); buf_free(&site);
    if ((INT_PTR)h <= 32) {
        fprintf(stderr, "Could not launch the elevated installer.\n");
        return 1;
    }
    if (!background) printf("Updater started. See the release page: %s\n", FT_RELEASE_URL);
    return 0;
}
