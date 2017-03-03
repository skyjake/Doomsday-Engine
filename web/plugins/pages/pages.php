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
require_once(DENG_API_DIR.'/include/builds.inc.php');

function generate_download_badge($db, $file_id)
{
    // Fetch all information about this file.
    $result = db_query($db, "SELECT * FROM ".DB_TABLE_FILES." WHERE id=$file_id");
    $file = $result->fetch_assoc();
    
    $build = db_get_build($db, $file['build']);
    $version = $build['version'];
    
    $result = db_query($db, "SELECT * FROM ".DB_TABLE_PLATFORMS." WHERE id=$file[plat_id]");
    $plat = $result->fetch_assoc();

    $fext = strtoupper(pathinfo($file['name'], PATHINFO_EXTENSION))
        ." ($plat[cpu_bits]-bit)";

    $title = "Doomsday ".omit_zeroes($version);
    if ($build['type'] != BT_STABLE) {
        $title .= " [#".$build['build']."]";
    }
    $title .= " &middot; $fext";
    $full_title = "Doomsday ".human_version($version, $build['build'], build_type_text($build['type']))
        ." for ".$plat['name']." (".$plat['cpu_bits']."-bit)";
    $download_url = 'http://api.dengine.net/1/builds?dl='.$file['name'];
    
    $metadata = '<span class="metalink">';
    $metadata .= '<span title="Release Date">'
        .substr($build['timestamp'], 0, 10)
        .'</span> &middot; '; 
    $metadata .= $plat['cpu_bits'].'-bit '.$plat['name'].' (or later)';
    $metadata .= '</span>';
    
    echo('<p><div class="package_badge">'
        ."<a class='package_name' href='$download_url' "
        ."title=\"Download $full_title\">$title</a><br />"
        .$metadata
        ."</div></p>\n");
    
    /*
        // Compose badge title.
        var cleanTitle = json.title + ' ' + json.version;

        // Generate metadata HMTL.
        var metaData = '<span class="metalink">';
        if(json.hasOwnProperty('is_unstable'))
        {
            var isUnstable = json.is_unstable;
            var buildId = '';

            if(json.hasOwnProperty('build_uniqueid'))
            {
                buildId += ' build' + json.build_uniqueid;
            }

            if(isUnstable)
            {
                cleanTitle += ' (Unstable)';
            }
        }

        if(json.hasOwnProperty('release_date'))
        {
            var releaseDate = new Date(Date.parse(json.release_date));
            var shortDate = (releaseDate.getDate()) + '/' + (1 + releaseDate.getMonth()) + '/' + releaseDate.getFullYear();
            metaData += '<span title="Release Date">' + shortDate + '</span> &middot; ';
        }
        metaData += 'Windows Vista (or later)</span>';

        return $('<div class="package_badge"><a class="package_name" href="' + downloadUri + '" title="Download ' + json.fulltitle + '">' + cleanTitle + '</a><br />' + metaData + '</div>');*/
}

function generate_badges($platform, $type)
{
    // Find the latest suitable files.
    $db = db_open();
    $result = db_latest_files($db, $platform, $type);
    $latest_build = 0;
    while ($row = $result->fetch_assoc()) {        
        // All suitable files of the latest build will be shown.
        if ($latest_build == 0) {
            $latest_build = $row['build'];
        }
        else if ($latest_build != $row['build']) {
            break;
        }
        generate_download_badge($db, $row['id']);
    }
    $db->close();
}

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
            $pageFile = __DIR__.'/html/'.$pageName.'.html';
            //$cacheName = 'pages/'.$pageName.'.html';

            // Try to generate the page and add it to the cache.
            if(file_exists($pageFile))
            {
                /*if($this->mustUpdateCachedPage($pageFile, $cacheName))
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
                }*/
                //}

            //if(FrontController::contentCache()->has($cacheName))
            //{
                FrontController::fc()->enqueueAction($this, array('page' => $pageName));
                return true; // Eat the request.
            }
        }

        return false; // Not for us.
    }

    /*public function generateHTML()
    {
        includeHTML('introduction', self::$name);
    }*/

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
        if ($page == 'mac_os') $mainHeading = 'macOS';
        $pageFile = __DIR__.'/html/'.$args['page'].'.html';

        $fc->outputHeader($mainHeading);
        $fc->beginPage($mainHeading, $page);

?>              <div id="contentbox"><?php require($pageFile); ?>
                </div><?php

        $fc->endPage();
    }
}
