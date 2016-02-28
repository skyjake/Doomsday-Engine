/** @file focuswidget.cpp  Input focus indicator.
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

#include "de/FocusWidget"

#include <QTimer>

namespace de {

static TimeDelta FLASH_SPAN = .3;

DENG2_PIMPL(FocusWidget)
{
    QTimer flashing;
    SafeWidgetPtr<GuiWidget const> reference;

    Instance(Public *i) : Base(i)
    {
        self.set(Background(Background::GradientFrame,
                            Style::get().colors().colorf("accent"), 6));

        flashing.setInterval(FLASH_SPAN.asMilliSeconds());
        flashing.setSingleShot(false);
    }

    void flash()
    {
        // Flashing depends on the reference widget's visibility.
        float const maxOpacity = (reference? reference->visibleOpacity() : 1.f);
        if(reference)
        {
            self.show(reference->isVisible());
        }

        if(self.opacity().target() == 0)
        {
            self.setOpacity(.8f * maxOpacity, FLASH_SPAN + .1, .1);
        }
        else if(self.opacity().target() > .5f)
        {
            self.setOpacity(.2f * maxOpacity, FLASH_SPAN);
        }
        else
        {
            self.setOpacity(.8f * maxOpacity, FLASH_SPAN);
        }
    }
};

FocusWidget::FocusWidget(String const &name)
    : LabelWidget(name)
    , d(new Instance(this))
{
    connect(&d->flashing, SIGNAL(timeout()), this, SLOT(updateFlash()));
}

void FocusWidget::startFlashing(GuiWidget const *reference)
{
    d->reference.reset(reference);
    show();
    if(!d->flashing.isActive())
    {
        setOpacity(0);
        d->flashing.start();
    }
}

void FocusWidget::stopFlashing()
{
    d->flashing.stop();
    hide();
}

void FocusWidget::updateFlash()
{
    d->flash();
}

} // namespace de

