/*
 * Windows.ApplicationModel.Package implementation for Wine
 *
 * Copyright 2024 Proton UWP Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/*
 * Implements Windows.ApplicationModel.Package
 *
 * This class provides information about the app package:
 * - Package identity (name, version, publisher)
 * - Install location
 * - Display name
 * - Status information
 */

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winstring.h"
#include "roapi.h"
#include "shlwapi.h"
#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(package);

/* Forward declarations */
struct package;
struct package_id;
struct storage_folder;

/* Package implementation */
struct package
{
    IInspectable IInspectable_iface;
    IPackage IPackage_iface;
    LONG refcount;

    struct package_id *package_id;
    WCHAR *install_location;
    WCHAR *display_name;
    WCHAR *publisher_display_name;
    WCHAR *description;
    BOOLEAN is_framework;
};

/* PackageId implementation */
struct package_id
{
    IInspectable IInspectable_iface;
    IPackageId IPackageId_iface;
    LONG refcount;

    WCHAR *name;
    WCHAR *version_string;
    WCHAR *publisher;
    WCHAR *publisher_id;
    WCHAR *family_name;
    WCHAR *full_name;
    UINT64 version;  /* Packed version number */
    INT32 architecture;
};

/* Static package instance (for Package.Current) */
static struct package *current_package = NULL;
static CRITICAL_SECTION package_cs;
static CRITICAL_SECTION_DEBUG package_cs_debug =
{
    0, 0, &package_cs,
    { &package_cs_debug.ProcessLocksList, &package_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": package_cs") }
};
static CRITICAL_SECTION package_cs = { &package_cs_debug, -1, 0, 0, 0, 0 };

static inline struct package *impl_from_IPackage(IPackage *iface)
{
    return CONTAINING_RECORD(iface, struct package, IPackage_iface);
}

static inline struct package_id *impl_from_IPackageId(IPackageId *iface)
{
    return CONTAINING_RECORD(iface, struct package_id, IPackageId_iface);
}

/***********************************************************************
 *      create_package_id
 *
 * Create a PackageId from manifest data
 */
static HRESULT create_package_id(const WCHAR *name, const WCHAR *version,
                                 const WCHAR *publisher, INT32 arch,
                                 struct package_id **out)
{
    struct package_id *pkg_id;

    TRACE("(%s, %s, %s, %d, %p)\n", debugstr_w(name), debugstr_w(version),
          debugstr_w(publisher), arch, out);

    pkg_id = heap_alloc_zero(sizeof(*pkg_id));
    if (!pkg_id)
        return E_OUTOFMEMORY;

    pkg_id->refcount = 1;

    /* Copy name */
    if (name)
    {
        pkg_id->name = heap_alloc((wcslen(name) + 1) * sizeof(WCHAR));
        if (pkg_id->name)
            wcscpy(pkg_id->name, name);
    }

    /* Copy version */
    if (version)
    {
        pkg_id->version_string = heap_alloc((wcslen(version) + 1) * sizeof(WCHAR));
        if (pkg_id->version_string)
            wcscpy(pkg_id->version_string, version);

        /* Parse version into packed format (major.minor.build.revision) */
        /* For now, just store as string */
        pkg_id->version = 0;
    }

    /* Copy publisher */
    if (publisher)
    {
        pkg_id->publisher = heap_alloc((wcslen(publisher) + 1) * sizeof(WCHAR));
        if (pkg_id->publisher)
            wcscpy(pkg_id->publisher, publisher);
    }

    /* Generate publisher ID (simplified - just use publisher for now) */
    if (publisher)
    {
        pkg_id->publisher_id = heap_alloc((wcslen(publisher) + 1) * sizeof(WCHAR));
        if (pkg_id->publisher_id)
            wcscpy(pkg_id->publisher_id, publisher);
    }

    /* Generate family name (Name_PublisherId) */
    if (name && publisher)
    {
        int len = wcslen(name) + 1 + wcslen(publisher) + 1;
        pkg_id->family_name = heap_alloc(len * sizeof(WCHAR));
        if (pkg_id->family_name)
            swprintf(pkg_id->family_name, len, L"%s_%s", name, publisher);
    }

