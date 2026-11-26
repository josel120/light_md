; Windows installer for light_md (Inno Setup 6): Start menu entry, uninstaller, and light_md
; registered for .md / .markdown files. It packs the folder made by "cmake --install":
;
;   cmake --install build --prefix dist
;   iscc /DAppVersion=1.4.0 /DNumericVersion=1.4.0 packaging\windows\light_md.iss
;
; GitHub Actions does this for every build and attaches the installer to each release, next
; to the zip (.github/workflows/build.yml). No administrator rights are needed: it installs for
; the current user, or for everyone when the user picks that and has the rights.

#ifndef AppVersion
  #define AppVersion "dev"
#endif
#ifndef NumericVersion
  #define NumericVersion "0.0.0"
#endif
; Paths are relative to this file.
#ifndef SourceDir
  #define SourceDir "..\..\dist"
#endif
#ifndef OutputDir
  #define OutputDir "..\.."
#endif
#ifndef OutputName
  #define OutputName "light_md-" + AppVersion + "-windows-x64-setup"
#endif

#define ExeName "light_md.exe"

[Setup]
; Never change the AppId: it is how a new version finds and replaces the installed one.
AppId={{BB31FC51-3C45-43C0-A9AE-61D354EF4569}
AppName=light_md
AppVersion={#AppVersion}
AppVerName=light_md {#AppVersion}
VersionInfoVersion={#NumericVersion}
AppPublisher=light_md contributors
AppPublisherURL=https://github.com/josel120/light_md
AppSupportURL=https://github.com/josel120/light_md/issues
AppUpdatesURL=https://github.com/josel120/light_md/releases
DefaultDirName={autopf}\light_md
DefaultGroupName=light_md
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile=..\..\LICENSE
SetupIconFile=..\..\icons\light_md.ico
UninstallDisplayIcon={app}\bin\{#ExeName}
UninstallDisplayName=light_md
OutputDir={#OutputDir}
OutputBaseFilename={#OutputName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
; Closes a running light_md before replacing it (its unsaved work is kept by crash recovery).
CloseApplications=yes

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "pt"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"

[Tasks]
Name: "assoc"; Description: "{cm:AssocFileExtension,light_md,.md}"
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; Tells light_md it was installed, so "Check for updates" offers the installer, not the zip.
Source: "installed.txt"; DestDir: "{app}\bin"; DestName: "light_md-installed"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\light_md"; Filename: "{app}\bin\{#ExeName}"
Name: "{autodesktop}\light_md"; Filename: "{app}\bin\{#ExeName}"; Tasks: desktopicon

[Registry]
; The document type: name, icon, and how to open it.
Root: HKA; Subkey: "Software\Classes\light_md.md"; ValueType: string; ValueName: ""; ValueData: "Markdown document"; Flags: uninsdeletekey; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\light_md.md\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\bin\{#ExeName},0"; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\light_md.md\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\{#ExeName}"" ""%1"""; Tasks: assoc
; light_md in "Open with" for .md and .markdown. The default program stays the user's (and
; Windows') choice: the extensions' own default is never written or removed, so another
; editor's setting is left alone. When no program is set, Windows offers light_md the first
; time a document is double-clicked.
Root: HKA; Subkey: "Software\Classes\.md\OpenWithProgids"; ValueType: string; ValueName: "light_md.md"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.markdown\OpenWithProgids"; ValueType: string; ValueName: "light_md.md"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
; The program itself, for "Open with" → "Choose another app".
Root: HKA; Subkey: "Software\Classes\Applications\{#ExeName}"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "light_md"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\{#ExeName}\SupportedTypes"; ValueType: string; ValueName: ".md"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\{#ExeName}\SupportedTypes"; ValueType: string; ValueName: ".markdown"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\{#ExeName}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\{#ExeName}"" ""%1"""

[Run]
Filename: "{app}\bin\{#ExeName}"; Description: "{cm:LaunchProgram,light_md}"; Flags: nowait postinstall skipifsilent
