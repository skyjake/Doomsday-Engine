/**\file dd_uri.h
 *\section License
 * License: GPL
 * Online License Link: http://www.trolltech.com/gpl/
 *
 *\author Daniel Swanson <danij@dengine.net>
 *\author Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
 *
 * This file is based on the semantics defined for the QUrl class, a component
 * of the Qt GUI Toolkit.
 *
 * \todo Derive from Qt::QUrl
 */

#ifndef LIBDENG_API_URI_H
#define LIBDENG_API_URI_H

#include "dd_string.h"

/**
 * Uri. Convenient interface class designed to assist working with URIs
 *      (Universal Resource Identifier) to engine-managed resources.
 */
#define URI_MINSCHEMELENGTH 2
typedef struct dduri_s {
    ddstring_t _scheme;
    ddstring_t _path;
} dduri_t;

dduri_t* Uri_ConstructDefault(void);
dduri_t* Uri_Construct2(const char* path, resourceclass_t defaultResourceClass);
dduri_t* Uri_Construct(const char* path);
dduri_t* Uri_ConstructCopy(const dduri_t* other);

void Uri_Destruct(dduri_t* uri);

void Uri_Clear(dduri_t* uri);
dduri_t* Uri_Copy(dduri_t* uri, const dduri_t* other);

ddstring_t* Uri_Resolved(const dduri_t* uri);

const ddstring_t* Uri_Scheme(const dduri_t* uri);
const ddstring_t* Uri_Path(const dduri_t* uri);

void Uri_SetScheme(dduri_t* uri, const char* scheme);
void Uri_SetUri3(dduri_t* uri, const char* path, resourceclass_t defaultResourceClass);
void Uri_SetUri2(dduri_t* uri, const char* path);
void Uri_SetUri(dduri_t* uri, const ddstring_t* path);

ddstring_t* Uri_ComposePath(const dduri_t* uri);

ddstring_t* Uri_ToString(const dduri_t* uri);

boolean Uri_Equality(const dduri_t* uri, const dduri_t* other);

#endif /* LIBDENG_API_URI_H */
