; DeGrain by FLEXTASY - Windows installer
; Built with Inno Setup (https://jrsoftware.org/isinfo.php)
; VERSION is passed in via /DAppVersion=X.Y.Z at compile time (see CI workflow)

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppName=DeGrain by FLEXTASY
AppVersion={#AppVersion}
AppPublisher=FLEXTASY
DefaultDirName={commoncf}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=output
OutputBaseFilename=Degrain-v{#AppVersion}-by-FLEXTASY-Setup
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern

[Files]
Source: "payload\VST3\Degrain.vst3\*"; DestDir: "{commoncf}\VST3\Degrain.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "payload\Standalone\Degrain.exe"; DestDir: "{commonpf}\DeGrain by FLEXTASY"; Flags: ignoreversion; DestName: "Degrain.exe"

[Icons]
Name: "{commonprograms}\DeGrain by FLEXTASY"; Filename: "{commonpf}\DeGrain by FLEXTASY\Degrain.exe"

[Messages]
WelcomeLabel1=Welcome to the DeGrain by FLEXTASY Setup Wizard
WelcomeLabel2=This will install DeGrain v{#AppVersion}, a granular delay plugin, on your computer.%n%nVST3 will be installed for all DAWs. A standalone version is also included.
FinishedLabel=DeGrain by FLEXTASY has been installed.%n%nVST3: rescan plugins in your DAW to find DeGrain.%n%nMake some noise.
