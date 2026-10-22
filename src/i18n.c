#include "i18n.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 SuiYe_TR (https://github.com/Suiye-18)
 *
 * 【本文件】四语言字符串包（cn/en/ja/ru）与运行时选择：四个 LangPack 字段顺序
 * 必须完全一致，新增词条需同步追加到全部四个包；lang_get 按码查找并回退 cn。
 *
 * Four string packs (cn/en/ja/ru) and runtime selection. Field order must
 * match across all four packs; a new entry is appended to every pack at
 * once. lang_get looks up by code and falls back to cn.
 */

/* ------------------------------------------------------------------ */
/* 中文（默认语言包）/ Chinese (default pack)                            */
/* ------------------------------------------------------------------ */
static const LangPack ZH = {
    "cn", "中文",
    "FileTgie — 轻量文件统计与查找辅助工具",
    "用法: FileTgie [--lang cn|en|ja|ru] [--exclude <通配符>] [--exclude-ext <扩展名>] <命令> [参数...]   输入 help 查看全部命令",
    "可用命令:",
    "当前语言",
    "未知命令「%s」，输入 help 查看命令列表",
    "参数不足，用法: %s",
    "路径不存在: %s",
    "目录不存在: %s",
    "无法读取: %s",
    "出错: %s",
    "需要数字参数: %s",
    /* labels */
    "文件", "文件夹", "大小", "字节", "修改时间", "创建时间", "访问时间",
    "行数", "单词", "字符", "扩展名", "类型", "合计", "命中", "空",
    "相同", "不同", "处理中...", "完成", "编码", "路径", "数量", "无",
    "阈值", "天", "上限", "可用", "磁盘", "剩余", "已用",
    "未知", "文本", "二进制", "驱动器",
    /* command help */
    "显示本帮助",
    "显示版本信息",
    "列出可用语言",
    "显示文件/目录详细信息",
    "递归统计目录内文件与文件夹数量",
    "统计单个文件的行数",
    "递归统计目录内文本文件总行数",
    "显示文件大小(人类可读)",
    "汇总目录总大小",
    "打印目录树",
    "按文件名通配符查找(支持 * ?)",
    "按扩展名查找文件",
    "在文件内容中查找文本",
    "查找空文件与空目录",
    "查找大于指定大小(MB)的文件",
    "查找超过 N 天未修改的文件",
    "查找最近 N 天内修改的文件",
    "计算文件 CRC32 校验值",
    "按大小+CRC32 查找重复文件",
    "检测文件编码(BOM/UTF-8/UTF-16/ANSI)",
    "按扩展名猜测文件类型",
    "列出目录内容",
    "逐字节比较两个文件是否相同",
    "显示文件前 N 行",
    "显示文件末尾 N 行",
    "统计行/词/字符数",
    "显示磁盘容量与剩余空间",
    "列出最近修改的 N 个文件",
    "按扩展名统计文件数量",
    "按扩展名汇总占用大小",
    "列出目录内所有文件及其 CRC32",
    "判断路径是否存在(文件/目录)",
    "判断文件是文本还是二进制",
    "找出文件最长行长度",
    "统计空行数",
    "统计字符数(UTF-8 感知)",
    "在 PATH 中查找可执行程序",
    /* expanded: statistics */
    "列出最近修改的文件",
    "列出最久未修改的文件",
    "计算平均文件大小",
    "按大小降序列出文件",
    "按大类(图片/视频/音频/文档/压缩/代码)统计数量",
    "列出隐藏文件",
    "列出只读文件",
    /* expanded: lines */
    "统计非空行数",
    "递归统计匹配文本的行数",
    "计算平均行长度",
    "输出含文本的行号",
    "统计中日韩汉字字符数",
    /* expanded: search */
    "按精确文件名查找",
    "查找不同目录下的同名文件",
    "查找今天修改过的文件",
    "按大小区间(MB)查找文件",
    "按多个扩展名查找(逗号分隔)",
    "按完整路径子串查找",
    /* v1.0.1 localized output */
    "FileTgie v%s  (C) 2026  纯 C 编写，支持 Windows / Linux",
    "类型", "大小", "名称",
    "CRC32:",
    "--- 重复文件 ---",
    "最长行长度: %zu",
    "文件: %lld  合计: %lld %s  平均: %s",
    "图片", "视频", "音频", "文档", "压缩包", "代码", "其他",
    "非空行数: %lld",
    "平均行长度: %.2f",
    "%d: %s",
    "CJK 字符: %lld",
    "(无)",
    /* logging */
    "查看自有运行日志",
    "删除自有运行日志(仅 FileTgie 自身日志文件)",
    "日志为空",
    "自有日志已删除",
    "日志文件不存在",
    /* v0.1.0 boundary hardening */
    "未知选项「%s」，输入 help 查看命令列表",
    "选项 %s 缺少参数值",
    "选项 %s 的值不能为空",
    "警告：未知语言码「%s」，已回退到 cn",
    "意外的多余参数: %s",
    "需要非负整数: %s",
    "需要非负数字: %s",
    "负数不允许: %s",
    "最小值不能大于最大值（%s > %s）",
    "需要目录，实际是文件: %s",
    "需要文件，实际是目录: %s",
    "未找到匹配项"
};

