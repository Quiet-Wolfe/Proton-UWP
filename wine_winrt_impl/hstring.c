/*
 * HSTRING implementation for Wine
 *
 * Copyright 2024 Proton UWP Project
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
 */

/*
 * HSTRING is an immutable, reference-counted string type used by WinRT.
 * Format: [header][string data]\0
 * Header contains: refcount, length, padding
 */

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winstring.h"
#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(hstring);

/* HSTRING internal structure */
struct hstring_private
{
    LONG refcount;
    UINT32 length;
    UINT32 padding;  /* For alignment */
    WCHAR buffer[1]; /* Variable length */
};

#define HSTRING_HEADER_SIZE offsetof(struct hstring_private, buffer)

/***********************************************************************
 *      WindowsCreateString (combase.@)
 */
HRESULT WINAPI WindowsCreateString(const WCHAR *source, UINT32 length, HSTRING *hstring)
{
    struct hstring_private *priv;
    UINT32 size;

    TRACE("(%s, %u, %p)\n", debugstr_wn(source, length), length, hstring);

    if (hstring == NULL)
        return E_INVALIDARG;

    *hstring = NULL;

    if (source == NULL && length > 0)
        return E_POINTER;

    if (length == 0)
    {
        *hstring = NULL;
        return S_OK;
    }

    size = HSTRING_HEADER_SIZE + (length + 1) * sizeof(WCHAR);
    priv = heap_alloc(size);
    if (!priv)
        return E_OUTOFMEMORY;

    priv->refcount = 1;
    priv->length = length;
    priv->padding = 0;
    memcpy(priv->buffer, source, length * sizeof(WCHAR));
    priv->buffer[length] = 0;

    *hstring = (HSTRING)priv;

    TRACE("Created HSTRING %p: '%s'\n", *hstring, debugstr_w(priv->buffer));

    return S_OK;
}

/***********************************************************************
 *      WindowsCreateStringReference (combase.@)
 */
HRESULT WINAPI WindowsCreateStringReference(const WCHAR *source, UINT32 length,
                                             HSTRING_HEADER *header, HSTRING *hstring)
{
    struct hstring_private *priv;

    TRACE("(%s, %u, %p, %p)\n", debugstr_wn(source, length), length, header, hstring);

    if (hstring == NULL || header == NULL)
        return E_INVALIDARG;

    if (source == NULL && length > 0)
        return E_POINTER;

    if (length == 0)
    {
        *hstring = NULL;
        return S_OK;
    }

    /* Use the header as storage for the string reference */
    priv = (struct hstring_private *)header;
    priv->refcount = 0;  /* Reference, not owned */
    priv->length = length;
    priv->padding = 0;

    /* The buffer pointer points to the external string */
    *hstring = (HSTRING)priv;
    /* Store the source pointer in a way that's retrievable */
    *(const WCHAR **)&priv->buffer = source;

    TRACE("Created string reference %p: '%s'\n", *hstring, debugstr_wn(source, length));

    return S_OK;
}

/***********************************************************************
 *      WindowsDeleteString (combase.@)
 */
HRESULT WINAPI WindowsDeleteString(HSTRING hstring)
{
    struct hstring_private *priv = (struct hstring_private *)hstring;

    TRACE("(%p)\n", hstring);

    if (hstring == NULL)
        return S_OK;

    /* Don't delete string references (refcount == 0) */
    if (priv->refcount == 0)
        return S_OK;

    if (InterlockedDecrement(&priv->refcount) == 0)
    {
        TRACE("Freeing HSTRING %p\n", hstring);
        heap_free(priv);
    }

    return S_OK;
}

/***********************************************************************
 *      WindowsDuplicateString (combase.@)
 */
HRESULT WINAPI WindowsDuplicateString(HSTRING hstring, HSTRING *newstring)
{
    struct hstring_private *priv = (struct hstring_private *)hstring;

    TRACE("(%p, %p)\n", hstring, newstring);

    if (newstring == NULL)
        return E_INVALIDARG;

    if (hstring == NULL)
    {
        *newstring = NULL;
        return S_OK;
    }

    /* For owned strings, increment refcount */
    if (priv->refcount > 0)
    {
        InterlockedIncrement(&priv->refcount);
        *newstring = hstring;
        return S_OK;
    }

    /* For string references, create a new owned string */
    return WindowsCreateString((const WCHAR *)priv->buffer, priv->length, newstring);
}

/***********************************************************************
 *      WindowsGetStringRawBuffer (combase.@)
 */
const WCHAR * WINAPI WindowsGetStringRawBuffer(HSTRING hstring, UINT32 *length)
{
    struct hstring_private *priv = (struct hstring_private *)hstring;

    TRACE("(%p, %p)\n", hstring, length);

    if (hstring == NULL)
    {
        if (length) *length = 0;
        return NULL;
    }

    if (length)
        *length = priv->length;

    /* For references, the buffer pointer is stored differently */
    if (priv->refcount == 0)
        return *(const WCHAR **)&priv->buffer;

    return priv->buffer;
}

/***********************************************************************
 *      WindowsGetStringLen (combase.@)
 */