    /* Generate full name (Name_Version_Arch_~~_PublisherId) */
    if (name && version && publisher)
    {
        const WCHAR *arch_str = L"x64";
        if (arch == 0) arch_str = L"x86";
        else if (arch == 5) arch_str = L"ARM";
        else if (arch == 12) arch_str = L"ARM64";

        int len = wcslen(name) + 1 + wcslen(version) + 1 + wcslen(arch_str) + 3 + wcslen(publisher) + 1;
        pkg_id->full_name = heap_alloc(len * sizeof(WCHAR));
        if (pkg_id->full_name)
            swprintf(pkg_id->full_name, len, L"%s_%s_%s_~~_%s", name, version, arch_str, publisher);
    }

    pkg_id->architecture = arch;

    *out = pkg_id;
    return S_OK;
}

/***********************************************************************
 *      load_package_from_manifest
 *
 * Load package information from AppxManifest.xml in install location
 */
static HRESULT load_package_from_manifest(const WCHAR *install_path, struct package **out)
{
    struct package *pkg;
    struct package_id *pkg_id;
    WCHAR manifest_path[MAX_PATH];
    HRESULT hr;

    TRACE("(%s, %p)\n", debugstr_w(install_path), out);

    /* Build path to AppxManifest.xml */
    swprintf(manifest_path, MAX_PATH, L"%s\\AppxManifest.xml", install_path);

    /* For now, create a dummy package with fake data */
    /* TODO: Actually parse AppxManifest.xml */

    pkg = heap_alloc_zero(sizeof(*pkg));
    if (!pkg)
        return E_OUTOFMEMORY;

    pkg->refcount = 1;

    /* Create PackageId with dummy data */
    hr = create_package_id(L"UnknownApp", L"1.0.0.0", L"CN=Unknown", 9 /* x64 */, &pkg_id);
    if (FAILED(hr))
    {
        heap_free(pkg);
        return hr;
    }

    pkg->package_id = pkg_id;

    /* Copy install location */
    pkg->install_location = heap_alloc((wcslen(install_path) + 1) * sizeof(WCHAR));
    if (pkg->install_location)
        wcscpy(pkg->install_location, install_path);

    /* Set display names */
    pkg->display_name = heap_alloc((wcslen(L"UWP Application") + 1) * sizeof(WCHAR));
    if (pkg->display_name)
        wcscpy(pkg->display_name, L"UWP Application");

    pkg->publisher_display_name = heap_alloc((wcslen(L"Unknown Publisher") + 1) * sizeof(WCHAR));
    if (pkg->publisher_display_name)
        wcscpy(pkg->publisher_display_name, L"Unknown Publisher");

    pkg->description = heap_alloc((wcslen(L"UWP Application") + 1) * sizeof(WCHAR));
    if (pkg->description)
        wcscpy(pkg->description, L"UWP Application");

    pkg->is_framework = FALSE;

    *out = pkg;

    TRACE("Created package: %s at %s\n", debugstr_w(pkg->display_name), debugstr_w(install_path));

    return S_OK;
}

/***********************************************************************
 *      get_current_package
 *
 * Get or create the current package instance
 */
static HRESULT get_current_package(struct package **out)
{
    HRESULT hr = S_OK;

    EnterCriticalSection(&package_cs);

    if (!current_package)
    {
        WCHAR install_path[MAX_PATH];
        const WCHAR *exe_path;

        /* Try to determine install location from current executable path */
        GetModuleFileNameW(NULL, install_path, MAX_PATH);

        /* Remove executable name to get directory */
        exe_path = wcsrchr(install_path, '\\');
        if (exe_path)
            install_path[exe_path - install_path] = 0;

        TRACE("Determined install path: %s\n", debugstr_w(install_path));

        /* Load package from manifest in install path */
        hr = load_package_from_manifest(install_path, &current_package);

        if (FAILED(hr))
        {
            WARN("Failed to load package from manifest: 0x%08x\n", hr);
            /* Create a minimal dummy package as fallback */
            current_package = heap_alloc_zero(sizeof(*current_package));
            if (current_package)
            {
                current_package->refcount = 1;
                create_package_id(L"UnknownApp", L"1.0.0.0", L"CN=Unknown", 9, &current_package->package_id);
                hr = S_OK;
            }
            else
            {
                hr = E_OUTOFMEMORY;
            }
        }
    }

    if (current_package)
    {
        *out = current_package;
        InterlockedIncrement(&current_package->refcount);
    }

    LeaveCriticalSection(&package_cs);

    return hr;
}

/***********************************************************************
 *      PackageId IInspectable methods
 */
