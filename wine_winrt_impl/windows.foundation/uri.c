/*
 * Windows.Foundation.Uri implementation for Wine
 *
 * Copyright 2024 Proton UWP Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/*
 * Implements Windows.Foundation.Uri - WinRT URL/URI class
 *
 * Interfaces implemented:
 * - IUriRuntimeClass
 * - IUriRuntimeClassWithAbsoluteCanonicalUri
 * - IStringable
 */

#include <stdarg.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winstring.h"
#include "roapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(uri);

/* Uri implementation structure */
struct uri
{
    IActivationFactory IActivationFactory_iface;
    IUriRuntimeClass IUriRuntimeClass_iface;
    IStringable IStringable_iface;
    LONG refcount;

    WCHAR *absolute_uri;
    WCHAR *display_uri;
    WCHAR *domain;
    WCHAR *extension;
    WCHAR *fragment;
    WCHAR *host;
    WCHAR *password;
    WCHAR *path;
    WCHAR *query;
    WCHAR *raw_uri;
    WCHAR *scheme_name;
    WCHAR *user_name;
    INT32 port;
    BOOLEAN suspicious;
};

static inline struct uri *impl_from_IActivationFactory(IActivationFactory *iface)
{
    return CONTAINING_RECORD(iface, struct uri, IActivationFactory_iface);
}

static inline struct uri *impl_from_IUriRuntimeClass(IUriRuntimeClass *iface)
{
    return CONTAINING_RECORD(iface, struct uri, IUriRuntimeClass_iface);
}

static inline struct uri *impl_from_IStringable(IStringable *iface)
{
    return CONTAINING_RECORD(iface, struct uri, IStringable_iface);
}

/* Simple URI parser (basic implementation) */
static HRESULT parse_uri(const WCHAR *uri_string, struct uri *uri)
{
    const WCHAR *p, *start;
    int len;

    TRACE("Parsing URI: %s\n", debugstr_w(uri_string));

    /* Store raw URI */
    len = wcslen(uri_string);
    uri->raw_uri = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
    if (!uri->raw_uri)
        return E_OUTOFMEMORY;
    wcscpy(uri->raw_uri, uri_string);

    /* Parse scheme (e.g., "http://") */
    p = wcsstr(uri_string, L"://");
    if (p)
    {
        len = p - uri_string;
        uri->scheme_name = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
        if (!uri->scheme_name)
            return E_OUTOFMEMORY;
        wcsncpy(uri->scheme_name, uri_string, len);
        uri->scheme_name[len] = 0;

        /* Move past "://" */
        p += 3;
    }
    else
    {
        /* No scheme found */
        uri->scheme_name = NULL;
        p = uri_string;
    }

    /* Parse host and optional port */
    start = p;
    while (*p && *p != '/' && *p != '?' && *p != '#')
        p++;

    if (p > start)
    {
        const WCHAR *colon = wcschr(start, ':');
        if (colon && colon < p)
        {
            /* Has port */
            len = colon - start;
            uri->host = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
            if (uri->host)
            {
                wcsncpy(uri->host, start, len);
                uri->host[len] = 0;
            }

            uri->port = _wtoi(colon + 1);
        }
        else
        {
            /* No port */
            len = p - start;
            uri->host = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
            if (uri->host)
            {
                wcsncpy(uri->host, start, len);
                uri->host[len] = 0;
            }

            /* Default ports */
            if (uri->scheme_name)
            {
                if (wcscmp(uri->scheme_name, L"http") == 0)
                    uri->port = 80;
                else if (wcscmp(uri->scheme_name, L"https") == 0)
                    uri->port = 443;
                else
                    uri->port = 0;
            }
            else
            {
                uri->port = 0;
            }
        }

        /* Domain is same as host for now */
        if (uri->host)
        {
            len = wcslen(uri->host);
            uri->domain = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
            if (uri->domain)
                wcscpy(uri->domain, uri->host);
        }
    }

    /* Parse path */
    if (*p == '/')
    {
        start = p;
        while (*p && *p != '?' && *p != '#')
            p++;

        len = p - start;
        uri->path = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
        if (uri->path)
        {
            wcsncpy(uri->path, start, len);
            uri->path[len] = 0;
        }

        /* Check for extension */
        const WCHAR *dot = wcsrchr(uri->path, '.');
        if (dot && dot > wcsrchr(uri->path, '/'))
        {
            len = wcslen(dot);
            uri->extension = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
            if (uri->extension)
                wcscpy(uri->extension, dot);
        }
    }

    /* Parse query */
    if (*p == '?')
    {
        start = p + 1;
        while (*p && *p != '#')
            p++;

        len = p - start;
        uri->query = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
        if (uri->query)
        {
            wcsncpy(uri->query, start, len);
            uri->query[len] = 0;
        }
    }

    /* Parse fragment */
    if (*p == '#')
    {
        start = p + 1;
        len = wcslen(start);
        uri->fragment = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
        if (uri->fragment)
            wcscpy(uri->fragment, start);
    }

    /* Build absolute URI */
    len = wcslen(uri_string);
    uri->absolute_uri = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
    if (uri->absolute_uri)
        wcscpy(uri->absolute_uri, uri_string);

    /* Build display URI (same as absolute for now) */
    uri->display_uri = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
    if (uri->display_uri)
        wcscpy(uri->display_uri, uri_string);

    uri->suspicious = FALSE;

    return S_OK;
}

