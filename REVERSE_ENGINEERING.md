# WinRT Reverse Engineering Guide

This guide helps you reverse engineer WinRT API usage on Windows to understand what needs to be implemented in Wine.

## Overview

**Windows Runtime (WinRT)** is a COM-based API introduced in Windows 8 for UWP applications.

### Key Components:
- **combase.dll**: Core WinRT runtime (RoInitialize, RoActivateInstance, HSTRING functions)
- **IInspectable**: Base interface for all WinRT objects (derives from IUnknown)
- **IActivationFactory**: Factory interface for creating WinRT objects
- **HSTRING**: WinRT string type (opaque handle to immutable string)
- **.winmd files**: Metadata files describing WinRT APIs (ECMA-335 format)

## Tools Needed

### On Windows PC:

1. **Frida** - Dynamic instrumentation toolkit
   - Download: https://frida.re/
   - Install: `pip install frida-tools`

2. **API Monitor** - Monitor and display API calls
   - Download: http://www.rohitab.com/apimonitor

3. **Process Monitor** - Monitor file, registry, and process activity
   - Download: https://learn.microsoft.com/en-us/sysinternals/downloads/procmon

4. **ILSpy** or **dnSpy** - Decompile .NET/WinRT metadata
   - ILSpy: https://github.com/icsharpcode/ILSpy
   - dnSpy: https://github.com/dnSpy/dnSpy

5. **WinMDExp** - Extract metadata from WinRT components
   - Included with Windows SDK

## Step 1: Trace WinRT API Calls with Frida

Use the Frida script included in this repository (`frida_winrt_trace.js`):

```bash
# Install Frida
pip install frida-tools

# Run the tracer on Valheim (or any UWP app)
# First, launch the app, then:
frida -l frida_winrt_trace.js -n Valheim.exe

# Or attach to running process by PID:
frida -l frida_winrt_trace.js -p 1234
```

This will log all:
- RoInitialize calls
- RoActivateInstance calls (with class names)
- HSTRING operations
- IInspectable/IActivationFactory calls

### What to Look For:

1. **Initialization**: `RoInitialize(RO_INIT_MULTITHREADED)`
2. **Class Activation**: `RoActivateInstance("Windows.Foundation.Uri", ...)`
3. **String Operations**: `WindowsCreateString`, `WindowsDeleteString`
4. **Interface Queries**: `QueryInterface` for IInspectable-based interfaces

## Step 2: Monitor with API Monitor

1. Launch API Monitor (64-bit version for x64 apps)
2. Select **Monitoring Filters**:
   - Add `combase.dll`
   - Add `api-ms-win-core-winrt-*.dll`
   - Add custom filters for specific namespaces

3. Start monitoring Valheim or target UWP app

4. Look for:
   - RoInitialize
   - RoUninitialize
   - RoGetActivationFactory
   - RoActivateInstance
   - WindowsCreateString/WindowsDeleteString
   - WindowsGetStringRawBuffer

5. Export the log to analyze the call sequence

## Step 3: Identify Required WinRT Namespaces

UWP apps use various WinRT namespaces. Common ones:

### Core Namespaces:
- **Windows.Foundation** - Basic types (Uri, DateTime, IAsyncOperation)
- **Windows.ApplicationModel** - App lifecycle, package info
- **Windows.ApplicationModel.Core** - CoreApplication
- **Windows.Storage** - File I/O
- **Windows.System** - User info, launcher

### For Games:
- **Windows.Gaming.Input** - Controller/gamepad support
- **Windows.Graphics.Display** - Display information
- **Windows.UI.Core** - Window management
- **Windows.UI.ViewManagement** - App view settings

### For Valheim Specifically:

Run this PowerShell on Windows with Valheim installed:

```powershell
# Find Valheim's installation
$valheim = Get-AppxPackage | Where-Object {$_.Name -like "*Valheim*"}
$valheim.InstallLocation

# List dependencies
$valheim.Dependencies | Select-Object Name, Version
```

## Step 4: Extract .winmd Metadata

WinRT APIs are defined in `.winmd` files (metadata).

### Windows SDK Location:
```
C:\Program Files (x86)\Windows Kits\10\References\<version>\Windows.Foundation.FoundationContract\<version>\
C:\Program Files (x86)\Windows Kits\10\UnionMetadata\<version>\Windows.winmd
```

### Extract with ILSpy:

1. Open ILSpy
2. Load `Windows.winmd` or specific `.winmd` files
3. Browse namespaces to see:
   - Interface definitions
   - Method signatures
   - Property definitions
   - Event definitions

4. Export as C# or just take notes on what needs to be implemented

### Example - Windows.Foundation.Uri:

