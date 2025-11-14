# Integrating WinRT Implementation into Wine/Proton

This guide explains how to integrate the WinRT implementations into Wine's source tree.

## Overview

We've implemented:
1. **HSTRING functions** - String handling for WinRT
2. **Core WinRT runtime** - RoInitialize, RoActivateInstance, RoGetActivationFactory
3. **Windows.Foundation.Uri** - Basic URI class
4. **Activation factory infrastructure** - Class registration and loading

## Directory Structure

```
wine/
├── dlls/
│   ├── combase/
│   │   ├── hstring.c          (NEW - add our implementation)
│   │   ├── combase_winrt.c    (NEW - add our implementation)
│   │   ├── activation.c       (NEW - add our implementation)
│   │   └── Makefile.in        (MODIFY - add new source files)
│   │
│   ├── windows.foundation/    (NEW DIRECTORY)
│   │   ├── Makefile.in        (NEW)
│   │   ├── main.c             (NEW)
│   │   ├── uri.c              (NEW - our implementation)
│   │   └── windows.foundation.spec  (NEW)
│   │
│   ├── windows.applicationmodel/    (NEW DIRECTORY - future)
│   └── windows.storage/             (NEW DIRECTORY - future)
│
└── include/
    ├── roapi.h                (CHECK if exists, add missing definitions)
    ├── activation.h           (NEW - activation factory interfaces)
    └── inspectable.idl        (CHECK if exists, update if needed)
```

## Step-by-Step Integration

### Step 1: Integrate HSTRING into combase.dll

```bash
cd wine/dlls/combase/
```

1. Copy our `hstring.c` to this directory
2. Edit `Makefile.in`:
   ```makefile
   C_SRCS = \
       classmoniker.c \
       combase_main.c \
       +hstring.c \
       ...
   ```

3. Build and test:
   ```bash
   make
   ```

### Step 2: Integrate WinRT Runtime Functions

```bash
cd wine/dlls/combase/
```

1. Copy `combase_winrt.c` to this directory
2. Update `Makefile.in`:
   ```makefile
   C_SRCS = \
       ...
       +combase_winrt.c \
       hstring.c \
       ...
   ```

3. Update `combase.spec` to export WinRT functions:
   ```
   @ stdcall RoInitialize(long)
   @ stdcall RoUninitialize()
   @ stdcall RoActivateInstance(ptr ptr)
   @ stdcall RoGetActivationFactory(ptr ptr ptr)
   @ stdcall RoGetApartmentIdentifier(ptr)
   @ stdcall WindowsCreateString(wstr long ptr)
   @ stdcall WindowsCreateStringReference(wstr long ptr ptr)
   @ stdcall WindowsDeleteString(ptr)
   @ stdcall WindowsDuplicateString(ptr ptr)
   @ stdcall WindowsGetStringRawBuffer(ptr ptr)
   @ stdcall WindowsGetStringLen(ptr)
   @ stdcall WindowsIsStringEmpty(ptr)
   @ stdcall WindowsStringHasEmbeddedNull(ptr ptr)
   @ stdcall WindowsCompareStringOrdinal(ptr ptr ptr)
   @ stdcall WindowsSubstring(ptr long ptr)
   @ stdcall WindowsSubstringWithSpecifiedLength(ptr long long ptr)
   @ stdcall WindowsConcatString(ptr ptr ptr)
   ```

### Step 3: Create Windows.Foundation DLL

```bash
cd wine/dlls/
mkdir windows.foundation
cd windows.foundation/
```

1. Create `Makefile.in`:
   ```makefile
   MODULE    = windows.foundation.dll
   IMPORTS   = combase

   C_SRCS = \
       main.c \
       uri.c

   RC_SRCS = version.rc
   ```

2. Create `main.c`:
   ```c
   #include "windef.h"
   #include "winbase.h"
   #include "wine/debug.h"

   WINE_DEFAULT_DEBUG_CHANNEL(foundation);

   BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
   {
       TRACE("(%p, %u, %p)\n", instance, reason, reserved);

       switch (reason)
       {
       case DLL_PROCESS_ATTACH:
           DisableThreadLibraryCalls(instance);
           break;
       }

       return TRUE;
   }
   ```

3. Copy our `uri.c` to this directory

4. Create `windows.foundation.spec`:
   ```
   @ stdcall DllGetActivationFactory(ptr ptr)
   ```

5. Add to Wine's configure.ac:
   ```
   WINE_CONFIG_MAKEFILE(dlls/windows.foundation)
   ```

6. Run `./configure` in Wine root

### Step 4: Create Required Headers

Create `wine/include/roapi.h` if it doesn't exist:

