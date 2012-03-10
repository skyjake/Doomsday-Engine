<?php
/**
 * @file abstractpackage.class.php
 * An abstract, downloadable Package object.
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

includeGuard('AbstractPackage');

require_once('basepackage.class.php');
require_once(dirname(__FILE__) . '/../downloadable.interface.php');

abstract class AbstractPackage extends BasePackage implements iDownloadable
{
    static protected $emptyString = '';

    protected $downloadUri = NULL;

    public function __construct($platformId=PID_ANY, $title=NULL, $version=NULL, $downloadUri=NULL)
    {
        parent::__construct($platformId, $title, $version);

        if(!is_null($downloadUri) && strlen($downloadUri) > 0)
            $this->downloadUri = "$downloadUri";
    }

    // Extends implementation in AbstractPackage.
    public function populateGraphTemplate(&$tpl)
    {
        if(!is_array($tpl))
            throw new Exception('Invalid template argument, array expected');

        parent::populateGraphTemplate($tpl);
        $tpl['download_uri'] = $this->downloadUri();
    }

    // Implements iDownloadable
    public function &downloadUri()
    {
        if(!$this->hasDownloadUri())
        {
            return $emptyString;
        }
        return $this->downloadUri;
    }

    // Implements iDownloadable
    public function hasDownloadUri()
    {
        return !is_null($this->downloadUri);
    }

    // Implements iDownloadable
    public function genDownloadBadge()
    {
        $fullTitle = $this->composeFullTitle();
        if($this->hasDownloadUri())
        {
            $html = '<a href="'. htmlspecialchars($this->downloadUri)
                   .'" title="'. ("Download $fullTitle")
                          .'">'. htmlspecialchars($fullTitle) .'</a>';
        }
        else
        {
            $html = htmlspecialchars($fullTitle);
        }
        return $html;
    }
}
