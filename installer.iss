; PotensioCE Installer Script
[Setup]
AppName=PotensioCE
AppVersion=0.1.0
AppVerName=PotensioCE 0.1.0
DefaultDirName={pf}\PotensioCE
DefaultGroupName=PotensioCE
OutputDir=output
OutputBaseFilename=PotensioCE-Installer
Compression=lzma
SolidCompression=yes
LicenseFile=LICENSE

; Optional icon
; SetupIconFile=resources\icons\app_icon.ico

[Files]
Source: "build\bin\Release\Potensio.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "external\winsparkle-x64\bin\WinSparkle.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\PotensioCE"; Filename: "{app}\Potensio.exe"
Name: "{commondesktop}\PotensioCE"; Filename: "{app}\Potensio.exe"; Tasks: desktopicon
Name: "{group}\Uninstall PotensioCE"; Filename: "{uninstallexe}"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"