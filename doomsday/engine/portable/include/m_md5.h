/**
 * @file m_md5.h
 *
 * MD5 Hash Generator.
 *
 * @ingroup misc
 *
 * @author Copyright &copy; 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MISC_MD5_H
#define LIBDENG_MISC_MD5_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MD5_BLOCK_WORDS         16
#define MD5_HASH_WORDS          4

typedef struct md5_ctx {
    uint hash[MD5_HASH_WORDS];
    uint block[MD5_BLOCK_WORDS];
    uint byte_count;
    uint pad32;
} md5_ctx_t;

void md5_init(void* ctx);
void md5_update(void* ctx, byte const* data, uint len);
void md5_final(void* ctx, byte* out);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_MISC_MD5_H */
