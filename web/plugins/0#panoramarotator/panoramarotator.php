<?php
/**
 * @file panoramarotator.php
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

includeGuard('PanoramaRotatorPlugin');

class PanoramaRotatorPlugin extends Plugin implements RequestInterpreter
{
    public static $name = 'panoramarotator';
    private $_title = 'Panorama Rotator';

    public function __construct() {}

    public function title()
    {
        return self::$_title;
    }

    static function panorama()
    {
        /**
         * Inspired by Matt Mullenweg's "Random Image Script"
         * @see http://ma.tt/scripts/randomimage/
         */

        $imageDir = 'images/panoramas/';

        $exts = array('jpg', 'jpeg', 'png', 'gif');

        $handle = opendir($imageDir);
        $i = 0;
        while(false !== ($file = readdir($handle)))
        {
            foreach($exts as $ext)
            {
                if(preg_match('/\.'.$ext.'$/i', $file, $test))
                {
                    if(!isset($paths))
                        $paths = array();

                    //$paths[] = $imageDir.$file;
                    $paths[] = 'http://dl.dropbox.com/u/11948701/dengine.net/images/panoramas/'.$file;
                    $i++;
                }
            }
        }
        closedir($handle);

        if($i > 0)
        {
            mt_srand((double) microtime() * 1000000);
            $rand = mt_rand(0, $i - 1);

            header('Location: '.$paths[$rand]);
        }
    }

    /**
     * Implements RequestInterpreter
     */
    public function InterpretRequest($Request)
    {
        $url = urldecode($Request->url()->path());

        // @kludge skip over the first '/' in the home URL.
        $url = strtolower(substr($url, 1));

        // Tokenize the request URL so we can begin to determine what to do.
        $tokens = explode('/', $url);

        if(count($tokens) && $tokens[0] == 'panoramarotator' )
        {
            $this->panorama();
            exit; // @todo Should not exit here, caller has authority!
        }

        return false;
    }
}
