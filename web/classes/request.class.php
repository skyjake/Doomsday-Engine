<?php
/**
 * @file request.class.php
 * Encapsulates an HTTP request with an object interface.
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

includeGuard('Request');

require_once(DIR_CLASSES.'/url.class.php');

class Request
{
    private $_url;
    private $_post;

    public function __construct($url, $postVars=NULL)
    {
        $this->_url = new Url($url);

        if(isset($postVars) && is_array($postVars))
        {
            $this->_post = array();

            foreach($postVars as $key => $val)
            {
                $this->_post[$key] = $val;
            }
        }

        $this->_post = NULL;
    }

    public function &url()
    {
        return $this->_url;
    }

    public function uri($sep='&')
    {
        return (string) $this->_url->toString($sep);
    }

    public function get($arg, $default=NULL)
    {
        $args =& $this->_url->args();
        if(isset($args[$arg]))
            return $args['$arg'];

        return $default;
    }

    public function post($arg, $default=NULL)
    {
        if(isset($this->_post) && isset($this->_post[$arg]))
            return $this->_post[$arg];

        return $default;
    }
}
