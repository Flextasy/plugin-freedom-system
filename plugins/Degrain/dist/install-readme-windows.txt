DeGrain v1.0.3 by FLEXTASY — Windows Installation Instructions
===============================================================

NOTE ON EARLIER VERSIONS:
- v1.0.0: blank/white plugin window (missing WebView2 backend selection)
- v1.0.1/v1.0.2: WebView2 loaded, but some machines showed
  "Cannot securely connect to this page" navigating to the plugin's
  internal UI address
- v1.0.3: updated the bundled WebView2 SDK (was pinned to a 2020
  version). This is our best working theory for the v1.0.1/v1.0.2
  error, but it has NOT been confirmed fixed on a real machine yet -
  if you still see the "Cannot securely connect" error on v1.0.3,
  report back with details (Windows version, antivirus/VPN in use).

INSTALLATION:
1. Double-click "Degrain-v1.0.3-by-FLEXTASY-Setup.exe"
2. If Windows SmartScreen warns "Windows protected your PC" — click
   "More info" then "Run anyway" (the installer isn't code-signed yet)
3. Follow the setup wizard
4. DeGrain will be installed to:
   - VST3:       C:\Program Files\Common Files\VST3\Degrain.vst3
   - Standalone: C:\Program Files\DeGrain by FLEXTASY\Degrain.exe

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
FIND DeGrain IN YOUR DAW
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Most DAWs auto-scan the common VST3 folder. If not, add it manually
in your DAW's plugin path settings, then rescan:
  C:\Program Files\Common Files\VST3

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
UNINSTALL
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Use "Add or Remove Programs" and remove "DeGrain by FLEXTASY",
or delete:
  C:\Program Files\Common Files\VST3\Degrain.vst3
  C:\Program Files\DeGrain by FLEXTASY\

PLUGIN INFO:
- Version: 1.0.3
- Formats: VST3, Standalone
- Requires Windows 10/11 (x64)
