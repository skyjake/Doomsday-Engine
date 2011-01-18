/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef SCALARRULE_H
#define SCALARRULE_H

#include "rule.h"

#include <de/Animator>

/**
 * Rule with a scalar value. The value is animated over time.
 */
class ScalarRule : public Rule
{
    Q_OBJECT

public:
    explicit ScalarRule(float initialValue, QObject *parent = 0);

    void set(float value, de::Time::Delta transition = 0);

    /**
     * Read-only access to the scalar animator.
     */
    const de::Animator& scalar() const {
        return _animator;
    }

protected:
    void update();

public slots:
    void currentTimeChanged();

private:
    de::Animator _animator;
};

#endif // SCALARRULE_H
