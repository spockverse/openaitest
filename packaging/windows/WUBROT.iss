#ifndef ProjectRoot
  #define ProjectRoot "..\.."
#endif
#ifndef PluginBundle
  #define PluginBundle ProjectRoot + "\build\WUBROT_artefacts\Release\VST3\WUBROT.vst3"
#endif
#ifndef AppVersion
  #define AppVersion "0.2.0"
#endif
#ifndef OutputDir
  #define OutputDir ProjectRoot + "\dist"
#endif
#if !FileExists(ProjectRoot + "\Resources\Presets\Factory\01 Riddim Monster\Samples\dna.wav")
  #error "Missing factory WAV for 01 Riddim Monster; supply samples and run tools/validate_factory.py first."
#endif
#if !FileExists(ProjectRoot + "\Resources\Presets\Factory\02 Liquid Glass\Samples\dna.wav")
  #error "Missing factory WAV for 02 Liquid Glass; supply samples and run tools/validate_factory.py first."
#endif
#if !FileExists(ProjectRoot + "\Resources\Presets\Factory\03 Metal Teeth\Samples\dna.wav")
  #error "Missing factory WAV for 03 Metal Teeth; supply samples and run tools/validate_factory.py first."
#endif
[Setup]
AppId={{57554252-4F54-4A55-8345-53504F434B31}
AppName=WUBROT
AppVersion={#AppVersion}
AppPublisher=WUBWARE AUDIO
DefaultDirName={commonpf64}\Spock Audio\WUBROT
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=WUBROT-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=WUBROT
[Files]
Source: "{#PluginBundle}\*"; DestDir: "{commoncf64}\VST3\WUBROT.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ProjectRoot}\Resources\Presets\*"; DestDir: "{commonappdata}\Spock Audio\WUBROT\Presets"; Excludes: "README.md"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ProjectRoot}\README.md"; DestDir: "{app}"; Flags: ignoreversion
[Tasks]
Name: "runtime"; Description: "Open Microsoft's WebView2 Runtime page after setup (if the editor requires it)"; Flags: unchecked
[Run]
Filename: "https://developer.microsoft.com/microsoft-edge/webview2/"; Flags: shellexec postinstall skipifsilent; Tasks: runtime
