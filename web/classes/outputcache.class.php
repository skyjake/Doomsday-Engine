<?php
/**
 * @file outputcache.class.php
 * Abstract class encapsulates PHP's output buffer.
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

includeGuard('OutputCache');

class OutputCache
{
    private $content = NULL;

    public function __construct() {}

    public function start()
    {
        $this->content = 0; // Obliterate content.
        ob_start();
    }

    public function &stop()
    {
        $this->content = ob_get_contents();
        ob_end_clean();

        return $this->content;
    }
}
