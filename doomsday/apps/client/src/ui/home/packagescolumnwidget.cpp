/** @file packagescolumnwidget.cpp
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/home/packagescolumnwidget.h"
#include "ui/home/packageswidget.h"

using namespace de;

DENG_GUI_PIMPL(PackagesColumnWidget)
{
    PackagesWidget *packages;

    Instance(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self.scrollArea();
        area.add(packages = new PackagesWidget("home-packages"));
        packages->rule()
                .setInput(Rule::Width, area.contentRule().width())
                .setInput(Rule::Top,   self.header().rule().bottom() +
                                       rule("gap"))
                .setInput(Rule::Left,  area.contentRule().left());
    }
};

PackagesColumnWidget::PackagesColumnWidget()
    : ColumnWidget("packages-column")
    , d(new Instance(this))
{
    header().title().setText(_E(s) "\n" _E(.) + tr("Packages"));
    header().info().setText(tr("Browse available packages and install new ones."));
    header().infoPanel().close(0);

    scrollArea().setContentSize(maximumContentWidth(),
                                header().rule().height() +
                                rule("gap") +
                                d->packages->rule().height());
}

String PackagesColumnWidget::tabHeading() const
{
    return tr("Packages");
}