static HRESULT WINAPI packageid_QueryInterface(IPackageId *iface, REFIID iid, void **out)
{
    struct package_id *impl = impl_from_IPackageId(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IPackageId))
    {
        IInspectable_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI packageid_AddRef(IPackageId *iface)
{
    struct package_id *impl = impl_from_IPackageId(iface);
    ULONG refcount = InterlockedIncrement(&impl->refcount);
    TRACE("iface %p, refcount %u.\n", iface, refcount);
    return refcount;
}

static ULONG WINAPI packageid_Release(IPackageId *iface)
{
    struct package_id *impl = impl_from_IPackageId(iface);
    ULONG refcount = InterlockedDecrement(&impl->refcount);

    TRACE("iface %p, refcount %u.\n", iface, refcount);

    if (!refcount)
    {
        heap_free(impl->name);
        heap_free(impl->version_string);
        heap_free(impl->publisher);
        heap_free(impl->publisher_id);
        heap_free(impl->family_name);
        heap_free(impl->full_name);
        heap_free(impl);
    }

    return refcount;
}

static HRESULT WINAPI packageid_GetIids(IPackageId *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT WINAPI packageid_GetRuntimeClassName(IPackageId *iface, HSTRING *class_name)
{
    TRACE("iface %p, class_name %p.\n", iface, class_name);
    return WindowsCreateString(L"Windows.ApplicationModel.PackageId", 35, class_name);
}

static HRESULT WINAPI packageid_GetTrustLevel(IPackageId *iface, TrustLevel *trust_level)
{
    TRACE("iface %p, trust_level %p.\n", iface, trust_level);
    *trust_level = BaseTrust;
    return S_OK;
}

/***********************************************************************
 *      PackageId property getters
 */
static HRESULT WINAPI packageid_get_Name(IPackageId *iface, HSTRING *value)
{
    struct package_id *impl = impl_from_IPackageId(iface);
    TRACE("iface %p, value %p.\n", iface, value);
    return WindowsCreateString(impl->name ? impl->name : L"",
                              impl->name ? wcslen(impl->name) : 0, value);
}

static HRESULT WINAPI packageid_get_Version(IPackageId *iface, PackageVersion *value)
{
    struct package_id *impl = impl_from_IPackageId(iface);
    TRACE("iface %p, value %p.\n", iface, value);

    /* Return dummy version for now */
    value->Major = 1;
    value->Minor = 0;
    value->Build = 0;
    value->Revision = 0;

    return S_OK;
}

static HRESULT WINAPI packageid_get_Architecture(IPackageId *iface, INT32 *value)
{
    struct package_id *impl = impl_from_IPackageId(iface);
    TRACE("iface %p, value %p.\n", iface, value);
    *value = impl->architecture;
    return S_OK;
}

static HRESULT WINAPI packageid_get_ResourceId(IPackageId *iface, HSTRING *value)
{
    TRACE("iface %p, value %p.\n", iface, value);
    return WindowsCreateString(L"", 0, value);
}

static HRESULT WINAPI packageid_get_Publisher(IPackageId *iface, HSTRING *value)
{
    struct package_id *impl = impl_from_IPackageId(iface);
    TRACE("iface %p, value %p.\n", iface, value);
    return WindowsCreateString(impl->publisher ? impl->publisher : L"",
                              impl->publisher ? wcslen(impl->publisher) : 0, value);
}

static HRESULT WINAPI packageid_get_PublisherId(IPackageId *iface, HSTRING *value)
{
    struct package_id *impl = impl_from_IPackageId(iface);
    TRACE("iface %p, value %p.\n", iface, value);
    return WindowsCreateString(impl->publisher_id ? impl->publisher_id : L"",
                              impl->publisher_id ? wcslen(impl->publisher_id) : 0, value);
}

static HRESULT WINAPI packageid_get_FullName(IPackageId *iface, HSTRING *value)
{
    struct package_id *impl = impl_from_IPackageId(iface);
    TRACE("iface %p, value %p.\n", iface, value);
    return WindowsCreateString(impl->full_name ? impl->full_name : L"",
                              impl->full_name ? wcslen(impl->full_name) : 0, value);
}

static HRESULT WINAPI packageid_get_FamilyName(IPackageId *iface, HSTRING *value)
{
    struct package_id *impl = impl_from_IPackageId(iface);
    TRACE("iface %p, value %p.\n", iface, value);
    return WindowsCreateString(impl->family_name ? impl->family_name : L"",
                              impl->family_name ? wcslen(impl->family_name) : 0, value);
}

static const struct IPackageIdVtbl packageid_vtbl =
{
    packageid_QueryInterface,
    packageid_AddRef,
    packageid_Release,
    packageid_GetIids,
    packageid_GetRuntimeClassName,
    packageid_GetTrustLevel,
    packageid_get_Name,
    packageid_get_Version,
    packageid_get_Architecture,
    packageid_get_ResourceId,
    packageid_get_Publisher,
    packageid_get_PublisherId,
    packageid_get_FullName,
    packageid_get_FamilyName,
};

/***********************************************************************
 *      Package IInspectable methods
 */
static HRESULT WINAPI package_QueryInterface(IPackage *iface, REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IPackage))
    {
        IInspectable_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI package_AddRef(IPackage *iface)
{
    struct package *impl = impl_from_IPackage(iface);
    ULONG refcount = InterlockedIncrement(&impl->refcount);
    TRACE("iface %p, refcount %u.\n", iface, refcount);
    return refcount;
}

static ULONG WINAPI package_Release(IPackage *iface)
{
    struct package *impl = impl_from_IPackage(iface);
    ULONG refcount = InterlockedDecrement(&impl->refcount);

    TRACE("iface %p, refcount %u.\n", iface, refcount);

    if (!refcount)
    {
        if (impl->package_id)
            IPackageId_Release(&impl->package_id->IPackageId_iface);
        heap_free(impl->install_location);
        heap_free(impl->display_name);
        heap_free(impl->publisher_display_name);
        heap_free(impl->description);
        heap_free(impl);
    }

    return refcount;
}

static HRESULT WINAPI package_GetIids(IPackage *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT WINAPI package_GetRuntimeClassName(IPackage *iface, HSTRING *class_name)
{
    TRACE("iface %p, class_name %p.\n", iface, class_name);
    return WindowsCreateString(L"Windows.ApplicationModel.Package", 33, class_name);
}

static HRESULT WINAPI package_GetTrustLevel(IPackage *iface, TrustLevel *trust_level)
{
    TRACE("iface %p, trust_level %p.\n", iface, trust_level);
    *trust_level = BaseTrust;
    return S_OK;
}

/***********************************************************************
 *      Package property getters
 */
static HRESULT WINAPI package_get_Id(IPackage *iface, IPackageId **value)
{
    struct package *impl = impl_from_IPackage(iface);
    TRACE("iface %p, value %p.\n", iface, value);

    if (!impl->package_id)
        return E_FAIL;

    *value = &impl->package_id->IPackageId_iface;
    IPackageId_AddRef(*value);
    return S_OK;
}

static HRESULT WINAPI package_get_InstalledLocation(IPackage *iface, IStorageFolder **value)
{
    struct package *impl = impl_from_IPackage(iface);
    FIXME("iface %p, value %p semi-stub!\n", iface, value);

    /* TODO: Return proper IStorageFolder implementation */
    *value = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI package_get_IsFramework(IPackage *iface, BOOLEAN *value)
{
    struct package *impl = impl_from_IPackage(iface);
    TRACE("iface %p, value %p.\n", iface, value);
    *value = impl->is_framework;
    return S_OK;
}

static HRESULT WINAPI package_get_Dependencies(IPackage *iface, void **value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    *value = NULL;
    return E_NOTIMPL;
}

static const struct IPackageVtbl package_vtbl =
{
    package_QueryInterface,
    package_AddRef,
    package_Release,
    package_GetIids,
    package_GetRuntimeClassName,
    package_GetTrustLevel,
    package_get_Id,
    package_get_InstalledLocation,
    package_get_IsFramework,
    package_get_Dependencies,
};

/***********************************************************************
 *      PackageStatics implementation (for Package.Current)
 */
struct package_statics
{
    IActivationFactory IActivationFactory_iface;
    IPackageStatics IPackageStatics_iface;
    LONG refcount;
};

static inline struct package_statics *impl_from_IActivationFactory_package(IActivationFactory *iface)
{
    return CONTAINING_RECORD(iface, struct package_statics, IActivationFactory_iface);
}

static inline struct package_statics *impl_from_IPackageStatics(IPackageStatics *iface)
{
    return CONTAINING_RECORD(iface, struct package_statics, IPackageStatics_iface);
}

static HRESULT WINAPI package_statics_factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    struct package_statics *impl = impl_from_IActivationFactory_package(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IActivationFactory))
    {
        IInspectable_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    if (IsEqualGUID(iid, &IID_IPackageStatics))
    {
        IInspectable_AddRef(&impl->IPackageStatics_iface);
        *out = &impl->IPackageStatics_iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI package_statics_factory_AddRef(IActivationFactory *iface)
{
    struct package_statics *impl = impl_from_IActivationFactory_package(iface);
    ULONG refcount = InterlockedIncrement(&impl->refcount);
    TRACE("iface %p, refcount %u.\n", iface, refcount);
    return refcount;
}

static ULONG WINAPI package_statics_factory_Release(IActivationFactory *iface)
{
    struct package_statics *impl = impl_from_IActivationFactory_package(iface);
    ULONG refcount = InterlockedDecrement(&impl->refcount);
    TRACE("iface %p, refcount %u.\n", iface, refcount);
    return refcount;
}

static HRESULT WINAPI package_statics_factory_GetIids(IActivationFactory *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT WINAPI package_statics_factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *class_name)
{
    TRACE("iface %p, class_name %p.\n", iface, class_name);
    return WindowsCreateString(L"Windows.ApplicationModel.Package", 33, class_name);
}

static HRESULT WINAPI package_statics_factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *trust_level)
{
    TRACE("iface %p, trust_level %p.\n", iface, trust_level);
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI package_statics_factory_ActivateInstance(IActivationFactory *iface, IInspectable **instance)
{
    TRACE("iface %p, instance %p.\n", iface, instance);
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl package_statics_factory_vtbl =
{
    package_statics_factory_QueryInterface,
    package_statics_factory_AddRef,
    package_statics_factory_Release,
    package_statics_factory_GetIids,
    package_statics_factory_GetRuntimeClassName,
    package_statics_factory_GetTrustLevel,
    package_statics_factory_ActivateInstance,
};

static HRESULT WINAPI package_statics_QueryInterface(IPackageStatics *iface, REFIID iid, void **out)
{
    struct package_statics *impl = impl_from_IPackageStatics(iface);
    return IActivationFactory_QueryInterface(&impl->IActivationFactory_iface, iid, out);
}

static ULONG WINAPI package_statics_AddRef(IPackageStatics *iface)
{
    struct package_statics *impl = impl_from_IPackageStatics(iface);
    return IActivationFactory_AddRef(&impl->IActivationFactory_iface);
}

static ULONG WINAPI package_statics_Release(IPackageStatics *iface)
{
    struct package_statics *impl = impl_from_IPackageStatics(iface);
    return IActivationFactory_Release(&impl->IActivationFactory_iface);
}

static HRESULT WINAPI package_statics_GetIids(IPackageStatics *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT WINAPI package_statics_GetRuntimeClassName(IPackageStatics *iface, HSTRING *class_name)
{
    TRACE("iface %p, class_name %p.\n", iface, class_name);
    return WindowsCreateString(L"Windows.ApplicationModel.Package", 33, class_name);
}

static HRESULT WINAPI package_statics_GetTrustLevel(IPackageStatics *iface, TrustLevel *trust_level)
{
    TRACE("iface %p, trust_level %p.\n", iface, trust_level);
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI package_statics_get_Current(IPackageStatics *iface, IPackage **value)
{
    struct package *pkg;
    HRESULT hr;

    TRACE("iface %p, value %p.\n", iface, value);

    hr = get_current_package(&pkg);
    if (FAILED(hr))
        return hr;

    *value = &pkg->IPackage_iface;
    return S_OK;
}

static const struct IPackageStaticsVtbl package_statics_vtbl =
{
    package_statics_QueryInterface,
    package_statics_AddRef,
    package_statics_Release,
    package_statics_GetIids,
    package_statics_GetRuntimeClassName,
    package_statics_GetTrustLevel,
    package_statics_get_Current,
};

static struct package_statics package_statics =
{
    {&package_statics_factory_vtbl},
    {&package_statics_vtbl},
    1,
};

/***********************************************************************
 *      DllGetActivationFactory
 */
HRESULT WINAPI DllGetActivationFactory(HSTRING classid, IActivationFactory **factory)
{
    const WCHAR *class_name;

    TRACE("(%p, %p)\n", classid, factory);

    class_name = WindowsGetStringRawBuffer(classid, NULL);

    if (wcscmp(class_name, L"Windows.ApplicationModel.Package") == 0)
    {
        *factory = &package_statics.IActivationFactory_iface;
        IActivationFactory_AddRef(*factory);
        return S_OK;
    }

    WARN("Unknown class: %s\n", debugstr_w(class_name));
    return CLASS_E_CLASSNOTAVAILABLE;
}
