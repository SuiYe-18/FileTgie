; ======================================================================
; FileTgie Inno Setup 6 installer script (source only; no setup EXE here).
;
; Prerequisite: install Inno Setup 6 (https://jrsoftware.org/isdl.php), then
; compile by right-clicking this file -> Compile, or on the command line:
;     iscc installer\installer.iss
; which produces FileTgie.windows.0.1.0.setup.exe.
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
#define MyAppVersion "0.1.0"
#define MyAppPublisher "SuiYe_TR (Suiye-18) — Single Author"
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
AppCopyright=SuiYe_TR (Suiye-18) — Single Author
; LGPL-3.0 license page shown during installation (relative to this .iss)
LicenseFile=..\LICENSE
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=".."
OutputBaseFilename=FileTgie.windows.0.1.0.setup
UninstallDisplayIcon={app}\{#MyAppExe}
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
WizardStyle=modern
#ifdef SetupIcon
SetupIconFile={#SetupIcon}
#endif

[Tasks]
; Optional shortcuts on the Select Tasks page: start menu checked by default,
; desktop unchecked.
Name: "startmenuicon"; Description: "在开始菜单创建快捷方式(&S)"; GroupDescription: "附加快捷方式:"
Name: "desktopicon"; Description: "在桌面创建快捷方式(&D)"; GroupDescription: "附加快捷方式:"; Flags: unchecked

[Files]
; Single-file deployment of the portable executable plus license texts.
Source: "..\{#MyAppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\COPYING"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\User Manual.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\assets\icon.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExe}"; IconFilename: "{app}\icon.ico"; Tasks: startmenuicon
Name: "{userdesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExe}"; IconFilename: "{app}\icon.ico"; Tasks: desktopicon

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

function SendMessageTimeoutW(hWnd: LongInt; Msg: DWORD; wParam: DWORD;
  lParam: string; fuFlags: DWORD; uTimeout: DWORD): LongInt;
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
begin
  SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
    'Environment', SMTO_ABORTIFHUNG, 5000);
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
    WriteSystemEnvironmentPath(JoinPath(List));
    BroadcastEnvironmentChange;
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
    WriteSystemEnvironmentPath(JoinPath(List));
    BroadcastEnvironmentChange;
  finally
    List.Free;
  end;
end;