```c
#ifndef __WINE_ROAPI_H
#define __WINE_ROAPI_H

#include "windef.h"
#include "winstring.h"

typedef enum
{
    RO_INIT_SINGLETHREADED = 0,
    RO_INIT_MULTITHREADED  = 1,
} RO_INIT_TYPE;

typedef HRESULT (WINAPI *PFNGETACTIVATIONFACTORY)(HSTRING, IActivationFactory **);
typedef void *RO_REGISTRATION_COOKIE;

HRESULT WINAPI RoInitialize(RO_INIT_TYPE type);
void WINAPI RoUninitialize(void);
HRESULT WINAPI RoActivateInstance(HSTRING classid, IInspectable **instance);
HRESULT WINAPI RoGetActivationFactory(HSTRING classid, REFIID iid, void **factory);
HRESULT WINAPI RoGetApartmentIdentifier(UINT64 *identifier);

#endif /* __WINE_ROAPI_H */
```

### Step 5: Build Wine

```bash
cd wine/
./configure --enable-win64
make
```

If you get errors about missing types, add them to the headers.

### Step 6: Test the Implementation

Create a simple test program:

```c
// test_winrt.c
#include <windows.h>
#include <roapi.h>
#include <stdio.h>

int main()
{
    HRESULT hr;
    HSTRING str, classid;
    IInspectable *instance;

    printf("Testing WinRT implementation...\n");

    // Initialize WinRT
    hr = RoInitialize(RO_INIT_MULTITHREADED);
    printf("RoInitialize: 0x%08x\n", hr);

    // Test HSTRING
    hr = WindowsCreateString(L"Hello, WinRT!", 13, &str);
    printf("WindowsCreateString: 0x%08x\n", hr);

    const WCHAR *raw = WindowsGetStringRawBuffer(str, NULL);
    wprintf(L"String value: %s\n", raw);

    WindowsDeleteString(str);

    // Test Uri activation
    hr = WindowsCreateString(L"Windows.Foundation.Uri", 22, &classid);
    printf("Created classid HSTRING: 0x%08x\n", hr);

    hr = RoActivateInstance(classid, &instance);
    printf("RoActivateInstance: 0x%08x\n", hr);

    if (SUCCEEDED(hr))
    {
        printf("Successfully activated Windows.Foundation.Uri!\n");
        IInspectable_Release(instance);
    }

    WindowsDeleteString(classid);

    RoUninitialize();

    printf("Test complete!\n");
    return 0;
}
```

Compile and run:
```bash
winegcc -o test_winrt test_winrt.c -lcombase
wine test_winrt.exe
```

### Step 7: Debug Output

Enable debugging:
```bash
WINEDEBUG=+winrt,+hstring,+combase,+foundation wine test_winrt.exe
```

## Adding More Classes

To add Windows.ApplicationModel.Package:

1. Create `wine/dlls/windows.applicationmodel/`
2. Implement `package.c` with IPackage interface
3. Export `DllGetActivationFactory`
4. Update combase to load it

## Testing with Proton

Once integrated into Wine:

1. Update Proton's Wine submodule to your branch
2. Build Proton: `make`
3. Test UWP apps:
   ```bash
   ./proton_uwp Valheim.appx
   ```

## Common Issues

### Issue: Undefined references

**Solution**: Make sure all interfaces are properly defined in IDL files and headers are generated.

### Issue: CLASS_E_CLASSNOTAVAILABLE

**Solution**: Check that:
- DLL is being loaded (use `WINEDEBUG=+module`)
- DllGetActivationFactory is exported
- Class name matches exactly

### Issue: E_NOINTERFACE

**Solution**: Implement all required interfaces (IInspectable, IActivationFactory, etc.)

## IDL Files

You may need to create IDL files for proper COM interface definitions:

`wine/include/windows.foundation.idl`:
```idl
import "inspectable.idl";

[
    object,
    uuid(9e365e57-48b2-4160-956f-c7385120bbfc)
]
interface IUriRuntimeClass : IInspectable
{
    [propget] HRESULT AbsoluteUri([out, retval] HSTRING *value);
    [propget] HRESULT DisplayUri([out, retval] HSTRING *value);
    [propget] HRESULT Domain([out, retval] HSTRING *value);
    // ... more properties
}

[
    uuid(44a9796f-723e-4fdf-a218-033e75b0c084)
]
interface IUriRuntimeClassFactory : IInspectable
{
    HRESULT CreateUri([in] HSTRING uri, [out, retval] IUriRuntimeClass **instance);
}
```

Then compile with `widl`:
```bash
widl -h -o windows.foundation.h windows.foundation.idl
```

## Next Steps

1. Implement more critical classes:
   - Windows.ApplicationModel.Package
   - Windows.Storage.ApplicationData
   - Windows.Gaming.Input.Gamepad

2. Test with real UWP applications

3. Submit patches to Wine project (if desired)

## Resources

- **Wine Developer Guide**: https://wiki.winehq.org/Wine_Developer%27s_Guide
- **COM in Wine**: `wine/dlls/ole32/`
- **WinRT Spec**: Microsoft documentation
- **Testing**: `wine/dlls/*/tests/` for examples

---

**Note**: This is a significant undertaking. Start with basic functionality and iterate based on which APIs are actually called by your target applications.
