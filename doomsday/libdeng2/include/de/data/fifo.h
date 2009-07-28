/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_FIFO_H
#define LIBDENG2_FIFO_H

#include "../Lockable"

#include <list>

namespace de
{
    /**
     * A template FIFO buffer that maintains pointers to objects.  This is a thread-safe
     * implementation, as lock() and unlock() are automatically called when necessary.
     *
     * @ingroup data
     */  
    template <typename Type>
    class FIFO : public Lockable
    {
    public:
        FIFO() : Lockable() {}
        virtual ~FIFO() {
            for(typename Objects::iterator i = objects_.begin(); i != objects_.end(); ++i) {
                delete *i;
            }
        }

        /**
         * Insert a new object to the head of the buffer.  The object
         * should not be accessed while it is being stored in the
         * buffer.
         *
         * @param object  Object to add to the buffer. FIFO gets ownership.
         */
        void put(Type* object) {
            // The FIFO must be locked before it can be modified.
            lock();
            objects_.push_front(object);
            unlock();
        }        
        
        /**
         * Get the oldest object in the buffer.  
         *
         * @return The oldest object in the buffer, or NULL if the buffer is empty. 
         * Caller gets ownership of the returned object.
         */
        Type* get() {
            // The FIFO must be locked before it can be modified.
            lock();
            if(objects_.empty()) {
                unlock();
                return NULL;
            }
            Type* last = objects_.back();
            objects_.pop_back();
            unlock();
            return last;
        }        
        
        /**
         * Peek at the oldest object in the buffer.
         *
         * @return The oldest object in the buffer, or NULL if the buffer is empty.
         * The object is not removed from the buffer.
         */
        Type* peek() {
            // The FIFO must be locked before it can be modified.
            lock();
            if(objects_.empty()) {
                unlock();
                return NULL;
            }
            Type* last = objects_.back();
            unlock();
            return last;
        }
        
        /**
         * Determines whether the buffer is empty.
         */
        bool empty() const {
            lock();
            bool empty = objects_.empty();
            unlock();
            return empty;
        }

    private:
        typedef std::list<Type*> Objects;
        Objects objects_;
    };
}

#endif /* LIBDENG2_FIFO_H */
