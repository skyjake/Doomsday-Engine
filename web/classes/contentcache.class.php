<?php
/**
 * @file contentcache.class.php
 * Local cache for dynamically generated content/resource.
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
 * @author Copyright @ 2009-2013 Daniel Swanson <danij@dengine.net>
 */

includeGuard('ContentCache');

class ContentInfo
{
    public $modifiedTime = NULL; // Unix timestamp.
}

class ContentCache
{
    private $_docRoot = NULL;

    public function __construct($docRoot=NULL)
    {
        $this->_docRoot = $docRoot.'/';
    }

    public function store($file, $content)
    {
        if(!isset($file) || $file == '' ||
           !isset($content) || $content == '')
            return false;

        $file = FrontController::absolutePath($this->_docRoot.'/'.$file);

        /*try
        {*/
            if(!file_exists($file))
            {
                $fh = fopen_recursive($file, 'w');
            }
            else if(!is_writable($file))
            {
                throw new Exception(sprintf('file %s not writeable.', $file));
            }
            else
            {
                $fh = fopen($file, 'w');
            }

            if(!$fh)
                throw new Exception(sprintf('failed opening file %s.', $file));

            if(!flock($fh, LOCK_EX))
                throw new Exception(sprintf('failed obtaining write lock on file %s.', $file));

            fwrite($fh, $content);
            flock($fh, LOCK_UN);
            fclose($fh);
        /*}
        catch(Exception $e)
        {
            die($e->getMessage());
        }*/

        return true;
    }

    /**
     * Is the specified content element present in the cache?
     *
     * @param relPath  (String) File name to look up.
     * @return  (Boolean) TRUE iff the content element is available.
     */
    public function has($relPath)
    {
        return (bool) file_exists(FrontController::nativePath($this->_docRoot."/$relPath"));
    }

    /**
     * Attempt to retrieve a content element from the cache, copying
     * its contents into a string.
     *
     * @param relPath  (String) File name to retrieve.
     * @return  (Boolean) FALSE if the content element could not be
     *     found, else the content as a string.
     */
    public function retrieve($relPath)
    {
        $path = FrontController::nativePath($this->_docRoot."/$relPath");

        if(!$path || !file_exists($path))
            throw new Exception(sprintf('file %s not present in content cache.', $relPath));

        if($stream = fopen($path, 'r'))
        {
            $contents = stream_get_contents($stream);
            fclose($stream);
            return $contents;
        }
        return "";
    }

    /**
     * Retrieve info about a file in the cache.
     *
     * @param relPath  (String) Name of the file to get info for.
     * @param info  (ContentInfo) Info record to be populated.
     * @return  (Boolean) FALSE if the specified file does not exist.
     */
    public function info($relPath, &$info)
    {
        if(!$info instanceof ContentInfo) return FALSE;

        $path = FrontController::nativePath($this->_docRoot."/$relPath");
        if(!$path || !file_exists($path)) return FALSE;

        $info->modifiedTime = filemtime($path);
        return TRUE;
    }

    /**
     * Touch a file in the cache (update modified time).
     *
     * @param relPath  (String) Name of the file to touch.
     * @return  (Boolean) FALSE if the specified file does not exist.
     */
    public function touch($relPath)
    {
        $path = FrontController::nativePath($this->_docRoot."/$relPath");
        if(!$path || !file_exists($path)) return FALSE;

        touch($path);
        return TRUE;
    }

    /**
     * Attempt to import a content element from the cache, outputting
     * its contents straight to the output buffer.
     *
     * @param relPath  (String) File name to retrieve.
     * @return  (Boolean) TRUE iff the content element was imported.
     */
    public function import($relPath)
    {
        $path = FrontController::nativePath($this->_docRoot."/$relPath");

        if(!$path || !file_exists($path))
            throw new Exception(sprintf('file %s not present in content cache.', $relPath));

        return @readfile($path);
    }
}
