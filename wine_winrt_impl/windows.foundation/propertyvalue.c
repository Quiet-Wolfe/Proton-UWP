/*
 * Windows.Foundation.PropertyValue implementation
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

WINE_DEFAULT_DEBUG_CHANNEL(propertyvalue);

/* PropertyValue implementation for different types */
struct property_value
{
    IInspectable IInspectable_iface;
    IPropertyValue IPropertyValue_iface;
    LONG refcount;

    PropertyType type;
    union
    {
        UINT8 u1;
        INT16 i2;
        UINT16 u2;
        INT32 i4;
        UINT32 u4;
        INT64 i8;
        UINT64 u8;
        FLOAT r4;
        DOUBLE r8;
        WCHAR c2;
        BOOLEAN b;
        HSTRING str;
        GUID guid;
        /* Arrays not yet implemented */
    } value;
};

/* PropertyValueStatics factory */
struct property_value_statics
{
    IActivationFactory IActivationFactory_iface;
    IPropertyValueStatics IPropertyValueStatics_iface;
    LONG refcount;
};

/* Forward declarations */
static inline struct property_value *impl_from_IInspectable(IInspectable *iface);
static inline struct property_value *impl_from_IPropertyValue(IPropertyValue *iface);
static inline struct property_value_statics *impl_from_IActivationFactory(IActivationFactory *iface);
static inline struct property_value_statics *impl_from_IPropertyValueStatics(IPropertyValueStatics *iface);

