<?php
/**
 * @file packagefactory.class.php
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

includeGuard('PackageFactory');

require_once('packages/basepackage.class.php');
require_once('packages/nullpackage.class.php');
require_once('packages/distributionpackage.class.php');
require_once('packages/unstabledistributionpackage.class.php');
require_once('packages/pluginpackage.class.php');
require_once('packages/unstablepluginpackage.class.php');

class PackageFactory
{
    private static $nullPackage = NULL;

    public static function newNullPackage()
    {
        if(!isset($nullPackage))
        {
            self::$nullPackage = new NullPackage();
        }
        return self::$nullPackage;
    }

    public static function newDistribution($platformId, $name, $version, $downloadUrl)
    {
        return new DistributionPackage($platformId, $name, $version, $downloadUrl);
    }

    public static function newUnstableDistribution($platformId, $name, $version, $downloadUrl)
    {
        return new UnstableDistributionPackage($platformId, $name, $version, $downloadUrl);
    }

    /**
     * Parse a new Package object from a SimpleXMLElement.
     *
     * @param $log_pack  (Object) SimpleXMLElement node to be "parsed".
     * @return  (Object) Resultant Package object.
     */
    public static function newFromSimpleXMLElement(&$log_pack, $releaseType='unstable')
    {
        if(!($log_pack instanceof SimpleXMLElement))
            throw new Exception('Received invalid log_pack');

        $platformId = BuildRepositoryPlugin::parsePlatformId(clean_text($log_pack->platform));
        $cleanDownloadUri = safe_url($log_pack->downloadUri);
        $compileLogUri    = safe_url($log_pack->compileLogUri);

        if(!empty($log_pack->name))
        {
            $name = clean_text($log_pack->name);
        }
        else
        {
            // We must resort to extracting the name from download Uri.
            $filename = basename(substr($cleanDownloadUri, 0, -9/*/download*/));
            $filename = preg_replace(array('/-/','/_/'), ' ', $filename);

            $words = explode(' ', substr($filename, 0, strrpos($filename, '.')));
            $name  = ucwords(implode(' ', $words));
        }

        if(!empty($log_pack->version))
            $version = clean_text($log_pack->version);
        else
            $version = NULL;

        // Determine package type.
        foreach($log_pack->attributes() as $attrib => $value)
        {
            if($attrib === 'type') $type = $value;
            break;
        }
        if(!isset($type)) $type = 'distribution';

        switch($type)
        {
        case 'plugin':
            if($releaseType === RT_STABLE)
            {
                $pack = new PluginPackage($platformId, $name, $version, $cleanDownloadUri,
                    $compileLogUri, (integer)$log_pack->compileWarnCount,
                    (integer)$log_pack->compileErrorCount);
            }
            else
            {
                $pack = new UnstablePluginPackage($platformId, $name, $version, $cleanDownloadUri,
                    $compileLogUri, (integer)$log_pack->compileWarnCount,
                    (integer)$log_pack->compileErrorCount);
            }
            break;
        default:
            if($releaseType === RT_STABLE)
            {
                $pack = new DistributionPackage($platformId, $name, $version, $cleanDownloadUri,
                    $compileLogUri, (integer)$log_pack->compileWarnCount,
                    (integer)$log_pack->compileErrorCount);
            }
            else
            {
                $pack = new UnstableDistributionPackage($platformId, $name, $version, $cleanDownloadUri,
                    $compileLogUri, (integer)$log_pack->compileWarnCount,
                    (integer)$log_pack->compileErrorCount);
            }
            break;
        }

        return $pack;
    }
}
