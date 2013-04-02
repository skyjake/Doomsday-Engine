<?php
/**
 * @file version.class.php
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
 * @author Copyright &copy; 2013 Daniel Swanson <danij@dengine.net>
 */

includeGuard('Version');

/**
 * Component values are public for convenience.
 */
class Version
{
    public $major;
    public $minor;
    public $patch;
    public $revision;

    /**
     * Construct a new Version from discrete component values.
     *
     * @param integer:$major     Version number, @em major component.
     * @param integer:$minor     Version number, @em minor component.
     * @param integer:$patch     Version number, @em patch component.
     * @param integer:$revision  Version number, @em revision component.
     */
    public function __construct($major = 0, $minor = 0, $patch = 0, $revision = 0)
    {
        $this->major = (int)$major;
        $this->minor = (int)$minor;
        $this->patch = (int)$patch;
        $this->revision = (int)$revision;
    }

    /**
     * Construct a new Version by parsing the given string.
     *
     * @param string:$string    Textual version string, expected to be in the
     *                          format: (major).(minor).(patch).(revision).
     *                          Components can be omitted if not used.
     */
    static public function fromString($str, $sep = '.')
    {
        $version = new Version();
        $comps = explode((string)$sep, (string)$str);
        $version->major    = isset($comps[0])? (int)$comps[0] : 0;
        $version->minor    = isset($comps[1])? (int)$comps[1] : 0;
        $version->patch    = isset($comps[2])? (int)$comps[2] : 0;
        $version->revision = isset($comps[3])? (int)$comps[3] : 0;
        return $version;
    }

    /**
     * Compare two Version instances, returning a delta describing the
     * logical relationship of @a a to @a b.
     */
    public static function compare(Version &$a, Version &$b)
    {
        if($a->major == $b->major)
        {
            if($a->minor == $b->minor)
            {
                if($a->patch == $b->patch)
                {
                    return $a->revision < $b->revision;
                }
                return $a->patch < $b->patch;
            }
            return $a->minor < $b->minor;
        }
        return $a->major < $b->major;
    }

    /**
     * usort() compatible comparision function (i.e., returns -1 | 0 | +1).
     */
    public static function ucompare($a, $b)
    {
        $result = compare($a, $b);
        if($result == 0) return 0;
        if($result > 0)  return 1;
        return -1;
    }

    /**
     * Returns a textual representation of the version, as follows:
     *
     * (major)(sep)(minor)(sep)(patch)(sep)(revision)
     *
     * @param string:$sep       Separator to delimit the components with.
     * @param bool:$omitTrailingZeros
     */
    public function asText($sep = '.', $omitTrailingZeros = false)
    {
        $sep = (string)$sep;
        $omitTrailingZeros = (bool)$omitTrailingZeros;

        if($omitTrailingZeros === false)
        {
            return "{$this->major}$sep{$this->minor}$sep{$this->patch}$sep{$this->revision}";
        }

        $text = '';
        if($this->revision > 0)
        {
            $text .= "{$this->revision}";
        }

        if($this->patch > 0 || !empty($text))
        {
            if(!empty($text)) $text = $sep . $text;
            $text = $this->patch . $text;
        }

        if($this->minor > 0 || !empty($text))
        {
            if(!empty($text)) $text = $sep . $text;
            $text = $this->minor . $text;
        }

        if(!empty($text)) $text = $sep . $text;
        $text = $this->major . $text;

        return $text;
    }

    /**
     * Assume the caller's intent is to output a string representation
     * in some human-facing context.
     */
    public function __toString()
    {
        return $this->asText('.', true /*omit trailing zeros*/);
    }

    /**
     * Returns an associative array representation of the version, with
     * each property value associated with a key according to name.
     */
    public function toAssocArray()
    {
        return array('major'    => $this->major,
                     'minor'    => $this->minor,
                     'patch'    => $this->patch,
                     'revision' => $this->revision);
    }
}
