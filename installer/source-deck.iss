#define MyAppName "Source Deck"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Fruit Dragon"

[Setup]
AppId={{B3DB04F8-56E4-4C17-8E13-86A66B54818A}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\obs-studio
DisableDirPage=no
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=SourceDeck-Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Uninstallable=yes

[Files]
Source: "..\dist\payload\obs-plugins\64bit\source-deck.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
Source: "..\dist\payload\data\obs-plugins\source-deck\locale\en-US.ini"; DestDir: "{app}\data\obs-plugins\source-deck\locale"; Flags: ignoreversion

[UninstallDelete]
Type: files; Name: "{app}\obs-plugins\64bit\source-deck.dll"
Type: filesandordirs; Name: "{app}\data\obs-plugins\source-deck"

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
