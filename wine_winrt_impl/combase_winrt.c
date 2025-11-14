/*
 * WinRT Runtime Functions for Wine (combase.dll)
 *
 * Copyright 2024 Proton UWP Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winstring.h"
#include "roapi.h"
#include "activation.h"
#include "wine/debug.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(winrt);

/* Thread-local storage for RO_INIT_TYPE */
static DWORD ro_init_tls = TLS_OUT_OF_INDEXES;

/* Activation factory registry */
struct activation_factory_entry
{
    struct list entry;
    WCHAR *class_name;
    IActivationFactory *factory;
    HMODULE module;  /* DLL containing the implementation */
};

static struct list activation_factories = LIST_INIT(activation_factories);
static CRITICAL_SECTION activation_cs;
static CRITICAL_SECTION_DEBUG activation_cs_debug =
{
    0, 0, &activation_cs,
    { &activation_cs_debug.ProcessLocksList, &activation_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": activation_cs") }
};
static CRITICAL_SECTION activation_cs = { &activation_cs_debug, -1, 0, 0, 0, 0 };

/***********************************************************************
 *      RoInitialize (combase.@)
 */
HRESULT WINAPI RoInitialize(RO_INIT_TYPE type)
{
    HRESULT hr;

    TRACE("(%u)\n", type);

    if (type != RO_INIT_SINGLETHREADED && type != RO_INIT_MULTITHREADED)
        return E_INVALIDARG;

    /* Allocate TLS for storing init type */
    if (ro_init_tls == TLS_OUT_OF_INDEXES)
    {
        ro_init_tls = TlsAlloc();
        if (ro_init_tls == TLS_OUT_OF_INDEXES)
            return E_OUTOFMEMORY;
    }

    /* Check if already initialized on this thread */
    if (TlsGetValue(ro_init_tls) != NULL)
    {
        WARN("Already initialized on this thread\n");
        return RPC_E_CHANGED_MODE;
    }

    /* Initialize COM with appropriate threading model */
    if (type == RO_INIT_SINGLETHREADED)
        hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    else
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        return hr;

    /* Store initialization type */
    TlsSetValue(ro_init_tls, (LPVOID)(DWORD_PTR)(type + 1));

    TRACE("WinRT runtime initialized (type=%u)\n", type);

    return S_OK;
}

/***********************************************************************
 *      RoUninitialize (combase.@)
 */
void WINAPI RoUninitialize(void)
{
    TRACE("()\n");

    if (ro_init_tls == TLS_OUT_OF_INDEXES)
        return;

    /* Check if initialized */
    if (TlsGetValue(ro_init_tls) == NULL)
    {
        WARN("Not initialized on this thread\n");
        return;
    }

    /* Clear initialization flag */
    TlsSetValue(ro_init_tls, NULL);

    /* Uninitialize COM */
    CoUninitialize();

    TRACE("WinRT runtime uninitialized\n");
}

/***********************************************************************
 *      register_activation_factory
 *
 * Internal function to register an activation factory
 */
static HRESULT register_activation_factory(const WCHAR *class_name,
                                          IActivationFactory *factory,
                                          HMODULE module)
{
    struct activation_factory_entry *entry;

    TRACE("(%s, %p, %p)\n", debugstr_w(class_name), factory, module);

    entry = HeapAlloc(GetProcessHeap(), 0, sizeof(*entry));
    if (!entry)
        return E_OUTOFMEMORY;

    entry->class_name = HeapAlloc(GetProcessHeap(), 0,
                                  (wcslen(class_name) + 1) * sizeof(WCHAR));
    if (!entry->class_name)
    {
        HeapFree(GetProcessHeap(), 0, entry);
        return E_OUTOFMEMORY;
    }

    wcscpy(entry->class_name, class_name);
    entry->factory = factory;
    entry->module = module;

    IActivationFactory_AddRef(factory);

    EnterCriticalSection(&activation_cs);
    list_add_tail(&activation_factories, &entry->entry);
    LeaveCriticalSection(&activation_cs);

    TRACE("Registered factory for %s\n", debugstr_w(class_name));

    return S_OK;
}

