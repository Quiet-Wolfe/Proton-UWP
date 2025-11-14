# UWP Support Documentation

Complete guide for running Universal Windows Platform applications with Proton.

## Quick Start

### 1. Build Proton
```bash
make install
```

### 2. Get UWP Package
Download from Microsoft Store using the helper:
```bash
./uwp_download_helper.sh 9NCBL78CG9N7  # Valheim's Product ID
```

Visit the provided link, download the x64 .appx file and any dependencies.

### 3. Launch
```bash
./proton_uwp Valheim.appx
```

## What is UWP?

Universal Windows Platform (UWP) is Microsoft's modern application platform for Windows 10+. UWP apps:
- Are distributed as APPX/MSIX packages (typically via Microsoft Store)
- Use Windows Runtime (WinRT) APIs instead of Win32
- Run in a sandboxed environment
- Support cross-device compatibility

## Features & Limitations

### ✅ What Works
- APPX/MSIX package extraction
- DirectX 11 games (via DXVK)
- Basic UWP applications
- Controller input
- Package manifest parsing

### ⚠️ Limited Support
- WinRT API coverage (only core APIs implemented)
- Some input handling
- Async operations

### ❌ Not Supported
- XAML UI framework (complex UI apps won't work)
- Xbox Live integration
- Microsoft Store DRM
- Windows-specific services

## Usage

### Basic Launch
```bash
./proton_uwp <package.appx>
```

### With Logging
```bash
PROTON_LOG=1 ./proton_uwp <package.appx>
# Check ~/proton_uwp.log
```

### Advanced Options
```bash
./proton_uwp --help
./proton_uwp --info package.appx          # Show package info
./proton_uwp --prefix ~/my_prefix app.appx  # Custom Wine prefix
./proton_uwp --force-extract app.appx     # Force re-extraction
```

### Environment Variables
- `PROTON_LOG=1` - Enable Wine logging
- `WINEDEBUG=+winrt,+combase` - Debug WinRT calls
- `PROTON_USE_WINED3D=1` - Use OpenGL instead of Vulkan

## Obtaining UWP Packages

### Method 1: Download Helper
```bash
./uwp_download_helper.sh <ProductID or URL>
```

### Method 2: Adguard Store (Manual)
1. Visit https://store.rg-adguard.net/
2. Enter Product ID or Microsoft Store URL
3. Select "ProductId" + "Retail"
4. Download x64 package + dependencies

### Method 3: From Windows Installation
If installed on Windows (requires admin):
1. Navigate to `C:\Program Files\WindowsApps\`
2. Take ownership of folder
3. Copy app folder

## Troubleshooting

### App crashes immediately
- Enable logging: `PROTON_LOG=1 ./proton_uwp app.appx`
- Check `~/proton_uwp.log` for errors
- Verify all dependencies were downloaded

### Graphics issues
- Try OpenGL: `PROTON_USE_WINED3D=1 ./proton_uwp app.appx`
- Verify Vulkan drivers installed
- Check GPU compatibility

### Missing DLLs
- Download dependency packages from Adguard Store
- Look for: Microsoft.VCLibs, .NET Runtime packages

### Performance issues
- Check if using DXVK (Vulkan) vs wined3d (OpenGL)
- Monitor system resources
- Try different Wine prefixes

## Architecture

### Components
1. **proton_uwp** - Main launcher script
2. **uwp_extractor.py** - APPX/MSIX extraction
3. **uwp_manifest.py** - AppxManifest.xml parser
4. **uwp_prefix.py** - Wine prefix configuration
5. **uwp_download_helper.sh** - Download assistant

### How It Works
1. Extract APPX package (it's a ZIP file)
2. Parse AppxManifest.xml for metadata
3. Create Wine prefix with WinRT configuration
4. Set up DLL overrides (combase, WinRT stubs)
5. Create WindowsApps directory structure
6. Launch executable through Wine64

### WinRT Implementation
Core WinRT APIs implemented (in `wine_winrt_impl/`):
- HSTRING - Immutable string type
- RoInitialize/RoUninitialize - Runtime initialization
- RoActivateInstance - Class instantiation
- RoGetActivationFactory - Factory retrieval
- Windows.Foundation.Uri - Basic URI class
- Windows.ApplicationModel.Package - Package info

## Python API

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
setup = UWPPrefixSetup(Path('~/.uwp_prefix'), Path('dist/files'))
setup.initialize_prefix()
setup.install_uwp_app(package_dir)
```

## Popular UWP Games

| Game | Product ID | Status |
|------|-----------|---------|
| Valheim | 9NCBL78CG9N7 | Testing |
| Minecraft for Windows | 9NBLGGH2JHXJ | Unknown |
| Forza Horizon 4 | 9PNJXVCVWD4K | Unknown |
| Sea of Thieves | 9P2N57MC619K | Unknown |

## Contributing

See `docs/Development.md` for:
- Reverse engineering guide
- Wine integration instructions
- Implementation status
- How to add new WinRT APIs

## License

This UWP support code is part of Proton and follows the same licensing. See LICENSE files for details.

---

**Disclaimer**: Running UWP apps outside Windows may violate terms of service. Use at your own risk. This is provided for educational and interoperability purposes.
