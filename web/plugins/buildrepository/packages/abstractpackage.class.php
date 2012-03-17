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
require_once(dirname(__FILE__) . '/../builderproduct.interface.php');

abstract class AbstractPackage extends BasePackage implements iDownloadable, iBuilderProduct
{
    static protected $emptyString = '';

    protected $directDownloadUri = NULL;

    protected $buildId = 0; /// Unique.

    protected $compileLogUri;
    protected $compileWarnCount;
    protected $compileErrorCount;

    public function __construct($platformId=PID_ANY, $title=NULL, $version=NULL, $directDownloadUri=NULL,
        $compileLogUri=NULL, $compileWarnCount=0, $compileErrorCount=0)
    {
        parent::__construct($platformId, $title, $version);

        if(!is_null($directDownloadUri) && strlen($directDownloadUri) > 0)
            $this->directDownloadUri = "$directDownloadUri";

        if(!is_null($compileLogUri))
            $this->compileLogUri = "$compileLogUri";

        $this->compileWarnCount  = intval($compileWarnCount);
        $this->compileErrorCount = intval($compileErrorCount);
    }

    // Extends implementation in AbstractPackage.
    public function populateGraphTemplate(&$tpl)
    {
        global $FrontController;

        if(!is_array($tpl))
            throw new Exception('Invalid template argument, array expected');

        parent::populateGraphTemplate($tpl);

        $tpl['direct_download_uri'] = $this->directDownloadUri();

        $build = $FrontController->findPlugin('BuildRepository')->buildByUniqueId($this->buildId);
        if($build instanceof BuildEvent)
        {
            $tpl['build_startdate'] = date(DATE_ATOM, $build->startDate());
            $tpl['build_uniqueid'] = $this->buildId;
        }

        $tpl['compile_loguri'] = $this->compileLogUri;
        $tpl['compile_errorcount'] = $this->compileErrorCount;
        $tpl['compile_warncount'] = $this->compileWarnCount;
    }

    // Implements iDownloadable
    public function &directDownloadUri()
    {
        if(!$this->hasDirectDownloadUri())
        {
            return $emptyString;
        }
        return $this->directDownloadUri;
    }

    // Implements iDownloadable
    public function hasDirectDownloadUri()
    {
        return !is_null($this->directDownloadUri);
    }

    // Implements iDownloadable
    public function genDownloadBadge()
    {
        $fullTitle = $this->composeFullTitle();
        if($this->hasDirectDownloadUri())
        {
            $html = '<a href="'. htmlspecialchars($this->directDownloadUri)
                   .'" title="'. htmlspecialchars("Download $fullTitle")
                          .'">'. htmlspecialchars($fullTitle) .'</a>';
        }
        else
        {
            $html = htmlspecialchars($fullTitle);
        }
        return $html;
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

    // Implements iBuilderProduct.
    public function &compileLogUri()
    {
        return $this->compileLogUri;
    }

    // Implements iBuilderProduct.
    public function compileWarnCount()
    {
        return $this->compileWarnCount;
    }

    // Implements iBuilderProduct.
    public function compileErrorCount()
    {
        return $this->compileErrorCount;
    }
}
