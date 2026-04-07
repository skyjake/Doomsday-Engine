/** @file api_uri.h  Public API for Universal Resource Identifiers.
 * @ingroup base
 *
 * @authores::Uriight &copy; 2010-2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2010-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_API_URI_H
#define DE_API_URI_H

#include "api_base.h"
#include <de/legacy/str.h>
#include <de/legacy/reader.h>
#include <de/legacy/writer.h>

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
 * @defgroup uriComposeAsTextFlags  Uri Compose As Text Flags
 * @ingroup base
 *
 * Flags which determine how a textual URI representation is composited.
 */
///@{
#define UCTF_OMITSCHEME      0x1 ///< Exclude the scheme.
#define UCTF_OMITPATH        0x2 ///< Exclude the path.
#define UCTF_DECODEPATH      0x4 ///< Decode percent-endcoded characters in the path.

#define DEFAULT_URI_COMPOSE_AS_TEXT_FLAGS 0
///@}

struct uri_s; // The uri instance (opaque).

/**
 * Uri instance. Created with Uri_New() or one of the other constructors.
 */
typedef struct uri_s UriWrapper;
typedef UriWrapper Uri;

DE_API_TYPEDEF(Uri) // v1
{
    de_api_t api;

    /**
     * Constructs a default (empty) Uri instance. The uri should be destroyed with
     * Uri_Delete() once it is no longer needed.
     */
    UriWrapper* (*New)(void);

    UriWrapper* (*NewWithPath3)(const char *defaultScheme, const char *path);

    /**
     * Constructs a Uri instance from @a path. The uri should be destroyed with
     * Uri_Delete() once it is no longer needed.
     *
     * @param path  Path to be parsed. Assumed to be in percent-encoded representation.
     * @param defaultResourceClass  If no scheme is defined in @a path and this is not @c FC_NULL,
     *      look for an appropriate default scheme for this class of resource.
     */
    UriWrapper* (*NewWithPath2)(const char *path, resourceclassid_t defaultResourceClass);

    UriWrapper* (*NewWithPath)(const char *path);

    /**
     * Constructs a Uri instance by duplicating @a other. The uri should be destroyed
     * with Uri_Delete() once it is no longer needed.
     */
    UriWrapper* (*Dup)(UriWrapper const* other);

    /**
     * Constructs a Uri instance by reading it from @a reader.  The uri should be
     * destroyed with Uri_Delete() once it is no longer needed.
     */
    UriWrapper* (*FromReader)(Reader1* reader);

    /**
     * Destroys the uri.
     */
    void (*Delete)(UriWrapper* uri);

    /**
     * Returns true if the path component of the URI is empty; otherwise false.
     * @param uri  Uri instance.
     */
    dd_bool (*IsEmpty)(UriWrapper const* uri);

    /**
     * Clears the uri, returning it to an empty state.
     * @param uri  Uri instance.
     * @return  Same as @a uri, for caller convenience.
     */
    UriWrapper* (*Clear)(UriWrapper* uri);

    /**
     * Copy the contents of @a other into this uri.
     *
     * @param uri  Uri instance.
     * @param other  Uri to be copied.
     * @return  Same as @a uri, for caller convenience.
     */
    UriWrapper* (*Copy)(Uri* uri, UriWrapper const* other);

    /**
     * Attempt to compose a resolved copy of this Uri. Substitutes known symbolics
     * in the possibly templated path. Resulting path is a well-formed, filesys
     * compatible path (perhaps base-relative).
     *
     * @param uri  Uri instance.
     *
     * @return  Resolved path else @c NULL if non-resolvable.
     */
    AutoStr* (*Resolved)(UriWrapper const* uri);

    /**
     * @param uri  Uri instance.
     * @return  Plain-text String representation of the current scheme.
     */
    const Str* (*Scheme)(UriWrapper const* uri);

    /**
     * @param uri  Uri instance.
     * @return  Plain-text String representation of the current path.
     */
    const Str* (*Path)(UriWrapper const* uri);

    /**
     * @param uri     Uri instance.
     * @param scheme  New scheme to be parsed.
     * @return  Same as @a uri, for caller convenience.
     */
    UriWrapper* (*SetScheme)(UriWrapper* uri, char const* scheme);

    /**
     * @param uri   Uri instance.
     * @param path  New path to be parsed.
     * @return  Same as @a uri, for caller convenience.
     */
    UriWrapper* (*SetPath)(UriWrapper* uri, char const* path);

    /**
     * Update uri by parsing new values from the specified arguments.
     *
     * @param uri   Uri instance.
     * @param path  Path to be parsed. Assumed to be in percent-encoded representation.
     * @param defaultResourceClass  If no scheme is defined in @a path and this is not
     *              @c FC_NULL, look for an appropriate default scheme for this class
     *              of resource.
     *
     * @return  Same as @a uri, for caller convenience.
     */
    UriWrapper* (*SetUri2)(UriWrapper* uri, char const* path, resourceclassid_t defaultResourceClass);

    UriWrapper* (*SetUri)(UriWrapper* uri, char const* path/* defaultResourceClass = RC_IMPLICIT*/);

    UriWrapper* (*SetUriStr)(UriWrapper* uri, ddstring_t const* path);

    /**
     * Transform the uri into a plain-text representation. Any internal encoding method
     * or symbolic identifiers will be included in their original, unresolved form in
     * the resultant string.
     *
     * @param uri  Uri instance.
     * @param flags  @ref uriComposeAsTextFlags.
     *
     * @return  Plain-text String representation.
     */
    AutoStr* (*Compose2)(UriWrapper const* uri, int flags);

    AutoStr* (*Compose)(UriWrapper const* uri);

    /**
     * Transform the uri into a human-friendly representation (percent decoding is done).
     *
     * @param uri  Uri instance.
     *
     * @return  Human-friendly String representation.
     */
    AutoStr* (*ToString)(UriWrapper const* uri);

    /**
     * Are these two uri instances considered equal once resolved?
     *
     * @todo Return a delta of lexicographical difference.
     *
     * @param uri  Uri instance.
     * @param other  Other uri instance.
     */
    dd_bool (*Equality)(UriWrapper const* uri, UriWrapper const* other);

    /**
     * Serialize @a uri using @a writer.
     *
     * @note Scheme should only be omitted when it can be unambiguously deduced from context.
     *
     * @param uri               Uri instance.
     * @param writer            Writer instance.
     * @param omitComponents    @ref uriComponentFlags
     */
    void (*Write2)(UriWrapper const* uri, Writer1* writer, int omitComponents);

    void (*Write)(UriWrapper const* uri, Writer1* writer/*, omitComponents = 0 (include everything)*/);

    /**
     * Deserializes @a uri using @a reader.
     *
     * @param uri  Uri instance.
     * @param reader  Reader instance.
     * @return  Same as @a uri, for caller convenience.
     */
    UriWrapper* (*Read)(UriWrapper* uri, Reader1* reader);

    /**
     * Deserializes @a uri using @a reader. If the deserialized Uri lacks a scheme,
     * @a defaultScheme will be used instead.
     *
     * @param uri               Uri instance.
     * @param reader            Reader instance.
     * @param defaultScheme     Default scheme.
     */
    void (*ReadWithDefaultScheme)(UriWrapper* uri, Reader1* reader, char const* defaultScheme);

} DE_API_T(Uri);

