# Proton UWP Support

Experimental support for running Universal Windows Platform (UWP) applications and games through Proton/Wine.

## ⚠️ Important Notice

This is **experimental** software. UWP support in Wine is limited, and many apps may not work properly. This implementation provides the infrastructure to run UWP apps, but success depends on:

- The specific APIs the app uses
- Graphics requirements (DirectX version)
- DRM and anti-cheat systems
- Xbox Live integration requirements

## What is UWP?

Universal Windows Platform (UWP) is Microsoft's modern application platform introduced with Windows 10. UWP apps are distributed as APPX/MSIX packages (typically through the Microsoft Store) and use the Windows Runtime (WinRT) APIs instead of traditional Win32 APIs.

## Features

- ✅ Extract APPX/MSIX packages
- ✅ Parse AppxManifest.xml metadata
- ✅ Set up Wine prefix with WinRT DLL configuration
- ✅ Launch UWP executables through Wine
- ✅ Support for DirectX 11 games (via DXVK)
- ⚠️ Limited WinRT API support (basic stubs only)
- ❌ No XAML UI framework support
- ❌ No Xbox Live integration
- ❌ No DRM support (Microsoft Store DRM will not work)

## System Requirements

- Linux system (tested on Ubuntu, Fedora, Arch)
- Vulkan-capable GPU (for DXVK)
- Built Proton installation
- Python 3.6 or later
- Standard Linux tools: `unzip`, `7z` (optional)

## Getting Started

### 1. Build Proton

First, build Proton if you haven't already:

```bash
cd Proton-UWP
make install
```

This will build Proton and install it to your Steam directory.

### 2. Obtain UWP Packages

You need to get the APPX/MSIX package files for the apps you want to run. Since the Microsoft Store doesn't allow direct downloads, you have a few options:

#### Option A: Use a Third-Party Downloader