/* ------------------------------------------------------------------ */
/* English 语言包 / English pack                                        */
/* ------------------------------------------------------------------ */
static const LangPack EN = {
    "en", "English",
    "FileTgie — lightweight file statistics & search helper",
    "Usage: FileTgie [--lang cn|en|ja|ru] [--exclude <wildcard>] [--exclude-ext <ext>] <command> [args...]   type help for all commands",
    "Available commands:",
    "Language",
    "Unknown command '%s'; type help for the command list",
    "Not enough arguments. Usage: %s",
    "Path not found: %s",
    "Directory not found: %s",
    "Cannot read: %s",
    "Error: %s",
    "A number is required: %s",
    /* labels */
    "File", "Directory", "Size", "Bytes", "Modified", "Created", "Accessed",
    "Lines", "Words", "Characters", "Extension", "Type", "Total", "Matches",
    "Empty", "Identical", "Different", "Processing...", "Done", "Encoding",
    "Path", "Count", "None", "Threshold", "days", "Limit", "available",
    "Disk", "Free", "Used", "Unknown", "Text", "Binary", "Drive",
    /* command help */
    "Show this help",
    "Show version",
    "List supported languages",
    "Show detailed info of a file/directory",
    "Recursively count files and folders",
    "Count lines of a single file",
    "Recursively count total lines of text files",
    "Show file size (human readable)",
    "Summarize total size of a directory",
    "Print directory tree",
    "Find files by name wildcard (supports * ?)",
    "Find files by extension",
    "Search text inside files",
    "Find empty files and folders",
    "Find files larger than threshold (MB)",
    "Find files not modified for N days",
    "Find files modified in the last N days",
    "Compute CRC32 checksum of a file",
    "Find duplicate files by size+CRC32",
    "Detect file encoding (BOM/UTF-8/UTF-16/ANSI)",
    "Guess file type from extension",
    "List directory contents",
    "Compare two files byte by byte",
    "Show first N lines of a file",
    "Show last N lines of a file",
    "Count lines / words / characters",
    "Show disk capacity and free space",
    "List N most recently modified files",
    "Count files per extension",
    "Summarize size per extension",
    "List files with their CRC32",
    "Check whether a path exists (file/dir)",
    "Decide whether a file is text or binary",
    "Find the longest line length",
    "Count blank lines",
    "Count characters (UTF-8 aware)",
    "Locate an executable in PATH",
    /* expanded: statistics */
    "Show the most recently modified file",
    "Show the oldest (least recently modified) file",
    "Average file size",
    "List files sorted by size descending",
    "Count by broad category (image/video/audio/doc/archive/code)",
    "List hidden files",
    "List read-only files",
    /* expanded: lines */
    "Count non-empty lines",
    "Recursively count lines matching text",
    "Average line length",
    "Print line numbers containing text",
    "Count CJK characters",
    /* expanded: search */
    "Find by exact file name",
    "Find identical file names in different folders",
    "Find files modified today",
    "Find files within a size range (MB)",
    "Find by several extensions (comma separated)",
    "Match substring against full path",
    /* v1.0.1 localized output */
    "FileTgie v%s  (C) 2026  built for Windows / Linux, pure C",
    "Type", "Size", "Name",
    "CRC32:",
    "--- duplicates ---",
    "max line length: %zu",
    "files: %lld  total: %lld %s  average: %s",
    "image", "video", "audio", "document", "archive", "code", "other",
    "non-empty lines: %lld",
    "avg line length: %.2f",
    "%d: %s",
    "CJK characters: %lld",
    "(none)",
    /* logging */
    "Show the own run log",
    "Delete the own run log (FileTgie log only)",
    "Log is empty",
    "Own log deleted",
    "Log file does not exist",
    /* v0.1.0 boundary hardening */
    "Unknown option '%s'; type help for the command list",
    "Missing value for option '%s'",
    "Value for option '%s' must not be empty",
    "warning: unknown language code '%s', falling back to cn",
    "Unexpected extra argument: %s",
    "A non-negative integer is required: %s",
    "A non-negative number is required: %s",
    "Negative values are not allowed: %s",
    "Minimum must not be greater than maximum (%s > %s)",
    "Expected a directory, got a file: %s",
    "Expected a file, got a directory: %s",
    "No matches found"
};

