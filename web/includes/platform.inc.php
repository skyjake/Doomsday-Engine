<?php
/**
 * @file platform.inc.php
 * Platform level default configuration and global constants.
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

/**
 * Helper utility to ensure code modules are only included once.
 *
 * To guard a section of code from being interpreted only once by PHP, call
 * this method with the symbolic name for your module. Once called, any
 * subsequent attempt to include the same file will produce an Exception.
 *
 * Also guards against directly accessing a source file (e.g., entering the
 * path to the module file the call appears in), resulting in a 403 forbidden
 * page redirect.
 *
 * @param moduleName  (string) Symbolic name of the module to guard.
 */
function includeGuard($moduleName)
{
    // Prevent direct access.
    // Should never happen as all requests are supposed to be rewritten and
    // passed to index.php
    if(count(get_included_files()) ==1)
    {
        header('HTTP/1.0 403 Forbidden');
        exit;
    }

    // Ensure this module is only included once.
    $moduleName = strtoupper("$moduleName");
    $finalModuleName = "MODULE_$moduleName";
    if(defined($finalModuleName))
        throw new Exception("Module name '$moduleName' already defined");

    define("$finalModuleName", TRUE);
}

// Guard this file too.
includeGuard('platform');

/**
 * Apply assumed default PHP configuration.
 *
 * Much of the configuration done here is actually overridden at a higher
 * level however we define a default for normalization reasons.
 */

mb_internal_encoding("utf-8");
mb_http_output("utf-8");

date_default_timezone_set('Europe/London');

/**
 * @defgroup basePaths Base Paths
 * @ingroup platform
 */
///{
define('DIR_CACHE',    'cache'); ///> Local content cache. Automatically rebuilt.
define('DIR_CLASSES',  'classes'); ///> 'Core' source module files.
define('DIR_EXTERNAL', 'external'); ///> External/3rd-party libraries (jQuery, Magpie, etc...).
define('DIR_HTML',     'html'); ///> Global template files used throughout the site.
define('DIR_IMAGES',   'images'); ///> Graphics used throughout the site.
define('DIR_INCLUDES', 'includes'); ///> Global utility functions.
define('DIR_PLUGINS',  'plugins'); /// 'Plugin' source module files
///}
