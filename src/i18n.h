/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 SuiYe_TR (https://github.com/Suiye-18)
 *
 * 【本文件】国际化字符串包定义：LangPack 集中存放横幅、错误、标签、每条命令的
 * 本地化帮助等全部文案；运行时由 --lang cn|en|ja|ru 选择，缺省 cn。
 *
 * FILETGIE - internationalization string packs. LangPack holds banner,
 * errors, labels and per-command localized help; selected at runtime via
 * --lang cn|en|ja|ru (default cn).
 */
#ifndef FILETGIE_I18N_H
#define FILETGIE_I18N_H

#ifdef __cplusplus
extern "C" {
#endif

/* 语言包：字段顺序在四个语言包（cn/en/ja/ru）中必须保持一致，新增字段需同步
 * 追加到全部四个包。
 * Language pack: field order must stay identical across the four packs
 * (cn/en/ja/ru); any new field is appended in all four at once. */
typedef struct {
    const char* code;
    const char* name;

    const char* banner;
    const char* usage;
    const char* help_title;
    const char* lang_now;

    const char* err_unknown;
    const char* err_args;
    const char* err_notfound;
    const char* err_dirmissing;
    const char* err_access;
    const char* err_generic;
    const char* err_numeric;

    const char* lbl_file;
    const char* lbl_dir;
    const char* lbl_size;
    const char* lbl_bytes;
    const char* lbl_mtime;
    const char* lbl_ctime;
    const char* lbl_atime;
    const char* lbl_lines;
    const char* lbl_words;
    const char* lbl_chars;
    const char* lbl_ext;
    const char* lbl_type;
    const char* lbl_total;
    const char* lbl_matches;
    const char* lbl_empty;
    const char* lbl_equal;
    const char* lbl_diff;
    const char* lbl_process;
    const char* lbl_done;
    const char* lbl_encoding;
    const char* lbl_path;
    const char* lbl_count;
    const char* lbl_none;
    const char* lbl_threshold;
    const char* lbl_days;
    const char* lbl_limit;
    const char* lbl_available;
    const char* lbl_disk;
    const char* lbl_free;
    const char* lbl_used;
    const char* lbl_unknown;
    const char* lbl_text;
    const char* lbl_binary;
    const char* lbl_drive;

    /* one short description per subcommand */
    const char* h_help;
    const char* h_version;
    const char* h_langs;
    const char* h_stat;
    const char* h_count;
    const char* h_lines;
    const char* h_linesdir;
    const char* h_size;
    const char* h_du;
    const char* h_tree;
    const char* h_findname;
    const char* h_findext;
    const char* h_findcontent;
    const char* h_findempty;
    const char* h_findlarge;
    const char* h_findold;
    const char* h_findnew;
    const char* h_hash;
    const char* h_duplicates;
    const char* h_encoding;
    const char* h_type;
    const char* h_list;
    const char* h_compare;
    const char* h_head;
    const char* h_tail;
    const char* h_wc;
    const char* h_disk;
    const char* h_recent;
    const char* h_extstats;
    const char* h_extsize;
    const char* h_checksum;
    const char* h_exists;
    const char* h_istext;
    const char* h_longest;
    const char* h_blank;
    const char* h_chars;
    const char* h_whereis;
    /* --- expanded: statistics --- */
    const char* h_newest;
    const char* h_oldest;
    const char* h_avgsize;
    const char* h_sizesort;
    const char* h_typecount;
    const char* h_hidden;
    const char* h_readonly;
    /* --- expanded: lines --- */
    const char* h_nonblank;
    const char* h_matchlines;
    const char* h_avglen;
    const char* h_findline;
    const char* h_cjk;
    /* --- expanded: search --- */
    const char* h_findfile;
    const char* h_dupenames;
    const char* h_today;
    const char* h_findrange;
    const char* h_findexts;
    const char* h_findpath;

    /* --- v1.0.1: localized output strings (appended, keep 4 packs in sync) --- */
    const char* ver_line;       /* version banner, %s = version number        */
    const char* hdr_type;      /* list table header: type                    */
    const char* hdr_size;       /* list table header: size                    */
    const char* hdr_name;      /* list table header: name                   */
    const char* lbl_crc32;     /* "CRC32:" prefix                           */
    const char* sep_duplicates; /* duplicates section separator               */
    const char* msg_maxline;   /* longest: %zu                               */
    const char* msg_avgsize;   /* files %lld, total %lld %s, average %s      */
    const char* cat_image;
    const char* cat_video;
    const char* cat_audio;
    const char* cat_document;
    const char* cat_archive;
    const char* cat_code;
    const char* cat_other;
    const char* msg_nonblank;  /* non-empty lines: %lld                     */
    const char* msg_avglen;     /* avg line length: %.2f                      */
    const char* fmt_findline;  /* "%d: %s"                                   */
    const char* msg_cjk;       /* CJK characters: %lld                      */
    const char* ext_none;      /* placeholder for files without extension    */
    /* --- logging: log / dellog --- */
    const char* h_log;
    const char* h_dellog;
    const char* log_empty;
    const char* log_deleted;
    const char* log_missing;

    /* --- v0.1.0: boundary hardening messages (appended; keep 4 packs in sync) --- */
    const char* err_bad_option;  /* unknown option token %s              */
    const char* err_opt_value;   /* option %s missing its value         */
    const char* err_opt_empty;    /* option %s value is empty            */
    const char* warn_lang;       /* unknown language code %s, fallback  */
    const char* err_extra_args;  /* unexpected trailing argument %s    */
    const char* err_int_bad;     /* non-negative integer required: %s   */
    const char* err_num_bad;     /* non-negative number required: %s    */
    const char* err_negative;    /* negative not allowed: %s            */
    const char* err_range_bad;    /* min > max: %s > %s                  */
    const char* err_need_dir;    /* expected dir, got file: %s          */
    const char* err_need_file;   /* expected file, got dir: %s          */
    const char* msg_no_match;    /* nothing matched / empty result      */

    /* --- v0.1.1: directory comparison (appended; keep 4 packs in sync) --- */
    const char* h_dirdiff;       /* dirdiff help text                   */
    const char* cmp_metric;      /* comparison metric column header      */
    const char* cmp_title;       /* comparison report title              */
    const char* cmp_col_a;       /* first directory column header        */
    const char* cmp_col_b;      /* second directory column header      */
    const char* cmp_col_delta;   /* B-A delta column header             */
    const char* cmp_text_files;  /* text files row label                */
    const char* cmp_hidden;      /* hidden files row label              */
    const char* cmp_avg_size;    /* average file size row label         */

    /* --- v0.2.0: manual update check / update (appended; keep 4 packs in sync) --- */
    const char* h_checkupdate;     /* checkupdate help text              */
    const char* h_update;          /* update help text                   */
    const char* upd_current;       /* current version line, %s           */
    const char* upd_latest;        /* latest version line, %s            */
    const char* upd_uptodate;      /* already latest                      */
    const char* upd_newer;         /* newer available, %s                */
    const char* upd_network;       /* network failure                     */
    const char* upd_parse;         /* release JSON parse failure          */
    const char* upd_no_asset;      /* Windows installer asset missing     */
    const char* upd_download;      /* downloading message                */
    const char* upd_downloaded;    /* saved installer path, %s           */
    const char* upd_launch;        /* launching installer                */
    const char* upd_done;          /* installer launched guidance        */
    const char* upd_linux_manual;  /* Linux manual upgrade note          */
    const char* upd_release_url;   /* release URL line, %s               */
    const char* upd_curl_missing;  /* Linux curl unavailable             */

    /* --- v0.3.0: scheduled update reminder (appended; keep 4 packs in sync) --- */
    const char* h_reminder;        /* reminder help text                 */
    const char* rem_usage;         /* reminder syntax                    */
    const char* rem_on_daily;      /* daily reminder enabled, %s time    */
    const char* rem_on_hourly;     /* hourly reminder enabled            */
    const char* rem_off;           /* reminder disabled                  */
    const char* rem_status_on;      /* reminder enabled status             */
    const char* rem_status_off;    /* reminder disabled status           */
    const char* rem_bad_schedule;  /* invalid schedule syntax             */
    const char* rem_scheduler_failed; /* Windows task configuration failed */
    const char* rem_linux_manual;  /* Linux cron guidance                */
} LangPack;

/* 按语言码（cn/en/ja/ru）返回对应包，无法识别时回退到 cn。
 * Return the pack for code ("cn","en","ja","ru"); falls back to cn. */
const LangPack* lang_get(const char* code);
/* 向标准输出列出全部受支持语言码（供 langs 命令）。
 * List all supported language codes to stdout. */
void lang_list_all(void);

#ifdef __cplusplus
}
#endif
#endif