/***********************************************************************
 *      find_activation_factory
 *
 * Internal function to find a registered activation factory
 */
static IActivationFactory *find_activation_factory(const WCHAR *class_name)
{
    struct activation_factory_entry *entry;
    IActivationFactory *factory = NULL;

    TRACE("(%s)\n", debugstr_w(class_name));

    EnterCriticalSection(&activation_cs);

    LIST_FOR_EACH_ENTRY(entry, &activation_factories, struct activation_factory_entry, entry)
    {
        if (wcscmp(entry->class_name, class_name) == 0)
        {
            factory = entry->factory;
            IActivationFactory_AddRef(factory);
            break;
        }
    }

    LeaveCriticalSection(&activation_cs);

    if (factory)
        TRACE("Found factory for %s: %p\n", debugstr_w(class_name), factory);
    else
        TRACE("No factory found for %s\n", debugstr_w(class_name));

    return factory;
}

/***********************************************************************
 *      load_activation_factory_from_dll
 *
 * Attempt to load activation factory from a DLL
 */
static HRESULT load_activation_factory_from_dll(const WCHAR *class_name,
                                                 IActivationFactory **factory)
{
    WCHAR dll_name[MAX_PATH];
    HMODULE module;
    typedef HRESULT (WINAPI *DllGetActivationFactoryFunc)(HSTRING, IActivationFactory **);
    DllGetActivationFactoryFunc pDllGetActivationFactory;
    HSTRING hclass_name;
    HRESULT hr;

    TRACE("(%s, %p)\n", debugstr_w(class_name), factory);

    /* Try to map class name to DLL name */
    /* For now, simple mapping: Windows.Foundation.* -> windows.foundation.dll */

    if (wcsncmp(class_name, L"Windows.Foundation.Collections.", 32) == 0)
        wcscpy(dll_name, L"windows.foundation.dll");
    else if (wcsncmp(class_name, L"Windows.Foundation.", 19) == 0)
        wcscpy(dll_name, L"windows.foundation.dll");
    else if (wcsncmp(class_name, L"Windows.ApplicationModel.", 25) == 0)
        wcscpy(dll_name, L"windows.applicationmodel.dll");
    else if (wcsncmp(class_name, L"Windows.Storage.", 16) == 0)
        wcscpy(dll_name, L"windows.storage.dll");
    else if (wcsncmp(class_name, L"Windows.Gaming.Input.", 21) == 0)
        wcscpy(dll_name, L"windows.gaming.input.dll");
    else
    {
        WARN("Unknown namespace for class %s\n", debugstr_w(class_name));
        return REGDB_E_CLASSNOTREG;
    }

    /* Load the DLL */
    module = LoadLibraryW(dll_name);
    if (!module)
    {
        WARN("Failed to load %s for class %s\n", debugstr_w(dll_name), debugstr_w(class_name));
        return REGDB_E_CLASSNOTREG;
    }

    /* Get DllGetActivationFactory export */
    pDllGetActivationFactory = (DllGetActivationFactoryFunc)
        GetProcAddress(module, "DllGetActivationFactory");

    if (!pDllGetActivationFactory)
    {
        WARN("DLL %s doesn't export DllGetActivationFactory\n", debugstr_w(dll_name));
        FreeLibrary(module);
        return REGDB_E_CLASSNOTREG;
    }

    /* Create HSTRING for class name */
    hr = WindowsCreateString(class_name, wcslen(class_name), &hclass_name);
    if (FAILED(hr))
    {
        FreeLibrary(module);
        return hr;
    }

    /* Get the activation factory */
    hr = pDllGetActivationFactory(hclass_name, factory);
    WindowsDeleteString(hclass_name);

    if (SUCCEEDED(hr))
    {
        TRACE("Got activation factory %p from %s\n", *factory, debugstr_w(dll_name));
        /* Register it for future use */
        register_activation_factory(class_name, *factory, module);
    }
    else
    {
        WARN("DllGetActivationFactory failed: 0x%08x\n", hr);
        FreeLibrary(module);
    }

    return hr;
}

