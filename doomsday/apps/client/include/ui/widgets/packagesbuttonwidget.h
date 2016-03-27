/** @file packagesbuttonwidget.h
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

#ifndef DENG_CLIENT_UI_PACKAGESBUTTONWIDGET_H
#define DENG_CLIENT_UI_PACKAGESBUTTONWIDGET_H

#include <de/ButtonWidget>
#include <QStringList>

/**
 * Button for selecting packages.
 */
class PackagesButtonWidget : public de::ButtonWidget
{
    Q_OBJECT

public:
    PackagesButtonWidget();

    void setNoneLabel(de::String const &noneLabel);
    void setDialogTitle(de::String const &title);
    void setPackages(de::StringList const &packageIds);
    de::StringList packages() const;

signals:
    void packageSelectionChanged(QStringList packageIds);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_UI_PACKAGESBUTTONWIDGET_H
