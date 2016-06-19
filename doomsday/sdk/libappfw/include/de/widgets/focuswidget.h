/** @file focuswidget.h  Input focus indicator.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBAPPFW_FOCUSWIDGET_H
#define LIBAPPFW_FOCUSWIDGET_H

#include <de/LabelWidget>

namespace de {

/**
 * Input focus indicator.
 *
 * GuiRootWidget owns an instance of FocusWidget to show where the input focus
 * is currently.
 */
class LIBAPPFW_PUBLIC FocusWidget : public LabelWidget
{
    Q_OBJECT

public:
    FocusWidget(de::String const &name = "focus");

    void startFlashing(GuiWidget const *reference = nullptr);
    void stopFlashing();

    // Events.
    void update() override;

protected slots:
    void updateFlash();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_FOCUSWIDGET_H
