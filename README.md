# FileTgie v0.1.0

轻量、只读、零依赖的文件统计与查找辅助工具。纯 C 编写，同一套源码同时编译为

**Windows** 与 **Linux** 可执行文件。



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

本次 `0.1.0` 是重新计数后的**首个版本**。版本号唯一来源是源码
`src/commands.h` 中的 `FILETGIE_VERSION`，`version` 命令与帮助横幅都读取
它，不在其他位置硬编码。

## 构建

### Windows（MinGW gcc）

双击或在命令行执行：



```
build.bat
```

等价于在项目根目录执行：

```
gcc -O2 -Wall -o FileTgie.exe src\main.c src\commands.c src\port.c src\util.c src\i18n.c
```

产出 `FileTgie.exe`，要求 `-O2 -Wall` 零警告。

### Linux（gcc / clang）



```
make
```

产出 `FileTgie`。`Makefile` 仅保持结构正确，未在本机 Linux 实机编译验证。

源码全部位于 `src/`，仅使用 C 标准库与平台原生 API，无第三方依赖。

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

## 命令一览（57 个；除 `log` / `dellog` 只读写 FileTgie 自身日志外，全部只读）



| 命令          | 用法                      | 作用                          |
| ----------- | ----------------------- | --------------------------- |
| help        | `help`                  | 显示帮助                        |
| version     | `version`               | 显示版本                        |
| langs       | `langs`                 | 列出可用语言                      |
| log         | `log`                   | 打印 FileTgie 自身运行日志            |
| dellog      | `dellog`                | 删除 FileTgie 自身运行日志(只删本工具日志) |
| stat        | `stat <路径>`             | 文件 / 目录详细信息（大小、三个时间）        |
| count       | `count <目录>`            | 递归统计文件数与文件夹数                |
| lines       | `lines <文件>`            | 统计单个文件行数                    |
| linesdir    | `linesdir <目录>`         | 递归统计文本文件总行数                 |
| size        | `size <文件>`             | 人类可读文件大小                    |
| du          | `du <目录>`               | 汇总目录总大小                     |
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

# 校验两个文件是否完全一致
FileTgie compare a.bin b.bin

# 看磁盘空间
FileTgie disk
```

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

* **绿色便携包**（`FileTgie.windows.0.1.0.portable.zip`）：仅含
  `FileTgie.exe`、`README.md`、`LICENSE`、`COPYING`。工具本身完全便携：
  不写注册表、不写入 Program Files，除 exe 旁自身的 `filetgie.log` 外不写
  任何位置，解压即用、删文件夹即卸载。

* **安装程序**：提供 Inno Setup 6 脚本 `installer\installer.iss`，本发布已用
  Inno Setup 6.7.3 编译出真实安装包 `FileTgie.windows.0.1.0.setup.exe`。脚本含
  LGPL 许可页（`LicenseFile=..\LICENSE`）、可选开始菜单（默认勾选）/桌面（默认
  不勾选）快捷方式复选框与标准卸载项；默认安装目录 `{autopf}\FileTgie`，单文件
  exe 部署；图标占位 `assets\icon.ico`，用 `#if FileExists` 条件包裹，文件缺失时
  脚本自动跳过图标引用，仍可直接编译。需要自行重建时，先安装
  [Inno Setup 6](https://jrsoftware.org/isdl.php)，右键 `installer\installer.iss`
  编译，或命令行执行 `iscc installer\installer.iss`，即可重新产出
  `FileTgie.windows.0.1.0.setup.exe`。

## 设计说明



* **只读安全**：所有命令只调用读取接口（枚举、stat、fread），不修改、删除、移动用户文件；唯一的写入是 `log` / `dellog` 对可执行文件旁 `filetgie.log`（FileTgie 自身日志）的追加与删除，绝不触碰任何其他文件。

* **运行日志**：每次调用都会向 exe 旁的 `filetgie.log` 追加一条记录（本地时间戳、语言码、命令、参数、退出码）；`log` 查看、`dellog` 清空。日志路径在程序内统一计算，不依赖当前工作目录。

* **大小单位（1024 二进制标准）**：B → KB → MB → GB 均按 1024 进位（1 KB = 1024 B，1 MB = 1024 KB），未到上一级单位时保持当前单位；`size/stat/du` 同时给出精确字节数。

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
├─ Makefile          # Linux 构建（结构保留，未在 Linux 实机验证）
├─ README.md
├─ LICENSE           # LGPL-3.0 完整官方文本
├─ COPYING           # LGPL 通俗中文说明（动态/静态链接）
├─ NOTICE            # 版权与许可声明
├─ installer/
│  └─ installer.iss  # Inno Setup 6 脚本（缺图标容错）
└─ src/
   ├─ main.c         # 入口、--lang/--exclude 参数解析、分发、运行日志
   ├─ commands.c    # 57 个子命令实现（含 log / dellog）
   ├─ commands.h    # 命令表与 FILETGIE_VERSION 唯一版本来源
   ├─ i18n.c        # cn/en/ja/ru 四语言字符串包
   ├─ i18n.h
   ├─ port.c        # 平台抽象（Windows/Linux）、日志路径解析
   ├─ port.h
   ├─ util.c        # CRC32、通配符、编码检测、大小格式化、排除过滤器
   └─ util.h
```
# FileTgie