// Macros for accessing exported functions.
#ifndef DE_NO_API_MACROS_URI
#define Uri_New                     _api_Uri.New
#define Uri_NewWithPath3            _api_Uri.NewWithPath3
#define Uri_NewWithPath2            _api_Uri.NewWithPath2
#define Uri_NewWithPath             _api_Uri.NewWithPath
#define Uri_Dup                     _api_Uri.Dup
#define Uri_FromReader              _api_Uri.FromReader
#define Uri_Delete                  _api_Uri.Delete
#define Uri_IsEmpty                 _api_Uri.IsEmpty
#define Uri_Clear                   _api_Uri.Clear
#define Uri_Copy                    _api_Uri.Copy
#define Uri_Resolved                _api_Uri.Resolved
#define Uri_Scheme                  _api_Uri.Scheme
#define Uri_Path                    _api_Uri.Path
#define Uri_SetScheme               _api_Uri.SetScheme
#define Uri_SetPath                 _api_Uri.SetPath
#define Uri_SetUri2                 _api_Uri.SetUri2
#define Uri_SetUri                  _api_Uri.SetUri
#define Uri_SetUriStr               _api_Uri.SetUriStr
#define Uri_Compose                 _api_Uri.Compose
#define Uri_Compose2                _api_Uri.Compose2
#define Uri_ToString                _api_Uri.ToString
#define Uri_Equality                _api_Uri.Equality
#define Uri_Write2                  _api_Uri.Write2
#define Uri_Write                   _api_Uri.Write
#define Uri_Read                    _api_Uri.Read
#define Uri_ReadWithDefaultScheme   _api_Uri.ReadWithDefaultScheme
#endif

// Internal access.
#ifdef __DOOMSDAY__
DE_USING_API(Uri);
#endif

#endif /* DE_API_URI_H */
