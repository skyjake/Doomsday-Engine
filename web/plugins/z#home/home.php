<?php
/** @file home.php Implements the dengine.net homepage.
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

includeGuard('HomePlugin');

require_once(DIR_CLASSES.'/requestinterpreter.interface.php');
require_once(DIR_CLASSES.'/visual.interface.php');

class HomePlugin extends Plugin implements Actioner, RequestInterpreter
{
    public static $name = 'Home Plugin';
    private static $title = 'Doomsday Engine';

    public function title()
    {
        return self::$title;
    }

    /// Implements RequestInterpreter
    public function InterpretRequest($request)
    {
        $url = urldecode($request->url()->path());
        // @todo kludge: skip over the first '/' in the home URL.
        $url = substr($url, 1);

        // Remove any trailing file extension, such as 'something.html'
        if(strrpos($url, '.'))
            $url = substr($url, 0, strrpos($url, '.'));

        // Our only action will be to display a page to the user.
        FrontController::fc()->enqueueAction($this, NULL);
        return true; // Eat the request.
    }

    /// Implements Actioner.
    public function execute($args=NULL)
    {
        $fc = &FrontController::fc();
        $fc->outputHeader($fc->siteDescription());
        $fc->beginPage($this->title());

        includeHTML('latestversion', 'z#home');

?><div class="asideboxgroup"><div id="downloadbox" class="asidebox"><?php

        includeHTML('getitnow', 'z#home');

?></div><?php

?><div id="socialbookmarkbox" class="asidebox"><?php

        includeHTML('socialbookmarks', 'z#home');

?></div><?php

?><div id="contentbox"><?php

        includeHTML('introduction', 'z#home');

?></div><?php

        $fc->endPage();
    }
}
