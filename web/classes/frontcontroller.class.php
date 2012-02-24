<?php
/**
 * @file frontcontroller.class.php
 * Centralized point of entry for HTTP request interpretation.
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
    private $_request;
    private $_plugins;
    private $_actions;
    private $_contentCache = NULL;

    private $_visibleErrors = false;

    private $_homeURL = '';
    private $_siteTitle = 'Website';
    private $_siteAuthor = 'Author';
    private $_siteAuthorEmail = 'N/A';
    private $_siteDesigner = 'N/A';
    private $_siteDesignerEmail = 'N/A';
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

        if(array_key_exists('Designer', $config))
            $this->_siteDesigner = $config['Designer'];

        if(array_key_exists('DesignerEmail', $config))
            $this->_siteDesignerEmail = $config['DesignerEmail'];

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

    public function __construct($config)
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
        set_error_handler(array(&$this,'ErrorHandler'));

        // Locate plugins.
        $this->_plugins = new Plugins(DIR_PLUGINS);

        // Construct the Request
        $url = $_SERVER['SERVER_PORT'] == '443' ? 'https' : 'http'
            .'://'.$_SERVER['HTTP_HOST'].$_SERVER['REQUEST_URI'];

        $this->_request = new Request($url, $_POST);
        $this->_actions = new Actions();
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

    public function siteDesigner()
    {
        return $this->_siteDesigner;
    }

    public function siteDesignerEmail()
    {
        return $this->_siteDesignerEmail;
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

    private function outputNewsFeed()
    {
        require_once(DIR_CLASSES.'/feed.class.php');

        $NewsFeed = new Feed('http://dengine.net/forums/rss.php?mode=news');
        $NewsFeed->generateHTML();
    }

    private function outputServerStatus()
    {
        /*require_once(DIR_CLASSES.'/gameservers.class.php');

        $GameServers = new GameServers('http://dengine.net/master.php?list=1');
        $GameServers->generateHTML();*/
    }

    private function outputTopPanel()
    {
?>
    <div id="panorama">
    </div>
<?php
    }

    private function outputFooter()
    {
?>
<p class="disclaimer"><a href="mailto:webmaster@dengine.net" title="Contact Webmaster">Webmaster</a> - Site design &copy; <?php echo date('Y'); ?> <a href="mailto:<?php echo $this->siteDesignerEmail(); ?>" title="Contact <?php echo $this->siteDesigner(); ?>"><?php echo $this->siteDesigner(); ?></a> - Built by <a href="mailto:<?php echo $this->siteDesignerEmail(); ?>" title="Contact <?php echo $this->siteDesigner(); ?>"><?php echo $this->siteDesigner(); ?></a> / <a href="mailto:<?php echo $this->siteAuthorEmail(); ?>" title="Contact <?php echo $this->siteAuthor(); ?>"><?php echo $this->siteAuthor(); ?></a></p><?php

        includeHTML('validatoricons');

?><p class="disclaimer">The Doomsday Engine is licensed under the terms of the <a href="http://www.gnu.org/licenses/gpl.html" rel="nofollow">GNU/GPL License ver 2</a>. The Doomsday Engine logo is Copyright &copy; 2005-<?php echo date('Y'); ?>, the deng team.</p>
<hr />
<?php

        includeHTML('legal');
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

                if(!is_null($page) && !strcasecmp($page, $tab['page']))
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
        $leftTabs[] = array('page'=>'/engine',   'label'=>'Engine',   'tooltip'=>'About Doomsday Engine');
        $leftTabs[] = array('page'=>'/games',    'label'=>'Games',    'tooltip'=>'Games playable with Doomsday Engine');
        $leftTabs[] = array('page'=>'/addons',   'label'=>'Add-ons',  'tooltip'=>'Add-ons for games playable with Doomsday Engine');

        $rightTabs = array();
        $rightTabs[] = array('page'=>'/dew',     'label'=>'Wiki',     'tooltip'=>'Doomsday Engine Wiki');
        $rightTabs[] = array('page'=>'/forums/viewforum.php?f=24',    'label'=>'Dev Blog', 'tooltip'=>'Doomsday Engine Developer Blog');
        $rightTabs[] = array('page'=>'/forums',  'label'=>'Forums',   'tooltip'=>'Doomsday Engine User Forums');

?>
        <div id="menu" class="hnav">
            <ul>
<?php
            echo $this->buildTabs($leftTabs, $page, "paddle_left", "paddle_left_select");
?>
<li class="logo"><a href="/" title="<?=$this->homeURL()?>"><span class="hidden">.</span></a></li>
<?php
            echo $this->buildTabs($rightTabs, $page, "paddle_right", "paddle_right_select");
?>
            </ul>
        </div>
<?php
    }

    public function beginPage($mainHeading='', $page=null)
    {
?>
<body>
<div id="mainouter">
    <div class="t"><div class="b"><div class="l"><div class="r"><div class="bl"><div class="br"><div class="tl"><div class="tr">
        <div id="divider"></div>
<?php
        $this->outputMainMenu($page);
?>
        <div id="divider2"></div>
        <div id="maininner">
            <div id="framepanel_bottom">
                <div id="pageheading">
                    <h1><?php

        if(strlen($mainHeading) > 0)
            echo htmlspecialchars($mainHeading);
        else
            echo $this->siteTitle();

?></h1>
                </div>
<?php
    }

    public function endPage()
    {
?>
            </div>
            <div class="framepanel" id="framepanel_top">
<?php

        $this->outputTopPanel();

?>              <div id="projectnews"><?php

        $this->outputNewsFeed();

?>              </div><?php

?>              <div id="servers"><?php

        $this->outputServerStatus();

?>              </div><?php

?>
            </div>
        </div>
    </div>
</div></div></div></div></div></div></div></div>
<div id="footer">
<?php

        $this->outputFooter();

?>
</div>
<div id="libicons">
<?php

        // Output the lib icons (OpenGL, SDL etc).
        includeHTML('libraryicons');

?>
</div>
</body>
</html>
<?php
    }

    public function outputHeader($mainHeading='')
    {
        $siteTitle = $this->siteTitle();
        if(strlen($mainHeading) > 0)
            $siteTitle = "$mainHeading &bull; $siteTitle";

        header('Content-type: text/html; charset=utf-8');

?><!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
    <meta http-equiv="content-type" content="text/html; charset=UTF-8" />
    <link rel="icon" href="http://code.iki.fi/dengine.net/images/favicon.png" type="image/png" />
    <link rel="shortcut icon" href="http://code.iki.fi/dengine.net/images/favicon.png" type="image/png" />
    <link rel="alternate" type="application/rss+xml" title="Doomsday Engine RSS News Feed" href="http://dengine.net/forums/rss.php?mode=news" />
    <meta http-equiv="expires" content="0" />
    <meta name="resource-type" content="DOCUMENT" />
    <meta name="distribution" content="GLOBAL" />
    <meta name="author" content="<?=$this->siteAuthor()?>" />
    <meta name="copyright" content="<?=$this->siteCopyright()?>" />
    <meta name="keywords" content="<?php { $keywords = $this->defaultPageKeywords(); foreach($keywords as $keyword) echo $keyword.','; } ?>" />
    <meta name="description" content="<?=$this->siteDescription()?>" />
    <meta name="robots" content="INDEX, FOLLOW" />
    <meta name="revisit-after" content="<?=$this->robotRevisitDays()?> DAYS" />
    <meta name="rating" content="GENERAL" />
    <meta name="generator" content="<?=$this->homeURL()?>" />
    <title><?=$siteTitle?></title>

    <!-- jQuery -->
    <script type="text/javascript" src="/external/jquery/js/jquery-1.6.2.min.js"></script>

    <!-- jCarousel -->
    <script type="text/javascript" src="/external/jquery.jcarousel/js/jquery.jcarousel.min.js"></script>
    <link rel="stylesheet" type="text/css" href="/external/jquery.jcarousel/skin.css" />

    <!-- Thickbox 3 -->
    <script type="text/javascript" src="/external/thickbox/js/thickbox.js"></script>
    <link rel="stylesheet" type="text/css" href="/external/thickbox/thickbox.css" />

    <meta http-equiv="content-style-type" content="text/css" />
    <link rel="stylesheet" href="/style.css" type="text/css" media="all" />
</head>
<?php
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

    public function contentCache()
    {
        if(!is_object($this->_contentCache))
        {
            $this->_contentCache = new ContentCache(DIR_CACHE);
        }

        return $this->_contentCache;
    }

    public static function absolutePath($path)
    {
        return $_SERVER['DOCUMENT_ROOT'] . self::nativePath('/'.$path);
    }

    public static function nativePath($path)
    {
        $path = str_replace(array('/', '\\'), DIRECTORY_SEPARATOR, $path);

        $result = array();
        $pathA = explode(DIRECTORY_SEPARATOR, $path);
        if(!$pathA[0])
            $result[] = '';
        foreach($pathA AS $key => $dir)
        {
            if($dir == '..')
            {
                if(end($result) == '..')
                {
                    $result[] = '..';
                }
                else if(!array_pop($result))
                {
                    $result[] = '..';
                }
            }
            else if($dir && $dir != '.')
            {
                $result[] = $dir;
            }
        }

        if(!end($pathA))
            $result[] = '';

        return implode(DIRECTORY_SEPARATOR, $result);
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
                              $errortype[$errno], $errmsg, $filename, $linenum));
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
        case E_STRICT:
            return true;

        default:
            exit(); // Get out of here!
        }
    }
}
