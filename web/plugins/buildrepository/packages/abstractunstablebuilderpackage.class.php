<?php
/**
 * @file abstractunstablebuilderpackage.class.php
 * An abstract, "unstable", autobuilder, downloadable Package object.
 *
 * @section License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * @author Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
 */

includeGuard('AbstractUnstableBuilderPackage');

require_once('abstractunstablepackage.class.php');

abstract class AbstractUnstableBuilderPackage extends AbstractUnstablePackage implements iBuilderProduct
{
    protected $buildId = 0; /// Unique.

    // Override implementation in AbstractUnstablePackage.
    public function composeFullTitle($includeVersion=true, $includePlatformName=true, $includeBuildId=true)
    {
        $includeVersion = (boolean) $includeVersion;
        $includeBuildId = (boolean) $includeBuildId;

        $title = $this->title;
        if($includeVersion && isset($this->version))
            $title .= ' '. $this->version;
        if($includeBuildId && $this->buildId !== 0)
            $title .= ' Build'. $this->buildId;
        if($includePlatformName && $this->platformId !== PID_ANY)
        {
            $plat = &BuildRepositoryPlugin::platform($this->platformId);
            $title .= ' for '. $plat['nicename'];
        }
        return $title;
    }

    // Extends implementation in AbstractBuilderPackage.
    public function populateGraphTemplate(&$tpl)
    {
        global $FrontController;

        if(!is_array($tpl))
            throw new Exception('Invalid template argument, array expected');

        parent::populateGraphTemplate($tpl);

        $build = $FrontController->findPlugin('BuildRepository')->buildByUniqueId($this->buildId);
        if($build instanceof BuildEvent)
        {
            $tpl['build_startdate'] = date(DATE_ATOM, $build->startDate());
            $tpl['build_uniqueid'] = $this->buildId;
        };
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
