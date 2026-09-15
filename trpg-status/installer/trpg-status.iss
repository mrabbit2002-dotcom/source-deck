#define MyAppName "TRPG Status"
#define MyAppVersion "0.1.1"
#define MyAppPublisher "Fruit Dragon"

[Setup]
AppId={{B9324E15-89E2-4B95-A942-7D4CF3D6B128}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\obs-studio
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=output
OutputBaseFilename=TRPGStatus-Setup
Compression=lzma2
SolidCompression=yes
Uninstallable=yes

[Files]
Source: "..\dist\payload\obs-plugins\64bit\trpg-status.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion

[UninstallDelete]
Type: files; Name: "{app}\obs-plugins\64bit\trpg-status.dll"

[Code]
function InitializeSetup(): Boolean;
var
  ObsExe: String;
begin
  ObsExe := ExpandConstant('{pf}\obs-studio\bin\64bit\obs64.exe');
  if not FileExists(ObsExe) then
    MsgBox('OBS Studio was not detected in the default folder. Please select the folder that contains your OBS Studio installation on the next screen.', mbInformation, MB_OK);
  Result := True;
end;
