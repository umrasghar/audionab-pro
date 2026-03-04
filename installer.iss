; AudioNab Pro — Inno Setup Installer Script
; Requires Inno Setup 6.x (https://jrsoftware.org/isinfo.php)

#define MyAppName "AudioNab Pro"
#define MyAppVersion "2.5.0"
#define MyAppPublisher "Umar Asghar"
#define MyAppURL "https://github.com/umrasghar/audionab-pro"
#define MyAppExeName "AudioNabPro.exe"

[Setup]
AppId={{B7E3F2A1-4D5C-48E9-A1B2-C3D4E5F60718}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputBaseFilename=AudioNabPro-Setup-{#MyAppVersion}
OutputDir=dist
SetupIconFile=assets\icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
PrivilegesRequired=admin
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "contextmenu"; Description: "Add ""Extract with AudioNab Pro"" to right-click context menu"; GroupDescription: "Shell Integration:"

[Files]
; Main executable
Source: "build\AudioNabPro.exe"; DestDir: "{app}"; Flags: ignoreversion

; FFmpeg binary (optional — only included if present at build time)
Source: "ffmpeg\ffmpeg.exe"; DestDir: "{app}\ffmpeg"; Flags: ignoreversion; Check: FFmpegExists

; Icon file
Source: "assets\icon.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\icon.ico"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\icon.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
; Register context menu if the task was selected
Filename: "{app}\{#MyAppExeName}"; Parameters: "--register-context-menu"; Flags: runhidden nowait; Tasks: contextmenu

[UninstallRun]
; Remove context menu entries on uninstall
Filename: "{app}\{#MyAppExeName}"; Parameters: "--unregister-context-menu"; Flags: runhidden; RunOnceId: "RemoveContextMenu"

[UninstallDelete]
; Clean up config/data left behind
Type: filesandordirs; Name: "{localappdata}\AudioNab Pro"

[Code]
function FFmpegExists(): Boolean;
begin
  Result := FileExists(ExpandConstant('{src}\ffmpeg\ffmpeg.exe'));
end;
