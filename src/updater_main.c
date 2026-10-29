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
#include <stdarg.h>

#define FT_GITHUB_API "https://api.github.com/repos/SuiYe-18/FileTgie/releases/latest"
#define FT_BAR_WIDTH 20
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
        /* Explicit bounds avoid an indefinite WinHTTP wait when github.com's
         * download host is unreachable but api.github.com still responds. */
        WinHttpSetTimeouts(session, 15000, 15000, 30000, 60000);

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

static void set_error(char* err, size_t len, const char* fmt, ...) {
    if (!err || len == 0) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(err, len, fmt, args);
    va_end(args);
}

/* Twenty segments, each equal to 5%. Green > means bytes received; white =
 * means bytes still waiting. The bar is redrawn only when a segment changes.
 * A negative percent means the total size is unknown (no Content-Length and
 * no Release API asset size); no percentage number is fabricated in that case. */
static void print_progress(int filled, int percent, int background) {
    if (background) return;
    if (filled < 0) filled = 0;
    if (filled > FT_BAR_WIDTH) filled = FT_BAR_WIDTH;

    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    WORD original = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    if (GetConsoleScreenBufferInfo(out, &info)) original = info.wAttributes;

    fputc('\r', stdout);
    SetConsoleTextAttribute(out, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    for (int i = 0; i < filled; i++) fputc('>', stdout);
    SetConsoleTextAttribute(out, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    for (int i = filled; i < FT_BAR_WIDTH; i++) fputc('=', stdout);
    SetConsoleTextAttribute(out, original);
    if (percent >= 0 && percent <= 100) printf(" %3d%%", percent);
    fflush(stdout);
}

static int http_download(const char* url_in, const char* dest, int background,
                        ULONGLONG known_size, char* err, size_t err_len) {
    char url[2048];
    snprintf(url, sizeof(url), "%s", url_in);
    FILE* f = fopen(dest, "wb");
    if (!f) {
        set_error(err, err_len, "Cannot create temporary installer file: %s", dest);
        return 0;
    }
    int ok = 0;
    for (int hop = 0; hop < 5; hop++) {
        char host[256], path[2048];
        if (split_https(url, host, sizeof(host), path, sizeof(path)) != 0) {
            set_error(err, err_len, "Invalid installer download URL.");
            break;
        }

        HINTERNET session = WinHttpOpen(L"FileTgieUpdater/0.2",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) {
            set_error(err, err_len, "Cannot open WinHTTP network session (error %lu).", GetLastError());
            break;
        }
        WinHttpSetTimeouts(session, 15000, 15000, 30000, 60000);

        wchar_t* whost = NULL;
        wchar_t* wpath = NULL;
        int ready = to_wide(host, &whost) == 0 && to_wide(path, &wpath) == 0;
        HINTERNET connect = ready ? WinHttpConnect(session, whost, INTERNET_DEFAULT_HTTPS_PORT, 0) : NULL;
        HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", wpath, NULL,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
        if (!request) {
            set_error(err, err_len, "Cannot connect to the download server (error %lu).", GetLastError());
        }

        DWORD status = 0;
        if (request && WinHttpSendRequest(request,
                L"Accept: application/octet-stream\r\nUser-Agent: FileTgieUpdater\r\n",
                (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(request, NULL)) {
            DWORD size = sizeof(status);
            WinHttpQueryHeaders(request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        } else if (request) {
            set_error(err, err_len, "Network request failed before response (WinHTTP error %lu).", GetLastError());
        }

        if (status == 200 && request) {
            ULONGLONG expected = known_size;
            ULONGLONG received = 0;
            int last_filled = -1;
            DWORD header_size = 0;
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH,
                    WINHTTP_HEADER_NAME_BY_INDEX, NULL, &header_size,
                    WINHTTP_NO_HEADER_INDEX) && header_size > 0) {
                char* header = (char*)malloc(header_size + 1);
                if (header) {
                    memset(header, 0, header_size + 1);
                    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH,
                            WINHTTP_HEADER_NAME_BY_INDEX, header, &header_size,
                            WINHTTP_NO_HEADER_INDEX)) {
                        ULONGLONG header_size_value = _strtoui64(header, NULL, 10);
                        if (header_size_value > 0) expected = header_size_value;
                    }
                    free(header);
                }
            }

            /* Content-Length wins over the Release API asset size; when
             * neither is available the percentage stays unknown instead of
             * being fabricated. */
            if (expected > 0) print_progress(0, 0, background);
            char chunk[16384];
            DWORD n = 0;
            while (WinHttpReadData(request, chunk, (DWORD)sizeof(chunk), &n) && n > 0) {
                if (fwrite(chunk, 1, n, f) != n) {
                    set_error(err, err_len, "Disk write error while saving the installer.");
                    status = 0;
                    break;
                }
                received += n;
                if (expected > 0) {
                    /* Floor (not round): a segment lights only once its 5%
                     * share is fully reached, so 0-4.99% keeps 20 white '='
                     * and the first '>' appears exactly at 5%. */
                    int percent = (int)(received * 100 / expected);
                    int filled = (int)(received * FT_BAR_WIDTH / expected);
                    if (filled != last_filled) {
                        print_progress(filled, percent, background);
                        last_filled = filled;
                    }
                }
            }
            ok = status == 200;
            if (ok && expected > 0) print_progress(FT_BAR_WIDTH, 100, background);
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
                } else {
                    set_error(err, err_len, "Invalid redirect received from the download server.");
                }
                free(loc);
            } else {
                set_error(err, err_len, "Redirect response did not include a Location header.");
            }
        } else if (status > 0) {
            set_error(err, err_len, "Unexpected HTTP status %lu from the download server.", status);
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

/* Read a numeric JSON field that appears after `start`; used for asset size. */
static const char* json_number_after(const char* start, const char* key) {
    if (!start) return NULL;
    char pat[80];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(start, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (isspace((unsigned char)*p)) p++;
    if (*p != ':') return NULL;
    p++;
    while (isspace((unsigned char)*p)) p++;
    return isdigit((unsigned char)*p) ? p : NULL;
}

/* Find the nearest JSON key at or before start, used to associate an asset
 * name with its preceding API asset URL without a full JSON parser. */
static const char* find_previous_key(const char* start, const char* key,
                                     const char* floor) {
    size_t klen = strlen(key);
    for (const char* p = start; p >= floor + klen; p--) {
        if (strncmp(p - klen, key, klen) == 0) return p - klen;
    }
    return NULL;
}

/* Prefer api.github.com/.../assets/<id>, because the browser_download_url
 * host github.com may time out even when the API host remains reachable. */
static int extract_setup_asset_api(const char* body, char* out, size_t len,
                                   ULONGLONG* asset_size) {
    out[0] = '\0';
    if (asset_size) *asset_size = 0;
    const char* p = body;
    while ((p = strstr(p, ".setup.exe")) != NULL) {
        const char* name_key = find_previous_key(p, "\"name\"", body);
        char name[256];
        json_copy(name_key ? json_value(name_key, "name") : NULL, name, sizeof(name));
        const char* url_key = name_key ? find_previous_key(name_key, "\"url\"", body) : NULL;
        const char* value = url_key ? json_value(url_key, "url") : NULL;
        if (value && strstr(name, "windows") && strstr(name, ".setup.exe")) {
            json_copy(value, out, len);
            if (out[0] && strstr(out, "/releases/assets/")) {
                if (asset_size) {
                    const char* size_value = json_number_after(name_key, "size");
                    if (size_value) *asset_size = _strtoui64(size_value, NULL, 10);
                }
                return 0;
            }
        }
        p++;
    }
    return -1;
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
 * show the alert on every wake. The state file is user-specific and never
 * touches the installation folder. */
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

/* Show a visible, foregrounded alert when the scheduled check finds a release.
 * PowerShell/WinRT toast channels can be suppressed or fail silently under Task
 * Scheduler, so the direct MessageBox is the reliable fallback; it needs no
 * third-party runtime and stays visible until the user responds. */
static void show_update_alert(const char* current, const char* latest) {
    char text[768];
    snprintf(text, sizeof(text),
        "FileTgie update available\n\n"
        "Current version: %s\n"
        "New version: %s\n\n"
        "Choose OK to open the release page, or Cancel to stay here.",
        current, latest);

    int choice = MessageBoxA(NULL, text, "FileTgie Update",
                             MB_OKCANCEL | MB_ICONINFORMATION | MB_TOPMOST |
                             MB_SETFOREGROUND | MB_SYSTEMMODAL);
    if (choice == IDOK)
        ShellExecuteA(NULL, "open", FT_RELEASE_URL, NULL, NULL, SW_SHOWNORMAL);
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
    ULONGLONG asset_size = 0;
    if (gh_ok && gh.data) {
        char tag[64];
        json_copy(json_value(gh.data, "tag_name"), tag, sizeof(tag));
        int gv[3];
        if (parse_semver(tag, gv) && (latest[0] < 0 || cmp_semver(gv, latest) > 0)) memcpy(latest, gv, sizeof(latest));
        if (extract_setup_asset_api(gh.data, asset, sizeof(asset), &asset_size) != 0) {
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
                show_update_alert(current_s, latest_s);
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

    char download_error[512] = "Unknown download failure.";
    if (!background) {
        printf("Downloading installer...\n");
        if (asset_size > 0) print_progress(0, 0, background);
    }
    if (!http_download(asset, dest, background, asset_size, download_error, sizeof(download_error))) {
        if (!background) fputc('\n', stderr);
        fprintf(stderr, "Installer download failed: %s\n", download_error);
        buf_free(&gh); buf_free(&site);
        return 1;
    }

    if (!background) {
        fputc('\n', stdout);
        printf("Launching silent overwrite installer...\n");
    }
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
