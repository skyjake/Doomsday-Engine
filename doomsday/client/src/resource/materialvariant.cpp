/** @file materialvariant.cpp Context-specialized logical material variant.
 *
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_platform.h"
#include "resource/material.h"

#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include "render/viewports.h"
#include <de/Error>
#include <de/Log>

using namespace de;

DENG2_PIMPL(Material::Variant)
{
    /// Superior material of which this is a derivative.
    Material *material;

    /// Specification used to derive this variant.
    MaterialVariantSpec const &spec;

    /// Cached state snapshot (if any).
    QScopedPointer<MaterialSnapshot> snapshot;

    /// Frame count when the snapshot was last prepared/updated.
    int snapshotPrepareFrame;

    Instance(Public *i, Material &generalCase, MaterialVariantSpec const &_spec)
        : Base(i), material(&generalCase),
          spec(_spec),
          snapshotPrepareFrame(-1)
    {}

    /**
     * Attach new snapshot data to the variant. If an existing snapshot is already
     * present it will be replaced. Ownership of @a materialSnapshot is given to
     * the variant.
     */
    void attachSnapshot(MaterialSnapshot *newSnapshot)
    {
        DENG2_ASSERT(newSnapshot);
        if(!snapshot.isNull())
        {
            LOG_AS("Material::Variant::AttachSnapshot");
            LOGDEV_RES_WARNING("A snapshot is already attached to %p, it will be replaced") << this;
        }
        snapshot.reset(newSnapshot);
    }
};

Material::Variant::Variant(Material &generalCase, MaterialVariantSpec const &spec)
    : d(new Instance(this, generalCase, spec))
{}

Material &Material::Variant::generalCase() const
{
    return *d->material;
}

MaterialVariantSpec const &Material::Variant::spec() const
{
    return d->spec;
}

MaterialContextId Material::Variant::context() const
{
    return spec().context;
}

MaterialSnapshot const &Material::Variant::prepare(bool forceSnapshotUpdate)
{
    // Time to attach a snapshot?
    if(d->snapshot.isNull())
    {
        d->attachSnapshot(new MaterialSnapshot(*const_cast<Material::Variant *>(this)));
        forceSnapshotUpdate = true;
    }

    MaterialSnapshot *snapshot = d->snapshot.data();
    // Time to update the snapshot?
    if(forceSnapshotUpdate || d->snapshotPrepareFrame != R_FrameCount())
    {
        d->snapshotPrepareFrame = R_FrameCount();
        snapshot->update();
    }
    return *snapshot;
}

bool MaterialVariantSpec::compare(MaterialVariantSpec const &other) const
{
    if(this == &other) return 1;
    if(context != other.context) return 0;
    return primarySpec == other.primarySpec;
}
