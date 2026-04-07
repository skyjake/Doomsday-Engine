/** @file inlinelistwidget.h  UI widget for a selectable, inline-list of items.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_UI_INLINELISTWIDGET
#define LIBCOMMON_UI_INLINELISTWIDGET

#include "listwidget.h"

namespace common {
namespace menu {

/**
 * @ingroup menu
 */
class InlineListWidget : public ListWidget
{
public:
    InlineListWidget();
    virtual ~InlineListWidget();

    void draw() const;
    void updateGeometry();
    int handleCommand(menucommand_e command);
};

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_INLINELISTWIDGET
