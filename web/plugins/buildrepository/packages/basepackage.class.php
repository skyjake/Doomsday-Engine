<?php
/**
 * @file basepackage.class.php
 * Abstract base for all Package objects.
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
 * @author Copyright &copy; 2009-2013 Daniel Swanson <danij@dengine.net>
 */

includeGuard('BasePackage');

require_once(DIR_CLASSES.'/version.class.php');

abstract class BasePackage
{
    protected $platformId;
    protected $title = '(unnamed)';
    protected $version = NULL;

    public function __construct($platformId=PID_ANY, $title=NULL, Version $version = NULL)
    {
        $this->platformId = $platformId;
        if(!is_null($title))
            $this->title = "$title";
        if(!is_null($version))
            $this->version = $version;
    }

    public function &platformId()
    {
        return $this->platformId;
    }

    public function &title()
    {
        return $this->title;
    }

    public function &version()
    {
        return $this->version;
    }

    public function composeFullTitle($includeVersion=true, $includePlatformName=true)
    {
        $includeVersion = (bool)$includeVersion;

        $title = $this->title;
        if($includeVersion && !is_null($this->version))
            $title .= " {$this->version}";
        if($includePlatformName && $this->platformId !== PID_ANY)
        {
            $plat = &BuildRepositoryPlugin::platform($this->platformId);
            $title .= ' for '. $plat['nicename'];
        }
        return $title;
    }

    public function __toString()
    {
        $fullTitle = $this->composeFullTitle();
        return '('.get_class($this).":$title)";
    }

    /**
     * Add the object graph properties for this to the specified template.
     *
     * @param tpl  (Array) Array to be filled with graph properties.
     */
    public function populateGraphTemplate(&$tpl)
    {
        if(!is_array($tpl))
            throw new Exception('Invalid template argument, array expected');

        $plat = &BuildRepositoryPlugin::platform($this->platformId());

        $tpl['platform_id']   = $this->platformId();
        $tpl['platform_name'] = $plat['name'];

        $tpl['title']         = $this->title();
        $tpl['fulltitle']     = $this->composeFullTitle();

        // Textual version representation.
        $tpl['version']       = is_null($this->version)? '0' : "{$this->version}";

        // Component version representation.
        $tpl['version_major']    = is_null($this->version)? '0' : $this->version->major;
        $tpl['version_minor']    = is_null($this->version)? '0' : $this->version->minor;
        $tpl['version_patch']    = is_null($this->version)? '0' : $this->version->patch;
        $tpl['version_revision'] = is_null($this->version)? '0' : $this->version->revision;
    }
}
