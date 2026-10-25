# FileTgie v0.4.1

> A lightweight, read-only, zero-dependency file statistics and search CLI written in pure C.
> One source tree builds for Windows and Linux; every command only reads and never modifies your files.

轻量、只读、零依赖的文件统计与查找辅助工具。纯 C 编写，同一套源码同时编译为

**Windows** 与 **Linux** 可执行文件。

## Download

| Package | Link |
| --- | --- |
| Official website | https://suiye-18.github.io/filetgie-official-website/ |
| Windows installer | [FileTgie.windows.0.4.1.setup.exe](https://github.com/SuiYe-18/FileTgie/releases/latest/download/FileTgie.windows.0.4.1.setup.exe) |
| Windows portable ZIP | [FileTgie.windows.0.4.1.portable.zip](https://github.com/SuiYe-18/FileTgie/releases/latest/download/FileTgie.windows.0.4.1.portable.zip) |
| Linux x86_64 glibc | Verified unchanged [v0.4.0 tarball](https://github.com/SuiYe-18/FileTgie/releases/download/v0.4.0/FileTgie.linux.0.4.0.tar.gz); other distributions build from source with `make` |
| Source archive | [FileTgie.source.0.4.1.zip](https://github.com/SuiYe-18/FileTgie/releases/latest/download/FileTgie.source.0.4.1.zip) |

## Highlights

- **61 built-in commands** for counting, line statistics, directory comparison, file search, checksums, encoding checks, duplicates, disk space, verification and optional update reminders.
- **Read-only by default:** every inspection command only reads files; it never modifies, deletes or moves user data. The explicit `update` command and bundled `FileTgieUpdater.exe` only upgrade FileTgie itself; `reminder` creates or removes one FileTgie-owned scheduled task; on Linux they print manual steps and never replace the binary.
- **Pure C, zero third-party dependency:** C standard library plus native Windows/POSIX APIs only.
- **Four interface languages:** switch messages with `--lang cn|en|ja|ru`.
- **Portable and small:** no runtime package, no configuration file, and no required network access.
- **LGPL-3.0-or-later:** free to run, study, modify and redistribute.

Common uses: file counting, line counting, directory size, filename/content search, duplicate detection, byte comparison, CRC32 checksums, encoding detection, hidden/read-only file lists and disk space overview.

```bash
FileTgie count ./src
FileTgie --lang en linesdir ./src
FileTgie findlarge ./logs 50
FileTgie compare a.bin b.bin
FileTgie --lang en checkupdate
FileTgie --lang en update
FileTgie --lang en reminder on
FileTgie --lang en reminder on every 5
FileTgie --lang en reminder status
FileTgie --lang en reminder off
```

* 核心理念：**一步一步慢慢来** —— 每个命令只做一件小事，结果清晰、可预期。

* 核心承诺：统计清明、只读安全，**不修改、不删除、不移动**任何用户文件，因此不会出现误操作错误。

* 界面：黑窗口（控制台），全部通过**命令行参数**执行。

* 语言：中文 `cn` / 英文 `en` / 日文 `ja` / 俄文 `ru`，用参数切换。

## 作者

* **作者（单作者）**：SuiYe_TR
* **GitHub**：[Suiye-18](https://github.com/Suiye-18)

## 版本制度

版本号格式为 **`major.minor.patch`**（三段数字，点分隔）：

* **major**：重大不兼容重构位，本次重新计数从 `0` 开始。
* **minor**：普通修改位（新增命令、行为调整、常规增强），从 `0` 起计，
  累加至 `9` 后进一位，即 `1.9` 的下一个普通版本为 `2.0`，而不是 `1.10`。
* **patch（.x）**：只用于小补丁、小插曲（修小问题、文案微调），不进入
  minor 的进位序列。

当前版本为 `0.4.1`：这是 `0.4.0` 的 Windows 补丁版，保留 1 分钟缺省只读检查、
`reminder on every N`（1–1439 分钟自定义间隔）、`daily HH:MM` 与 `hourly`，以及按
版本号去重的系统通知。本次只改进独立更新器的前台下载体验：下载开始前显示白色 `=`
进度条，已完成字节显示绿色 `>`，剩余部分保持白色 `=`；临时文件、无效 URL、网络会话、
连接、无响应、HTTP 状态、重定向与磁盘写入失败都会输出明确错误，不再只显示笼统失败。
`checkupdate` 只查询 GitHub Release 并比较版本；Windows 的 `update` 调用独立更新器，
同时检查 GitHub 与官网，后台下载新版安装包并静默覆盖；Linux 只给出手动源码升级
命令，不替换二进制。安装包会检测已安装 FileTgie 或旧版残留，并提供“覆盖安装”勾选；
覆盖前把旧的主程序、更新器和文档移动到安装目录的 `.old` 备份文件夹，新文件留在
根目录，`.old` 不注册 PATH、不进入快捷方式，也不会被启动。
版本号唯一来源是源码 `src/commands.h` 中的 `FILETGIE_VERSION`，`version`
命令与帮助横幅都读取它，不在其他位置硬编码。

## 构建

### Windows（MinGW gcc）

双击或在命令行执行：



```
build.bat
```

等价于在项目根目录执行：

```
gcc -O2 -Wall -Wextra -o FileTgie.exe src\main.c src\commands.c src\port.c src\util.c src\i18n.c src\update.c -lwinhttp -lshell32
gcc -O2 -Wall -Wextra -o FileTgieUpdater.exe src\updater_main.c -lwinhttp -lshell32
```

`build.bat` 同时产出 `FileTgie.exe` 与 Windows 专用 `FileTgieUpdater.exe`，要求 `-O2 -Wall -Wextra` 零警告；Linux `make` 不构建 Windows 更新器。

### Linux（gcc / clang）



```
make
```

产出 `FileTgie`。源码全部位于 `src/`，仅使用 C 标准库与平台原生 API，无第三方依赖。

#### Linux 工具链口径

* 历史实机验证：GCC 11.4.0（Ubuntu 22.04 / x86_64 / glibc）曾以
  `-O2 -Wall -Wextra` 零警告编译，并在 C99/GNU99/C11/GNU11/GNU17 标准下通过。
* 预期可编译：支持 C99 的 GCC 5+ 与现代 glibc；v0.4.0 的 x86_64 glibc 预编译包
  已实机验证，v0.4.1 未改动 Linux 平台代码，未逐版本实测更低 GCC。
* musl/Alpine 不保证；其他发行版直接 `make` 从源码构建。Clang 本次未实测。

## 通用用法



```
FileTgie [--lang cn|en|ja|ru] [--exclude <通配符>] [--exclude-ext <扩展名>] <命令> [参数...]
```



* `--lang`（也可写 `-lang`、`-l`，支持 `--lang=en`、`-l=ja` 等号写法，语言码大小写不敏感）可放在命令前或命令后；无法识别的语言码会给出本地化警告并回退默认 `cn`，程序继续运行。

* `--exclude <通配符>` / `--exclude=<通配符>`：从所有目录遍历结果中跳过文件名匹配通配符（`*` `?`，对 basename 大小写不敏感）的文件；可重复、可放在命令前后。

* `--exclude-ext <扩展名>` / `--exclude-ext=<扩展名>`：跳过指定扩展名的文件，带不带前导点等价（`.c` 与 `c` 相同）、大小写不敏感；可重复。两个排除参数只作用于遍历型命令，显式指定单个文件的命令（如 `lines`、`hash`）不受影响。

* 不带任何参数或输入 `help` 显示完整命令表（退出码 0）。

* 语言示例：



```
FileTgie --lang en count D:\docs
FileTgie -lang ja lines ./a.txt
FileTgie -l ru disk
```

## 命令一览（61 个；检查/统计类命令只读；`log` / `dellog` 只读写 FileTgie 自身日志；`update` 仅按用户显式触发更新 FileTgie 自身；`reminder` 只创建/删除 FileTgie 自己的计划任务）



| 命令          | 用法                      | 作用                          |
| ----------- | ----------------------- | --------------------------- |
| help        | `help`                  | 显示帮助                        |
| version     | `version`               | 显示版本                        |
| langs       | `langs`                 | 列出可用语言                      |
| log         | `log`                   | 打印 FileTgie 自身运行日志            |
| dellog      | `dellog`                | 删除 FileTgie 自身运行日志(只删本工具日志) |
| checkupdate | `checkupdate`           | 查询 GitHub 最新 Release 并比较版本，只报告不安装 |
| update      | `update`                | Windows 调用独立更新器检查 GitHub/官网并后台覆盖；Linux 只打印手动升级步骤 |
| reminder    | `reminder on [every N|daily HH:MM|hourly]` / `reminder status` / `reminder off` | Windows 开启/查询/删除可选的用户级定时更新检查提醒；缺省每 1 分钟，`every N` 为 1–1439 分钟，每个新版本只通知一次；Linux 只打印手动 cron 说明 |
| stat        | `stat <路径>`             | 文件 / 目录详细信息（大小、三个时间）        |
| count       | `count <目录>`            | 递归统计文件数与文件夹数                |
| lines       | `lines <文件>`            | 统计单个文件行数                    |
| linesdir    | `linesdir <目录>`         | 递归统计文本文件总行数                 |
| size        | `size <文件>`             | 人类可读文件大小                    |
| du          | `du <目录>`               | 汇总目录总大小                     |
| dirdiff     | `dirdiff <目录A> <目录B>`    | 分组对比两个目录的文件数、目录数、大小、文本文件数、文本行数、隐藏文件数与平均文件大小；差值列为 B-A，`+` 更多、`-` 更少、`=0` 无变化 |
| tree        | `tree <目录> [深度]`        | 打印目录树（深度缺省 20，允许 0）         |
| list        | `list <目录>`             | 列出目录内容（大小 / 时间 / 名称）        |
| findname    | `findname <目录> <通配符>`   | 按文件名查找，支持 `*` `?`           |
| findext     | `findext <目录> <扩展名>`    | 按扩展名查找（如 `txt`）             |
| findcontent | `findcontent <目录> <文本>` | 在文件内容中搜索文本并给出行号             |
| findempty   | `findempty <目录>`        | 查找空文件                       |
| findlarge   | `findlarge <目录> <MB>`   | 查找大于阈值 (MB，支持小数如 0.5) 的文件，按大小降序、最大在前 |
| findold     | `findold <目录> <天数>`     | 查找超过 N 天未修改的文件（天数支持小数）   |
| findnew     | `findnew <目录> <天数>`     | 查找最近 N 天内修改的文件（天数支持小数）   |
| hash        | `hash <文件>`             | 计算 CRC32 校验值                |
| duplicates  | `duplicates <目录>`       | 按大小 + CRC32 查找重复文件          |
| checksum    | `checksum <目录>`         | 列出目录内所有文件及其 CRC32           |
| encoding    | `encoding <文件>`         | 检测编码（BOM/UTF-8/UTF-16/ANSI） |
| type        | `type <文件>`             | 按扩展名猜测文件类型                  |
| istext      | `istext <文件>`           | 判断文本 / 二进制                  |
| exists      | `exists <路径>`           | 判断路径存在（文件 / 目录 / 不存在）       |
| compare     | `compare <文件A> <文件B>`   | 逐字节比较，报告首个差异偏移              |
| head        | `head <文件> [N]`         | 显示前 N 行（默认 10）              |
| tail        | `tail <文件> [N]`         | 显示末尾 N 行（默认 10）             |
| wc          | `wc <文件>`               | 统计行 / 词 / 字符                |
| chars       | `chars <文件>`            | UTF-8 感知的字符数                |
| longest     | `longest <文件>`          | 最长行长度                       |
| blank       | `blank <文件>`            | 统计空行                        |
| disk        | `disk`                  | 各磁盘容量、已用、剩余                 |
| recent      | `recent <目录> [N]`       | 最近修改的 N 个文件（默认 10）          |
| extstats    | `extstats <目录>`         | 按扩展名统计文件数量                  |
| extsize     | `extsize <目录>`          | 按扩展名汇总占用大小                  |
| whereis     | `whereis <程序名>`         | 在 PATH 中查找可执行程序             |

### 扩充：文件统计

| 命令 | 用法 | 作用 |
| --- | --- | --- |
| newest | `newest <目录>` | 列出最近修改的文件 |
| oldest | `oldest <目录>` | 列出最久未修改的文件 |
| avgsize | `avgsize <目录>` | 计算平均文件大小 |
| sizesort | `sizesort <目录> [N]` | 按大小降序列出前 N 个文件 |
| typecount | `typecount <目录>` | 按大类（图片/视频/音频/文档/压缩/代码）统计数量 |
| hidden | `hidden <目录>` | 列出隐藏文件 |
| readonly | `readonly <目录>` | 列出只读文件 |

### 扩充：行数统计

| 命令 | 用法 | 作用 |
| --- | --- | --- |
| nonblank | `nonblank <文件>` | 统计非空行数 |
| matchlines | `matchlines <目录> <文本>` | 递归统计匹配文本的行数 |
| avglen | `avglen <文件>` | 计算平均行长度 |
| findline | `findline <文件> <文本>` | 输出含文本的行号与内容 |
| cjk | `cjk <文件>` | 统计中日韩汉字字符数 |

### 扩充：快速查找

| 命令 | 用法 | 作用 |
| --- | --- | --- |
| findfile | `findfile <目录> <精确文件名>` | 按精确文件名查找 |
| dupenames | `dupenames <目录>` | 查找不同目录下的同名文件 |
| today | `today <目录>` | 查找今天修改过的文件 |
| findrange | `findrange <目录> <最小MB> <最大MB>` | 按大小区间(MB)查找文件，边界支持小数（如 0.4 0.6），且最小 ≤ 最大 |
| findexts | `findexts <目录> <ext1,ext2,...>` | 按多个扩展名查找（逗号分隔） |
| findpath | `findpath <目录> <子串>` | 按完整路径子串查找 |

## 常用示例



```
# 统计某文件夹里有多少文件
FileTgie count D:\Photos

# 统计一个代码库的总行数
FileTgie --lang en linesdir ./src

# 找所有 .log 大文件
FileTgie findlarge D:\app\logs 50

# 在项目里搜一段文字
FileTgie findcontent ./src "TODO"

# 统计时跳过所有 .o 文件与 .c 源文件
FileTgie count ./src --exclude=*.o --exclude-ext c

# 比较两个目录的文件数、大小与文本行数
FileTgie --lang en dirdiff ./old ./new

# 校验两个文件是否完全一致
FileTgie compare a.bin b.bin

# 看磁盘空间
FileTgie disk
```

## Optional companion: fd

FileTgie does not call, bundle or depend on fd. fd is a separate fast filename-search tool that can locate candidates and use its own `-x` / `-X` options for batch execution, including delete/move operations. Those actions belong to fd or the shell; FileTgie itself remains read-only and never deletes or moves user files.

Safe inspection pairs fd with FileTgie after the candidates are located:

```bash
fd -e c -x FileTgie hash
fd -e txt -x FileTgie lines
```

Use `fd -u` when the candidate set must include hidden files and paths ignored by `.gitignore`; FileTgie's own traversal still applies its read-only statistics independently.

## 健壮性与边界处理

0.1.0 对命令行与输入做了工业级边界加固，所有提示均为四语言本地化：

* **全局选项**：不认识的 `-`/`--` 开头 token（如 `--foo`、`-x`）按"未知选项"
  报错并指向 help，退出码 1，不会被误当成未知命令；`--lang` / `--exclude` /
  `--exclude-ext` 缺少跟随值、或 `=` 后留空（`--lang=`）都会报"缺少选项值 /
  值不能为空"，rc=1；无效语言码（非 cn/en/ja/ru）打印警告后回退 cn 继续运行。
  选项仍可出现在命令前或命令后，`--exclude` / `--exclude-ext` 可重复。

* **参数个数**：固定参数命令多给尾随参数时报"意外的多余参数"，rc=1；
  `tree/head/tail/recent/sizesort` 的可选 N 缺省时用默认值，多给参数报错。

* **数值严格解析**：整数 N（树深、head/tail/recent/sizesort）与小数阈值
  （findlarge 的 MB、findold/findnew 的天数、findrange 的区间）全部用
  endptr 校验：空串、非数字、负数、小数混入整数位、NaN/Inf、溢出都给
  本地化友好错误，rc=1，绝不当成 0 静默继续；`findrange` 额外校验
  最小 ≤ 最大，否则报"最小值不能大于最大值"。

* **路径角色**：缺路径给用法；路径不存在按文件/目录分别报"路径不存在 /
  目录不存在"；把文件传给目录命令报"需要目录，实际是文件"，把目录传给
  文件命令报"需要文件，实际是目录"，不再用同一句"目录不存在"掩盖；
  存在但无权读取（锁定/权限拒绝）报"无法读取"，不崩溃、不静默输出 0。
  宽字符层完整支持空格、中文、日文、俄文、特殊符号、相对/绝对路径、
  尾斜杠与盘符根。

* **空目录 / 空文件 / 无匹配**：空目录是合法输入，遍历命令无文件或无匹配时
  打印一行本地化"未找到匹配项"，rc=0（newest/oldest/avgsize 原先无结果
  返回 1，已统一为 rc=0）；所有除法（avgsize、avglen）先判 0；0 字节文件的
  行数/词/字符/最长行/空行/CJK 等输出 0 或"无"，正常返回。

* **内存与退出码**：所有字符串用 `snprintf` 截断；calloc/malloc/realloc
  返回值均检查，失败走清理与友好错误；退出码统一为 0（成功）、1（用法 /
  未找到 / 参数错误）。

## 便携包与安装程序

* **Windows 绿色便携包**（`FileTgie.windows.0.4.1.portable.zip`）：含
  `FileTgie.exe`、`FileTgieUpdater.exe`、`README.md`、`User Manual.txt`、`LICENSE`、
  `COPYING`。便携模式不写注册表、不写入 Program Files，除 exe 旁自身的
  `filetgie.log` 外不写用户位置，解压即用、删文件夹即卸载。

* **Linux 构建**：源码包在 Linux 执行 `make` 即可生成 `FileTgie`；检查与统计
  命令为 POSIX 只读路径，`update` / `reminder` 在 Linux 只打印手动步骤，不替换
  二进制、不修改 crontab。v0.4.1 只改动 Windows 专用更新器，Linux 代码保持不变；
  已验证的 x86_64 glibc 预编译包仍为 v0.4.0 tarball，其他发行版或新版本请用
  `make` 从源码构建。

* **安装程序**：提供 Inno Setup 6 脚本 `installer\installer.iss`，编译出
  `FileTgie.windows.0.4.1.setup.exe`。脚本含 LGPL 许可页、可选开始菜单（默认勾选）
  /桌面（默认不勾选）快捷方式、标准卸载项，以及默认勾选的“覆盖安装”选项；安装前
  检测已注册的 FileTgie、App Paths 与旧版残留文件，不勾选覆盖时需另选目录。覆盖时
  先把旧的 `FileTgie.exe`、`FileTgieUpdater.exe` 和文档移动到安装目录 `.old` 备份，
  再把新文件复制到根目录；注册表、PATH 和快捷方式始终指向根目录新文件，`.old`
  不启用。默认安装目录 `{autopf}\FileTgie`，部署主程序、独立更新器并注册 App Paths
  与系统 PATH。需要自行重建时，先安装 [Inno Setup 6](https://jrsoftware.org/isdl.php)，
  右键 `installer\installer.iss` 编译，或执行 `iscc installer\installer.iss`。

## 设计说明



* **只读安全**：检查与统计命令只调用读取接口（枚举、stat、fread），不修改、删除、移动用户文件；`log` / `dellog` 只追加与删除可执行文件旁 `filetgie.log`（FileTgie 自身日志）。显式 `update` 只更新 FileTgie 自身：Windows 下载并启动安装包，旧程序文件备份到 `.old` 后由新文件接管，Linux 只打印手动升级步骤、不替换二进制。`reminder` 只创建或删除名为 `FileTgieReminder` 的用户级计划任务，不修改用户文件；`reminder off` 会移除该任务。

* **运行日志**：每次调用都会向 exe 旁的 `filetgie.log` 追加一条记录（本地时间戳、语言码、命令、参数、退出码）；`log` 查看、`dellog` 清空。日志路径在程序内统一计算，不依赖当前工作目录。

* **大小单位（1024 二进制标准）**：B → KB → MB → GB 均按 1024 进位（1 KB = 1024 B，1 MB = 1024 KB），未到上一级单位时保持当前单位；`size/stat/du/dirdiff` 同时给出精确字节数。

* **跨平台**：`src/port.c` 隔离 Windows（宽字符 Win32 API，正确处理中日俄文件名）与 Linux（POSIX）差异；其余代码平台无关。

* **CRC32**：内置标准 CRC32 查表实现，用于 `hash / duplicates / checksum`，可独立校验。

* **编码**：`encoding` 识别 UTF-8 BOM、UTF-16 LE/BE、合法 UTF-8/ASCII，其余归为本地代码页（如 GBK）。

## 许可证

FileTgie 采用 **LGPL-3.0-or-later**（GNU Lesser General Public License
第 3 版或更新版），版权所有 (C) 2026 SuiYe_TR（GitHub: Suiye-18）；完整法律条款见
`LICENSE`，通俗说明见 `COPYING`。

* **用户自由**：可自由运行、研究、修改与再分发；动态链接场景下闭源专有
  程序也可使用。
* **主要义务**：修改过的库文件需标注修改处并继续以 LGPL 发布；再分发须
  随附许可证文本并保持 LGPL；静态链接分发需提供可替换目标文件 / 重新链接
  所需的源码或目标文件手段。
* **动态 / 静态链接**：动态链接（DLL/.so，运行时加载、可替换）对使用方最
  宽松；静态链接（代码编入同一可执行文件）必须提供用户可替换库、可重新
  链接的手段，详见 `COPYING`。

## 目录结构



```
FileTgie/
├─ build.bat          # Windows 构建脚本
├─ Makefile          # Linux 构建（gcc -Wall -Wextra，历史实机验证）
├─ README.md
├─ LICENSE           # LGPL-3.0 完整官方文本
├─ COPYING           # LGPL 通俗中文说明（动态/静态链接）
├─ NOTICE            # 版权与许可声明
├─ installer/
│  └─ installer.iss  # Inno Setup 6 脚本（缺图标容错）
└─ src/
   ├─ main.c         # 入口、--lang/--exclude 参数解析、分发、运行日志
   ├─ commands.c    # 61 个子命令实现（含 log / dellog / checkupdate / update / reminder）
   ├─ commands.h    # 命令表与 FILETGIE_VERSION 唯一版本来源
   ├─ update.c      # GitHub Release 手动更新检测、Windows 安装包更新与可选提醒任务
   ├─ update.h
   ├─ updater_main.c # 独立 Windows 更新器：GitHub/官网双检测、后台静默覆盖
   ├─ i18n.c        # cn/en/ja/ru 四语言字符串包
   ├─ i18n.h
   ├─ port.c        # 平台抽象（Windows/Linux）、日志路径解析
   ├─ port.h
   ├─ util.c        # CRC32、通配符、编码检测、大小格式化、排除过滤器
   └─ util.h
```