UINT32 WINAPI WindowsGetStringLen(HSTRING hstring)
{
    struct hstring_private *priv = (struct hstring_private *)hstring;

    TRACE("(%p)\n", hstring);

    if (hstring == NULL)
        return 0;

    return priv->length;
}

/***********************************************************************
 *      WindowsIsStringEmpty (combase.@)
 */
BOOL WINAPI WindowsIsStringEmpty(HSTRING hstring)
{
    TRACE("(%p)\n", hstring);

    return hstring == NULL || WindowsGetStringLen(hstring) == 0;
}

/***********************************************************************
 *      WindowsStringHasEmbeddedNull (combase.@)
 */
HRESULT WINAPI WindowsStringHasEmbeddedNull(HSTRING hstring, BOOL *hasEmbedNull)
{
    const WCHAR *buffer;
    UINT32 length, i;

    TRACE("(%p, %p)\n", hstring, hasEmbedNull);

    if (hasEmbedNull == NULL)
        return E_INVALIDARG;

    *hasEmbedNull = FALSE;

    if (hstring == NULL)
        return S_OK;

    buffer = WindowsGetStringRawBuffer(hstring, &length);

    for (i = 0; i < length; i++)
    {
        if (buffer[i] == 0)
        {
            *hasEmbedNull = TRUE;
            return S_OK;
        }
    }

    return S_OK;
}

/***********************************************************************
 *      WindowsCompareStringOrdinal (combase.@)
 */
HRESULT WINAPI WindowsCompareStringOrdinal(HSTRING hstring1, HSTRING hstring2, INT32 *result)
{
    const WCHAR *str1, *str2;
    UINT32 len1, len2;
    int cmp;

    TRACE("(%p, %p, %p)\n", hstring1, hstring2, result);

    if (result == NULL)
        return E_INVALIDARG;

    str1 = WindowsGetStringRawBuffer(hstring1, &len1);
    str2 = WindowsGetStringRawBuffer(hstring2, &len2);

    if (str1 == NULL && str2 == NULL)
    {
        *result = 0;
        return S_OK;
    }

    if (str1 == NULL)
    {
        *result = -1;
        return S_OK;
    }

    if (str2 == NULL)
    {
        *result = 1;
        return S_OK;
    }

    cmp = wcsncmp(str1, str2, min(len1, len2));

    if (cmp == 0)
    {
        if (len1 < len2)
            *result = -1;
        else if (len1 > len2)
            *result = 1;
        else
            *result = 0;
    }
    else
    {
        *result = cmp;
    }

    return S_OK;
}

/***********************************************************************
 *      WindowsSubstring (combase.@)
 */
HRESULT WINAPI WindowsSubstring(HSTRING hstring, UINT32 start, HSTRING *newstring)
{
    const WCHAR *buffer;
    UINT32 length;

    TRACE("(%p, %u, %p)\n", hstring, start, newstring);

    if (newstring == NULL)
        return E_INVALIDARG;

    *newstring = NULL;

    buffer = WindowsGetStringRawBuffer(hstring, &length);

    if (start > length)
        return E_BOUNDS;

    if (start == length)
        return S_OK;

    return WindowsCreateString(buffer + start, length - start, newstring);
}

/***********************************************************************
 *      WindowsSubstringWithSpecifiedLength (combase.@)
 */
HRESULT WINAPI WindowsSubstringWithSpecifiedLength(HSTRING hstring, UINT32 start,
                                                    UINT32 length, HSTRING *newstring)
{
    const WCHAR *buffer;
    UINT32 str_length;

    TRACE("(%p, %u, %u, %p)\n", hstring, start, length, newstring);

    if (newstring == NULL)
        return E_INVALIDARG;

    *newstring = NULL;

    buffer = WindowsGetStringRawBuffer(hstring, &str_length);

    if (start + length > str_length)
        return E_BOUNDS;

    if (length == 0)
        return S_OK;

    return WindowsCreateString(buffer + start, length, newstring);
}

/***********************************************************************
 *      WindowsConcatString (combase.@)
 */
HRESULT WINAPI WindowsConcatString(HSTRING hstring1, HSTRING hstring2, HSTRING *newstring)
{
    const WCHAR *str1, *str2;
    UINT32 len1, len2;
    WCHAR *buffer;
    HRESULT hr;

    TRACE("(%p, %p, %p)\n", hstring1, hstring2, newstring);

    if (newstring == NULL)
        return E_INVALIDARG;

    *newstring = NULL;

    str1 = WindowsGetStringRawBuffer(hstring1, &len1);
    str2 = WindowsGetStringRawBuffer(hstring2, &len2);

    if (len1 == 0 && len2 == 0)
        return S_OK;

    if (len1 == 0)
        return WindowsDuplicateString(hstring2, newstring);

    if (len2 == 0)
        return WindowsDuplicateString(hstring1, newstring);

    buffer = heap_alloc((len1 + len2) * sizeof(WCHAR));
    if (!buffer)
        return E_OUTOFMEMORY;

    memcpy(buffer, str1, len1 * sizeof(WCHAR));
    memcpy(buffer + len1, str2, len2 * sizeof(WCHAR));

    hr = WindowsCreateString(buffer, len1 + len2, newstring);
    heap_free(buffer);

    return hr;
}
