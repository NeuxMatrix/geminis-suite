; Instalador de Windows de Geminis Suite (Inno Setup 6).
;
; GitHub Actions lo genera después de compilar:
;   iscc /DAppVersion=0.3.0 installer\GeminisSuite.iss
; El instalador queda en installer\Output.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\build"
#endif

#define AppName   "Geminis Suite"
#define Publisher "Geminis Audio"
#define FxDir     BuildDir + "\GeminisFX_artefacts\Release"
#define InstDir   BuildDir + "\GeminisInstrument_artefacts\Release"

[Setup]
AppId={{6E3B1F2A-9C4D-4A57-8B21-5D0E7F3C9A14}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#Publisher}
VersionInfoVersion={#AppVersion}
VersionInfoDescription=Instalador de {#AppName}
DefaultDirName={autopf}\{#Publisher}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0
OutputDir=Output
OutputBaseFilename=Geminis-Suite-{#AppVersion}-Instalador
SetupIconFile=..\Assets\logo\geminis.ico
UninstallDisplayIcon={app}\geminis.ico
UninstallDisplayName={#AppName}
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
CloseApplications=yes

[Languages]
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"

[Types]
Name: "completa"; Description: "Instalación completa"
Name: "personalizada"; Description: "Instalación personalizada"; Flags: iscustom

[Components]
Name: "programas"; Description: "Programas independientes (Geminis FX y Geminis Instrument)"; Types: completa personalizada
Name: "vst3"; Description: "Plugins VST3 para tu DAW (Cubase, Maschine, Ableton Live...)"; Types: completa personalizada

[Tasks]
Name: "escritorio"; Description: "Crear accesos directos en el escritorio"; GroupDescription: "Accesos directos:"; Components: programas

[Files]
Source: "..\Assets\logo\geminis.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#FxDir}\Standalone\Geminis FX.exe"; DestDir: "{app}"; Components: programas; Flags: ignoreversion
Source: "{#InstDir}\Standalone\Geminis Instrument.exe"; DestDir: "{app}"; Components: programas; Flags: ignoreversion
Source: "{#FxDir}\VST3\Geminis FX.vst3\*"; DestDir: "{commoncf64}\VST3\Geminis FX.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#InstDir}\VST3\Geminis Instrument.vst3\*"; DestDir: "{commoncf64}\VST3\Geminis Instrument.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Geminis FX"; Filename: "{app}\Geminis FX.exe"; Components: programas
Name: "{group}\Geminis Instrument"; Filename: "{app}\Geminis Instrument.exe"; Components: programas
Name: "{group}\Desinstalar {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Geminis FX"; Filename: "{app}\Geminis FX.exe"; Tasks: escritorio
Name: "{autodesktop}\Geminis Instrument"; Filename: "{app}\Geminis Instrument.exe"; Tasks: escritorio

[Run]
Filename: "{app}\Geminis FX.exe"; Description: "Abrir Geminis FX"; Components: programas; Flags: nowait postinstall skipifsilent unchecked
Filename: "{app}\Geminis Instrument.exe"; Description: "Abrir Geminis Instrument"; Components: programas; Flags: nowait postinstall skipifsilent unchecked

[Messages]
FinishedLabel=Geminis Suite ya está instalado.%n%nLos plugins VST3 están en C:\Program Files\Common Files\VST3. Si tu DAW estaba abierto, vuelve a buscar plugins. En Maschine: Preferencias > Plug-ins > Rescan.
