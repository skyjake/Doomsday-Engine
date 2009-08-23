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

#ifndef LIBDENG2_OBSERVERS_H
#define LIBDENG2_OBSERVERS_H

#include "../deng.h"

#include <set>

/**
 * Macro for defining an observer interface with a single method.
 *
 * @param Name    Name of the audience. E.g., "Deletion" produces @c DeletionAudience 
 *                and @c audienceForDeletion.
 * @param Method  The pure virtual method that the observer has to implement. 
 *                The @c virtual keyword and <code>=0</code> are automatically included.
 */
#define DEFINE_AUDIENCE(Name, Method) \
class I##Name##Observer { \
public: \
    virtual ~I##Name##Observer() {} \
    virtual Method = 0; \
}; \
typedef de::Observers<I##Name##Observer> Name##Audience; \
Name##Audience audienceFor##Name;   

/**
 * Macro for looping through the audience members.
 *
 * @param Name  Name of the audience.
 * @param Var   Variable used in the loop.
 */
#define FOR_AUDIENCE(Name, Var) for(Name##Audience::Loop Var(audienceFor##Name); !Var.done(); ++Var)

/**
 * Macro for looping through all observers. @note The @a Audience type needs to be defined
 * in the scope.
 *
 * @param SetName  Name of the observer set class.
 * @param Var      Variable used in the loop.
 * @param Name     Name of the observer set.
 */
#define FOR_EACH_OBSERVER(SetName, Var, Name) for(SetName::Loop Var(Name); !Var.done(); ++Var)

namespace de
{
    /**
     * Template for observer sets. The template type should be an interface
     * implemented by all the observers.
     *
     * @ingroup data
     */
    template <typename Type>
    class Observers
    {
    public:
        typedef std::set<Type*> Members;
        typedef typename Members::iterator iterator;
        typedef typename Members::const_iterator const_iterator;
        typedef typename Members::size_type size_type;
        
        /**
         * Iteration utility for observers. This should be used when notifying 
         * observers, because it is safe against the observer removing itself
         * from the observer set.
         */
        class Loop {
        public:
            Loop(Observers& observers) : observers_(observers) {
                next_ = observers.begin();
                next();
            }
            bool done() {
                return current_ == observers_.end();
            }
            void next() {
                current_ = next_;
                if(next_ != observers_.end())
                {
                    ++next_;
                }
            }
            iterator& get() {
                return current_;
            }
            Type* operator -> () {
                return *get();
            }
            Loop& operator ++ () {
                next();
                return *this;
            }
        private:
            Observers& observers_;
            iterator current_;
            iterator next_;
        };
        
    public:
        Observers() : members_(0) {}

        virtual ~Observers() {
            clear();
        }
        
        void clear() {
            delete members_;
            members_ = 0;
        }

        /// Add an observer into the set. The set does not receive
        /// ownership of the observer instance.
        void add(Type* observer) {
            checkExists();
            members_->insert(observer);
        }            
        
        void remove(Type* observer) {
            if(members_) {
                members_->erase(observer);
            }
            if(!members_->size()) {
                delete members_;
                members_ = 0;
            }
        }
        
        size_type size() const {
            if(members_) {
                return members_->size();
            }
            return 0;
        }
        
        iterator begin() {
            checkExists();
            return members_->begin();
        }

        iterator end() {
            checkExists();
            return members_->end();
        }
        
        const_iterator begin() const {
            if(!members_) {
                return 0;
            }
            return members_->begin();
        }

        const_iterator end() const {
            if(!members_) {
                return 0;
            }
            return members_->end();
        }

    protected:
        void checkExists() {
            if(!members_) {
                members_ = new Members;
            }            
        }
        
    private:
        Members* members_;
    };
};

#endif /* LIBDENG2_OBSERVERS_H */
