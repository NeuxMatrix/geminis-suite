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
#define AppDir    BuildDir + "\GeminisApp_artefacts\Release"

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
Name: "programa"; Description: "Programa Géminis (pantalla de inicio con guitarra, voz, sintetizador, batería e instrumentos)"; Types: completa personalizada
Name: "vst3"; Description: "Plugins VST3 para tu DAW (Cubase, Maschine, Ableton Live...)"; Types: completa personalizada

[Tasks]
Name: "escritorio"; Description: "Crear un acceso directo en el escritorio"; GroupDescription: "Accesos directos:"; Components: programa

[Files]
Source: "..\Assets\logo\geminis.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#AppDir}\Geminis.exe"; DestDir: "{app}"; Components: programa; Flags: ignoreversion
Source: "{#FxDir}\VST3\Geminis FX.vst3\*"; DestDir: "{commoncf64}\VST3\Geminis FX.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#InstDir}\VST3\Geminis Instrument.vst3\*"; DestDir: "{commoncf64}\VST3\Geminis Instrument.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; La versión 0.3.0 instalaba dos programas con dos accesos directos; ahora es uno solo.
Type: files; Name: "{app}\Geminis FX.exe"
Type: files; Name: "{app}\Geminis Instrument.exe"
Type: files; Name: "{group}\Geminis FX.lnk"
Type: files; Name: "{group}\Geminis Instrument.lnk"
Type: files; Name: "{autodesktop}\Geminis FX.lnk"
Type: files; Name: "{autodesktop}\Geminis Instrument.lnk"

[Icons]
Name: "{group}\Géminis"; Filename: "{app}\Geminis.exe"; Components: programa
Name: "{group}\Desinstalar {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Géminis"; Filename: "{app}\Geminis.exe"; Tasks: escritorio

[Run]
Filename: "{app}\Geminis.exe"; Description: "Abrir Géminis"; Components: programa; Flags: nowait postinstall skipifsilent

[Messages]
FinishedLabel=Géminis ya está instalado. Ábrelo desde el menú Inicio o el escritorio.%n%nPara tu DAW, los plugins VST3 Geminis FX (pista de audio) y Geminis Instrument (pista MIDI) están en C:\Program Files\Common Files\VST3. Si tu DAW estaba abierto, vuelve a buscar plugins. En Maschine: Preferencias > Plug-ins > Rescan.
