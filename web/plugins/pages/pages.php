<?php
/**
 * @file pages.php
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
 * @author Copyright &copy; 2009-2013 Daniel Swanson <danij@dengine.net>
 */

includeGuard('PagesPlugin');

class PagesPlugin extends Plugin implements Actioner, RequestInterpreter
{
    public static $name = 'pages';

    private $_displayOptions = 0;
    private $_pageName;

    public function __construct() {}

    public function title()
    {
        return 'Pages';
    }

    public function mustUpdateCachedPage(&$pageFile, &$cacheName)
    {
        if(!FrontController::contentCache()->has($cacheName)) return TRUE;

        $cacheInfo = new ContentInfo();
        FrontController::contentCache()->info($cacheName, $cacheInfo);
        return (filemtime($pageFile) > $cacheInfo->modifiedTime);
    }

    /**
     * Implements RequestInterpreter
     */
    public function InterpretRequest($Request)
    {
        $url = urldecode($Request->url()->path());

        // @kludge skip over the first '/' in the home URL.
        $url = substr($url, 1);

        // Remove any trailing file extension, such as 'something.html'
        if(strrpos($url, '.'))
            $url = substr($url, 0, strrpos($url, '.'));

        // Tokenize the request URL so we can begin to determine what to do.
        $tokens = explode('/', $url);

        if(count($tokens))
        {
            $pageName = str_replace(" ", "_", $tokens[0]);
            $pageFile = DIR_PLUGINS.'/'.self::$name.'/html/'.$pageName.'.html';
            $cacheName = 'pages/'.$pageName.'.html';

            // Try to generate the page and add it to the cache.
            if(file_exists($pageFile))
            {
                if($this->mustUpdateCachedPage($pageFile, $cacheName))
                {
                    try
                    {
                        $content = file_get_contents($pageFile);
                        FrontController::contentCache()->store($cacheName, $content);
                    }
                    catch(Exception $e)
                    {
                        // @todo do not ignore!
                    }
                }
            }

            if(FrontController::contentCache()->has($cacheName))
            {
                FrontController::fc()->enqueueAction($this, array('page' => $pageName));
                return true; // Eat the request.
            }
        }

        return false; // Not for us.
    }

    public function generateHTML()
    {
        includeHTML('introduction', self::$name);
    }

    /**
     * Implements Actioner.
     */
    public function execute($args=NULL)
    {
        $fc = &FrontController::fc();

        if(!is_array($args) && !array_key_exists('page', $args))
            throw new Exception('Unexpected arguments passed.');

        $page = $args['page'];
        $mainHeading = ucwords(mb_ereg_replace('_', ' ', $page));
        $pageFile = 'pages/'.$args['page'].'.html';

        $fc->outputHeader($mainHeading);
        $fc->beginPage($mainHeading, $page);

?>              <div id="contentbox2"><?php

        try
        {
            FrontController::contentCache()->import($pageFile);
        }
        catch(Exception $e)
        {
?>
                <p>No content for this page</p>
<?php
        }

?>              </div><?php

        $fc->endPage();
    }
}
