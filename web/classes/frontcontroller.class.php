<?php
/** @file frontcontroller.class.php Centralized point of entry
 *
 * Also oversees and routes HTTP request interpretation to plugins.
 *
 * @authors Copyright Â© 2009-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

// Define the "platform", our expected default configuration.
require_once('includes/platform.inc.php');

includeGuard('FrontController');

require(DIR_INCLUDES.'/utilities.inc.php');

require_once(DIR_CLASSES.'/contentcache.class.php');
require_once(DIR_CLASSES.'/actions.class.php');
require_once(DIR_CLASSES.'/request.class.php');
require_once(DIR_CLASSES.'/plugins.class.php');
require_once(DIR_CLASSES.'/actioner.interface.php');
require_once(DIR_CLASSES.'/requestinterpreter.interface.php');

class FrontController
{
    private static $singleton = NULL;

    private $_request;
    private $_plugins;
    private $_actions;
    private $_contentCache = NULL;

    private $_visibleErrors = false;

    private $_homeURL = '';
    private $_siteTitle = 'Website';
    private $_siteAuthor = 'Author';
    private $_siteAuthorEmail = 'N/A';
    private $_siteCopyright = 'N/A';
    private $_siteDescription = '';
    private $_siteRobotRevisitDays = 0;
    private $_defaultPageKeywords = NULL;

    private function parseConfig($config)
    {
        if(!is_array($config))
            return;

        if(array_key_exists('VisibleErrors', $config))
            $this->_visibleErrors = (bool) $config['VisibleErrors'];

        if(array_key_exists('HomeAddress', $config))
            $this->_homeURL = $config['HomeAddress'];

        if(array_key_exists('Title', $config))
            $this->_siteTitle = $config['Title'];

        if(array_key_exists('Author', $config))
            $this->_siteAuthor = $config['Author'];

        if(array_key_exists('AuthorEmail', $config))
            $this->_siteAuthorEmail = $config['AuthorEmail'];

        if(array_key_exists('HomeURL', $config))
            $this->_siteHomeURL = $config['HomeURL'];

        if(array_key_exists('Copyright', $config))
            $this->_siteCopyright = $config['Copyright'];

        if(array_key_exists('Description', $config))
            $this->_siteDescription = $config['Description'];

        if(array_key_exists('RobotRevisitDays', $config))
            $this->_siteRobotRevisitDays = $config['RobotRevisitDays'];

        if(array_key_exists('Keywords', $config))
            $this->_defaultPageKeywords = $config['Keywords'];
    }

    private function __construct($config)
    {
        if(!is_array($config))
            throw new Exception('Invalid config, array expected');

        if((double)phpversion() < 5.1)
            throw new Exception('PHP version 5.1 required.');

        $this->parseConfig($config);

        // Set error handling behavior.
        error_reporting(E_ALL);
        ini_set('display_errors', (bool) $this->_visibleErrors);
        ini_set('display_startup_errors', (bool) $this->_visibleErrors);
        set_error_handler(array(&$this,"ErrorHandler"));

        // Locate plugins.
        $this->_plugins = new Plugins(DIR_PLUGINS);

        // Construct the Request
        $url = ($_SERVER['SERVER_PORT'] == '443' ? 'https' : 'http') .'://';
        if(isset($_SERVER['HTTP_HOST']))
            $url .= $_SERVER['HTTP_HOST'];
        if(isset($_SERVER['REQUEST_URI']))
            $url .= $_SERVER['REQUEST_URI'];

        $this->_request = new Request($url, $_POST);
        $this->_actions = new Actions();
    }

    private function __clone() {}

    /**
     * Returns the Singleton front controller instance.
     */
    public static function &fc()
    {
        if(is_null(self::$singleton))
        {
            $siteconfig = array();
            include('./config.inc.php');
            self::$singleton = new FrontController($siteconfig);
        }
        return self::$singleton;
    }

    public function &request()
    {
        return $this->_request;
    }

    public function &plugins()
    {
        return $this->_plugins;
    }

    public function findPlugin($pluginName)
    {
        return $this->plugins()->find($pluginName);
    }

    public function siteTitle()
    {
        return $this->_siteTitle;
    }

    public function siteAuthor()
    {
        return $this->_siteAuthor;
    }

    public function siteAuthorEmail()
    {
        return $this->_siteAuthorEmail;
    }

    public function siteCopyright()
    {
        return $this->_siteCopyright;
    }

    public function siteDescription()
    {
        return $this->_siteDescription;
    }

    public function robotRevisitDays()
    {
        return $this->_siteRobotRevisitDays;
    }

    public function &defaultPageKeywords()
    {
        return $this->_defaultPageKeywords;
    }

    public function homeURL()
    {
        return $this->_homeURL;
    }

    public function enqueueAction($actioner, $args=NULL)
    {
        try
        {
            $this->_actions->enqueue($actioner, $args);
        }
        catch(Exception $e)
        {
            throw new Exception('Failed to enqueue Action');
        }
    }

    public function interpretRequest()
    {
        // Determine action:
        try
        {
            // Maybe a plugin?
            $this->plugins()->interpretRequest($this->_request);

            foreach($this->_actions as $ActionRecord)
            {
                try
                {
                    $ActionRecord['actioner']->execute($ActionRecord['args']);
                }
                catch(Exception $e)
                {
                    throw $e; // Lets simply re-throw and abort for now.
                }
            }
        }
        catch(Exception $e)
        {
            // We'll show the homepage.
            $this->enqueueAction($this->findPlugin('home'), NULL);
        }
    }

    private function &getContentCache()
    {
        if(!is_object($this->_contentCache))
        {
            $this->_contentCache = new ContentCache(DIR_CACHE);
        }

        return $this->_contentCache;
    }

    public static function &contentCache()
    {
        return self::fc()->getContentCache();
    }

    public static function ErrorHandler($errno, $errmsg, $filename, $linenum, $vars)
    {
        $errortype = array (
                    E_ERROR              => 'Error',
                    E_WARNING            => 'Warning',
                    E_PARSE              => 'Parsing Error',
                    E_NOTICE             => 'Notice',
                    E_CORE_ERROR         => 'Core Error',
                    E_CORE_WARNING       => 'Core Warning',
                    E_COMPILE_ERROR      => 'Compile Error',
                    E_COMPILE_WARNING    => 'Compile Warning',
                    E_USER_ERROR         => 'User Error',
                    E_USER_WARNING       => 'User Warning',
                    E_USER_NOTICE        => 'User Notice',
                    E_STRICT             => 'Runtime Notice'
                    );

        // Log it?
        if(ini_get('log_errors'))
        {
            error_log(sprintf("PHP %s:  %s in %s on line %d",
                              isset($errortype[$errno])? $errortype[$errno] : "$errno", $errmsg, $filename, $linenum));
        }

        // Display it?
        if(ini_get('display_errors'))
        {
            printf("<br />\n<b>%s</b>: %s in <b>%s</b> on line <b>%d</b><br /><br />\n",
                   $errortype[$errno], $errmsg, $filename, $linenum);

            return true;
        }

        switch($errno)
        {
        case E_NOTICE:
        case E_USER_NOTICE:
        case E_USER_WARNING:
        case E_STRICT:
            return true;

        default:
            exit(); // Get out of here!
        }
    }

    public function outputHeader($mainHeading='')
    {
        $siteTitle = $this->siteTitle();
        if(strlen($mainHeading) > 0)
            $siteTitle = "$mainHeading &bull; $siteTitle";

        header('X-Frame-Options: SAMEORIGIN');

?><!DOCTYPE html>
<html dir="ltr" lang="en-GB">
<head>
    <title><?=$siteTitle?></title>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width" />
    <meta name="author" content="<?=$this->siteAuthor()?>" />
    <meta name="keywords" content="<?php { $keywords = $this->defaultPageKeywords(); foreach($keywords as $keyword) echo $keyword.','; } ?>" />
    <meta name="description" content="<?=$this->siteDescription()?>" />
    <meta name="robots" content="index, follow" />
    <meta name="revisit-after" content="<?=$this->robotRevisitDays()?> DAYS" />
    <meta name="rating" content="GENERAL" />
    <meta name="generator" content="<?=$this->homeURL()?>" />
    <link rel="icon" href="http://dl.dropboxusercontent.com/u/11948701/dengine.net/images/favicon.png" type="image/png" />
    <link rel="shortcut icon" href="http://dl.dropboxusercontent.com/u/11948701/dengine.net/images/favicon.png" type="image/png" />
    <link rel="alternate" type="application/rss+xml" title="Doomsday Engine RSS News Feed" href="http://dengine.net/forums/rss.php?mode=news" />

    <script src="//ajax.googleapis.com/ajax/libs/jquery/1.9.1/jquery.min.js"></script>
    <script src="//ajax.googleapis.com/ajax/libs/jqueryui/1.9.1/jquery-ui.min.js"></script>
    <link rel="stylesheet" media="all" href="/style.css" />
    <!--[if lt IE 9]>
    <script src="http://html5shiv.googlecode.com/svn/trunk/html5.js"></script>
    <![endif]-->
</head>
<?php
    }

    public function beginPage($mainHeading='', $page=null)
    {

?><body>
    <div id="main">
        <div id="framepanel_bottom" role="main"><?php

        $this->outputMainMenu($page);

?><section id="pageheading"><h1><?php

        if(strlen($mainHeading) > 0)
            echo htmlspecialchars($mainHeading);
        else
            echo $this->siteTitle();

?></h1><p style="display:none"><?php

        echo $this->siteDescription();

?></p></section><?php

    }

    public function endPage()
    {
?>
            </div>
            <div id="framepanel_top" class="framepanel">
<?php

        $this->outputTopPanel();

?>
        </div>
        <footer role="contentinfo">
<?php

        $this->outputFooter();

?>        </footer>
    </div>
</body>
</html>
<?php
    }

    /**
     * Generate HTML markup for a summary of this server.
     */
    private function generateServerSummary(&$info)
    {
        if(!is_array($info))
            throw new Exception('Invalid info argument, array expected.');

        $serverMetadataLabel = 'Address: '. htmlspecialchars($info['at']).':'. htmlspecialchars($info['port'])
                              .' Open: '. ($info['open']? 'yes':'no');
                              // Should we include the extra info?
                              /*.' Info: '. htmlspecialchars($info['info']);*/

        // Any required addons?.
        $addonArr = array_filter(explode(';', $info['pwads']));
        if(count($addonArr))
        {
            $serverMetadataLabel .= 'Add-ons: '. implode(' ', $addonArr);
        }

        // Format the game setup label.
        $setupArr = array_filter(explode(' ', $info['setup']));
        $gameMetadataLabel = 'Map: '. htmlspecialchars($info['map']) .' Setup: '. htmlspecialchars(implode(' ', $setupArr));

        $playerCountLabel = 'Number of players currently in-game';
        $playerMaxLabel = 'Maximum number of players';

        // Begin html generation.
        $html = '<span class="player-summary"><label title="'. htmlspecialchars($playerCountLabel) .'">'. htmlspecialchars($info['nump']) .'</label> / <label title="'. htmlspecialchars($playerMaxLabel) .'">'. htmlspecialchars($info['maxp']) .'</label></span> '
               .'<label title="'. htmlspecialchars($serverMetadataLabel) .'"><span class="name">'. htmlspecialchars($info['name']) .'</span></label> '
               .'<label title="'. htmlspecialchars($gameMetadataLabel) .'"><span class="game-mode">'. htmlspecialchars($info['mode']) .'</span></label>';

        // Wrap it in the container used for styling and visual element ordering.
        return '<span class="server">'. $html .'</span>';
    }

    public static function serverSorter($b, $a)
    {
        // Open servers are grouped together.
        $diff = (integer)($b['open'] - $a['open']);
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

    private function outputMasterStatus()
    {
if(0)
{
        // Maximum number of servers listed in the summary.
        $limit = 3;

        /**
         * @todo We do NOT need to interface with the master server. We are able
         *       to implement all expected functionality by simply parsing the
         *       XML feed output.
         */
        require_once(DIR_CLASSES.'/masterserver.class.php');
        $db = new MasterServer();

        // Build the servers collection.
        $servers = array();
        while(list($ident, $info) = each($db->servers))
        {
            if(!is_array($info)) continue;
            $servers[] = $info;
        }

        // Sort the collection.
        uasort($servers, array('self', 'serverSorter'));
        $serverCount = count($servers);

        // Generate the content.
        $content = '<span id="servers-label">'. ($serverCount > 0? 'Most Active Servers':'No Active Servers') .'</span><br />';
        if($serverCount)
        {
            $playerCount = (integer)0;
            foreach($servers as &$server)
            {
                $playerCount += $server['nump'];
            }

            $content .= '<ul>';

            $n = (integer)0;
            foreach($servers as &$server)
            {
                if($limit !== 0 && $n++ == $limit) break;

                $content .= '<li>'. $this->generateServerSummary($server) .'</li>';
            }

            $content .= '</ul>';
            $content .= '<span id="summary"><a href="/masterserver" title="Click here to see the complete server listing">'. ("$serverCount ". ($serverCount === 1? 'server':'servers') ." in total, {$playerCount} active ". ($playerCount === 1? 'player':'players')) .'</a></span>';
        }

        $content .= '<br /><span id="servers-timestamp">'. date("d-M-y H:i:s T" /*DATE_RFC850*/, time()). '</span>';
        echo $content;
}

?><script>
(function (e) {
    e.fn.interpretMasterServerStatus = function (t) {
        var n = {
            serverUri: "http://dengine.net/master.php?xml",
            maxItems: 3,
            generateServerSummaryHtml: 0
        };
        if (t) {
            e.extend(n, t);
        }
        var r = e(this).attr("id");
        e.ajax({
            url: n.serverUri,
            dataType: 'xml',
            success: function (t) {
                e("#" + r).empty();
                var root = $('masterserver', t);
                var serverList = root.find('serverlist')[0];
                var serverCount = serverList.attributes.getNamedItem('size').nodeValue;

                var html = '<span id="servers-label">' + (serverCount > 0? 'Most Active Servers':'No Active Servers') + '</span><br />';

                if(serverCount) {
                    var idx = 0;
                    var playerCount = 0;

                    html += '<ul>';
                    for(var i = 0; i < serverList.childNodes.length; ++i) {
                        if(serverList.childNodes[i].nodeName != 'server')
                            continue;

                        var server = $(serverList.childNodes[i]);
                        if(n.maxItems <= 0 || idx < n.maxItems)
                        {
                            html += '<li>' + n.generateServerSummaryHtml(n, server) + '</li>';
                        }

                        playerCount += Number(server.find('gameinfo>numplayers').text());
                        idx++;
                    };

                    html += '</ul>';
                    html += '<span id="summary"><a href="/masterserver" title="Click here to see the complete server listing">' + serverCount + ' ' + (serverCount == 1? 'server':'servers') + ' in total, ' + playerCount + ' active ' + (playerCount == 1? 'player':'players') + '</a></span>';
                }

                html += '<br /><span id="servers-timestamp">' + root.find('channel>pubdate').text() + '</span>';
                e("#" + r).append(html);
            }
        })
    }
})(jQuery)

$(document).ready(function () {
    $('#servers').interpretMasterServerStatus({
        generateServerSummaryHtml: function (n, info) {

            var serverMetadataLabel = 'Address: ' + info.children('ip').text() + ':' + info.children('port').text()
                                    + ' Open: ' + info.children('open').text();

            // Any required addons?.
            /*var addonArr = array_filter(explode(';', info['pwads']));
            if(count(addonArr))
            {
                serverMetadataLabel .= 'Add-ons: '. implode(' ', addonArr);
            }*/

            var gameInfo = $(info.children('gameinfo'));
            var gameMetadataLabel = 'Map: ' + gameInfo.children('map').text() + ' Setup: ' + gameInfo.children('setupstring').text();

            var playerCountLabel = 'Number of players currently in-game';
            var playerMaxLabel = 'Maximum number of players';

            var html = '<span class="player-summary"><label title="' + playerCountLabel + '">' + gameInfo.children('numplayers').text() + '</label> / <label title="' + playerMaxLabel + '">' + gameInfo.children('maxplayers').text() + '</label></span> ';

            html += '<label title="' + serverMetadataLabel + '"><span class="name">' + info.find('name').text() + '</span></label> ';
            html += '<label title="' + gameMetadataLabel + '"><span class="game-mode">' + gameInfo.children('mode').text() + '</span></label>';

            return '<span class="server">' + html + '</span>';
        }
    });
});

</script><div id="servers"></div><?php

    }

    private function buildTabs($tabs, $page=NULL, $normalClassName=NULL, $selectClassName=NULL)
    {
        $result = "";

        if(is_array($tabs))
        {
            for($i = 0; $i < count($tabs); ++$i)
            {
                $tab = $tabs[$i];

                $result .= '<li>';

                if(!is_null($page) && !strcasecmp($page, substr($tab['page'], 1)))
                {
                    $result .= '<span ';
                    if(isset($selectClassName))
                        $result .= "class=\"$selectClassName\" ";
                    $result .= '>'.$tab['label'].'</span>';
                }
                else
                {
                    $result .= '<a ';
                    if(isset($normalClassName))
                        $result .= "class=\"$normalClassName\" ";
                    $result .= 'href="'.$tab['page'].'" title="'.$tab['tooltip'].'">'.$tab['label'].'</a>';
                }

                $result .= '</li>';
            }
        }
        return $result;
    }

    private function outputMainMenu($page=NULL)
    {
        $leftTabs = array();
        $leftTabs[] = array('page'=>'/engine',  'label'=>'Engine',   'tooltip'=>'About the Doomsday Engine');
        $leftTabs[] = array('page'=>'/games',   'label'=>'Games',    'tooltip'=>'Games playable with the Doomsday Engine');
        $leftTabs[] = array('page'=>'/dew',     'label'=>'Wiki',     'tooltip'=>'Doomsday Engine wiki (documentation)');

        $rightTabs = array();
        $rightTabs[] = array('page'=>'/addons',       'label'=>'Add-ons', 'tooltip'=>'Add-ons for games playable with the Doomsday Engine');
        $rightTabs[] = array('page'=>'/forums',       'label'=>'Forums',  'tooltip'=>'Doomsday Engine user forums');
        $rightTabs[] = array('page'=>'/masterserver', 'label'=>'Servers', 'tooltip'=>'Doomsday Engine server browser');

?>
        <div id="menuouter"><nav id="menu" class="hnav">
        <div id="divider"></div>
            <ul><section class="left">
<?php
            echo $this->buildTabs($leftTabs, $page, "paddle_left", "paddle_left_select");
?></section>
<li class="logo"><a href="/" title="<?=$this->homeURL()?>"><span class="hidden">.</span></a></li><section class="right">
<?php
            echo $this->buildTabs($rightTabs, $page, "paddle_right", "paddle_right_select");
?></section>
            </ul>
        <div id="divider2"></div>
        </nav></div>
<?php
    }

    private function outputTopPanel()
    {
?>
    <div id="panorama" role="banner">
    </div>
<?php
    }

    private function outputFooter()
    {
?>
<p class="disclaimer"><a href="mailto:webmaster@dengine.net" title="Contact Webmaster">Webmaster</a> - Site design &copy; <?php echo date('Y'); ?> Deng Team - Built by <a href="mailto:<?php echo $this->siteAuthorEmail(); ?>" title="Contact <?php echo $this->siteAuthor(); ?>"><?php echo $this->siteAuthor(); ?></a></p>
<p class="disclaimer">The Doomsday Engine is licensed under the terms of the <a href="http://www.gnu.org/licenses/gpl.html" rel="nofollow">GNU/GPL License ver 2</a>. The Doomsday Engine logo is Copyright &copy; 2005-<?php echo date('Y'); ?>, the deng team.</p>
<hr />
<?php

        includeHTML('legal');
    }
}
