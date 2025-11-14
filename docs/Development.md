# UWP Development Documentation

Developer guide for implementing and extending WinRT support in Proton.

---

## Table of Contents

1. [Reverse Engineering](#reverse-engineering)
2. [WinRT Implementation](#winrt-implementation)
3. [Wine Integration](#wine-integration)
4. [Implementation Status](#implementation-status)
5. [Testing](#testing)

---

## Reverse Engineering

### Tools Required

**On Windows PC:**
- Frida - Dynamic instrumentation
- API Monitor - Windows API monitoring
- Process Monitor - System activity monitoring
- ILSpy/dnSpy - .NET/WinRT decompiler

### Installation

```bash
# Frida
pip install frida-tools

# API Monitor - http://www.rohitab.com/apimonitor
# Process Monitor - https://learn.microsoft.com/sysinternals/downloads/procmon
# ILSpy - https://github.com/icsharpcode/ILSpy
```

### Trace WinRT API Calls

```bash
# Launch app, then:
frida -l frida_winrt_trace.js -n <app.exe>
```

The tracer logs:
- RoInitialize calls
- RoActivateInstance with class names
- HSTRING operations
- Interface queries

**Example output:**
```
[RoActivateInstance] #1
  Class: Windows.ApplicationModel.Package
  Result: S_OK

[RoActivateInstance] #2
  Class: Windows.Storage.ApplicationData
  Result: S_OK
```

### Identify Required APIs

After tracing multiple apps, look for **common patterns**:

1. Every UWP app uses:
   - Windows.ApplicationModel.Package.Current
   - Windows.Storage.ApplicationData.Current

2. Games typically use:
   - Windows.Gaming.Input.Gamepad
   - Windows.Graphics.Display.DisplayInformation

3. Apps with networking:
   - Windows.Networking.* (though many use Win32 sockets)

### Extract .winmd Metadata

**Location on Windows:**
```
C:\Program Files (x86)\Windows Kits\10\References\
C:\Program Files (x86)\Windows Kits\10\UnionMetadata\
```

**Using ILSpy:**
1. Open Windows.winmd
2. Browse namespaces
3. Export interface definitions
4. Note method signatures

---

## WinRT Implementation

### Directory Structure

```
wine_winrt_impl/
├── hstring.c                    # HSTRING implementation
├── combase_winrt.c              # Core runtime
├── windows.foundation/
│   └── uri.c                    # Windows.Foundation.Uri
├── windows.applicationmodel/
│   └── package.c                # Package class
└── windows.storage/
    └── applicationdata.c        # ApplicationData class
```

### Coding Standards

**Wine requires C89** with these modern features allowed:
- `inline` functions
- Variadic macros
- Designated initializers

**NOT allowed:**
- C++ comments (`//`) - use `/* */`
- Variable-length arrays
- Mixed declarations and code
- Declarations after statements

**Wine philosophy:**
- Match surrounding code style
- Don't reformat existing code
- Use Wine debug macros

### Example Implementation

```c
/*
 * Windows.Example.Class implementation
 *
 * Copyright 2024 Your Name
 *
 * LGPL 2.1 license header...
 */

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winstring.h"
#include "roapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(example);

struct example_class
{
    IInspectable IInspectable_iface;
    IExampleClass IExampleClass_iface;
    LONG refcount;
    /* ... members ... */
};

/* All variables declared at top of function - C89 requirement */
static HRESULT WINAPI example_method(IExampleClass *iface, INT32 *value)
{
    struct example_class *impl;
    HRESULT hr;

    impl = impl_from_IExampleClass(iface);

    TRACE("iface %p, value %p\n", iface, value);

    /* ... implementation ... */

    return S_OK;
}
```

### Key WinRT Patterns

**Pattern 1: HSTRING Creation**
```c
HSTRING str;
HRESULT hr;

hr = WindowsCreateString(L"Hello", 5, &str);
if (FAILED(hr)) return hr;

/* use str */

WindowsDeleteString(str);
```

**Pattern 2: Class Activation**
```c
HSTRING className;
IInspectable *instance;
HRESULT hr;

hr = WindowsCreateString(L"Windows.Foundation.Uri", 22, &className);
if (FAILED(hr)) return hr;

hr = RoActivateInstance(className, &instance);

WindowsDeleteString(className);
```

**Pattern 3: Static Properties**
```c
/* Windows.ApplicationModel.Package.Current */
static HRESULT WINAPI package_statics_get_Current(
    IPackageStatics *iface, IPackage **value)
{
    struct package *pkg;
    HRESULT hr;

    hr = get_current_package(&pkg);
    if (FAILED(hr)) return hr;

    *value = &pkg->IPackage_iface;
    return S_OK;
}
```

---

## Wine Integration

### Prerequisites

Wine source is included as Proton submodule at `wine/`.

### Step 1: Add to combase.dll

```bash
cd wine/dlls/combase/

# Copy implementations
cp ../../../wine_winrt_impl/hstring.c .
cp ../../../wine_winrt_impl/combase_winrt.c .

# Edit Makefile.in
```

**Makefile.in:**
```makefile
C_SRCS = \
    ...
    combase_winrt.c \
    hstring.c \
    ...
```

**combase.spec:**
```
@ stdcall RoInitialize(long)
@ stdcall RoUninitialize()
@ stdcall RoActivateInstance(ptr ptr)
@ stdcall RoGetActivationFactory(ptr ptr ptr)
@ stdcall WindowsCreateString(wstr long ptr)
@ stdcall WindowsDeleteString(ptr)
...
```

### Step 2: Create WinRT DLLs

```bash
cd wine/dlls/
mkdir windows.foundation
cd windows.foundation/
```

**Makefile.in:**
```makefile
MODULE    = windows.foundation.dll
IMPORTS   = combase

C_SRCS = \
    main.c \
    uri.c
```

**main.c:**
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

**windows.foundation.spec:**
```
@ stdcall DllGetActivationFactory(ptr ptr)
```

### Step 3: Configure & Build

```bash
# Add to configure.ac
WINE_CONFIG_MAKEFILE(dlls/windows.foundation)

# Reconfigure
./configure --enable-win64

# Build
make
```

### Step 4: Test

```c
/* test_winrt.c */
#include <windows.h>
#include <roapi.h>
#include <stdio.h>

int main(void)
{
    HRESULT hr;
    HSTRING str;

    hr = RoInitialize(RO_INIT_MULTITHREADED);
    printf("RoInitialize: 0x%08x\n", hr);

    hr = WindowsCreateString(L"Test", 4, &str);
    printf("WindowsCreateString: 0x%08x\n", hr);

    WindowsDeleteString(str);
    RoUninitialize();

    return 0;
}
```

```bash
# Compile
winegcc -o test_winrt test_winrt.c -lcombase

# Run
wine test_winrt.exe
```

---

## Implementation Status

### ✅ Complete
- HSTRING (456 lines)
- Core WinRT runtime (374 lines)
- Windows.Foundation.Uri (383 lines)
- Windows.ApplicationModel.Package (774 lines)
- **Windows.Foundation.Collections.PropertySet** (725 lines) - Critical for Valheim!
- **Windows.Foundation.PropertyValue** (1,089 lines) - Factory for typed values
- Frida tracer
- Documentation

### 🚧 In Progress
- Windows.Storage.ApplicationData
- Windows.Gaming.Input.Gamepad
- Async operations

### ⏳ TODO (High Priority)
Based on Valheim trace data (93+ PropertySet activations):

1. **Windows.Storage.ApplicationData** - Save games, settings
2. **Windows.Gaming.Input.Gamepad** - Controller support
3. **IAsyncOperation** - Async pattern used by many APIs
4. **Windows.Foundation.Collections.ValueSet** - Key-value collection (similar to PropertySet)
5. **Windows.Foundation.Collections.IVector** - Generic vector collection
6. **Windows.System.User** - User information

### ⏳ TODO (Medium Priority)
6. **Windows.Graphics.Display** - Display information
7. **Windows.Storage.Streams** - File I/O
8. **Windows.UI.Core** - Window management (non-XAML)
9. **Windows.Networking.Sockets** - If apps don't use Win32

### ❌ Not Feasible
- XAML framework (massive undertaking)
- Xbox Live services (requires MS cloud)
- Microsoft Store DRM (anti-RE)

---

## Testing

### Unit Tests

Create tests in `wine/dlls/combase/tests/`:

```c
/* test_winrt.c */
#include <windows.h>
#include <roapi.h>
#include "wine/test.h"

static void test_RoInitialize(void)
{
    HRESULT hr;

    hr = RoInitialize(RO_INIT_MULTITHREADED);
    ok(hr == S_OK, "RoInitialize failed: 0x%08x\n", hr);

    RoUninitialize();
}

START_TEST(winrt)
{
    test_RoInitialize();
    test_hstring();
    test_activation();
}
```

### Integration Tests

```bash
# Test with simple UWP app
./proton_uwp MarbleMaze.appx

# Test with logging
WINEDEBUG=+winrt,+combase ./proton_uwp app.appx

# Test specific APIs
WINEDEBUG=+package,+storage ./proton_uwp app.appx
```

### Debugging

```bash
# Full Wine debug log
WINEDEBUG=+all wine app.exe > wine.log 2>&1

# Specific channels
WINEDEBUG=+winrt,+hstring,+combase wine app.exe

# With GDB
winedbg app.exe
```

---

## Code Statistics

| Component | Lines | Status |
|-----------|-------|--------|
| HSTRING | 456 | Complete |
| WinRT Runtime | 374 | Complete |
| Windows.Foundation.Uri | 383 | Partial |
| Windows.ApplicationModel.Package | 774 | Complete |
| Windows.Foundation.Collections.PropertySet | 725 | Complete |
| Windows.Foundation.PropertyValue | 1,089 | Complete |
| windows.foundation DLL main | 115 | Complete |
| **Total C Code** | **~3,900** | **75%** |

---

## References

- [Wine Developer's Guide](https://wiki.winehq.org/Wine_Developer%27s_Guide)
- [Microsoft WinRT Docs](https://learn.microsoft.com/en-us/windows/uwp/)
- [C++/WinRT](https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/)
- [WinRT Internals (PDF)](https://conference.hitb.org/hitbsecconf2018pek/materials/D1T2%20-%20The%20Inner%20Workings%20of%20the%20Windows%20Runtime%20-%20James%20Forshaw.pdf)

---

## Contributing

1. Pick an API from TODO list
2. Research on Microsoft Docs
3. Implement in Wine C89 style
4. Test with standalone app
5. Submit pull request

**Before implementing:**
- Check if API is actually used (trace apps first!)
- Read Microsoft documentation
- Look at wine-uwp project for reference
- Follow Wine coding standards

---

For user documentation, see `docs/UWP.md`.
