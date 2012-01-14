/**
 * @file uri.h
 * Universal Resource Identifier.
 *
 * @ingroup base
 *
 * Convenient interface class designed to assist working with URIs (Universal
 * Resource Identifier) to engine-managed resources. This class is based on the
 * semantics defined for the QUrl class, a component of the Qt GUI Toolkit.
 *
 * @todo Derive from Qt::QUrl
 *
 * @authors Daniel Swanson <danij@dengine.net>
 * @authors Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_API_URI_H
#define LIBDENG_API_URI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dd_string.h"
#include "reader.h"
#include "writer.h"

struct uri_s; // The uri instance (opaque).

/**
 * Uri instance. Created with Uri_New() or one of the other constructors.
 */
typedef struct uri_s Uri;

/// Schemes must be at least this many characters.
#define URI_MINSCHEMELENGTH         2

Uri* Uri_New(void);
Uri* Uri_NewWithPath2(const char* path, resourceclass_t defaultResourceClass);
Uri* Uri_NewWithPath(const char* path);
Uri* Uri_NewCopy(const Uri* other);

/**
 * Constructs a Uri instance by reading it from @a reader.
 */
Uri* Uri_NewFromReader(Reader* reader);

void Uri_Delete(Uri* uri);

void Uri_Clear(Uri* uri);
Uri* Uri_Copy(Uri* uri, const Uri* other);

/**
 * Attempt to compose a resolved copy of this Uri. Substitutes known symbolics
 * in the possibly templated path. Resulting path is a well-formed, filesys
 * compatible path (perhaps base-relative).
 *
 * @return  Resolved path else @c NULL if non-resolvable. Caller should ensure
 *          to Str_Delete() when no longer needed.
 */
ddstring_t* Uri_Resolved(const Uri* uri);

const ddstring_t* Uri_Scheme(const Uri* uri);
const ddstring_t* Uri_Path(const Uri* uri);

void Uri_SetScheme(Uri* uri, const char* scheme);
void Uri_SetPath(Uri* uri, const char* path);

Uri* Uri_SetUri3(Uri* uri, const char* path, resourceclass_t defaultResourceClass);
Uri* Uri_SetUri2(Uri* uri, const char* path);
Uri* Uri_SetUri(Uri* uri, const ddstring_t* path);

ddstring_t* Uri_ComposePath(const Uri* uri);

ddstring_t* Uri_ToString(const Uri* uri);

boolean Uri_Equality(const Uri* uri, const Uri* other);

void Uri_Write(const Uri* uri, Writer* writer);

/**
 * Serialize @a uri without a scheme using @a writer. Use this only when the
 * scheme can be deduced from context without ambiguity.
 */
void Uri_WriteOmitScheme(const Uri* uri, Writer* writer);

void Uri_Read(Uri* uri, Reader* reader);

/**
 * Deserializes @a uri using @a reader. If the deserialized Uri lacks a scheme,
 * @a defaultScheme will be used instead.
 */
void Uri_ReadWithDefaultScheme(Uri* uri, Reader* reader, const char* defaultScheme);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_API_URI_H */
