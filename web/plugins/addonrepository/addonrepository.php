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
     *                   game modes to look for (the needles).
     */
    private function addonSupportsGameMode(&$addon, &$gameModes)
    {
        if(!is_array($addon))
            throw new Exception('Invalid addon argument, array expected');

        if(!isset($addon['games'])) return true;
        if(!is_array($gameModes)) return false;

        $supportedModes = &$addon['games'];
        foreach($gameModes as $mode)
        {
            if(isset($supportedModes[$mode])) return true;
        }

        return false;
    }

    /**
     * Output an HTML list of addons to the output stream
     *
     * @param addons  (Array) Collection of Addon records to process.
     * @param gameModes  (Array) Game modes to filter the addon list by.
     */
    private function outputAddonList(&$addons, &$gameModes)
    {
        if(!is_array($addons))
            throw new Exception('Invalid addons argument, array expected');

?><table class="directory">
<tr><th>Name</th><th>Description</th><th>Notes</th></tr><?php

        foreach($addons as $addon)
        {
            if(!$this->addonSupportsGameMode($addon, $gameModes)) continue;

?><tr><td><a href="<?php echo $addon['downloadUri']; ?>" title="Download <?php echo $addon['title']; ?>"><?php echo htmlspecialchars($addon['title']); ?></a></td>
<td><?php echo htmlspecialchars($addon['description']); ?></td>
<td><?php echo $addon['notes']; ?></td></tr><?
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

?><h3>jDoom</h3>
<p>The following add-ons are for use with <strong>DOOM</strong>, <strong>DOOM2</strong>, <strong>Ultimate DOOM</strong> and <strong>Final DOOM (TNT/Plutonia)</strong>. Some of which may even be used with the shareware version of DOOM (check the <em>Notes</em>).</p>
<?php

        $doomGames = array('doom1', 'doom1-ultimate', 'doom1-share', 'doom2', 'doom2-plut', 'doom2-tnt');
        $this->outputAddonList($addons, $doomGames);

?><h3>jHeretic</h3>
<p>The following add-ons are for use with <strong>Heretic</strong> and <strong>Heretic: Shadow of the Serpent Riders </strong>. Some of which may even be used with the shareware version of Heretic (check the <em>Notes</em>).</p>
<?php

        $hereticGames = array('heretic', 'heretic-share', 'heretic-ext');
        $this->outputAddonList($addons, $hereticGames);

?><h3>jHexen</h3>
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
