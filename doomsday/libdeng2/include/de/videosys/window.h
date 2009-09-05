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

#ifndef LIBDENG2_WINDOW_H
#define LIBDENG2_WINDOW_H

#include "../Rectangle"
#include "../Visual"

namespace de
{
    class Video;
    class Surface;
    
    /**
     * The abstract base class for windows in the operating system's windowing system.
     *
     * @ingroup video
     */ 
    class LIBDENG2_API Window
    {
    public:
        typedef Rectangleui Placement;

        /// Window is in fullscreen mode.
        DEFINE_FINAL_FLAG(FULLSCREEN, 0, Mode);
        
    public:
        virtual ~Window();

        /**
         * Returns the video subsystem that governs the window.
         */
        Video& video() const { return _video; }

        /**
         * Returns the drawing surface of the window.
         */
        Surface& surface() const;
        
        /**
         * Returns the root visual of the window.
         */
        const Visual& root() const { return _root; }

        /**
         * Returns the root visual of the window.
         */
        Visual& root() { return _root; }

        virtual void setPlace(const Placement& p);

        /**
         * Returns the placement of the window.
         */
        const Placement& place() const { return _place; }
        
        duint width() const { return _place.width(); }

        duint height() const { return _place.height(); }
        
        /**
         * Returns the mode of the window.
         */
        const Mode& mode() const { return _mode; }

        /**
         * Changes the value of the mode flags. Subclass should override to
         * update the state of the concrete window.
         *
         * @param modeFlags  Mode flag(s).
         * @param yes  @c true, if the flag(s) should be set. @c false to unset.
         */
        virtual void setMode(Flag modeFlags, bool yes = true);

        /**
         * Sets the title of the window.
         *
         * @param title  Title text.
         */
        virtual void setTitle(const String& title) = 0;

        /**
         * Draws the contents of the window.
         */
        virtual void draw();
        
    protected:
        /**
         * Constructs the window and its drawing surface. 
         *
         * @param video  Video subsystem that constructed the window.
         * @param place  Placemenet of the window.
         * @param mode  Initial mode of the window.
         * @param surface  Drawing surface. Window gets ownership.
         *      All windows are required to have a drawing surface.
         */
        Window(Video& video, const Placement& place, const Mode& mode, Surface* surface = 0);
        
        /**
         * Sets the drawing surface of the window. It will automatically resized
         * when the window size changes. 
         *
         * @param surface  Window gets ownership.
         */
        void setSurface(Surface* surface);
        
    private:
        /// Video subsystem that governs the window.
        Video& _video;
        
        /// Window rectangle.
        Placement _place;
        
        /// Window mode.
        Mode _mode;
        
        /// Drawing surface of the window.
        Surface* _surface;
        
        /// Root visual of the window.
        Visual _root;
    };
};

#endif /* LIBDENG2_WINDOW_H */
