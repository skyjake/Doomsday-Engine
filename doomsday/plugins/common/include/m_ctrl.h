/** @file m_ctrl.h  Controls menu page and associated widgets.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_MENU_CONTROLS
#define LIBCOMMON_MENU_CONTROLS

#include "hu_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

void Hu_MenuInitControlsPage(void);
void Hu_MenuDrawControlsPage(mn_page_t *page, Point2Raw const *origin);
void Hu_MenuControlGrabDrawer(char const *niceName, float alpha);

/**
 * Bindings visualizer widget.
 */
typedef struct mndata_bindings_s {
    char const *text;
    char const *bindContext;
    char const *controlName;
    char const *command;
    int flags;
} mndata_bindings_t;

mn_object_t *MNBindings_New(void);
void MNBindings_Delete(mn_object_t *ob);

void MNBindings_Ticker(mn_object_t *ob);
void MNBindings_Drawer(mn_object_t *ob, Point2Raw const *origin);
int MNBindings_CommandResponder(mn_object_t *ob, menucommand_e command);
int MNBindings_PrivilegedResponder(mn_object_t *ob, event_t *ev);
void MNBindings_UpdateGeometry(mn_object_t *ob, mn_page_t *page);

char const *MNBindings_ControlName(mn_object_t *ob);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_MENU_CONTROLS
