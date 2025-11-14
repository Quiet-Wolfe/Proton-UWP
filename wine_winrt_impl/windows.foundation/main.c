/*
 * Windows.Foundation DLL main
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

WINE_DEFAULT_DEBUG_CHANNEL(foundation);

/* External declarations */
extern HRESULT get_property_set_factory(IActivationFactory **out);
extern HRESULT get_property_value_factory(IActivationFactory **out);
extern HRESULT create_uri_factory(IActivationFactory **out);

/***********************************************************************
 *      DllMain
 */
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    TRACE("(%p, %u, %p)\n", instance, reason, reserved);

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(instance);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}

/***********************************************************************
 *      DllGetActivationFactory
 *
 * Main entry point for WinRT class activation
 */
HRESULT WINAPI DllGetActivationFactory(HSTRING classid, IActivationFactory **factory)
{
    const WCHAR *class_name;
    UINT32 length;
    HRESULT hr;

    TRACE("(%p, %p)\n", classid, factory);

    if (!classid || !factory)
        return E_INVALIDARG;

    *factory = NULL;

    class_name = WindowsGetStringRawBuffer(classid, &length);
    if (!class_name)
        return E_INVALIDARG;

    TRACE("Requested class: %s\n", debugstr_w(class_name));

    /* Handle Windows.Foundation.Collections.PropertySet */
    if (wcscmp(class_name, L"Windows.Foundation.Collections.PropertySet") == 0)
    {
        hr = get_property_set_factory(factory);
        if (SUCCEEDED(hr))
        {
            TRACE("Returning PropertySet factory %p\n", *factory);
            return S_OK;
        }
        return hr;
    }

    /* Handle Windows.Foundation.PropertyValue */
    if (wcscmp(class_name, L"Windows.Foundation.PropertyValue") == 0)
    {
        hr = get_property_value_factory(factory);
        if (SUCCEEDED(hr))
        {
            TRACE("Returning PropertyValue factory %p\n", *factory);
            return S_OK;
        }
        return hr;
    }

    /* Handle Windows.Foundation.Uri */
    if (wcscmp(class_name, L"Windows.Foundation.Uri") == 0)
    {
        hr = create_uri_factory(factory);
        if (SUCCEEDED(hr))
        {
            TRACE("Returning Uri factory %p\n", *factory);
            return S_OK;
        }
        return hr;
    }

    WARN("Class %s not implemented\n", debugstr_w(class_name));
    return REGDB_E_CLASSNOTREG;
}
