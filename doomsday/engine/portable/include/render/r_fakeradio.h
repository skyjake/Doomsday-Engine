/**\file r_fakeradio.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

/**
 * Faked Radiosity Lighting.
 *
 * Perhaps the most distinctive characteristic of radiosity lighting
 * is that the corners of a room are slightly dimmer than the rest of
 * the surfaces.  (It's not the only characteristic, however.)  We
 * will fake these shadowed areas by generating shadow polygons for
 * wall segments and determining, which BSP leaf vertices will be
 * shadowed.
 *
 * In other words, walls use shadow polygons (over entire hedges), while
 * planes use vertex lighting.  Since planes are usually tesselated
 * into a great deal of BSP leafs (and triangles), they are better
 * suited for vertex lighting.  In some cases we will be forced to
 * split a BSP leaf into smaller pieces than strictly necessary in
 * order to achieve better accuracy in the shadow effect.
 */

#ifndef LIBDENG_REFRESH_FAKERADIO_H
#define LIBDENG_REFRESH_FAKERADIO_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * To be called after map load to perform necessary initialization within this module.
 */
void R_InitFakeRadioForMap(void);

/// @return  @c true if @a lineDef qualifies as a (edge) shadow caster.
boolean R_IsShadowingLinedef(LineDef* lineDef);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_REFRESH_FAKERADIO_H */