/* ------------------------------------------------------------------ */
/* 日本語（日语包）/ Japanese pack                                      */
/* ------------------------------------------------------------------ */
static const LangPack JA = {
    "ja", "日本語",
    "FileTgie — 軽量ファイル統計・検索ツール",
    "使い方: FileTgie [--lang cn|en|ja|ru] [--exclude <ワイルドカード>] [--exclude-ext <拡張子>] <コマンド> [引数...]   help で全コマンドを表示",
    "利用可能なコマンド:",
    "言語",
    "不明なコマンド「%s」。help で一覧を確認してください",
    "引数が不足しています。使い方: %s",
    "パスが存在しません: %s",
    "フォルダが存在しません: %s",
    "読み取れません: %s",
    "エラー: %s",
    "数値が必要です: %s",
    /* labels */
    "ファイル", "フォルダ", "サイズ", "バイト", "最終更新", "作成", "アクセス",
    "行数（ぎょうすう）", "単語", "文字", "拡張子", "種別", "合計", "一致", "空っぽ",
    "同一", "異なる", "処理中...", "完了", "エンコーディング",
    "パス", "件数", "なし", "閾値", "日", "制限値（せいげんち）", "利用可能",
    "ディスク", "空き", "使用中", "不明", "テキスト", "バイナリ", "ドライブ",
    /* command help */
    "このヘルプを表示",
    "バージョンを表示",
    "対応言語を一覧表示",
    "ファイル/フォルダの詳細を表示",
    "ファイルとフォルダを再帰的に数える",
    "単一ファイルの行数を数える",
    "テキストファイルの合計行数を再帰的に数える",
    "ファイルサイズを表示(人間可読)",
    "フォルダ全体のサイズを集計",
    "ディレクトリツリーを出力",
    "名前のワイルドカードで検索(* ?)",
    "拡張子でファイルを検索",
    "ファイル内容からテキストを検索",
    "空ファイル・空フォルダを探す",
    "指定サイズ(MB)より大きいファイルを探す",
    "N 日以上更新されていないファイルを探す",
    "直近 N 日に更新されたファイルを探す",
    "ファイルの CRC32 を計算",
    "サイズ+CRC32 で重複ファイルを探す",
    "エンコーディングを判定(BOM/UTF-8/UTF-16/ANSI)",
    "拡張子からファイル種別を推定",
    "フォルダ内容を一覧表示",
    "2つのファイルをバイト単位で比較",
    "先頭 N 行を表示",
    "末尾 N 行を表示",
    "行/単語/文字数を数える",
    "ディスク容量と空きを表示",
    "最近更新された N 件を表示",
    "拡張子ごとのファイル数を集計",
    "拡張子ごとの使用サイズを集計",
    "全ファイルと CRC32 を一覧表示",
    "パスの存在(ファイル/フォルダ)を判定",
    "テキストかバイナリかを判定",
    "最長行の長さを求める",
    "空行数を数える",
    "文字数を数える(UTF-8 対応)",
    "PATH から実行ファイルを探す",
    /* expanded: statistics */
    "最新更新されたファイルを表示",
    "最も古いファイルを表示",
    "平均ファイルサイズ",
    "サイズ降順で一覧",
    "種別(画像/動画/音声/文書/圧縮/コード)ごとに集計",
    "隠しファイルを一覧",
    "読み取り専用ファイルを一覧",
    /* expanded: lines */
    "空でない行数を数える",
    "テキストに一致する行数を再帰集計",
    "平均行長",
    "テキストを含む行番号を表示",
    "漢字(CJK)文字数を数える",
    /* expanded: search */
    "正確なファイル名で検索",
    "別フォルダの同名ファイルを探す",
    "今日更新されたファイルを探す",
    "サイズ範囲(MB)で検索",
    "複数拡張子で検索(カンマ区切り)",
    "フルパスの部分一致で検索",
    /* v1.0.1 localized output */
    "FileTgie v%s  (C) 2026  Windows / Linux 対応、純粋 C",
    "種別", "サイズ", "名前",
    "CRC32:",
    "--- 重複ファイル ---",
    "最長行の長さ: %zu",
    "ファイル: %lld  合計: %lld %s  平均: %s",
    "画像", "動画", "音声", "文書", "アーカイブ", "コード", "その他",
    "空でない行数: %lld",
    "平均行長: %.2f",
    "%d: %s",
    "CJK 文字: %lld",
    "(なし)",
    /* logging */
    "自身の実行ログを表示",
    "自身の実行ログを削除(FileTgie のログのみ)",
    "ログは空です",
    "ログを削除しました",
    "ログファイルが存在しません",
    /* v0.1.0 boundary hardening */
    "不明なオプション「%s」。help で一覧を確認してください",
    "オプション %s に値がありません",
    "オプション %s の値は空にできません",
    "警告: 不明な言語コード「%s」、cn にフォールバックします",
    "予期しない余分な引数: %s",
    "非負整数が必要です: %s",
    "非負の数値が必要です: %s",
    "負の値は許可されません: %s",
    "最小値は最大値を超えてはいけません（%s > %s）",
    "フォルダが必要ですが、ファイルです: %s",
    "ファイルが必要ですが、フォルダです: %s",
    "一致するものが見つかりません"
};