/***********************************************************************
 *      RoGetActivationFactory (combase.@)
 */
HRESULT WINAPI RoGetActivationFactory(HSTRING classid, REFIID iid, void **factory)
{
    const WCHAR *class_name;
    IActivationFactory *ifactory;
    HRESULT hr;

    TRACE("(%p, %s, %p)\n", classid, debugstr_guid(iid), factory);

    if (!classid || !factory)
        return E_INVALIDARG;

    *factory = NULL;

    class_name = WindowsGetStringRawBuffer(classid, NULL);
    if (!class_name)
        return E_INVALIDARG;

    TRACE("Looking for activation factory for %s\n", debugstr_w(class_name));

    /* Try to find registered factory */
    ifactory = find_activation_factory(class_name);

    /* If not found, try to load from DLL */
    if (!ifactory)
    {
        hr = load_activation_factory_from_dll(class_name, &ifactory);
        if (FAILED(hr))
        {
            WARN("Failed to get activation factory for %s: 0x%08x\n",
                 debugstr_w(class_name), hr);
            return hr;
        }
    }

    /* Query for requested interface */
    hr = IActivationFactory_QueryInterface(ifactory, iid, factory);
    IActivationFactory_Release(ifactory);

    if (FAILED(hr))
        WARN("QueryInterface for %s failed: 0x%08x\n", debugstr_guid(iid), hr);

    return hr;
}

/***********************************************************************
 *      RoActivateInstance (combase.@)
 */
HRESULT WINAPI RoActivateInstance(HSTRING classid, IInspectable **instance)
{
    IActivationFactory *factory;
    HRESULT hr;

    TRACE("(%p, %p)\n", classid, instance);

    if (!instance)
        return E_INVALIDARG;

    *instance = NULL;

    if (!classid)
        return E_INVALIDARG;

    /* Get the activation factory */
    hr = RoGetActivationFactory(classid, &IID_IActivationFactory, (void **)&factory);
    if (FAILED(hr))
    {
        const WCHAR *class_name = WindowsGetStringRawBuffer(classid, NULL);
        WARN("Failed to get activation factory for %s: 0x%08x\n",
             debugstr_w(class_name), hr);
        return hr;
    }

    /* Activate the instance */
    hr = IActivationFactory_ActivateInstance(factory, instance);
    IActivationFactory_Release(factory);

    if (FAILED(hr))
    {
        const WCHAR *class_name = WindowsGetStringRawBuffer(classid, NULL);
        WARN("ActivateInstance failed for %s: 0x%08x\n",
             debugstr_w(class_name), hr);
    }
    else
    {
        const WCHAR *class_name = WindowsGetStringRawBuffer(classid, NULL);
        TRACE("Successfully activated %s: %p\n", debugstr_w(class_name), *instance);
    }

    return hr;
}

/***********************************************************************
 *      RoRegisterActivationFactories (combase.@)
 */
HRESULT WINAPI RoRegisterActivationFactories(HSTRING *classids,
                                             PFNGETACTIVATIONFACTORY *callbacks,
                                             UINT32 count, RO_REGISTRATION_COOKIE *cookie)
{
    FIXME("(%p, %p, %u, %p): stub\n", classids, callbacks, count, cookie);
    return E_NOTIMPL;
}

/***********************************************************************
 *      RoRevokeActivationFactories (combase.@)
 */
void WINAPI RoRevokeActivationFactories(RO_REGISTRATION_COOKIE cookie)
{
    FIXME("(%p): stub\n", cookie);
}

/***********************************************************************
 *      RoGetApartmentIdentifier (combase.@)
 */
HRESULT WINAPI RoGetApartmentIdentifier(UINT64 *identifier)
{
    FIXME("(%p): stub\n", identifier);
    if (!identifier)
        return E_INVALIDARG;
    *identifier = 1;  /* Fake apartment ID */
    return S_OK;
}
