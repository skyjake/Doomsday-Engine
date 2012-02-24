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

require_once('addonsparser.class.php');

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

    /**
     * Does the addon support any of these game modes?
     *
     * @param addon  (Array) Addon record object.
     * @param gameModes  (Array) Associative array containing the list of
     *                   game modes to look for (the needles). If none are
     *                   specified; assume this addon supports it.
     * @return  (Boolean)
     */
    private function addonSupportsGameMode(&$addon, &$gameModes)
    {
        if(!is_array($addon))
            throw new Exception('Invalid addon argument, array expected');

        if(!isset($addon['games'])) return true;
        if(!is_array($gameModes)) return true;

        $supportedModes = &$addon['games'];
        foreach($gameModes as $mode)
        {
            if(isset($supportedModes[$mode])) return true;
        }

        return false;
    }

    /**
     * @return  (Boolean) @c true if this addon is marked 'featured'.
     */
    private function addonHasFeatured(&$addon)
    {
        if(!is_array($addon))
            throw new Exception('Invalid addon argument, array expected');

        if(!is_array($addon['attributes'])) return false;

        $attribs = &$addon['attributes'];
        return isset($attribs['featured']) && (boolean)$attribs['featured'];
    }

    private function outputAddonListElement(&$addon)
    {
        if(!is_array($addon))
            throw new Exception('Invalid addon argument, array expected');

        $addonFullTitle = $addon['title'];
        if(isset($addon['version']))
            $addonFullTitle .= ' '. $addon['version'];

?><tr><td><?php

        if(isset($addon['downloadUri']))
        {
?><a href="<?php echo $addon['downloadUri']; ?>" title="Download <?php echo $addonFullTitle; ?>" rel="nofollow"><?php echo $addonFullTitle; ?></a><?php
        }
        else if(isset($addon['homepageUri']))
        {
?><a href="<?php echo $addon['homepageUri']; ?>" title="Visit homepage for <?php echo $addonFullTitle; ?>" rel="nofollow"><?php echo $addonFullTitle; ?></a><?php
        }
        else
        {
            echo $addonFullTitle;
        }

?></td>
<td><?php if(isset($addon['description'])) echo $addon['description']; ?></td>
<td><?php if(isset($addon['notes'])) echo $addon['notes']; ?></td></tr><?
    }

    /**
     * Output an HTML list of addons to the output stream
     *
     * @param addons  (Array) Collection of Addon records to process.
     * @param filter_gameModes  (Array) Game modes to filter the addon list by.
     * @param filter_featured  (Mixed) @c < 0 no filter
     *                                    = 0 not featured
     *                                    = 1 featured
     */
    private function outputAddonList(&$addons, $filter_gameModes, $filter_featured=-1)
    {
        if(!is_array($addons))
            throw new Exception('Invalid addons argument, array expected');

        // Sanitize filter arguments.
        $filter_featured = intval($filter_featured);
        if($filter_featured < 0) $filter_featured = -1; // Any.
        else                     $filter_featured = $filter_featured? 1 : 0;

        // Output the table.
?><table class="directory">
<tr><th>Name</th><th>Description</th><th>Notes</th></tr><?php

        foreach($addons as &$addon)
        {
            if($filter_featured != -1 && (boolean)$filter_featured != $this->addonHasFeatured($addon)) continue;
            if(!$this->addonSupportsGameMode($addon, $filter_gameModes)) continue;

            $this->outputAddonListElement($addon);
        }

?></table><?php

    }

    public function generateHTML()
    {
        global $FrontController;

        $addonListXml = file_get_contents(FrontController::nativePath("plugins/addonrepository/addons.xml"));

        $addons = array();
        AddonsParser::parse($addonListXml, $addons);

        includeHTML('overview', self::$name);

?><h3>DOOM</h3>
<p>The following add-ons are for use with <strong>DOOM</strong>, <strong>DOOM2</strong>, <strong>Ultimate DOOM</strong> and <strong>Final DOOM (TNT/Plutonia)</strong>. Some of which may even be used with the shareware version of DOOM (check the <em>Notes</em>).</p>
<?php

        $doomGames = array('doom1', 'doom1-ultimate', 'doom1-share', 'doom2', 'doom2-plut', 'doom2-tnt');
        $this->outputAddonList($addons, $doomGames);

?><h3>Heretic</h3>
<p>The following add-ons are for use with <strong>Heretic</strong> and <strong>Heretic: Shadow of the Serpent Riders </strong>. Some of which may even be used with the shareware version of Heretic (check the <em>Notes</em>).</p>
<?php

        $hereticGames = array('heretic', 'heretic-share', 'heretic-ext');
        $this->outputAddonList($addons, $hereticGames);

?><h3>Hexen</h3>
<p>The following add-ons are for use with <strong>Hexen</strong> and <strong>Hexen:Deathkings of the Dark Citadel</strong>. Some of which may even be used with the shareware version of Hexen (check the <em>Notes</em>).</p>
<?php

        $hexenGames = array('hexen', 'hexen-dk', 'hexen-demo');
        $this->outputAddonList($addons, $hexenGames);

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
