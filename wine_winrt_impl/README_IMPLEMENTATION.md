# WinRT Implementation for Wine

This directory contains WinRT (Windows Runtime) implementations for Wine to support UWP applications.

## Structure

### Core Runtime (combase.dll)
- `combase_winrt.c` - Core WinRT functions (RoInitialize, RoActivateInstance, etc.)
- `hstring.c` - HSTRING implementation
- `activation.c` - Activation factory registry and management

### Windows.Foundation
- `windows.foundation/uri.c` - Windows.Foundation.Uri implementation
- `windows.foundation/async.c` - IAsyncOperation/IAsyncAction implementations
- `windows.foundation/collections.c` - IVector, IMap implementations

### Windows.ApplicationModel
- `windows.applicationmodel/package.c` - Package class implementation
- `windows.applicationmodel/core.c` - CoreApplication implementation

### Windows.Storage
- `windows.storage/applicationdata.c` - ApplicationData implementation
- `windows.storage/storagefolder.c` - StorageFolder implementation

### Windows.Gaming.Input
- `windows.gaming.input/gamepad.c` - Gamepad support

## Integration with Wine

These files need to be integrated into Wine's source tree:

1. **combase.dll** implementations go in: `wine/dlls/combase/`
2. **windows.* DLLs** may need new DLL directories created

### Building

After integrating into Wine source:

```bash
cd wine
./configure --enable-win64
make
```

### Testing

Run with debug output:

```bash
WINEDEBUG=+winrt,+combase,+ole wine /path/to/uwp/app.exe
```

## Implementation Status

### Implemented

- [x] HSTRING basic operations
- [x] RoInitialize/RoUninitialize
- [x] Basic activation factory infrastructure
- [ ] RoActivateInstance (partial - needs class registry)
- [ ] RoGetActivationFactory (partial)

### In Progress

- [ ] Windows.Foundation.Uri
- [ ] Windows.ApplicationModel.Package
- [ ] Windows.Storage.ApplicationData

### TODO

- [ ] IInspectable implementation
- [ ] IActivationFactory implementation
- [ ] Async operations
- [ ] Collections
- [ ] Event handling
- [ ] XAML framework (major undertaking)

## Priority APIs for Valheim

Based on reverse engineering, implement these first:

1. **Windows.ApplicationModel.Package.Current**
   - Returns package information
   - Used for save paths

2. **Windows.Storage.ApplicationData.Current**
   - Returns app data folder
   - Used for save games

3. **Windows.Gaming.Input.Gamepad**
   - Controller support
   - Critical for gameplay

4. **Windows.Foundation.Uri**
   - Basic URL handling
   - May be used for resources

## Reference

- Wine COM Implementation: `wine/dlls/ole32/`
- Wine Class Factory: `wine/dlls/ole32/compobj.c`
- WinRT Spec: Microsoft's Windows Runtime documentation

---

**To Wine Developers**: These implementations follow Wine's coding standards and can be integrated into the main Wine tree or maintained as patches for Proton.
