<?php
/**
 * @file abstractbuilderpackage.class.php
 * An abstract, autobuilder, downloadable Package object.
 *
 * @authors Copyright @ 2009-2013 Daniel Swanson <danij@dengine.net>
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

includeGuard('AbstractBuilderPackage');

require_once('abstractpackage.class.php');
require_once(dirname(__FILE__) . '/../builderproduct.interface.php');

abstract class AbstractBuilderPackage extends AbstractPackage implements iBuilderProduct
{
    static protected $emptyString = '';

    protected $buildId = 0; /// Unique.

    // Extends implementation in AbstractPackage.
    public function populateGraphTemplate(&$tpl)
    {
        if(!is_array($tpl))
            throw new Exception('Invalid template argument, array expected');

        parent::populateGraphTemplate($tpl);

        $build = FrontController::fc()->findPlugin('BuildRepository')->buildByUniqueId($this->buildId);
        if($build instanceof BuildEvent)
        {
            $tpl['build_startdate'] = date(DATE_ATOM, $build->startDate());
            $tpl['build_uniqueid'] = $this->buildId;
        }
    }

    // Implements iBuilderProduct.
    public function setBuildUniqueId($id)
    {
        $this->buildId = intval($id);
    }

    // Implements iBuilderProduct.
    public function buildUniqueId()
    {
        return $this->buildId;
    }
}
