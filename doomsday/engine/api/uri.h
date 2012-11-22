/**
 * @file uri.h
 * Universal Resource Identifier (C wrapper). @ingroup base
 *
 * @author Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2010-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "dd_types.h"
#include <de/str.h>
#include <de/reader.h>
#include <de/writer.h>

/// Schemes must be at least this many characters.
#define URI_MINSCHEMELENGTH 2

/**
 * @defgroup uriComponentFlags  Uri Component Flags
 * @ingroup base
 *
 * Flags which identify the logical components of a uri. Used with Uri_Write() to
 * indicate which components of the Uri should be serialized.
 */
///@{
#define UCF_SCHEME           0x1 ///< Scheme.
#define UCF_USER             0x2 ///< User. (Reserved) Not presently implemented.
#define UCF_PASSWORD         0x4 ///< Password. (Reserved) Not presently implemented.
#define UCF_HOST             0x8 ///< Host. (Reserved) Not presently implemented.
#define UCF_PORT            0x10 ///< Port. (Reserved) Not presently implemented.
#define UCF_PATH            0x20 ///< Path.
#define UCF_FRAGMENT        0x40 ///< Fragment. (Reserved) Not presently implemented.
#define UCF_QUERY           0x80 ///< Query. (Reserved) Not presently implemented.
///@}

/**
 * @defgroup printUriFlags  Print Uri Flags
 * @ingroup base
 */
///@{
#define UPF_OUTPUT_RESOLVED           0x1 ///< Include the resolved path in the output.
#define UPF_TRANSFORM_PATH_MAKEPRETTY 0x2 ///< Transform paths making them "pretty".

#define DEFAULT_PRINTURIFLAGS (UPF_OUTPUT_RESOLVED|UPF_TRANSFORM_PATH_MAKEPRETTY)
///@}

struct uri_s; // The uri instance (opaque).

/**
 * Uri instance. Created with Uri_New() or one of the other constructors.
 */
typedef struct uri_s Uri;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Constructs a default (empty) Uri instance. The uri should be destroyed with
 * Uri_Delete() once it is no longer needed.
 */
Uri* Uri_New(void);

/**
 * Constructs a Uri instance from @a path. The uri should be destroyed with
 * Uri_Delete() once it is no longer needed.
 *
 * @param path  Path to be parsed. Assumed to be in percent-encoded representation.
 * @param defaultResourceClasslasslass  If no scheme is defined in @a path and this is not @c FC_NULL,
 *      look for an appropriate default scheme for this class of resource.
 */
Uri* Uri_NewWithPath2(char const* path, resourceclassid_t defaultResourceClass);
Uri* Uri_NewWithPath(char const* path);

/**
 * Constructs a Uri instance by duplicating @a other. The uri should be destroyed
 * with Uri_Delete() once it is no longer needed.
 */
Uri* Uri_Dup(Uri const* other);

/**
 * Constructs a Uri instance by reading it from @a reader.  The uri should be
 * destroyed with Uri_Delete() once it is no longer needed.
 */
Uri* Uri_FromReader(Reader* reader);

/**
 * Destroys the uri.
 */
void Uri_Delete(Uri* uri);

/**
 * Returns true if the path component of the URI is empty; otherwise false.
 * @param uri  Uri instance.
 */
boolean Uri_IsEmpty(Uri const* uri);

/**
 * Clears the uri, returning it to an empty state.
 * @param uri  Uri instance.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_Clear(Uri* uri);

/**
 * Copy the contents of @a other into this uri.
 *
 * @param uri  Uri instance.
 * @param other  Uri to be copied.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_Copy(Uri* uri, Uri const* other);

/**
 * Attempt to compose a resolved copy of this Uri. Substitutes known symbolics
 * in the possibly templated path. Resulting path is a well-formed, filesys
 * compatible path (perhaps base-relative).
 *
 * @param uri  Uri instance.
 *
 * @return  Resolved path else @c NULL if non-resolvable.
 */
AutoStr* Uri_Resolved(Uri const* uri);

/**
 * @param uri  Uri instance.
 * @return  Plain-text String representation of the current scheme.
 */
const Str* Uri_Scheme(Uri const* uri);

/**
 * @param uri  Uri instance.
 * @return  Plain-text String representation of the current path.
 */
const Str* Uri_Path(Uri const* uri);

/**
 * @param uri     Uri instance.
 * @param scheme  New scheme to be parsed.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_SetScheme(Uri* uri, char const* scheme);

/**
 * @param uri   Uri instance.
 * @param path  New path to be parsed.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_SetPath(Uri* uri, char const* path);

/**
 * Update uri by parsing new values from the specified arguments.
 *
 * @param uri   Uri instance.
 * @param path  Path to be parsed. Assumed to be in percent-encoded representation.
 * @param defaultResourceClasslasslass  If no scheme is defined in @a path and this is not
 *              @c FC_NULL, look for an appropriate default scheme for this class
 *              of resource.
 *
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_SetUri2(Uri* uri, char const* path, resourceclassid_t defaultResourceClass);
Uri* Uri_SetUri(Uri* uri, char const* path/* defaultResourceClass = RC_UNKNOWN*/);

Uri* Uri_SetUriStr(Uri* uri, ddstring_t const* path);

/**
 * Transform the uri into a plain-text representation. Any internal encoding method
 * or symbolic identifiers will be included in their original, unresolved form in
 * the resultant string.
 *
 * @param uri  Uri instance.
 * @return  Plain-text String representation.
 */
AutoStr* Uri_Compose(Uri const* uri);

/**
 * Transform the uri into a human-friendly representation (percent decoding is done).
 *
 * @param uri  Uri instance.
 *
 * @return  Human-friendly String representation.
 */
AutoStr* Uri_ToString(Uri const* uri);

/**
 * Are these two uri instances considered equal once resolved?
 *
 * @todo Return a delta of lexicographical difference.
 *
 * @param uri  Uri instance.
 * @param other  Other uri instance.
 */
boolean Uri_Equality(Uri const* uri, Uri const* other);

/**
 * Serialize @a uri using @a writer.
 *
 * @note Scheme should only be omitted when it can be unambiguously deduced from context.
 *
 * @param uri               Uri instance.
 * @param writer            Writer instance.
 * @param omitComponents    @ref uriComponentFlags
 */
void Uri_Write2(Uri const* uri, Writer* writer, int omitComponents);
void Uri_Write(Uri const* uri, Writer* writer/*, omitComponents = 0 (include everything)*/);

/**
 * Deserializes @a uri using @a reader.
 *
 * @param uri  Uri instance.
 * @param reader  Reader instance.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_Read(Uri* uri, Reader* reader);

/**
 * Deserializes @a uri using @a reader. If the deserialized Uri lacks a scheme,
 * @a defaultScheme will be used instead.
 *
 * @param uri               Uri instance.
 * @param reader            Reader instance.
 * @param defaultScheme     Default scheme.
 */
void Uri_ReadWithDefaultScheme(Uri* uri, Reader* reader, char const* defaultScheme);

/**
 * Print debug output for @a uri.
 *
 * @param uri               Uri instance.
 * @param indent            Number of characters to indent the print output.
 * @param flags             @ref printUriFlags
 * @param unresolvedText    Text string to be printed if @a uri is not resolvable.
 */
void Uri_DebugPrint3(Uri const* uri, int indent, int flags, char const* unresolvedText);
void Uri_DebugPrint2(Uri const* uri, int indent, int flags/*, use the default unresolved text */);
void Uri_DebugPrint(Uri const* uri, int indent/*, flags = DEFAULT_PRINTURIFLAGS */);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_API_URI_H */
