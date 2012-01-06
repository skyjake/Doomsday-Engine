/**\file uri.h
 *\section License
 * License: GPL
 * Online License Link: http://www.trolltech.com/gpl/
 *
 *\author Daniel Swanson <danij@dengine.net>
 *\author Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
 *
 * This class is based on the semantics defined for the QUrl class, a component
 * of the Qt GUI Toolkit.
 *
 * \todo Derive from Qt::QUrl
 */

#ifndef LIBDENG_API_URI_H
#define LIBDENG_API_URI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dd_string.h"
#include "reader.h"
#include "writer.h"

/**
 * Uri: Convenient interface class designed to assist working with URIs
 * (Universal Resource Identifier) to engine-managed resources.
 */
struct uri_s; // The uri instance (opaque).
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
 *          to Str_Delete when no longer needed.
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
void Uri_Read(Uri* uri, Reader* reader);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_API_URI_H */
