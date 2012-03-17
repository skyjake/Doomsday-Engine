<?php
/**
 * @file baseaddon.class.php
 * Abstract base for all Addon objects.
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
 * @author Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
 */

includeGuard('BaseAddon');

abstract class BaseAddon
{
    protected $title = '(unnamed)';
    protected $version = NULL;
    protected $attribs = array('featured'=>0);

    public function __construct($title=NULL, $version=NULL)
    {
        if(!is_null($title))
            $this->title = "$title";
        if(!is_null($version))
            $this->version = "$version";
    }

    public function &title()
    {
        return $this->title;
    }

    public function &version()
    {
        return $this->version;
    }

    public function setAttrib($name, $value)
    {
        $name = strtolower("$name");
        if(!array_key_exists($name, $this->attribs)) return FALSE;

        $this->attribs[$name] = (boolean)$value;
    }

    /**
     * @return  (Boolean) @c true if this addon is marked 'featured'.
     */
    public function hasFeatured()
    {
        return (boolean)$this->attribs['featured'];
    }

    public function composeFullTitle($includeVersion=true)
    {
        $includeVersion = (boolean) $includeVersion;

        $title = $this->title;
        if($includeVersion && !is_null($this->version))
            $title .= ' '. $this->version;

        return $title;
    }

    public function __toString()
    {
        $fullTitle = $this->composeFullTitle();
        return '('.get_class($this).":$title)";
    }
}

class Addon extends BaseAddon
{
    static protected $emptyString = '';

    protected $games = NULL;

    protected $downloadUri = NULL;
    protected $homepageUri = NULL;
    protected $description = NULL;
    protected $notes = NULL;

    public function __construct($title=NULL, $version=NULL, $downloadUri=NULL, $homepageUri=NULL)
    {
        parent::__construct($title, $version);

        if(!is_null($downloadUri) && strlen($downloadUri) > 0)
            $this->downloadUri = "$downloadUri";

        if(!is_null($homepageUri) && strlen($homepageUri) > 0)
            $this->homepageUri = "$homepageUri";
    }

    public function addGames(&$games)
    {
        if(!isset($this->games))
        {
            $this->games = array();
        }

        $this->games = array_merge($this->games, $games);
    }

    /**
     * Does the addon support any of these game modes?
     *
     * @param addon  (Array) Addon record object.
     * @param gameModes  (Array) Associative array containing the list of
     *                   game modes to look for (the needles). If none are
     *                   specified; assume this addon supports it.
     * @return  (Boolean)
     */
    public function supportsGameMode(&$gameModes)
    {
        if(!isset($this->games)) return true;
        if(!is_array($gameModes)) return true;

        foreach($gameModes as $mode)
        {
            if(isset($this->games[$mode])) return true;
        }

        return false;
    }

    /**
     * Does the addon support ALL of these game modes?
     *
     * @param addon  (Array) Addon record object.
     * @param gameModes  (Array) Associative array containing the list of
     *                   game modes to look for (the needles). If none are
     *                   specified; assume this addon supports it.
     * @return  (Boolean)
     */
    public function supportsAllGameModes(&$gameModes)
    {
        if(!isset($this->games)) return true;
        if(!is_array($gameModes)) return true;

        foreach($gameModes as $mode)
        {
            if(!isset($this->games[$mode])) return false;
        }

        return true;
    }

    public function &downloadUri()
    {
        if(!$this->hasDownloadUri())
        {
            return $emptyString;
        }
        return $this->downloadUri;
    }

    public function hasDownloadUri()
    {
        return !is_null($this->downloadUri);
    }

    public function &homepageUri()
    {
        if(!$this->hasHomepageUri())
        {
            return $emptyString;
        }
        return $this->homepageUri;
    }

    public function hasHomepageUri()
    {
        return !is_null($this->homepageUri);
    }

    public function setDescription($description)
    {
        $this->description = "$description";
    }

    public function &description()
    {
        if(!$this->hasDescription())
        {
            return $emptyString;
        }
        return $this->description;
    }

    public function hasDescription()
    {
        return !is_null($this->description);
    }

    public function setNotes($notes)
    {
        $this->notes = "$notes";
    }

    public function &notes()
    {
        if(!$this->hasNotes())
        {
            return $emptyString;
        }
        return $this->notes;
    }

    public function hasNotes()
    {
        return !is_null($this->notes);
    }

    public function genDownloadBadge()
    {
        $html = '';

        if($this->hasDownloadUri())
        {
            $downloadUri = htmlspecialchars($this->downloadUri);
            $title = htmlspecialchars($this->title);

            $html .= "<div class=\"icon\">"
                        ."<a href=\"{$downloadUri}\""
                          ." title=\"Download {$title}\">"
                        ."<img src=\"images/packageicon.png\" alt=\"Package icon\" /></a></div>";

            $html .= "<a href=\"{$downloadUri}\""
                      ." title=\"Download {$title}\">";
        }
        else if($this->hasHomepageUri())
        {
            $title = htmlspecialchars($this->title);
            $homepageUri = htmlspecialchars($this->homepageUri());
            $homepageUriLabel = htmlspecialchars("Visit {$this->title} homepage");

            $html .= "<a href=\"{$homepageUri}\""
                      ." title=\"{$homepageUriLabel}\">";
        }

        $html .= '<span class="name">'. htmlspecialchars($this->title()) .'</span>';
        if($this->hasDescription())
        {
            $html .= '<span class="description">'. htmlspecialchars($this->description()) .'</span>';
        }

        if($this->hasDownloadUri() || $this->hasHomepageUri())
        {
            $html .= '</a>';
        }

        $html .= '<div class="metadata">';

        if($this->hasDownloadUri())
        {
            $html .= '<span class="version">Version: '. htmlspecialchars($this->version()) .'</span>';

            if(!isset($this->games))
            {
                $gameString = '<label title="For use with all games">Any</label>';
            }
            else if($this->supportsAllGameModes(AddonRepositoryPlugin::$doomGameModes))
            {
                $gameString = '<label title="For use with any version of DOOM">Any DOOM</label>';
            }
            else if($this->supportsAllGameModes(AddonRepositoryPlugin::$hereticGameModes))
            {
                $gameString = '<label title="For use with any version of Heretic">Any Heretic</label>';
            }
            else if($this->supportsAllGameModes(AddonRepositoryPlugin::$hexenGameModes))
            {
                $gameString = '<label title="For use with any version of Hexen">Any Hexen</label>';
            }
            else
            {
                $gameString = '<label title="For use with these games only">'. htmlspecialchars(implode_keys('|', $this->games)) .'</label>';
            }
            $html .= '<br /><span class="games">Games: '. $gameString .'</span>';
        }
        else
        {
            // We have no interesting meta data for simple links. What to say here?
        }

        $html .= '</div>';

        /*if($this->hasNotes())
        {
            $html .= '<br /><p>'. $this->notes() .'</p>';
        }*/

        return '<div class="addon_badge">'. $html .'</div>';
    }
}