```csharp
namespace Windows.Foundation
{
    [ContractVersion(typeof(FoundationContract), 65536u)]
    [DualApiPartition(version = 100794368u)]
    [MarshalingBehavior(MarshalingType.Agile)]
    [Threading(ThreadingModel.Both)]
    public sealed class Uri : IUriRuntimeClass, IUriRuntimeClassWithAbsoluteCanonicalUri
    {
        public Uri(string uri);
        public string AbsoluteUri { get; }
        public string DisplayUri { get; }
        public string Domain { get; }
        // ... more properties
    }
}
```

## Step 5: Capture Network Traffic (for Xbox Live)

If the app uses Xbox Live:

```bash
# Use Fiddler or Wireshark
# Monitor traffic to:
# - *.xboxlive.com
# - *.microsoft.com
```

## Step 6: Document Your Findings

Create a file like `valheim_winrt_apis.txt`:

```
=== Valheim WinRT API Requirements ===

Initialization:
- RoInitialize(RO_INIT_MULTITHREADED)

Required Classes:
- Windows.Foundation.Uri
- Windows.ApplicationModel.Package
- Windows.Storage.ApplicationData
- Windows.Gaming.Input.Gamepad

Critical APIs:
1. Windows.ApplicationModel.Package.Current
   - Get app package information
   - Used for: Save game paths

2. Windows.Storage.ApplicationData.Current
   - Get app local folder
   - Used for: Save games, settings

3. Windows.Gaming.Input.Gamepad
   - Controller support
   - Methods: GetGamepads(), Reading events

Not Used (can skip):
- Windows.UI.Xaml.* (no XAML UI)
- Windows.Networking.* (uses Win32 sockets)
```

## Step 7: Test Individual APIs

Create a simple test app on Windows:

```cpp
// test_winrt.cpp
#include <windows.h>
#include <roapi.h>
#include <wrl.h>
#include <windows.foundation.h>

using namespace Microsoft::WRL;
using namespace ABI::Windows::Foundation;

int main()
{
    RoInitialize(RO_INIT_MULTITHREADED);

    // Test URI creation
    ComPtr<IUriRuntimeClassFactory> uriFactory;
    HSTRING uriStr;
    WindowsCreateString(L"http://example.com", 19, &uriStr);

    RoGetActivationFactory(
        HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
        IID_PPV_ARGS(&uriFactory)
    );

    ComPtr<IUriRuntimeClass> uri;
    uriFactory->CreateUri(uriStr, &uri);

    // ... test the API ...

    RoUninitialize();
    return 0;
}
```

Compile and run, then trace with Frida to see exact call sequence.

## Step 8: Compare with Wine Stubs

Check what Wine currently has:

```bash
# On Linux with Wine source:
grep -r "RoActivateInstance" wine/dlls/combase/
grep -r "Windows.Foundation" wine/dlls/
```

Document what's missing.

## Common WinRT Patterns

### Pattern 1: Initialize Runtime
```cpp
RoInitialize(RO_INIT_MULTITHREADED);
```

### Pattern 2: Create HSTRING
```cpp
HSTRING str;
WindowsCreateString(L"Hello", 5, &str);
// ... use str ...
WindowsDeleteString(str);
```

### Pattern 3: Activate Class
```cpp
HSTRING className;
WindowsCreateString(L"Windows.Foundation.Uri", 22, &className);
IInspectable* instance;
RoActivateInstance(className, &instance);
```

### Pattern 4: Get Activation Factory
```cpp
IActivationFactory* factory;
RoGetActivationFactory(className, IID_IActivationFactory, (void**)&factory);
factory->ActivateInstance(&instance);
```

## Useful Commands

### List all loaded DLLs in a UWP process:
```powershell
Get-Process Valheim | Select-Object -ExpandProperty Modules | Select-Object FileName
```

### Find UWP app executable:
```powershell
$pkg = Get-AppxPackage | Where-Object {$_.Name -like "*Valheim*"}
$manifest = Get-AppxPackageManifest -Package $pkg.Name
$manifest.Package.Applications.Application.Executable
```

### Dump app capabilities:
```powershell
$manifest.Package.Capabilities
```

## Next Steps

Once you've identified the APIs:

1. **Prioritize**: Focus on APIs actually called by Valheim
2. **Implement stubs**: Start with minimal implementations in Wine
3. **Test incrementally**: Run Valheim after each implementation
4. **Iterate**: Add more functionality as needed

## Reference Documentation

- **Official WinRT Docs**: https://learn.microsoft.com/en-us/windows/uwp/
- **C++/WinRT Guide**: https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/
- **WinRT Internals**: https://conference.hitb.org/hitbsecconf2018pek/materials/D1T2%20-%20The%20Inner%20Workings%20of%20the%20Windows%20Runtime%20-%20James%20Forshaw.pdf

---

**Transfer this knowledge to Wine implementation to make UWP apps actually work!**