/* ------------------------------------------------------------------ */
/* Русский（俄语包）/ Russian pack                                      */
/* ------------------------------------------------------------------ */
static const LangPack RU = {
    "ru", "Русский",
    "FileTgie — лёгкий помощник для статистики и поиска файлов",
    "Использование: FileTgie [--lang cn|en|ja|ru] [--exclude <шаблон>] [--exclude-ext <расширение>] <команда> [арг...]   help — список команд",
    "Доступные команды:",
    "Язык",
    "Неизвестная команда '%s'; наберите help для списка",
    "Недостаточно аргументов. Использование: %s",
    "Путь не найден: %s",
    "Папка не найдена: %s",
    "Невозможно прочитать: %s",
    "Ошибка: %s",
    "Требуется число: %s",
    /* labels */
    "Файл", "Папка", "Размер", "Байт", "Изменён", "Создан", "Открыт",
    "Строк", "Слов", "Символов", "Расширение", "Тип", "Итого", "Найдено",
    "Пусто", "Идентичны", "Различаются", "Обработка...", "Готово", "Кодировка",
    "Путь", "Кол-во", "Нет", "Порог", "дней", "Лимит", "доступно",
    "Диск", "Свободно", "Занято", "Неизвестно", "Текст", "Двоичный", "Диск",
    /* command help */
    "Показать эту справку",
    "Показать версию",
    "Список языков",
    "Подробная информация о файле/папке",
    "Рекурсивно посчитать файлы и папки",
    "Посчитать строки одного файла",
    "Рекурсивно посчитать строки текстовых файлов",
    "Размер файла (читаемо)",
    "Суммарный размер папки",
    "Дерево каталогов",
    "Поиск по шаблону имени (* ?)",
    "Поиск по расширению",
    "Поиск текста внутри файлов",
    "Найти пустые файлы и папки",
    "Файлы больше порога (МБ)",
    "Файлы без изменения N дней",
    "Файлы, изменённые за N дней",
    "CRC32 файла",
    "Дубликаты по размеру+CRC32",
    "Определить кодировку",
    "Тип файла по расширению",
    "Список содержимого папки",
    "Побайтовое сравнение двух файлов",
    "Первые N строк",
    "Последние N строк",
    "Строки / слова / символы",
    "Ёмкость и свобода диска",
    "N последних изменённых файлов",
    "Файлы по расширениям",
    "Размер по расширениям",
    "Файлы и их CRC32",
    "Проверить существование пути",
    "Текст или двоичный файл",
    "Длина самой длинной строки",
    "Посчитать пустые строки",
    "Посчитать символы (UTF-8)",
    "Найти программу в PATH",
    /* expanded: statistics */
    "Самый свежий файл",
    "Самый старый файл",
    "Средний размер файла",
    "Файлы по размеру (убывание)",
    "По категориям (изображения/видео/аудио/док/архив/код)",
    "Скрытые файлы",
    "Файлы только для чтения",
    /* expanded: lines */
    "Непустые строки",
    "Рекурсивно посчитать совпавшие строки",
    "Средняя длина строки",
    "Номера строк с текстом",
    "Посчитать CJK-иероглифы",
    /* expanded: search */
    "Точное имя файла",
    "Одноимённые файлы в разных папках",
    "Изменённые сегодня файлы",
    "Файлы в диапазоне размера (МБ)",
    "Несколько расширений через запятую",
    "Подстрока в полном пути",
    /* v1.0.1 localized output */
    "FileTgie v%s  (C) 2026  для Windows / Linux, чистый C",
    "Тип", "Размер", "Имя",
    "CRC32:",
    "--- дубликаты ---",
    "длина самой длинной строки: %zu",
    "файлов: %lld  всего: %lld %s  среднее: %s",
    "изображения", "видео", "аудио", "документы", "архивы", "код", "прочее",
    "непустых строк: %lld",
    "средняя длина строки: %.2f",
    "%d: %s",
    "CJK-иероглифов: %lld",
    "(нет)",
    /* logging */
    "Показать собственный журнал",
    "Удалить собственный журнал (только лог FileTgie)",
    "Журнал пуст",
    "Журнал удалён",
    "Файл журнала не найден",
    /* v0.1.0 boundary hardening */
    "Неизвестный параметр '%s'; наберите help для списка",
    "Не задано значение параметра '%s'",
    "Значение параметра '%s' не может быть пустым",
    "предупреждение: неизвестный код языка '%s', переход на cn",
    "Лишний неожиданный аргумент: %s",
    "Требуется неотрицательное целое число: %s",
    "Требуется неотрицательное число: %s",
    "Отрицательные значения запрещены: %s",
    "Минимум не может быть больше максимума (%s > %s)",
    "Ожидалась папка, получен файл: %s",
    "Ожидался файл, получена папка: %s",
    "Ничего не найдено"
};

