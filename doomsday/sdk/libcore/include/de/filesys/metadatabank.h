/** @file metadatabank.h  File metadata cache.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_METADATABANK_H
#define LIBDENG2_METADATABANK_H

#include <de/Bank>

namespace de {

/**
 * File metadata cache.
 *
 * @ingroup fs
 */
class DENG2_PUBLIC MetadataBank : public Bank
{
public:
    MetadataBank();

    static MetadataBank &get();

    /**
     * Adds a new metadata entry into the bank.
     *
     * @param category  Metadata category. Each category is saved in its own subfolder.
     * @param id        Meta ID.
     *
     * @return The cached metadata, if available. This will be an empty Block if no
     * metadata has yet been cached.
     */
    Block check(String const &category, Block const &id);

    void setMetadata(String const &category, Block const &id, Block const &metadata);

    Block metadata(String const &category, Block const &id) const;

protected:
    IData *loadFromSource(ISource &source) override;

    IData *newData() override;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_METADATABANK_H