Sites like [Adguard Store](https://store.rg-adguard.net/) allow you to download packages directly:

1. Go to https://store.rg-adguard.net/
2. Paste the Microsoft Store URL or Product ID
3. Select "Retail" for the channel
4. Download the appropriate packages:
   - Main `.appx` or `.msix` file (x64 architecture)
   - Any dependency packages (`.appx` files)

#### Option B: Extract from Windows Installation

If you have the app installed on a Windows machine:

1. Installed UWP apps are located in `C:\Program Files\WindowsApps\`
2. You need to take ownership of this folder (it's protected)
3. Copy the app folder and create an APPX from it using `makeappx.exe`

**For Valheim specifically:**
- Microsoft Store page: https://www.microsoft.com/store/productId/9NCBL78CG9N7
- Product ID: `9NCBL78CG9N7`
- Download the x64 package and any dependencies (usually Visual C++ Runtime, .NET Runtime)

### 3. Extract and Inspect Package

Before launching, you can inspect the package:

```bash
./proton_uwp --info Valheim.appx
```

This will show you:
- Package name and version
- Publisher information
- Main executable
- Dependencies
- Required capabilities

### 4. Launch the App

```bash
./proton_uwp Valheim.appx
```

The launcher will:
1. Extract the APPX package to `~/.proton_uwp_apps/`
2. Create a Wine prefix at `~/.proton_uwp_prefix/`
3. Configure WinRT DLL overrides
4. Launch the main executable

### 5. Debugging

Enable Wine debugging to troubleshoot issues:

```bash
PROTON_LOG=1 ./proton_uwp Valheim.appx
```

This creates a log file at `~/proton_uwp.log` with detailed Wine output.

For more specific debugging:

```bash
WINEDEBUG=+winrt,+combase,+ole ./proton_uwp Valheim.appx
```

## Command-Line Options

```
usage: proton_uwp [-h] [--extract-dir EXTRACT_DIR] [--prefix PREFIX]
                  [--proton-dir PROTON_DIR] [--force-extract]
                  [--force-prefix] [-v] [--info]
                  package [app_args ...]

Launch UWP/APPX applications with Proton

positional arguments:
  package               Path to APPX/MSIX file or extracted directory
  app_args              Arguments to pass to the application

optional arguments:
  -h, --help            show this help message and exit
  --extract-dir EXTRACT_DIR
                        Directory to extract packages (default: ~/.proton_uwp_apps)
  --prefix PREFIX       Wine prefix path (default: ~/.proton_uwp_prefix)
  --proton-dir PROTON_DIR
                        Proton installation directory (default: auto-detect)
  --force-extract       Force re-extraction of package
  --force-prefix        Force re-initialization of Wine prefix
  -v, --verbose         Verbose output
  --info                Show package info without launching
```

## Advanced Usage

### Custom Prefix Location

```bash
./proton_uwp --prefix ~/my_valheim_prefix Valheim.appx
```

### Launch from Extracted Directory

After extracting once, you can launch directly:

```bash
./proton_uwp ~/.proton_uwp_apps/Valheim_1.0.0.0_x64__abcdefg/
```

### Force Re-extraction

If the app was updated:

```bash
./proton_uwp --force-extract Valheim.appx
```

### Multiple Apps

Each app can have its own prefix to avoid conflicts:

```bash
./proton_uwp --prefix ~/.uwp_prefix_valheim Valheim.appx
./proton_uwp --prefix ~/.uwp_prefix_minecraft Minecraft.appx
```

## Python API

You can also use the UWP support programmatically:

```python
from pathlib import Path
from uwp_extractor import UWPPackageExtractor
from uwp_manifest import AppxManifest
from uwp_prefix import UWPPrefixSetup

# Extract package
extractor = UWPPackageExtractor()
package_dir = extractor.extract_package(
    Path('Valheim.appx'),
    Path('~/uwp_apps')
)

# Read manifest
manifest = AppxManifest(package_dir / 'AppxManifest.xml')
print(manifest.to_dict())

# Setup prefix
setup = UWPPrefixSetup(
    Path('~/.uwp_prefix'),
    Path('dist/files')
)
setup.initialize_prefix()
setup.install_uwp_app(package_dir)
```

## Known Issues and Limitations

### General Limitations

1. **WinRT API Coverage**: Wine has minimal WinRT API support. Only basic APIs are stubbed out.
2. **XAML UI**: Apps using XAML UI framework will not work.
3. **Xbox Live**: No Xbox Live service integration.
4. **DRM**: Microsoft Store DRM is not supported.
5. **Keyboard/Mouse Input**: Some UWP apps may have input handling issues.

### Valheim-Specific

- **DirectX 11**: Valheim uses DX11, which works well with DXVK
- **Crossplay**: Should work if Xbox Live isn't required for authentication
- **Saves**: May store saves in different location than Steam version
- **Performance**: May differ from native Windows or Steam Proton version

### Troubleshooting

**App crashes immediately:**
- Check Wine log for missing WinRT APIs
- Verify all dependencies were downloaded
- Try with `WINEDEBUG=+winrt,+combase` for details

**Graphics issues:**
- Ensure Vulkan drivers are installed
- Try `PROTON_USE_WINED3D=1` for OpenGL fallback
- Check DXVK compatibility

**Missing DLLs:**
- Some UWP apps need additional Windows Runtime dependencies
- Check the AppxManifest.xml for `<PackageDependency>` entries
- Download and extract dependency packages to the same location

**Performance issues:**
- UWP apps may have additional overhead compared to Win32 versions
- DXVK requires a Vulkan-capable GPU
- Check `PROTON_LOG` for bottlenecks

## Technical Architecture

### Components

1. **uwp_manifest.py**: Parse AppxManifest.xml files
2. **uwp_extractor.py**: Extract APPX/MSIX packages
3. **uwp_prefix.py**: Configure Wine prefix for UWP
4. **proton_uwp**: Main launcher script

### How It Works

1. **Package Extraction**: APPX/MSIX files are ZIP archives. They're extracted using Python's zipfile module.

2. **Manifest Parsing**: AppxManifest.xml contains app metadata, dependencies, and capabilities. We parse this to find the main executable.

3. **Wine Prefix Setup**:
   - Creates directory structure (`C:\Program Files\WindowsApps\`)
   - Sets Wine version to Windows 10
   - Configures WinRT DLL overrides (combase.dll, etc.)
   - Creates registry entries for UWP subsystem

4. **Launching**: The app executable is launched through Wine with:
   - Proper WinRT DLL overrides
   - DXVK for DirectX translation
   - Working directory set to package root

### WinRT DLL Configuration

The following DLLs are configured as builtin:

- `combase.dll` - COM runtime for WinRT
- `api-ms-win-core-winrt-*.dll` - WinRT API stubs
- `kernel32`, `ntdll`, etc. - Core Windows APIs

## Contributing

This is experimental software. Contributions welcome!

Areas that need work:
- [ ] More WinRT API implementations (see wine-uwp project)
- [ ] XAML UI framework support
- [ ] Better input handling
- [ ] Save game location management
- [ ] Bundle (.appxbundle) handling improvements
- [ ] Dependency resolution automation

## References

- [Wine UWP Fork](https://github.com/Rosentti/wine-uwp) - Experimental Wine with UWP API implementations
- [Microsoft UWP Documentation](https://learn.microsoft.com/en-us/windows/uwp/)
- [APPX Package Format](https://learn.microsoft.com/en-us/windows/msix/package/packaging-uwp-apps)
- [WineHQ](https://www.winehq.org/) - Wine development

## License

This UWP support code is part of Proton and follows the same licensing as the rest of the project. See LICENSE files for details.

---

**Disclaimer**: This software is provided as-is for educational and experimental purposes. Running UWP apps outside of Windows may violate terms of service. Use at your own risk.
