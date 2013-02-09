/** @file localserverdialog.h  Dialog for starting a local server.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LOCALSERVERGUIDIALOG_H
#define LOCALSERVERGUIDIALOG_H

#include <QDialog>
#include <de/NativePath>

class LocalServerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LocalServerDialog(QWidget *parent = 0);
    ~LocalServerDialog();

    quint16 port() const;
    QString gameMode() const;
    QStringList additionalOptions() const;
    de::NativePath runtimeFolder() const;

protected slots:
    void pickFolder();
    void configureGameOptions();
    void saveState();
    void validate();

private:
    struct Instance;
    Instance *d;
};

#endif // LOCALSERVERGUIDIALOG_H
