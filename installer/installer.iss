; ======================================================================
; FileTgie Inno Setup 6 installer script (source only; no setup EXE here).
;
; Prerequisite: install Inno Setup 6 (https://jrsoftware.org/isdl.php), then
; compile by right-clicking this file -> Compile, or on the command line:
;     iscc installer\installer.iss
; which produces FileTgie.windows.0.5.3.setup.exe.
;
; The optional application icon lives at assets\icon.ico (placeholder path).
; When the file is absent the script still compiles cleanly: the icon
; reference is wrapped in #if FileExists, otherwise the installer falls back
; to the executable's own icon.
;
; Elevated installation (PrivilegesRequired=admin, UAC prompt): App Paths
; and the system PATH entry live under HKLM; the PATH dedup/remove logic
; and WM_SETTINGCHANGE broadcast are managed from [Code].
; ======================================================================

#define MyAppName "FileTgie"
#define MyAppVersion "0.5.3"
#define MyAppPublisher "SuiYe_TR (Suiye-18) - Single Author"
#define MyAppExe "FileTgie.exe"

; Reference the optional icon only when it actually exists, so a missing
; assets\icon.ico never breaks compilation.
#if FileExists("..\assets\icon.ico")
  #define SetupIcon "..\assets\icon.ico"
#endif

[Setup]
AppId={{7C9E1A24-6B3D-4E2F-9C01-000000000001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppCopyright=SuiYe_TR (Suiye-18) - Single Author
; LGPL-3.0 license page shown during installation (relative to this .iss)
LicenseFile=..\LICENSE
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=".."
OutputBaseFilename=FileTgie.windows.0.5.3.setup
UninstallDisplayIcon={app}\{#MyAppExe}
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
WizardStyle=modern
#ifdef SetupIcon
SetupIconFile={#SetupIcon}
#endif

[Tasks]
; Overwrite option is checked by default for an upgrade; uncheck it and choose a
; different directory when the user wants to keep the old copy untouched.
Name: "overwrite"; Description: "覆盖安装现有 FileTgie（旧文件先备份到 .old，再替换为新文件，不删除用户数据）(&O)"; GroupDescription: "安装选项:"
; Optional shortcuts on the Select Tasks page: start menu checked by default,
; desktop unchecked.
Name: "startmenuicon"; Description: "在开始菜单创建快捷方式(&S)"; GroupDescription: "附加快捷方式:"
Name: "desktopicon"; Description: "在桌面创建快捷方式(&D)"; GroupDescription: "附加快捷方式:"; Flags: unchecked

[Files]
; Main executable, standalone background updater and documentation texts.
Source: "..\{#MyAppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\FileTgieUpdater.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\COPYING"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\User Manual.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\assets\icon.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\examples\demo.ftg"; DestDir: "{app}\examples"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExe}"; IconFilename: "{app}\icon.ico"; Tasks: startmenuicon
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExe}"; IconFilename: "{app}\icon.ico"; Tasks: desktopicon

[Registry]
; Machine-wide App Paths registration so "FileTgie" resolves from any new
; terminal without typing the full path. HKLM; uninsdeletekey removes exactly
; this key on uninstall and leaves every other key untouched.
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\FileTgie.exe"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExe}"; Flags: uninsdeletekey

[UninstallDelete]
; The tool is read-only for user files; its only write is the own filetgie.log
; next to the executable. Remove just that log on uninstall.
Type: files; Name: "{app}\filetgie.log"

[Code]
const
  WM_SETTINGCHANGE = $001A;
  SMTO_ABORTIFHUNG = $0002;
  EnvSubKey = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';

var
  ExistingInstallDir: string;
  ResidualDir: string;

function CmdLineParamExists(const Name: string): Boolean;
var
  I: Integer;
  Param: string;
begin
  Result := False;
  for I := 1 to ParamCount do
  begin
    Param := Trim(ParamStr(I));
    if SameText(Param, Name) or SameText(Param, Name + '=1') then
      Result := True;
  end;
end;

function IsResidualDir(const Dir: string): Boolean;
begin
  Result := (Dir <> '') and DirExists(Dir) and
    (FileExists(Dir + '\README.md') or
     FileExists(Dir + '\User Manual.txt') or
     FileExists(Dir + '\FileTgie.exe'));
end;

function DetectExistingInstall: string;
var
  AppPath: string;
  DefaultDir: string;
begin
  Result := '';
  if RegQueryStringValue(HKLM,
      'SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\FileTgie.exe', '', AppPath) then
    if FileExists(AppPath) then
    begin
      Result := ExtractFileDir(AppPath);
      Exit;
    end;
  if RegQueryStringValue(HKLM,
      'SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\App Paths\FileTgie.exe', '', AppPath) then
    if FileExists(AppPath) then
    begin
      Result := ExtractFileDir(AppPath);
      Exit;
    end;
  DefaultDir := ExpandConstant('{autopf}\FileTgie');
  if FileExists(DefaultDir + '\FileTgie.exe') then
    Result := DefaultDir;
end;

function InitializeSetup: Boolean;
begin
  Result := True;
  ExistingInstallDir := DetectExistingInstall;
  ResidualDir := '';

  if (ExistingInstallDir = '') and
     IsResidualDir(ExpandConstant('{autopf}\FileTgie')) then
  begin
    ResidualDir := ExpandConstant('{autopf}\FileTgie');
    MsgBox('检测到旧版 FileTgie 残留文件：' + ResidualDir + #13#10 +
           '建议覆盖安装；若要保留残留文件，请在下一步改选其他目录。',
           mbConfirmation, MB_OK);
    ExistingInstallDir := ResidualDir;
  end;

  // The overwrite task is checked by default. Silent upgrades use Inno's
  // /MERGETASKS=overwrite so the .old backup/replacement path stays explicit.
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectTasks then
  begin
    if (ExistingInstallDir <> '') and
       not WizardIsTaskSelected('overwrite') and
       SameText(Trim(WizardDirValue), Trim(ExistingInstallDir)) then
    begin
      MsgBox('检测到已安装的 FileTgie。请勾选“覆盖安装”，或改选另一个目录。',
             mbError, MB_OK);
      Result := False;
    end;
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID = wpSelectDir) and (ExistingInstallDir <> '') then
    WizardForm.DirEdit.Text := ExistingInstallDir;
end;

function AddBackslash(const S: string): string;
begin
  Result := Trim(S);
  if (Result = '') or (Result[Length(Result)] = '\') then
    Exit;
  Result := Result + '\';
end;

function HasActiveProgramFiles(const AppDir: string): Boolean;
begin
  Result := FileExists(AddBackslash(AppDir) + 'FileTgie.exe') or
    FileExists(AddBackslash(AppDir) + 'FileTgieUpdater.exe') or
    FileExists(AddBackslash(AppDir) + 'README.md') or
    FileExists(AddBackslash(AppDir) + 'User Manual.txt');
end;

// Before Inno copies the new files, move the previous program files into
// {app}\.old. The active registration, shortcuts and PATH continue to point
// at the new files in {app}; .old is only a disabled backup, never launched.
procedure BackupExistingToOld(const AppDir: string);
var
  OldDir: string;
  Src: string;
  Dst: string;
  Files: array[0..5] of string;
  I: Integer;
begin
  OldDir := AddBackslash(AppDir) + '.old';
  if not DirExists(OldDir) then
    CreateDir(OldDir);

  Files[0] := 'FileTgie.exe';
  Files[1] := 'FileTgieUpdater.exe';
  Files[2] := 'README.md';
  Files[3] := 'User Manual.txt';
  Files[4] := 'LICENSE';
  Files[5] := 'COPYING';

  for I := Low(Files) to High(Files) do
  begin
    Src := AddBackslash(AppDir) + Files[I];
    Dst := AddBackslash(OldDir) + Files[I];
    if FileExists(Src) then
    begin
      if FileExists(Dst) then
        DeleteFile(Dst);
      RenameFile(Src, Dst);
    end;
  end;

  SaveStringToFile(
    AddBackslash(OldDir) + 'README.txt',
    'Disabled backup of the previous FileTgie files.'#13#10 +
    'This .old folder is not added to PATH, not registered, and not launched.'#13#10,
    False
  );
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  AppDir: string;
begin
  Result := '';
  NeedsRestart := False;
  AppDir := ExpandConstant('{app}');
  if not DirExists(AppDir) then Exit;

  if WizardIsTaskSelected('overwrite') then
    BackupExistingToOld(AppDir)
  else if HasActiveProgramFiles(AppDir) then
    Result := 'Unchecking overwrite keeps the current files. Choose an empty folder or another directory instead of replacing this installation.';
end;

// Correct WinAPI signature: lParam carries a pointer-sized value (LongInt in
// this 32-bit installer), not a string. Declaring it as string mismatches the
// stdcall stack and causes an access violation (0xC0000005) after file copying.
function SendMessageTimeoutW(hWnd: LongInt; Msg: LongInt; wParam: LongInt;
  lParam: LongInt; fuFlags: LongInt; uTimeout: LongInt;
  var lResult: LongInt): LongInt;
  external 'SendMessageTimeoutW@user32.dll stdcall';

function NormalizePathEntry(const S: string): string;
var
  T: string;
begin
  T := Trim(S);
  while (Length(T) > 0) and (T[Length(T)] = '\') do
    Delete(T, Length(T), 1);
  Result := LowerCase(T);
end;

function ReadSystemEnvironmentPath(out Path: string): Boolean;
begin
  Result := RegQueryStringValue(HKLM, EnvSubKey, 'Path', Path);
  if not Result then
    Path := '';
end;

procedure WriteSystemEnvironmentPath(const Path: string);
begin
  RegWriteExpandStringValue(HKLM, EnvSubKey, 'Path', Path);
end;

function JoinPath(const List: TStringList): string;
var
  I: Integer;
begin
  Result := '';
  for I := 0 to List.Count - 1 do
  begin
    if Result <> '' then
      Result := Result + ';';
    Result := Result + List[I];
  end;
end;

procedure BroadcastEnvironmentChange;
var
  EnvName: string;
  Res: LongInt;
begin
  EnvName := 'Environment';
  // HWND_BROADCAST = $FFFF; lParam is the pointer to 'Environment'.
  SendMessageTimeoutW($FFFF, WM_SETTINGCHANGE, 0,
    LongInt(EnvName), SMTO_ABORTIFHUNG, 5000, Res);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Path: string;
  List: TStringList;
  I: Integer;
  Entry: string;
  AppDir: string;
  Already: Boolean;
begin
  if CurStep <> ssPostInstall then Exit;
  AppDir := ExpandConstant('{app}');
  ReadSystemEnvironmentPath(Path);
  List := TStringList.Create;
  try
    List.Delimiter := ';';
    List.StrictDelimiter := True;
    List.DelimitedText := Path;
    Already := False;
    for I := List.Count - 1 downto 0 do
    begin
      Entry := Trim(List[I]);
      if Entry = '' then
        List.Delete(I)
      else if NormalizePathEntry(Entry) = NormalizePathEntry(AppDir) then
        Already := True;
    end;
    if not Already then
      List.Add(AppDir);
    // Never let a PATH broadcast failure abort an otherwise finished install.
    try
      WriteSystemEnvironmentPath(JoinPath(List));
      BroadcastEnvironmentChange;
    except
    end;
  finally
    List.Free;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Path: string;
  List: TStringList;
  I: Integer;
  Entry: string;
  AppDir: string;
begin
  if CurUninstallStep <> usPostUninstall then Exit;
  AppDir := ExpandConstant('{app}');
  ReadSystemEnvironmentPath(Path);
  List := TStringList.Create;
  try
    List.Delimiter := ';';
    List.StrictDelimiter := True;
    List.DelimitedText := Path;
    for I := List.Count - 1 downto 0 do
    begin
      Entry := Trim(List[I]);
      if NormalizePathEntry(Entry) = NormalizePathEntry(AppDir) then
        List.Delete(I);
    end;
    try
      WriteSystemEnvironmentPath(JoinPath(List));
      BroadcastEnvironmentChange;
    except
    end;
  finally
    List.Free;
  end;
end;
