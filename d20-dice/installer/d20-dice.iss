#define MyAppName "D20 Dice"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Fruit Dragon"

[Setup]
AppId={{4D821CE0-6D84-4D9F-AE2A-2A62F4037B55}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\obs-studio
DisableDirPage=no
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=D20Dice-Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Uninstallable=yes

[Files]
Source: "..\dist\payload\obs-plugins\64bit\d20-dice.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion

[UninstallDelete]
Type: files; Name: "{app}\obs-plugins\64bit\d20-dice.dll"

[Code]
function InitializeSetup(): Boolean;
var
  ObsExe: String;
begin
  ObsExe := ExpandConstant('{pf}\obs-studio\bin\64bit\obs64.exe');
  if not FileExists(ObsExe) then
    MsgBox('OBS Studio was not detected in the default folder. You can select your OBS Studio folder on the next screen.', mbInformation, MB_OK);
  Result := True;
end;