/* 全部语言包的注册表，顺序即 lang_list_all 的输出顺序。
 * Registry of all packs; order here is the langs command's output order. */
static const LangPack* PACKS[] = { &ZH, &EN, &JA, &RU };
#define N_PACKS (int)(sizeof(PACKS)/sizeof(PACKS[0]))

/* ASCII 大小写不敏感的语言码比较（局部实现，避免引入额外依赖）。
 * ASCII case-insensitive code comparison (local, dependency-free). */
static int ci_eq(const char* a, const char* b) {
    for (; *a && *b; a++, b++) {
        char ca = (char)tolower((unsigned char)*a);
        char cb = (char)tolower((unsigned char)*b);
        if (ca != cb) return 0;
    }
    return *a == '\0' && *b == '\0';
}

/* 按语言码取包：NULL 或未命中都回退默认中文包。
 * Pick a pack by code; NULL or unknown falls back to the Chinese default. */
const LangPack* lang_get(const char* code) {
    if (!code) return &ZH;
    for (int i = 0; i < N_PACKS; i++)
        if (ci_eq(PACKS[i]->code, code)) return PACKS[i];
    return &ZH;
}

/* 逐行打印所有语言码与本地显示名（langs 命令）。
 * Print every language code and its native name, one per line. */
void lang_list_all(void) {
    for (int i = 0; i < N_PACKS; i++)
        printf("  %s  —  %s\n", PACKS[i]->code, PACKS[i]->name);
}
