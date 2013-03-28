<?php
/**
 * @file masterbrowser.php
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
 * @author Copyright &copy; 2013 Daniel Swanson <danij@dengine.net>
 */

includeGuard('MasterBrowserPlugin');

require_once(DIR_CLASSES.'/masterserver.class.php');

class MasterBrowserPlugin extends Plugin implements Actioner, RequestInterpreter
{
    public static $name = 'masterbrowser';
    private $_displayOptions = 0;

    private static $baseRequestName = 'masterserver';

    private static $serverSummaryCacheName = 'masterbrowser/summary.html';

    /// Master Server instance (temporary).
    private $db;

    public function __construct() {}

    public function title()
    {
        return 'Master Server';
    }

    private function mustUpdateServerSummaryHtml()
    {
        global $FrontController;

        if(!$FrontController->contentCache()->isPresent(self::$serverSummaryCacheName)) return TRUE;

        $this->db = new MasterServer();

        $cacheInfo = new ContentInfo();
        $FrontController->contentCache()->getInfo(self::$serverSummaryCacheName, $cacheInfo);
        return ($this->db->lastUpdate > $cacheInfo->modifiedTime);
    }

    /**
     * Generate HTML markup for a visual "badge" to represent this server.
     *
     * Presently unused properties:
     *
     * '<span class="game_plugin">'. $info['game'] .'</span>'.
     * '<span class="iwad_info">'. $info['iwad'] .' ('. dechex($info['wcrc']) .')</span>';
     * '<span class="player-names">'. $info['plrn'] .'</span>';
     * '<span class="version">'. $info['ver'] .'</span>'.
     */
    private function generateServerBadgeHtml(&$info)
    {
        if(!is_array($info))
            throw new Exception('Invalid info argument, array expected.');

        $addonArr = array_filter(explode(';', $info['pwads']));
        $setupArr = array_filter(explode(' ', $info['setup']));

        $playerCountLabel = 'Number of players currently in-game';

        $playerMaxLabel = 'Maximum number of players';

        $openStatusLabel = $info['locked']? 'Server requires a password to join' : 'Server is open to all players';

        $addressLabel = 'IP address and port number of the server';

        // Format the game setup argument string.
        $setupStr = implode(' ', $setupArr);

        // Begin html generation.
        $html = '<span class="game-mode">'. htmlspecialchars($info['mode']) .'</span>'.
                '<div class="wrapper"><div class="heading collapsible" title="Toggle server info display">'.
                '<div class="player-summary">'.
                    '<span class="player-count"><label title="'. htmlspecialchars($playerCountLabel) .'">'. htmlspecialchars($info['nump']) .'</label></span>/'.
                    '<span class="player-max"><label title="'. htmlspecialchars($playerMaxLabel) .'">'. htmlspecialchars($info['maxp']) .'</label></span>'.
                '</div>'.
                '<span class="name">'. htmlspecialchars($info['name']) .'</span>'.
                '<div class="server-metadata">'.
                     '<span class="address"><label title="'. htmlspecialchars($addressLabel) .'">'. htmlspecialchars($info['at']) .'<span class="port" '. (((integer)$info['port']) === 0? 'style="color:red;"' : '') .'>'. htmlspecialchars($info['port']) .'</span></label></span>'.
                     '<div class="'. ($info['locked']? 'lock-on' : 'lock-off') .'" title="'. htmlspecialchars($openStatusLabel) .'"></div>'.
                '</div>'.
                '<div class="game-metadata">'.
                    'Setup: <span class="game-setup">'. htmlspecialchars($setupStr) .'</span>'.
                    'Current Map: <span class="game-current-map">'. htmlspecialchars($info['map']) .'</span>'.
                '</div></div>';

        $html .='<div class="extended-info"><blockquote class="game-info"><p>'. htmlspecialchars($info['info']) .'</p></blockquote>';

        // Any required addons?.
        if(count($addonArr))
        {
            $addonStr = implode(' ', $addonArr);

            $html .= 'Addons: <label title="Addons required to join this game"><span class="addon-list">'. htmlspecialchars($addonStr) .'</span></label>';
        }

        $html .= '</div></div>';

        // Wrap it in the container used for styling and visual element ordering.
        return '<div class="server_badge">'. $html .'</div>';
    }

