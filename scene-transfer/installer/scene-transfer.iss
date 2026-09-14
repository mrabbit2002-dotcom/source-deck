#define MyAppName "Scene Transfer"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Fruit Dragon"

[Setup]
AppId={{2A9E6A61-724F-452F-9983-663D706B1282}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\obs-studio
DisableDirPage=no
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=SceneTransfer-Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Uninstallable=yes

[Files]
Source: "..\dist\payload\obs-plugins\64bit\scene-transfer.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion

[UninstallDelete]
Type: files; Name: "{app}\obs-plugins\64bit\scene-transfer.dll"
