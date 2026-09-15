[Setup]
AppName=TRPG Status
AppVersion=0.1.0
AppPublisher=Fruit Dragon
DefaultDirName={autopf}\obs-studio
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
OutputDir=output
OutputBaseFilename=TRPGStatus-Setup
Compression=lzma2
SolidCompression=yes

[Files]
Source: "..\dist\payload\obs-plugins\64bit\trpg-status.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
