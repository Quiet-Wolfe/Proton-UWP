/*
 * Windows.Foundation.Collections.PropertySet implementation
 *
 * Copyright 2024 Proton-UWP Contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winstring.h"
#include "roapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(propertyset);

/* Maximum number of entries in PropertySet */
#define MAX_PROPERTY_SET_ENTRIES 256

/* Entry in the PropertySet hash table */
struct property_entry
{
    HSTRING key;
    IInspectable *value;
    struct property_entry *next;
};

/* PropertySet implementation */
struct property_set
{
    IInspectable IInspectable_iface;
    IMap_HSTRING_IInspectable IMap_iface;
    IPropertySet IPropertySet_iface;
    LONG refcount;

    struct property_entry *entries[MAX_PROPERTY_SET_ENTRIES];
    UINT32 count;
    CRITICAL_SECTION lock;
};

/* Forward declarations */
static inline struct property_set *impl_from_IInspectable(IInspectable *iface);
static inline struct property_set *impl_from_IMap(IMap_HSTRING_IInspectable *iface);
static inline struct property_set *impl_from_IPropertySet(IPropertySet *iface);

/* Hash function for HSTRING */
static UINT32 hash_hstring(HSTRING str)
{
    const WCHAR *buffer;
    UINT32 length, hash, i;

    buffer = WindowsGetStringRawBuffer(str, &length);
    if (!buffer)
        return 0;

    hash = 0;
    for (i = 0; i < length; i++)
    {
        hash = hash * 31 + buffer[i];
    }

    return hash % MAX_PROPERTY_SET_ENTRIES;
}

/* Find entry by key */
static struct property_entry *find_entry(struct property_set *impl, HSTRING key, struct property_entry ***prev_next)
{
    UINT32 hash;
    struct property_entry *entry, **prev;
    INT32 cmp;

    hash = hash_hstring(key);
    prev = &impl->entries[hash];
    entry = *prev;

    while (entry)
    {
        WindowsCompareStringOrdinal(entry->key, key, &cmp);
        if (cmp == 0)
        {
            if (prev_next)
                *prev_next = prev;
            return entry;
        }
        prev = &entry->next;
        entry = entry->next;
    }

    if (prev_next)
        *prev_next = prev;
    return NULL;
}

