/** @file editor.h
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef GLOOM_EDITOR_H
#define GLOOM_EDITOR_H

#include <QWidget>
#include <gloom/world/map.h>

class Editor : public QWidget
{
    Q_OBJECT

public:
    enum Mode {
        EditPoints,
        EditLines,
        EditSectors,
        EditPlanes,
        EditVolumes,
        EditEntities,
    };
    enum { ModeCount = 6 };

    Editor();

    de::String mapId() const;
    gloom::Map &map();
    de::String packageName() const;

    void exportPackage() const;
    void updateWindowTitle() const;
    bool maybeClose();
    QSet<gloom::ID> selection() const;
    void markAsChanged();

    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;

signals:
    void modeChanged(int mode);
    void lineSelectionChanged();
    void planeSelectionChanged();
    void buildMapRequested();

private:
    DE_PRIVATE(d)
};

#endif // GLOOM_EDITOR_H