/* IInspectable implementation for PropertyValue */
static HRESULT WINAPI property_value_inspectable_QueryInterface(IInspectable *iface, REFIID iid, void **out)
{
    struct property_value *impl;

    impl = impl_from_IInspectable(iface);

    TRACE("iface %p, iid %s, out %p\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IInspectable))
    {
        *out = &impl->IInspectable_iface;
    }
    else if (IsEqualGUID(iid, &IID_IPropertyValue))
    {
        *out = &impl->IPropertyValue_iface;
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

static ULONG WINAPI property_value_inspectable_AddRef(IInspectable *iface)
{
    struct property_value *impl;
    ULONG refcount;

    impl = impl_from_IInspectable(iface);
    refcount = InterlockedIncrement(&impl->refcount);

    TRACE("iface %p, refcount %u\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI property_value_inspectable_Release(IInspectable *iface)
{
    struct property_value *impl;
    ULONG refcount;

    impl = impl_from_IInspectable(iface);
    refcount = InterlockedDecrement(&impl->refcount);

    TRACE("iface %p, refcount %u\n", iface, refcount);

    if (!refcount)
    {
        if (impl->type == PropertyType_String && impl->value.str)
            WindowsDeleteString(impl->value.str);
        HeapFree(GetProcessHeap(), 0, impl);
    }

    return refcount;
}

static HRESULT WINAPI property_value_inspectable_GetIids(IInspectable *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    *iid_count = 0;
    *iids = NULL;
    return S_OK;
}

static HRESULT WINAPI property_value_inspectable_GetRuntimeClassName(IInspectable *iface, HSTRING *class_name)
{
    TRACE("iface %p, class_name %p\n", iface, class_name);
    return WindowsCreateString(L"Windows.Foundation.PropertyValue", 33, class_name);
}

static HRESULT WINAPI property_value_inspectable_GetTrustLevel(IInspectable *iface, TrustLevel *trust_level)
{
    TRACE("iface %p, trust_level %p\n", iface, trust_level);
    *trust_level = BaseTrust;
    return S_OK;
}

static const IInspectableVtbl property_value_inspectable_vtbl =
{
    property_value_inspectable_QueryInterface,
    property_value_inspectable_AddRef,
    property_value_inspectable_Release,
    property_value_inspectable_GetIids,
    property_value_inspectable_GetRuntimeClassName,
    property_value_inspectable_GetTrustLevel
};

/* IPropertyValue implementation */
static HRESULT WINAPI property_value_get_Type(IPropertyValue *iface, PropertyType *type)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, type %p\n", iface, type);

    if (!type)
        return E_POINTER;

    *type = impl->type;
    return S_OK;
}

static HRESULT WINAPI property_value_get_IsNumericScalar(IPropertyValue *iface, BOOLEAN *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;

    *value = (impl->type >= PropertyType_UInt8 && impl->type <= PropertyType_Double);
    return S_OK;
}

static HRESULT WINAPI property_value_GetUInt8(IPropertyValue *iface, UINT8 *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;
    if (impl->type != PropertyType_UInt8)
        return E_INVALIDARG;

    *value = impl->value.u1;
    return S_OK;
}

static HRESULT WINAPI property_value_GetInt16(IPropertyValue *iface, INT16 *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;
    if (impl->type != PropertyType_Int16)
        return E_INVALIDARG;

    *value = impl->value.i2;
    return S_OK;
}

static HRESULT WINAPI property_value_GetUInt16(IPropertyValue *iface, UINT16 *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;
    if (impl->type != PropertyType_UInt16)
        return E_INVALIDARG;

    *value = impl->value.u2;
    return S_OK;
}

static HRESULT WINAPI property_value_GetInt32(IPropertyValue *iface, INT32 *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;
    if (impl->type != PropertyType_Int32)
        return E_INVALIDARG;

    *value = impl->value.i4;
    return S_OK;
}

static HRESULT WINAPI property_value_GetUInt32(IPropertyValue *iface, UINT32 *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;
    if (impl->type != PropertyType_UInt32)
        return E_INVALIDARG;

    *value = impl->value.u4;
    return S_OK;
}

static HRESULT WINAPI property_value_GetInt64(IPropertyValue *iface, INT64 *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;
    if (impl->type != PropertyType_Int64)
        return E_INVALIDARG;

    *value = impl->value.i8;
    return S_OK;
}

static HRESULT WINAPI property_value_GetUInt64(IPropertyValue *iface, UINT64 *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;
    if (impl->type != PropertyType_UInt64)
        return E_INVALIDARG;

    *value = impl->value.u8;
    return S_OK;
}

static HRESULT WINAPI property_value_GetSingle(IPropertyValue *iface, FLOAT *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;
    if (impl->type != PropertyType_Single)
        return E_INVALIDARG;

    *value = impl->value.r4;
    return S_OK;
}

static HRESULT WINAPI property_value_GetDouble(IPropertyValue *iface, DOUBLE *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;
    if (impl->type != PropertyType_Double)
        return E_INVALIDARG;

    *value = impl->value.r8;
    return S_OK;
}

static HRESULT WINAPI property_value_GetChar16(IPropertyValue *iface, WCHAR *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;
    if (impl->type != PropertyType_Char16)
        return E_INVALIDARG;

    *value = impl->value.c2;
    return S_OK;
}

static HRESULT WINAPI property_value_GetBoolean(IPropertyValue *iface, BOOLEAN *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;
    if (impl->type != PropertyType_Boolean)
        return E_INVALIDARG;

    *value = impl->value.b;
    return S_OK;
}

static HRESULT WINAPI property_value_GetString(IPropertyValue *iface, HSTRING *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;
    if (impl->type != PropertyType_String)
        return E_INVALIDARG;

    return WindowsDuplicateString(impl->value.str, value);
}

static HRESULT WINAPI property_value_GetGuid(IPropertyValue *iface, GUID *value)
{
    struct property_value *impl;

    impl = impl_from_IPropertyValue(iface);

    TRACE("iface %p, value %p\n", iface, value);

    if (!value)
        return E_POINTER;
    if (impl->type != PropertyType_Guid)
        return E_INVALIDARG;

    *value = impl->value.guid;
    return S_OK;
}

/* Stub implementations for array and other types */
static HRESULT WINAPI property_value_GetDateTime(IPropertyValue *iface, DateTime *value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetTimeSpan(IPropertyValue *iface, TimeSpan *value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetPoint(IPropertyValue *iface, Point *value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetSize(IPropertyValue *iface, Size *value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetRect(IPropertyValue *iface, Rect *value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetUInt8Array(IPropertyValue *iface, UINT32 *length, UINT8 **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetInt16Array(IPropertyValue *iface, UINT32 *length, INT16 **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetUInt16Array(IPropertyValue *iface, UINT32 *length, UINT16 **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetInt32Array(IPropertyValue *iface, UINT32 *length, INT32 **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetUInt32Array(IPropertyValue *iface, UINT32 *length, UINT32 **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetInt64Array(IPropertyValue *iface, UINT32 *length, INT64 **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetUInt64Array(IPropertyValue *iface, UINT32 *length, UINT64 **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetSingleArray(IPropertyValue *iface, UINT32 *length, FLOAT **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetDoubleArray(IPropertyValue *iface, UINT32 *length, DOUBLE **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetChar16Array(IPropertyValue *iface, UINT32 *length, WCHAR **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetBooleanArray(IPropertyValue *iface, UINT32 *length, BOOLEAN **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetStringArray(IPropertyValue *iface, UINT32 *length, HSTRING **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetInspectableArray(IPropertyValue *iface, UINT32 *length, IInspectable ***value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetGuidArray(IPropertyValue *iface, UINT32 *length, GUID **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetDateTimeArray(IPropertyValue *iface, UINT32 *length, DateTime **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetTimeSpanArray(IPropertyValue *iface, UINT32 *length, TimeSpan **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetPointArray(IPropertyValue *iface, UINT32 *length, Point **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetSizeArray(IPropertyValue *iface, UINT32 *length, Size **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_GetRectArray(IPropertyValue *iface, UINT32 *length, Rect **value)
{
    FIXME("iface %p, length %p, value %p stub!\n", iface, length, value);
    return E_NOTIMPL;
}

static const IPropertyValueVtbl property_value_vtbl =
{
    property_value_inspectable_QueryInterface,
    property_value_inspectable_AddRef,
    property_value_inspectable_Release,
    property_value_inspectable_GetIids,
    property_value_inspectable_GetRuntimeClassName,
    property_value_inspectable_GetTrustLevel,
    property_value_get_Type,
    property_value_get_IsNumericScalar,
    property_value_GetUInt8,
    property_value_GetInt16,
    property_value_GetUInt16,
    property_value_GetInt32,
    property_value_GetUInt32,
    property_value_GetInt64,
    property_value_GetUInt64,
    property_value_GetSingle,
    property_value_GetDouble,
    property_value_GetChar16,
    property_value_GetBoolean,
    property_value_GetString,
    property_value_GetGuid,
    property_value_GetDateTime,
    property_value_GetTimeSpan,
    property_value_GetPoint,
    property_value_GetSize,
    property_value_GetRect,
    property_value_GetUInt8Array,
    property_value_GetInt16Array,
    property_value_GetUInt16Array,
    property_value_GetInt32Array,
    property_value_GetUInt32Array,
    property_value_GetInt64Array,
    property_value_GetUInt64Array,
    property_value_GetSingleArray,
    property_value_GetDoubleArray,
    property_value_GetChar16Array,
    property_value_GetBooleanArray,
    property_value_GetStringArray,
    property_value_GetInspectableArray,
    property_value_GetGuidArray,
    property_value_GetDateTimeArray,
    property_value_GetTimeSpanArray,
    property_value_GetPointArray,
    property_value_GetSizeArray,
    property_value_GetRectArray
};

/* Helper functions */
static inline struct property_value *impl_from_IInspectable(IInspectable *iface)
{
    return CONTAINING_RECORD(iface, struct property_value, IInspectable_iface);
}

static inline struct property_value *impl_from_IPropertyValue(IPropertyValue *iface)
{
    return CONTAINING_RECORD(iface, struct property_value, IPropertyValue_iface);
}

static inline struct property_value_statics *impl_from_IActivationFactory(IActivationFactory *iface)
{
    return CONTAINING_RECORD(iface, struct property_value_statics, IActivationFactory_iface);
}

static inline struct property_value_statics *impl_from_IPropertyValueStatics(IPropertyValueStatics *iface)
{
    return CONTAINING_RECORD(iface, struct property_value_statics, IPropertyValueStatics_iface);
}

/* Factory static methods - CreateString, CreateInt32, etc. */
static HRESULT create_property_value(PropertyType type, const void *data, IInspectable **out)
{
    struct property_value *impl;
    HRESULT hr;

    TRACE("type %d, data %p, out %p\n", type, data, out);

    if (!out)
        return E_POINTER;

    impl = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*impl));
    if (!impl)
        return E_OUTOFMEMORY;

    impl->IInspectable_iface.lpVtbl = &property_value_inspectable_vtbl;
    impl->IPropertyValue_iface.lpVtbl = &property_value_vtbl;
    impl->refcount = 1;
    impl->type = type;

    /* Copy data based on type */
    switch (type)
    {
    case PropertyType_UInt8:
        impl->value.u1 = *(const UINT8 *)data;
        break;
    case PropertyType_Int16:
        impl->value.i2 = *(const INT16 *)data;
        break;
    case PropertyType_UInt16:
        impl->value.u2 = *(const UINT16 *)data;
        break;
    case PropertyType_Int32:
        impl->value.i4 = *(const INT32 *)data;
        break;
    case PropertyType_UInt32:
        impl->value.u4 = *(const UINT32 *)data;
        break;
    case PropertyType_Int64:
        impl->value.i8 = *(const INT64 *)data;
        break;
    case PropertyType_UInt64:
        impl->value.u8 = *(const UINT64 *)data;
        break;
    case PropertyType_Single:
        impl->value.r4 = *(const FLOAT *)data;
        break;
    case PropertyType_Double:
        impl->value.r8 = *(const DOUBLE *)data;
        break;
    case PropertyType_Char16:
        impl->value.c2 = *(const WCHAR *)data;
        break;
    case PropertyType_Boolean:
        impl->value.b = *(const BOOLEAN *)data;
        break;
    case PropertyType_String:
        hr = WindowsDuplicateString(*(const HSTRING *)data, &impl->value.str);
        if (FAILED(hr))
        {
            HeapFree(GetProcessHeap(), 0, impl);
            return hr;
        }
        break;
    case PropertyType_Guid:
        impl->value.guid = *(const GUID *)data;
        break;
    default:
        FIXME("Unsupported property type %d\n", type);
        HeapFree(GetProcessHeap(), 0, impl);
        return E_NOTIMPL;
    }

    *out = &impl->IInspectable_iface;
    return S_OK;
}

/* IPropertyValueStatics implementation */
static HRESULT WINAPI property_value_statics_QueryInterface(IPropertyValueStatics *iface, REFIID iid, void **out)
{
    struct property_value_statics *impl;

    impl = impl_from_IPropertyValueStatics(iface);

    TRACE("iface %p, iid %s, out %p\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IActivationFactory))
    {
        *out = &impl->IActivationFactory_iface;
    }
    else if (IsEqualGUID(iid, &IID_IPropertyValueStatics))
    {
        *out = &impl->IPropertyValueStatics_iface;
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

static ULONG WINAPI property_value_statics_AddRef(IPropertyValueStatics *iface)
{
    struct property_value_statics *impl;
    ULONG refcount;

    impl = impl_from_IPropertyValueStatics(iface);
    refcount = InterlockedIncrement(&impl->refcount);

    TRACE("iface %p, refcount %u\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI property_value_statics_Release(IPropertyValueStatics *iface)
{
    struct property_value_statics *impl;
    ULONG refcount;

    impl = impl_from_IPropertyValueStatics(iface);
    refcount = InterlockedDecrement(&impl->refcount);

    TRACE("iface %p, refcount %u\n", iface, refcount);

    if (!refcount)
        HeapFree(GetProcessHeap(), 0, impl);

    return refcount;
}

static HRESULT WINAPI property_value_statics_GetIids(IPropertyValueStatics *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    *iid_count = 0;
    *iids = NULL;
    return S_OK;
}

static HRESULT WINAPI property_value_statics_GetRuntimeClassName(IPropertyValueStatics *iface, HSTRING *class_name)
{
    TRACE("iface %p, class_name %p\n", iface, class_name);
    return WindowsCreateString(L"Windows.Foundation.PropertyValue", 33, class_name);
}

static HRESULT WINAPI property_value_statics_GetTrustLevel(IPropertyValueStatics *iface, TrustLevel *trust_level)
{
    TRACE("iface %p, trust_level %p\n", iface, trust_level);
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI property_value_statics_CreateUInt8(IPropertyValueStatics *iface, UINT8 value, IInspectable **out)
{
    TRACE("iface %p, value %u, out %p\n", iface, value, out);
    return create_property_value(PropertyType_UInt8, &value, out);
}

static HRESULT WINAPI property_value_statics_CreateInt16(IPropertyValueStatics *iface, INT16 value, IInspectable **out)
{
    TRACE("iface %p, value %d, out %p\n", iface, value, out);
    return create_property_value(PropertyType_Int16, &value, out);
}

static HRESULT WINAPI property_value_statics_CreateUInt16(IPropertyValueStatics *iface, UINT16 value, IInspectable **out)
{
    TRACE("iface %p, value %u, out %p\n", iface, value, out);
    return create_property_value(PropertyType_UInt16, &value, out);
}

static HRESULT WINAPI property_value_statics_CreateInt32(IPropertyValueStatics *iface, INT32 value, IInspectable **out)
{
    TRACE("iface %p, value %d, out %p\n", iface, value, out);
    return create_property_value(PropertyType_Int32, &value, out);
}

static HRESULT WINAPI property_value_statics_CreateUInt32(IPropertyValueStatics *iface, UINT32 value, IInspectable **out)
{
    TRACE("iface %p, value %u, out %p\n", iface, value, out);
    return create_property_value(PropertyType_UInt32, &value, out);
}

static HRESULT WINAPI property_value_statics_CreateInt64(IPropertyValueStatics *iface, INT64 value, IInspectable **out)
{
    TRACE("iface %p, value %s, out %p\n", iface, wine_dbgstr_longlong(value), out);
    return create_property_value(PropertyType_Int64, &value, out);
}

static HRESULT WINAPI property_value_statics_CreateUInt64(IPropertyValueStatics *iface, UINT64 value, IInspectable **out)
{
    TRACE("iface %p, value %s, out %p\n", iface, wine_dbgstr_longlong(value), out);
    return create_property_value(PropertyType_UInt64, &value, out);
}

static HRESULT WINAPI property_value_statics_CreateSingle(IPropertyValueStatics *iface, FLOAT value, IInspectable **out)
{
    TRACE("iface %p, value %f, out %p\n", iface, value, out);
    return create_property_value(PropertyType_Single, &value, out);
}

static HRESULT WINAPI property_value_statics_CreateDouble(IPropertyValueStatics *iface, DOUBLE value, IInspectable **out)
{
    TRACE("iface %p, value %f, out %p\n", iface, value, out);
    return create_property_value(PropertyType_Double, &value, out);
}

static HRESULT WINAPI property_value_statics_CreateChar16(IPropertyValueStatics *iface, WCHAR value, IInspectable **out)
{
    TRACE("iface %p, value %c, out %p\n", iface, value, out);
    return create_property_value(PropertyType_Char16, &value, out);
}

static HRESULT WINAPI property_value_statics_CreateBoolean(IPropertyValueStatics *iface, BOOLEAN value, IInspectable **out)
{
    TRACE("iface %p, value %d, out %p\n", iface, value, out);
    return create_property_value(PropertyType_Boolean, &value, out);
}

static HRESULT WINAPI property_value_statics_CreateString(IPropertyValueStatics *iface, HSTRING value, IInspectable **out)
{
    TRACE("iface %p, value %p, out %p\n", iface, value, out);
    return create_property_value(PropertyType_String, &value, out);
}

static HRESULT WINAPI property_value_statics_CreateGuid(IPropertyValueStatics *iface, GUID value, IInspectable **out)
{
    TRACE("iface %p, value %s, out %p\n", iface, debugstr_guid(&value), out);
    return create_property_value(PropertyType_Guid, &value, out);
}

/* Stub implementations for other factory methods */
static HRESULT WINAPI property_value_statics_CreateDateTime(IPropertyValueStatics *iface, DateTime value, IInspectable **out)
{
    FIXME("iface %p, value %p, out %p stub!\n", iface, &value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateTimeSpan(IPropertyValueStatics *iface, TimeSpan value, IInspectable **out)
{
    FIXME("iface %p, value %p, out %p stub!\n", iface, &value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreatePoint(IPropertyValueStatics *iface, Point value, IInspectable **out)
{
    FIXME("iface %p, value %p, out %p stub!\n", iface, &value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateSize(IPropertyValueStatics *iface, Size value, IInspectable **out)
{
    FIXME("iface %p, value %p, out %p stub!\n", iface, &value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateRect(IPropertyValueStatics *iface, Rect value, IInspectable **out)
{
    FIXME("iface %p, value %p, out %p stub!\n", iface, &value, out);
    return E_NOTIMPL;
}

/* Array creation stubs */
static HRESULT WINAPI property_value_statics_CreateUInt8Array(IPropertyValueStatics *iface, UINT32 length, UINT8 *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateInt16Array(IPropertyValueStatics *iface, UINT32 length, INT16 *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateUInt16Array(IPropertyValueStatics *iface, UINT32 length, UINT16 *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateInt32Array(IPropertyValueStatics *iface, UINT32 length, INT32 *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateUInt32Array(IPropertyValueStatics *iface, UINT32 length, UINT32 *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateInt64Array(IPropertyValueStatics *iface, UINT32 length, INT64 *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateUInt64Array(IPropertyValueStatics *iface, UINT32 length, UINT64 *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateSingleArray(IPropertyValueStatics *iface, UINT32 length, FLOAT *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateDoubleArray(IPropertyValueStatics *iface, UINT32 length, DOUBLE *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateChar16Array(IPropertyValueStatics *iface, UINT32 length, WCHAR *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateBooleanArray(IPropertyValueStatics *iface, UINT32 length, BOOLEAN *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateStringArray(IPropertyValueStatics *iface, UINT32 length, HSTRING *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateInspectableArray(IPropertyValueStatics *iface, UINT32 length, IInspectable **value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateGuidArray(IPropertyValueStatics *iface, UINT32 length, GUID *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateDateTimeArray(IPropertyValueStatics *iface, UINT32 length, DateTime *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateTimeSpanArray(IPropertyValueStatics *iface, UINT32 length, TimeSpan *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreatePointArray(IPropertyValueStatics *iface, UINT32 length, Point *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateSizeArray(IPropertyValueStatics *iface, UINT32 length, Size *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_value_statics_CreateRectArray(IPropertyValueStatics *iface, UINT32 length, Rect *value, IInspectable **out)
{
    FIXME("iface %p, length %u, value %p, out %p stub!\n", iface, length, value, out);
    return E_NOTIMPL;
}

static const IPropertyValueStaticsVtbl property_value_statics_vtbl =
{
    property_value_statics_QueryInterface,
    property_value_statics_AddRef,
    property_value_statics_Release,
    property_value_statics_GetIids,
    property_value_statics_GetRuntimeClassName,
    property_value_statics_GetTrustLevel,
    property_value_statics_CreateUInt8,
    property_value_statics_CreateInt16,
    property_value_statics_CreateUInt16,
    property_value_statics_CreateInt32,
    property_value_statics_CreateUInt32,
    property_value_statics_CreateInt64,
    property_value_statics_CreateUInt64,
    property_value_statics_CreateSingle,
    property_value_statics_CreateDouble,
    property_value_statics_CreateChar16,
    property_value_statics_CreateBoolean,
    property_value_statics_CreateString,
    property_value_statics_CreateGuid,
    property_value_statics_CreateDateTime,
    property_value_statics_CreateTimeSpan,
    property_value_statics_CreatePoint,
    property_value_statics_CreateSize,
    property_value_statics_CreateRect,
    property_value_statics_CreateUInt8Array,
    property_value_statics_CreateInt16Array,
    property_value_statics_CreateUInt16Array,
    property_value_statics_CreateInt32Array,
    property_value_statics_CreateUInt32Array,
    property_value_statics_CreateInt64Array,
    property_value_statics_CreateUInt64Array,
    property_value_statics_CreateSingleArray,
    property_value_statics_CreateDoubleArray,
    property_value_statics_CreateChar16Array,
    property_value_statics_CreateBooleanArray,
    property_value_statics_CreateStringArray,
    property_value_statics_CreateInspectableArray,
    property_value_statics_CreateGuidArray,
    property_value_statics_CreateDateTimeArray,
    property_value_statics_CreateTimeSpanArray,
    property_value_statics_CreatePointArray,
    property_value_statics_CreateSizeArray,
    property_value_statics_CreateRectArray
};

/* IActivationFactory implementation for PropertyValue */
static HRESULT WINAPI property_value_factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    struct property_value_statics *impl;

    impl = impl_from_IActivationFactory(iface);
    return IPropertyValueStatics_QueryInterface(&impl->IPropertyValueStatics_iface, iid, out);
}

static ULONG WINAPI property_value_factory_AddRef(IActivationFactory *iface)
{
    struct property_value_statics *impl;

    impl = impl_from_IActivationFactory(iface);
    return IPropertyValueStatics_AddRef(&impl->IPropertyValueStatics_iface);
}

static ULONG WINAPI property_value_factory_Release(IActivationFactory *iface)
{
    struct property_value_statics *impl;

    impl = impl_from_IActivationFactory(iface);
    return IPropertyValueStatics_Release(&impl->IPropertyValueStatics_iface);
}

static HRESULT WINAPI property_value_factory_GetIids(IActivationFactory *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    *iid_count = 0;
    *iids = NULL;
    return S_OK;
}

static HRESULT WINAPI property_value_factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *class_name)
{
    TRACE("iface %p, class_name %p\n", iface, class_name);
    return WindowsCreateString(L"Windows.Foundation.PropertyValue", 33, class_name);
}

static HRESULT WINAPI property_value_factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *trust_level)
{
    TRACE("iface %p, trust_level %p\n", iface, trust_level);
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI property_value_factory_ActivateInstance(IActivationFactory *iface, IInspectable **instance)
{
    FIXME("iface %p, instance %p stub!\n", iface, instance);
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl property_value_factory_vtbl =
{
    property_value_factory_QueryInterface,
    property_value_factory_AddRef,
    property_value_factory_Release,
    property_value_factory_GetIids,
    property_value_factory_GetRuntimeClassName,
    property_value_factory_GetTrustLevel,
    property_value_factory_ActivateInstance
};

/* Global statics instance */
static struct property_value_statics property_value_statics_impl =
{
    { &property_value_factory_vtbl },
    { &property_value_statics_vtbl },
    1
};

/* Public factory getter */
HRESULT get_property_value_factory(IActivationFactory **out)
{
    TRACE("out %p\n", out);

    if (!out)
        return E_POINTER;

    *out = &property_value_statics_impl.IActivationFactory_iface;
    IActivationFactory_AddRef(*out);

    return S_OK;
}