/* IInspectable implementation */
static HRESULT WINAPI property_set_inspectable_QueryInterface(IInspectable *iface, REFIID iid, void **out)
{
    struct property_set *impl;

    impl = impl_from_IInspectable(iface);

    TRACE("iface %p, iid %s, out %p\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IInspectable))
    {
        *out = &impl->IInspectable_iface;
    }
    else if (IsEqualGUID(iid, &IID_IMap_HSTRING_IInspectable))
    {
        *out = &impl->IMap_iface;
    }
    else if (IsEqualGUID(iid, &IID_IPropertySet))
    {
        *out = &impl->IPropertySet_iface;
    }
    else
    {
        WARN("Unsupported interface %s\n", debugstr_guid(iid));
        *out = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI property_set_inspectable_AddRef(IInspectable *iface)
{
    struct property_set *impl;
    ULONG refcount;

    impl = impl_from_IInspectable(iface);
    refcount = InterlockedIncrement(&impl->refcount);

    TRACE("iface %p, refcount %u\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI property_set_inspectable_Release(IInspectable *iface)
{
    struct property_set *impl;
    ULONG refcount;
    UINT32 i;
    struct property_entry *entry, *next;

    impl = impl_from_IInspectable(iface);
    refcount = InterlockedDecrement(&impl->refcount);

    TRACE("iface %p, refcount %u\n", iface, refcount);

    if (!refcount)
    {
        /* Free all entries */
        for (i = 0; i < MAX_PROPERTY_SET_ENTRIES; i++)
        {
            entry = impl->entries[i];
            while (entry)
            {
                next = entry->next;
                WindowsDeleteString(entry->key);
                if (entry->value)
                    IInspectable_Release(entry->value);
                HeapFree(GetProcessHeap(), 0, entry);
                entry = next;
            }
        }

        DeleteCriticalSection(&impl->lock);
        HeapFree(GetProcessHeap(), 0, impl);
    }

    return refcount;
}

static HRESULT WINAPI property_set_inspectable_GetIids(IInspectable *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    *iid_count = 0;
    *iids = NULL;
    return S_OK;
}

static HRESULT WINAPI property_set_inspectable_GetRuntimeClassName(IInspectable *iface, HSTRING *class_name)
{
    TRACE("iface %p, class_name %p\n", iface, class_name);
    return WindowsCreateString(L"Windows.Foundation.Collections.PropertySet", 42, class_name);
}

static HRESULT WINAPI property_set_inspectable_GetTrustLevel(IInspectable *iface, TrustLevel *trust_level)
{
    TRACE("iface %p, trust_level %p\n", iface, trust_level);
    *trust_level = BaseTrust;
    return S_OK;
}

static const IInspectableVtbl property_set_inspectable_vtbl =
{
    property_set_inspectable_QueryInterface,
    property_set_inspectable_AddRef,
    property_set_inspectable_Release,
    property_set_inspectable_GetIids,
    property_set_inspectable_GetRuntimeClassName,
    property_set_inspectable_GetTrustLevel
};

/* IMap<HSTRING, IInspectable*> implementation */
static HRESULT WINAPI property_set_map_QueryInterface(IMap_HSTRING_IInspectable *iface, REFIID iid, void **out)
{
    struct property_set *impl;

    impl = impl_from_IMap(iface);
    return IInspectable_QueryInterface(&impl->IInspectable_iface, iid, out);
}

static ULONG WINAPI property_set_map_AddRef(IMap_HSTRING_IInspectable *iface)
{
    struct property_set *impl;

    impl = impl_from_IMap(iface);
    return IInspectable_AddRef(&impl->IInspectable_iface);
}

static ULONG WINAPI property_set_map_Release(IMap_HSTRING_IInspectable *iface)
{
    struct property_set *impl;

    impl = impl_from_IMap(iface);
    return IInspectable_Release(&impl->IInspectable_iface);
}

static HRESULT WINAPI property_set_map_GetIids(IMap_HSTRING_IInspectable *iface, ULONG *iid_count, IID **iids)
{
    struct property_set *impl;

    impl = impl_from_IMap(iface);
    return IInspectable_GetIids(&impl->IInspectable_iface, iid_count, iids);
}

static HRESULT WINAPI property_set_map_GetRuntimeClassName(IMap_HSTRING_IInspectable *iface, HSTRING *class_name)
{
    struct property_set *impl;

    impl = impl_from_IMap(iface);
    return IInspectable_GetRuntimeClassName(&impl->IInspectable_iface, class_name);
}

static HRESULT WINAPI property_set_map_GetTrustLevel(IMap_HSTRING_IInspectable *iface, TrustLevel *trust_level)
{
    struct property_set *impl;

    impl = impl_from_IMap(iface);
    return IInspectable_GetTrustLevel(&impl->IInspectable_iface, trust_level);
}

static HRESULT WINAPI property_set_map_Lookup(IMap_HSTRING_IInspectable *iface, HSTRING key, IInspectable **value)
{
    struct property_set *impl;
    struct property_entry *entry;

    impl = impl_from_IMap(iface);

    TRACE("iface %p, key %p, value %p\n", iface, key, value);

    if (!value)
        return E_POINTER;

    EnterCriticalSection(&impl->lock);

    entry = find_entry(impl, key, NULL);
    if (!entry)
    {
        LeaveCriticalSection(&impl->lock);
        return E_BOUNDS;
    }

    *value = entry->value;
    if (*value)
        IInspectable_AddRef(*value);

    LeaveCriticalSection(&impl->lock);

    return S_OK;
}

static HRESULT WINAPI property_set_map_get_Size(IMap_HSTRING_IInspectable *iface, UINT32 *size)
{
    struct property_set *impl;

    impl = impl_from_IMap(iface);

    TRACE("iface %p, size %p\n", iface, size);

    if (!size)
        return E_POINTER;

    EnterCriticalSection(&impl->lock);
    *size = impl->count;
    LeaveCriticalSection(&impl->lock);

    return S_OK;
}

static HRESULT WINAPI property_set_map_HasKey(IMap_HSTRING_IInspectable *iface, HSTRING key, BOOLEAN *found)
{
    struct property_set *impl;
    struct property_entry *entry;

    impl = impl_from_IMap(iface);

    TRACE("iface %p, key %p, found %p\n", iface, key, found);

    if (!found)
        return E_POINTER;

    EnterCriticalSection(&impl->lock);
    entry = find_entry(impl, key, NULL);
    *found = (entry != NULL);
    LeaveCriticalSection(&impl->lock);

    return S_OK;
}

static HRESULT WINAPI property_set_map_GetView(IMap_HSTRING_IInspectable *iface, IMapView_HSTRING_IInspectable **view)
{
    FIXME("iface %p, view %p stub!\n", iface, view);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_set_map_Insert(IMap_HSTRING_IInspectable *iface, HSTRING key, IInspectable *value, BOOLEAN *replaced)
{
    struct property_set *impl;
    struct property_entry *entry, **prev_next;
    HSTRING key_copy;
    HRESULT hr;
    UINT32 hash;

    impl = impl_from_IMap(iface);

    TRACE("iface %p, key %p, value %p, replaced %p\n", iface, key, value, replaced);

    if (!key)
        return E_INVALIDARG;

    /* Duplicate the key */
    hr = WindowsDuplicateString(key, &key_copy);
    if (FAILED(hr))
        return hr;

    EnterCriticalSection(&impl->lock);

    /* Check if key already exists */
    entry = find_entry(impl, key, &prev_next);
    if (entry)
    {
        /* Replace existing value */
        if (entry->value)
            IInspectable_Release(entry->value);
        entry->value = value;
        if (value)
            IInspectable_AddRef(value);

        if (replaced)
            *replaced = TRUE;

        WindowsDeleteString(key_copy);
    }
    else
    {
        /* Create new entry */
        entry = HeapAlloc(GetProcessHeap(), 0, sizeof(*entry));
        if (!entry)
        {
            LeaveCriticalSection(&impl->lock);
            WindowsDeleteString(key_copy);
            return E_OUTOFMEMORY;
        }

        entry->key = key_copy;
        entry->value = value;
        if (value)
            IInspectable_AddRef(value);
        entry->next = NULL;

        /* Add to hash table */
        hash = hash_hstring(key);
        *prev_next = entry;

        impl->count++;

        if (replaced)
            *replaced = FALSE;
    }

    LeaveCriticalSection(&impl->lock);

    return S_OK;
}

static HRESULT WINAPI property_set_map_Remove(IMap_HSTRING_IInspectable *iface, HSTRING key)
{
    struct property_set *impl;
    struct property_entry *entry, **prev_next;

    impl = impl_from_IMap(iface);

    TRACE("iface %p, key %p\n", iface, key);

    EnterCriticalSection(&impl->lock);

    entry = find_entry(impl, key, &prev_next);
    if (!entry)
    {
        LeaveCriticalSection(&impl->lock);
        return E_BOUNDS;
    }

    /* Remove from list */
    *prev_next = entry->next;

    /* Free entry */
    WindowsDeleteString(entry->key);
    if (entry->value)
        IInspectable_Release(entry->value);
    HeapFree(GetProcessHeap(), 0, entry);

    impl->count--;

    LeaveCriticalSection(&impl->lock);

    return S_OK;
}

static HRESULT WINAPI property_set_map_Clear(IMap_HSTRING_IInspectable *iface)
{
    struct property_set *impl;
    UINT32 i;
    struct property_entry *entry, *next;

    impl = impl_from_IMap(iface);

    TRACE("iface %p\n", iface);

    EnterCriticalSection(&impl->lock);

    for (i = 0; i < MAX_PROPERTY_SET_ENTRIES; i++)
    {
        entry = impl->entries[i];
        while (entry)
        {
            next = entry->next;
            WindowsDeleteString(entry->key);
            if (entry->value)
                IInspectable_Release(entry->value);
            HeapFree(GetProcessHeap(), 0, entry);
            entry = next;
        }
        impl->entries[i] = NULL;
    }

    impl->count = 0;

    LeaveCriticalSection(&impl->lock);

    return S_OK;
}

static const IMap_HSTRING_IInspectableVtbl property_set_map_vtbl =
{
    property_set_map_QueryInterface,
    property_set_map_AddRef,
    property_set_map_Release,
    property_set_map_GetIids,
    property_set_map_GetRuntimeClassName,
    property_set_map_GetTrustLevel,
    property_set_map_Lookup,
    property_set_map_get_Size,
    property_set_map_HasKey,
    property_set_map_GetView,
    property_set_map_Insert,
    property_set_map_Remove,
    property_set_map_Clear
};

/* IPropertySet implementation */
static HRESULT WINAPI property_set_propertyset_QueryInterface(IPropertySet *iface, REFIID iid, void **out)
{
    struct property_set *impl;

    impl = impl_from_IPropertySet(iface);
    return IInspectable_QueryInterface(&impl->IInspectable_iface, iid, out);
}

static ULONG WINAPI property_set_propertyset_AddRef(IPropertySet *iface)
{
    struct property_set *impl;

    impl = impl_from_IPropertySet(iface);
    return IInspectable_AddRef(&impl->IInspectable_iface);
}

static ULONG WINAPI property_set_propertyset_Release(IPropertySet *iface)
{
    struct property_set *impl;

    impl = impl_from_IPropertySet(iface);
    return IInspectable_Release(&impl->IInspectable_iface);
}

static HRESULT WINAPI property_set_propertyset_GetIids(IPropertySet *iface, ULONG *iid_count, IID **iids)
{
    struct property_set *impl;

    impl = impl_from_IPropertySet(iface);
    return IInspectable_GetIids(&impl->IInspectable_iface, iid_count, iids);
}

static HRESULT WINAPI property_set_propertyset_GetRuntimeClassName(IPropertySet *iface, HSTRING *class_name)
{
    struct property_set *impl;

    impl = impl_from_IPropertySet(iface);
    return IInspectable_GetRuntimeClassName(&impl->IInspectable_iface, class_name);
}

static HRESULT WINAPI property_set_propertyset_GetTrustLevel(IPropertySet *iface, TrustLevel *trust_level)
{
    struct property_set *impl;

    impl = impl_from_IPropertySet(iface);
    return IInspectable_GetTrustLevel(&impl->IInspectable_iface, trust_level);
}

static const IPropertySetVtbl property_set_propertyset_vtbl =
{
    property_set_propertyset_QueryInterface,
    property_set_propertyset_AddRef,
    property_set_propertyset_Release,
    property_set_propertyset_GetIids,
    property_set_propertyset_GetRuntimeClassName,
    property_set_propertyset_GetTrustLevel
};

/* Helper functions */
static inline struct property_set *impl_from_IInspectable(IInspectable *iface)
{
    return CONTAINING_RECORD(iface, struct property_set, IInspectable_iface);
}

static inline struct property_set *impl_from_IMap(IMap_HSTRING_IInspectable *iface)
{
    return CONTAINING_RECORD(iface, struct property_set, IMap_iface);
}

static inline struct property_set *impl_from_IPropertySet(IPropertySet *iface)
{
    return CONTAINING_RECORD(iface, struct property_set, IPropertySet_iface);
}

/* Public constructor */
HRESULT create_property_set(IInspectable **out)
{
    struct property_set *impl;
    UINT32 i;

    TRACE("out %p\n", out);

    if (!out)
        return E_POINTER;

    impl = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*impl));
    if (!impl)
        return E_OUTOFMEMORY;

    impl->IInspectable_iface.lpVtbl = &property_set_inspectable_vtbl;
    impl->IMap_iface.lpVtbl = &property_set_map_vtbl;
    impl->IPropertySet_iface.lpVtbl = &property_set_propertyset_vtbl;
    impl->refcount = 1;
    impl->count = 0;

    for (i = 0; i < MAX_PROPERTY_SET_ENTRIES; i++)
        impl->entries[i] = NULL;

    InitializeCriticalSection(&impl->lock);

    *out = &impl->IInspectable_iface;

    TRACE("Created PropertySet %p\n", impl);

    return S_OK;
}

/* PropertySet factory */
struct property_set_factory
{
    IActivationFactory IActivationFactory_iface;
    LONG refcount;
};

static inline struct property_set_factory *impl_from_IActivationFactory_PropertySet(IActivationFactory *iface)
{
    return CONTAINING_RECORD(iface, struct property_set_factory, IActivationFactory_iface);
}

static HRESULT WINAPI property_set_factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    struct property_set_factory *impl;

    impl = impl_from_IActivationFactory_PropertySet(iface);

    TRACE("iface %p, iid %s, out %p\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IActivationFactory))
    {
        *out = &impl->IActivationFactory_iface;
        IUnknown_AddRef((IUnknown *)*out);
        return S_OK;
    }

    WARN("Unsupported interface %s\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI property_set_factory_AddRef(IActivationFactory *iface)
{
    struct property_set_factory *impl;
    ULONG refcount;

    impl = impl_from_IActivationFactory_PropertySet(iface);
    refcount = InterlockedIncrement(&impl->refcount);

    TRACE("iface %p, refcount %u\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI property_set_factory_Release(IActivationFactory *iface)
{
    struct property_set_factory *impl;
    ULONG refcount;

    impl = impl_from_IActivationFactory_PropertySet(iface);
    refcount = InterlockedDecrement(&impl->refcount);

    TRACE("iface %p, refcount %u\n", iface, refcount);

    if (!refcount)
        HeapFree(GetProcessHeap(), 0, impl);

    return refcount;
}

static HRESULT WINAPI property_set_factory_GetIids(IActivationFactory *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    *iid_count = 0;
    *iids = NULL;
    return S_OK;
}

static HRESULT WINAPI property_set_factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *class_name)
{
    TRACE("iface %p, class_name %p\n", iface, class_name);
    return WindowsCreateString(L"Windows.Foundation.Collections.PropertySet", 42, class_name);
}

static HRESULT WINAPI property_set_factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *trust_level)
{
    TRACE("iface %p, trust_level %p\n", iface, trust_level);
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI property_set_factory_ActivateInstance(IActivationFactory *iface, IInspectable **instance)
{
    TRACE("iface %p, instance %p\n", iface, instance);
    return create_property_set(instance);
}

static const IActivationFactoryVtbl property_set_factory_vtbl =
{
    property_set_factory_QueryInterface,
    property_set_factory_AddRef,
    property_set_factory_Release,
    property_set_factory_GetIids,
    property_set_factory_GetRuntimeClassName,
    property_set_factory_GetTrustLevel,
    property_set_factory_ActivateInstance
};

static struct property_set_factory property_set_factory_impl =
{
    { &property_set_factory_vtbl },
    1
};

/* Public factory getter */
HRESULT get_property_set_factory(IActivationFactory **out)
{
    TRACE("out %p\n", out);

    if (!out)
        return E_POINTER;

    *out = &property_set_factory_impl.IActivationFactory_iface;
    IActivationFactory_AddRef(*out);

    return S_OK;
}
