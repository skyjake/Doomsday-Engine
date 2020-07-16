/** @file lineeditwidget.h  UI widget for an editable line of text.
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

#ifndef LIBCOMMON_UI_LINEEDITWIDGET
#define LIBCOMMON_UI_LINEEDITWIDGET

#include <de/string.h>
#include "widget.h"

namespace common {
namespace menu {

#if __JDOOM__ || __JDOOM64__
#  define MNDATA_EDIT_TEXT_COLORIDX             (0)
#  define MNDATA_EDIT_OFFSET_X                  (0)
#  define MNDATA_EDIT_OFFSET_Y                  (0)
#  define MNDATA_EDIT_BACKGROUND_OFFSET_X       (-11)
#  define MNDATA_EDIT_BACKGROUND_OFFSET_Y       (-4)
#  define MNDATA_EDIT_BACKGROUND_PATCH_LEFT     ("M_LSLEFT")
#  define MNDATA_EDIT_BACKGROUND_PATCH_RIGHT    ("M_LSRGHT")
#  define MNDATA_EDIT_BACKGROUND_PATCH_MIDDLE   ("M_LSCNTR")
#elif __JHERETIC__ || __JHEXEN__
#  define MNDATA_EDIT_TEXT_COLORIDX             (2)
#  define MNDATA_EDIT_OFFSET_X                  (13)
#  define MNDATA_EDIT_OFFSET_Y                  (5)
#  define MNDATA_EDIT_BACKGROUND_OFFSET_X       (-13)
#  define MNDATA_EDIT_BACKGROUND_OFFSET_Y       (-5)
//#  define MNDATA_EDIT_BACKGROUND_PATCH_LEFT   ("")
//#  define MNDATA_EDIT_BACKGROUND_PATCH_RIGHT  ("")
#  define MNDATA_EDIT_BACKGROUND_PATCH_MIDDLE   ("M_FSLOT")
#endif

/**
 * @defgroup mneditSetTextFlags  MNEdit Set Text Flags
 * @{
 */
#define MNEDIT_STF_NO_ACTION            0x1 /// Do not call any linked action function.
#define MNEDIT_STF_REPLACEOLD           0x2 /// Replace the "old" copy (used for canceled edits).
/**@}*/

/**
 * UI widget for an editable line of text.
 *
 * @ingroup menu
 */
class LineEditWidget : public Widget
{
public:
    LineEditWidget();
    virtual ~LineEditWidget();

    void draw() const;
    void updateGeometry();
    int handleEvent(const event_t &ev);
    int handleCommand(menucommand_e command);

    LineEditWidget &setMaxLength(int newMaxLength);
    int maxLength() const;

    /**
     * Change the current contents of the edit field.
     * @param newText  New text value.
     * @param flags    @ref mneditSetTextFlags
     */
    LineEditWidget &setText(const de::String &newText, int flags = MNEDIT_STF_NO_ACTION);

    /**
     * Returns a copy of the current editable value.
     */
    de::String text() const;

    LineEditWidget &setEmptyText(const de::String &newEmptyText);
    de::String emptyText() const;

public:
    static void loadResources();

private:
    DE_PRIVATE(d)
};

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_LINEEDITWIDGET
