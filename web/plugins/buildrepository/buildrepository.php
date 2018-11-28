<?php
/**
 * @file buildrepository.php  Build Repository.
 *
 * @authors Copyright © 2009-2014 Daniel Swanson <danij@dengine.net>
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

includeGuard('BuildRepositoryPlugin');

require_once(DENG_API_DIR.'/include/builds.inc.php');

class BuildRepositoryPlugin extends Plugin implements Actioner, RequestInterpreter
{
    /// Plugin name.
    public static $name = 'buildrepository';

    /// @return  Plugin name.
    public function title()
    {
        return self::$name;
    }

    /**
     * Implements RequestInterpreter
     */
    public function InterpretRequest($request)
    {
        $uri = urldecode($request->url()->path());
        // @kludge skip over the first '/' in the home URL.
        $uri = substr($uri, 1);
        // Remove any trailing file extension, such as 'something.html'
        if(strrpos($uri, '.'))
            $uri = substr($uri, 0, strrpos($uri, '.'));

        $uriArgs = $request->url()->args();

        // Tokenize the request URL so we can begin to determine what to do.
        $tokens = explode('/', $uri);
        if(!count($tokens)) return false; // How peculiar...

        // A symbolic build name?
        if($tokens[0] === 'latestbuild')
        {
            if(isset($uriArgs['platform']))
            {
                $args = ['platform' => $uriArgs['platform'],
                         'unstable' => isset($uriArgs['unstable'])];

                FrontController::fc()->enqueueAction($this, $args);
            }
            return true; // Eat the request.
        }

        // Perhaps a named build or the index?
        if(!strncasecmp('build', $tokens[0], 5))
        {
            $buildId = intval(substr($tokens[0], 5));
            FrontController::fc()->enqueueAction($this, ['build' => $buildId]);
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

        // Redirect to a particular build?
        if (isset($args['build'])) {
            header('Status: 307 Temporary Redirect');
            header("Location: ".DENG_API_URL."/builds?number=$args[build]&format=html");
        }
        else {        
            // JSON response.
            generate_platform_latest_json($args['platform'], 
                $args['unstable']? 'unstable' : 'stable');
        }
        exit;
    }
}