    /**
     * Ouput the javascripts used for this interface to the output stream.
     */
    private function outputJavascript()
    {
?><script type="text/javascript">
jQuery(document).ready(function() {
    jQuery(".extended-info").hide();
    jQuery(".collapsible").click(function()
    {
        jQuery(this).next(".extended-info").slideToggle(300);
    });
});
</script><?php
    }

    public static function serverSorter($b, $a)
    {
        // Group servers according to lock status.
        $diff = (integer)($b['locked'] - $a['locked']);
        if($diff) return $diff;

        // Servers with active players get priority
        $diff = (integer)($b['nump'] - $a['nump']);
        if($diff) return -($diff);

        // Order by lexicographical difference in the server name.
        $diff = strcmp($b['mode'], $a['mode']);
        if($diff) return $diff;

        // Order by lexicographical difference in the server name.
        return strcmp($b['name'], $a['name']);
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
            // Try to generate the summary page and add it to the cache.
            if($this->mustUpdateServerSummaryHtml())
            {
                try
                {
                    $content = '';

                    /**
                     * @todo Do we really need to interface with the master server?
                     *       We may be able to implement all expected functionality
                     *       by simply parsing the XML feed output.
                     */

                    if(!$this->db)
                    {
                        $this->db = new MasterServer();
                    }

                    // Build the servers collection.
                    $servers = array();
                    while(list($ident, $info) = each($this->db->servers))
                    {
                        if(!is_array($info)) continue;
                        $servers[] = $info;
                    }

                    // Sort the collection.
                    uasort($servers, array('self', 'serverSorter'));

                    // Generate the page.
                    $serverCount = count($servers);
                    if($serverCount)
                    {
                        $playerCount = (integer)0;
                        foreach($servers as &$server)
                        {
                            $playerCount += $server['nump'];
                        }

                        $content .= '<h3>Active servers</h3>'.
                                    "<p id=\"summary\">{$serverCount} ". ($serverCount === 1? 'server':'servers') ." in total with {$playerCount} active ". ($playerCount === 1? 'player':'players') .'</p>'.
                                    '<div class="server_list"><ul>';

                        foreach($servers as &$server)
                        {
                            $content .= '<li>'. $this->generateServerBadgeHtml($server) .'</li>';
                        }

                        $content .= '</ul></div>';
                        $content .= '<p id="pubDate">Last updated '. date(DATE_RFC850, time()) .'</p>';
                    }
                    else
                    {
                        $content .= '<p>There are presently no active servers. Please try again later.</p>';
                    }

                    // Write this to our cache.
                    $FrontController->contentCache()->store(self::$serverSummaryCacheName, $content);
                }
                catch(Exception $e)
                {
                    // @todo do not ignore!
                }
            }

            if($FrontController->contentCache()->isPresent(self::$serverSummaryCacheName))
            {
                $FrontController->enqueueAction($this, NULL);
                return true; // Eat the request.
            }
        }

        return false; // Not for us.
    }

    /**
     * Implements Actioner.
     */
    public function execute($args=NULL)
    {
        global $FrontController;

        $FrontController->outputHeader($this->title(), 'masterserver');
        $FrontController->beginPage($this->title(), 'masterserver');

?><div id="masterbrowser"><?php

        // Output a "join us" foreword.
        includeHTML('joinus', self::$name);

        try
        {
            $FrontController->contentCache()->import(self::$serverSummaryCacheName);

            $this->outputJavascript();
        }
        catch(Exception $e)
        {
?><p>A master server listing is not presently available. Please try again later.</p><?php
        }

        // Output footnotes.
        includeHTML('footnotes', self::$name);

?></div><?php

        $FrontController->endPage();
    }
}
