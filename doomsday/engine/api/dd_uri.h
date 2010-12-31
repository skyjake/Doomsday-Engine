/**\file dd_uri.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBDENG_API_URI_H
#define LIBDENG_API_URI_H

#include "dd_string.h"

/**
 * Uri. Convenient interface class designed to assist working with URIs
 *      (Universal Resource Identifier) to engine-managed resources.
 * @todo: Derive from Qt::QUrl
 */
#define URI_MINSCHEMELENGTH 2
typedef struct dduri_s {
    ddstring_t _scheme;
    ddstring_t _path;
} dduri_t;

dduri_t* Uri_ConstructDefault(void);
dduri_t* Uri_Construct2(const char* path, resourceclass_t defaultResourceClass);
dduri_t* Uri_Construct(const char* path);

void Uri_Destruct(dduri_t* uri);

void Uri_Clear(dduri_t* uri);

ddstring_t* Uri_Resolved(const dduri_t* uri);

const ddstring_t* Uri_Scheme(const dduri_t* uri);
const ddstring_t* Uri_Path(const dduri_t* uri);

void Uri_SetUri3(dduri_t* uri, const char* path, resourceclass_t defaultResourceClass);
void Uri_SetUri2(dduri_t* uri, const char* path);
void Uri_SetUri(dduri_t* uri, const ddstring_t* path);

ddstring_t* Uri_ComposePath(const dduri_t* uri);

ddstring_t* Uri_ToString(const dduri_t* uri);

#endif /* LIBDENG_API_URI_H */
