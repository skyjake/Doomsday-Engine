<?php
/**
 * @file buildevent.class.php
 * Abstract (conceptual) class used to model an autobuilder event.
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

includeGuard('BuildEvent');

class BuildEvent
{
    private $uniqueId;
    private $startDate;
    private $authorName;
    private $authorEmail;
    private $releaseType;

    /// \todo Collections should be private but allow public iteration.
    public $packages = array();
    public $commits = array();

    /**
     * @param uniqueId (integer)
     * @param startDate  (string) Unix timestamp when build commenced.
     * @param authorName (string)
     * @param authorEmail (string)
     * @param releaseType (integer)
     */
    public function __construct($uniqueId, $startDate, $authorName, $authorEmail,
        $releaseType=RT_UNSTABLE)
    {
        $this->uniqueId = $uniqueId;
        $this->startDate = $startDate;
        $this->authorName = $authorName;
        $this->authorEmail = $authorEmail;
        $this->releaseType = $releaseType;
    }

    public function uniqueId()
    {
        return $this->uniqueId;
    }

    public function composeName()
    {
        return "Build$this->uniqueId";
    }

    public function &startDate()
    {
        return $this->startDate;
    }

    public function composeBuildUri()
    {
        return "build$this->uniqueId";
    }

    public function releaseType()
    {
        return $this->releaseType;
    }

    public function addPackage(&$package)
    {
        $this->packages[] = $package;
    }

    /**
     * Add a new Commit record to this build event.
     *
     * @param commit  (Array) Commit record.
     *
     * 'submitDate': (object) Date
     * 'author': (string)
     * 'repositoryUri': (string)
     * 'sha1': (string)
     * 'title': (string)
     * 'message': (string)
     * 'tags': (array)
     */
    public function addCommit($commit)
    {
        $this->commits[] = $commit;
    }

    public function genBadge()
    {
        $shortDate = date("d/m/y", $this->startDate);
        $buildUriTitle = $shortDate .' - '. $this->composeName();

        $html = '<a href="'.$this->composeBuildUri().'" title="'.$buildUriTitle.'">'.
            $this->composeName() .'</a>';
        return '<div class="build_news">'.$html.'</div>';
    }

    public function __toString()
    {
        return '('.get_class($this).":$this->name)";
    }
}
