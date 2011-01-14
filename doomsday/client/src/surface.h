/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_SURFACE_H
#define LIBDENG2_SURFACE_H

#include <de/deng.h>
#include <de/Vector>

#include <QImage>

/**
 * Graphics surface. The video subsystem will define its own drawing surfaces
 * based on this.
 *
 * @ingroup video
 */
class Surface
{
public:
    /// Conversion of the drawing surface to an image failed. @ingroup errors
    DEFINE_ERROR(CaptureError);

public:
    virtual ~Surface();

    /**
     * Returns the size of the drawing surface.
     */
    virtual QSize size() const = 0;

    /**
     * Sets the size of the drawing surface.
     */
    virtual void setSize(const QSize& size) = 0;

    /**
     * Activates the surface as the current rendering target of the video subsystem.
     */
    virtual void activate() = 0;

    /**
     * Deactivates the surface.
     */
    virtual void deactivate() = 0;

    /**
     * Captures the contents of the drawing surface and stores them into an image.
     *
     * @return  Captured image. Caller gets ownership.
     */
    virtual QImage captureImage() const;
};

#endif /* LIBDENG2_SURFACE_H */
