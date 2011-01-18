/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GLWINDOWSURFACE_H
#define GLWINDOWSURFACE_H

#include <QGLWidget>
#include "surface.h"

/**
 * Drawing surface that is also a window with an OpenGL surface and context.
 */
class GLWindowSurface : public QGLWidget, public Surface
{
    Q_OBJECT

public:
    explicit GLWindowSurface(const QGLFormat& format, QWidget *parent = 0, const QGLWidget* shareWidget = 0);

    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();

    QSize size() const;
    void setSize(const QSize& size);

    virtual void draw() = 0;

signals:

public slots:
};

#endif // GLWINDOWSURFACE_H
