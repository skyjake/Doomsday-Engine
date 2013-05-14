/** @file style.h  User interface style.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/style.h"

using namespace de;

DENG2_PIMPL(Style)
{
    String packPath;
    RuleBank rules;
    FontBank fonts;
    ColorBank colors;
    ImageBank images;

    Instance(Public *i) : Base(i)
    {}

    void clear()
    {
        rules.clear();
        fonts.clear();
        colors.clear();
        images.clear();
    }

    void load(String const &path)
    {
        packPath = path;
        rules.addFromInfo(path / "rules.dei");
        fonts.addFromInfo(path / "fonts.dei");
        colors.addFromInfo(path / "colors.dei");
        images.addFromInfo(path / "images.dei");
    }
};

Style::Style() : d(new Instance(this))
{}

void Style::load(String const &pack)
{
    d->clear();
    d->load(pack);
}

RuleBank const &Style::rules() const
{
    return d->rules;
}

FontBank const &Style::fonts() const
{
    return d->fonts;
}

ColorBank const &Style::colors() const
{
    return d->colors;
}

ImageBank const &Style::images() const
{
    return d->images;
}
