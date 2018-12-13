<?php
/**
 * @file example.php
 * Example of a dengine.net plugin.
 *
 * To activate, rename the directory which contains this file, removing
 * the 'disabled.' prefix from the directory name. After which you can
 * then interface with this plugin live in your web browser.
 *
 * For this example, any page request which begins "example" will be
 * caught by this plugin and a plain Hello World page is displayed.
 *
 * e.g., http://dengine.net/example
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

includeGuard('ExamplePlugin');

require_once(DIR_CLASSES.'/requestinterpreter.interface.php');
require_once(DIR_CLASSES.'/visual.interface.php');

class ExamplePlugin extends Plugin implements Actioner, RequestInterpreter
{
    /// Required string which names this plugin in plugins collection
    /// owned by the FrontController.
    public static $name = 'Example Plugin';

    /// The sole page bares this identifier.
    private static $baseRequestName = 'example';

    /// @deprecated Older method for determining the name of this plugin
    /// which is no longer used but still required (to be removed).
    public function title()
    {
        return self::$name;
    }

    /**
     * Implements RequestInterpreter
     *
     * Plugins which need to respond to HTTP requests should implement this
     * interface and then use the interface methods of FrontController to
     * examine a request to determine if it is intended for that plugin.
     *
     * FrontController will call this function automatically for every HTTP
     * request it receives, unless it has already been eaten by a request
     * interpreter with higher priority than this.
     *
     * @c true should only be returned if the request is eaten (and no other
     *     interpreters will have a chance at processing it).
     *
     * @param request (object) Request. Represents the HTTP request.
     */
    public function InterpretRequest($request)
    {
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
            // Our only action will be to display a page to the user.
            FrontController::fc()->enqueueAction($this, NULL);
            return true; // Eat the request.
        }

        return false; // Not for us.
    }

    /**
     * Implements Actioner.
     */
    public function execute($args=NULL)
    {
        $fc = &FrontController::fc();

        $fc->outputHeader($this->title());
        $fc->beginPage($this->title());

?><div id="example_page" style="padding: 1em;background-color: white;"><p>Hello World!</p></div><?php

        $fc->endPage();
    }
}
