<?php
/**
 * @file addonrepository.php
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
 * @author Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
 */

includeGuard('AddonRepositoryPlugin');

class AddonRepositoryPlugin extends Plugin implements Actioner, RequestInterpreter
{
    public static $name = 'addonrepository';

    public static $baseRequestName = 'addons';

    private $_displayOptions = 0;

    public function __construct() {}

    public function title()
    {
        return 'Add-ons';
    }

    /**
     * Implements RequestInterpreter
     */
    public function InterpretRequest($request)
    {
        global $FrontController;

        $url = urldecode($request->url()->path());

        // @kludge skip over the first '/' in the home URL.
        $url = substr($url, 1);

        // Remove any trailing file extension, such as 'something.html'
        if(strrpos($url, '.'))
            $url = substr($url, 0, strrpos($url, '.'));

        // Tokenize the request URL so we can begin to determine what to do.
        $tokens = explode('/', $url);

        if(count($tokens) && !strcasecmp(self::$baseRequestName, $tokens[0]))
        {
            $FrontController->enqueueAction($this, NULL);
            return true; // Eat the request.
        }

        return false; // Not for us.
    }

    public function outputAddonList(&$addons)
    {
        if(!is_array($addons))
            throw new Exception('Invalid addons argument, array expected');

?><table class="directory">
<tr><th>Name</th><th>Description</th><th>Notes</th></tr><?php

        foreach($addons as $addon)
        {
?><tr><td><a href="<?php echo $addon['downloadUri']; ?>" title="Download <?php echo $addon['title']; ?>"><?php echo htmlspecialchars($addon['title']); ?></a></td>
<td><?php echo htmlspecialchars($addon['description']); ?></td>
<td><?php echo $addon['notes']; ?></td></tr><?
        }

?></table><?php

    }

    public function generateHTML()
    {
        $doomAddons = array(
            array('downloadUri'=>'http://files.dengine.net/tracker/torrents/jdrp-packaged-20070404.zip.torrent',
                  'title'=>'jDRP v1.01 (packaged)',
                  'description'=>'DOOM Resource Pack',
                  'notes'=>'<em>Unzip</em> this pack into the Snowberry addon folder'),
            array('downloadUri'=>'http://files.dengine.net/tracker/torrents/jdmu-doom-classic-20080930.pk3.torrent',
                  'title'=>'DOOM Classic Music',
                  'description'=>'DOOM Music Recorded from a genuine Roland Sound Canvas SC-155',
                  'notes'=>'<em>Move</em> this pack into the Snowberry addon folder'),
            array('downloadUri'=>'http://files.dengine.net/tracker/torrents/jdmu-doom2-classic-20080930.pk3.torrent',
                  'title'=>'DOOM2 Classic Music',
                  'description'=>'DOOM2 Music Recorded from a genuine Roland Sound Canvas SC-155',
                  'notes'=>'<em>Move</em> this pack into the Snowberry addon folder'),
            array('downloadUri'=>'http://files.dengine.net/tracker/torrents/tnt-remix-lorcan-20071225.pk3.torrent',
                  'title'=>'TNT Lorcan Remix',
                  'description'=>'TNT Music Remixed by Lorcan',
                  'notes'=>'<em>Move</em> this pack into the Snowberry addon folder'),
            array('downloadUri'=>'http://files.dengine.net/tracker/torrents/jdmu-all-remix-Sycraft-v4.pk3.torrent',
                  'title'=>'Sycraft Remixes',
                  'description'=>'DOOM, DOOMII and Final DOOM music remastered by Sycraft',
                  'notes'=>'<em>Move</em> this pack into the Snowberry addon folder'),
            array('downloadUri'=>'http://dhtp.freelanzer.com/',
                  'title'=>'DOOM High-resolution Texture Project',
                  'description'=>'DOOM high resolution textures',
                  'notes'=>''),
            array('downloadUri'=>'http://files.dengine.net/tracker/torrents/jdui-all-20120223.pk3.torrent',
                  'title'=>'DOOM High-resolution User interface Pack',
                  'description'=>'DOOM High-resolution User interface Pack',
                  'notes'=>'<em>Move</em> this pack into the Snowberry addon folder'),
            array('downloadUri'=>'http://files.dengine.net/tracker/torrents/pk-doom-sfx-20100109.pk3.torrent',
                  'title'=>'DOOM High-quality sound pack',
                  'description'=>'Compiled by Per Kristian Risvik',
                  'notes'=>'<em>Move</em> this pack into the Snowberry addon folder'),
            array('downloadUri'=>'http://files.dengine.net/tracker/torrents/slide-skyboxes.torrent',
                  'title'=>'Slide\'s Skyboxes',
                  'description'=>'Created by slide for all the DOOM games',
                  'notes'=>'<em>Move</em> these packs into the Snowberry addon folder')
            );

        $hereticAddons = array(
            array('downloadUri'=>'http://torrage.com/torrent/584A6DDB49940C73753CB6B425B4301E6137E945.torrent',
                  'title'=>'jHRP 2009.07.03 (packaged)',
                  'description'=>'3D Models, hi-res interface elements and textures',
                  'notes'=>'<em>Move</em> this pack into the Snowberry addon folder')
            );

        $hexenAddons = array(
            array('downloadUri'=>'http://files.dengine.net/tracker/torrents/xhtp-20100714.pk3.torrent',
                  'title'=>'xHTP 2010.07.14 (packaged)',
                  'description'=>'High resolution texture pack',
                  'notes'=>'<em>Move</em> this pack into the Snowberry addon folder')
            );

        includeHTML('overview', self::$name);

?><h3>jDoom</h3>
<p>The following add-ons are for use with <strong>DOOM</strong>, <strong>DOOM2</strong>, <strong>Ultimate DOOM</strong> and <strong>Final DOOM (TNT/Plutonia)</strong>. Some of which may even be used with the shareware version of DOOM (check the <em>Notes</em>).</p>
<?php

        $this->outputAddonList($doomAddons);

?><h3>jHeretic</h3>
<p>The following add-ons are for use with <strong>Heretic</strong> and <strong>Heretic: Shadow of the Serpent Riders </strong>. Some of which may even be used with the shareware version of Heretic (check the <em>Notes</em>).</p>
<?php

        $this->outputAddonList($hereticAddons);

?><h3>jHexen</h3>
<p>The following add-ons are for use with <strong>Hexen</strong> and <strong>Hexen:Deathkings of the Dark Citadel</strong>. Some of which may even be used with the shareware version of Hexen (check the <em>Notes</em>).</p>
<?php

        $this->outputAddonList($hexenAddons);

        includeHTML('instructions', self::$name);
    }

    /**
     * Implements Actioner.
     */
    public function execute($args=NULL)
    {
        global $FrontController;

        $FrontController->outputHeader($this->title());
        $FrontController->beginPage($this->title());

?><div id="addons"><?php

        $this->generateHTML();

?></div><?php

        $FrontController->endPage();
    }
}
