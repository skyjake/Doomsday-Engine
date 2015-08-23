/** @file guiwidgetref.h  Smart pointer to a GuiWidget.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_GUIWIDGETREF_H
#define LIBAPPFW_GUIWIDGETREF_H

#include "../GuiWidget"

namespace de {

/**
 * Smart pointer to a GuiWidget. Does not own the target widget.
 */
template <typename WidgetType>
class GuiWidgetRef : DENG2_OBSERVES(Widget, Deletion)
{
public:
    GuiWidgetRef(WidgetType *ptr = nullptr) {
        reset(ptr);
    }
    ~GuiWidgetRef() {
        reset(nullptr);
    }
    void reset(WidgetType *ptr) {
        if(_ptr) _ptr->Widget::audienceForDeletion() -= this;
        _ptr = ptr;
        if(_ptr) _ptr->Widget::audienceForDeletion() += this;
    }
    WidgetType *operator -> () const {
        return _ptr;
    }
    operator WidgetType const * () const {
        return _ptr;
    }
    operator WidgetType * () {
        return _ptr;
    }
    explicit operator bool() const {
        return _ptr != nullptr;
    }
    void widgetBeingDeleted(Widget &widget) {
        if(&widget == _ptr) {
            _ptr = nullptr;
        }
    }

private:
    WidgetType *_ptr = nullptr;
};

} // namespace de

#endif // LIBAPPFW_GUIWIDGETREF_H