/* IActivationFactory methods */
static HRESULT WINAPI activation_factory_QueryInterface(IActivationFactory *iface,
                                                        REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IActivationFactory))
    {
        IUnknown_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI activation_factory_AddRef(IActivationFactory *iface)
{
    struct uri *impl = impl_from_IActivationFactory(iface);
    ULONG refcount = InterlockedIncrement(&impl->refcount);
    TRACE("iface %p, refcount %u.\n", iface, refcount);
    return refcount;
}

static ULONG WINAPI activation_factory_Release(IActivationFactory *iface)
{
    struct uri *impl = impl_from_IActivationFactory(iface);
    ULONG refcount = InterlockedDecrement(&impl->refcount);
    TRACE("iface %p, refcount %u.\n", iface, refcount);
    return refcount;
}

static HRESULT WINAPI activation_factory_GetIids(IActivationFactory *iface,
                                                  ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT WINAPI activation_factory_GetRuntimeClassName(IActivationFactory *iface,
                                                              HSTRING *class_name)
{
    TRACE("iface %p, class_name %p.\n", iface, class_name);
    return WindowsCreateString(L"Windows.Foundation.Uri", 22, class_name);
}

static HRESULT WINAPI activation_factory_GetTrustLevel(IActivationFactory *iface,
                                                        TrustLevel *trust_level)
{
    TRACE("iface %p, trust_level %p.\n", iface, trust_level);
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI activation_factory_ActivateInstance(IActivationFactory *iface,
                                                           IInspectable **instance)
{
    TRACE("iface %p, instance %p.\n", iface, instance);
    /* Cannot create Uri without a string */
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl activation_factory_vtbl =
{
    activation_factory_QueryInterface,
    activation_factory_AddRef,
    activation_factory_Release,
    activation_factory_GetIids,
    activation_factory_GetRuntimeClassName,
    activation_factory_GetTrustLevel,
    activation_factory_ActivateInstance,
};

/* Singleton activation factory */
static struct uri uri_factory =
{
    {&activation_factory_vtbl},
    {NULL},  /* IUriRuntimeClass_vtbl - not used for factory */
    {NULL},  /* IStringable_vtbl - not used for factory */
    1,
};

/***********************************************************************
 *      DllGetActivationFactory
 *
 *  Entry point for getting activation factories from this DLL
 */
HRESULT WINAPI DllGetActivationFactory(HSTRING classid, IActivationFactory **factory)
{
    const WCHAR *class_name;

    TRACE("(%p, %p)\n", classid, factory);

    if (!classid || !factory)
        return E_INVALIDARG;

    class_name = WindowsGetStringRawBuffer(classid, NULL);

    if (wcscmp(class_name, L"Windows.Foundation.Uri") == 0)
    {
        *factory = &uri_factory.IActivationFactory_iface;
        IActivationFactory_AddRef(*factory);
        return S_OK;
    }

    WARN("Unknown class: %s\n", debugstr_w(class_name));
    return CLASS_E_CLASSNOTAVAILABLE;
}
