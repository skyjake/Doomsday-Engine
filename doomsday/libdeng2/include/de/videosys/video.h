/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_VIDEO_H
#define LIBDENG2_VIDEO_H

#include "../ISubsystem"
#include "../Window"

#include <set>

/**
 * @defgroup video Video Subsystem
 *
 * Classes that provide common generic functionality for all video subsystems.
 * These also define the interface of the video subsystem.
 */

namespace de
{
    /**
     * The abstract base class for a video subsystem. The video subsystem
     * is responsible for graphical presentation of the UI.
     * 
     * @ingroup video
     */
    class LIBDENG2_API Video : public ISubsystem
    {
    public:
        typedef std::set<Window*> Windows;
        
    public:
        /**
         * Initializes the video subsystem so that it's ready for use.
         * This include the main window, which is created using configuration provided 
         * by the App.
         */
        Video();
        
        /**
         * Shuts down the video subsystem.
         */
        virtual ~Video();
        
        /** 
         * The main window is the primary outlet for presentation. When the video
         * subsystem exists, there is always a main window as well.
         */
        Window& mainWindow() const;
        
        /**
         * Sets the main window.
         *
         * @param window  Window to become the main window. The subsystem gets ownership
         *      of the window.
         */ 
        virtual void setMainWindow(Window* window);
        
        /**
         * Sets the drawing surface used for drawing operations.
         * A target must be set before performing any drawing.
         *
         * @param surface  Surface to draw on.
         */
        virtual void setTarget(Surface& surface);
        
        /**
         * Releases the current drawing target. This should be called once all 
         * the drawing operations are done.
         */
        virtual void releaseTarget();
        
        /**
         * Returns the current target drawing surface.
         *
         * @return Surface or NULL.
         */
        Surface* target() const;

        /// Returns the window list (read access only).
        const Windows& windows() const { return _windows; }
        
        /**
         * Constructs a new Window.
         *
         * @param where  Initial placement of the window.
         * @param mode  Initial mode.
         *
         * @return  Window. The video subsystem retains ownership. The window will
         *      be destroyed when the video subsystem is deleted.
         */
        virtual Window* newWindow(const Window::Placement& where, const Window::Mode& mode) = 0;

    protected:        
        /// Returns the window list.
        Windows& windows() { return _windows; }
                
    private:
        Window* _mainWindow;

        /// List of all windows owned by the video subsystem. The main window is one of these.
        Windows _windows;

        /// Current target drawing surface.
        Surface* _target;
    };      
};

#endif /* LIBDENG2_VIDEO_H */
