<?php
/**
 * @file url.class.php
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

includeGuard('Url');

class Url
{
    private $scheme = NULL;
    private $host = NULL;
    private $user = NULL;
    private $pass = NULL;
    private $path = NULL;
    private $fragment = NULL;
    private $args = NULL;

    public function __construct($str=NULL)
    {
        if(isset($str) && is_array($str))
        {
            foreach($str as $key => $value)
            {
                if(!strcasecmp('scheme', $key))
                    $this->scheme = strtolower($value);
                else if(!strcasecmp('host', $key))
                    $this->host = strtolower($value);
                else if(!strcasecmp('user', $key))
                    $this->user = $value;
                else if(!strcasecmp('pass', $key))
                    $this->pass = $value;
                else if(!strcasecmp('path', $key))
                    $this->path = $value;
                else if(!strcasecmp('fragment', $key))
                    $this->fragment = $value;
                else if(!strcasecmp('args', $key))
                    $this->args = $value;
            }

            return;
        }

        if(isset($str))
            $this->parse($str);
    }

    private function parse($str)
    {
        $u = parse_url(trim($str));

        if(isset($u['scheme']))
            $this->scheme = $u['scheme'];
        if(isset($u['host']))
            $this->host = $u['host'];
        if(isset($u['user']))
            $this->user = $u['user'];
        if(isset($u['pass']))
            $this->pass = $u['pass'];
        if(isset($u['path']))
            $this->path = $u['path'];
        if(isset($u['fragment']))
            $this->fragment = $u['fragment'];

        $a = array();

        if(isset($u['query']))
        {
            $args = explode('&', $u['query']);

            foreach($args as $arg)
            {
                if(trim($arg) == '') continue;

                $pair = explode('=', $arg);
                $key = urldecode($pair[0]);
                if(isset($pair[1]))
                {
                    $a[$key] = urldecode($pair[1]);
                }
                else
                {
                    $a[$key] = (boolean)TRUE;
                }
            }
        }

        $this->args = $a;
    }

    public function setScheme($scheme='')
    {
        if(!strcasecmp(gettype($scheme), 'string') && strlen($scheme))
        {
            $this->scheme = $scheme;
        }
        else
        {
            unset($this->scheme);
        }
        return $this;
    }

    public function &scheme()
    {
        return $this->scheme;
    }

    public function setHost($host='')
    {
        if(!strcasecmp(gettype($host), 'string') && strlen($host))
        {
            $this->host = $host;
        }
        else
        {
            unset($this->host);
        }
        return $this;
    }

    public function &host()
    {
        return $this->host;
    }

    public function &path()
    {
        return $this->path;
    }

    public function &args()
    {
        return $this->args;
    }

    /**
     * Convert back to a string:
     *
     * [scheme]://[user]:[pass]@[host]/[path]?[query]#[fragment]
     */
    public function toString($sep='&amp;', $encode=true)
    {
        $str = '';
        if(isset($this->scheme))
        {
            $str .= $this->scheme . '://';

            if(isset($this->user))
            {
                $str .= $this->user;
                if(isset($this->pass))
                    $str .= ':' . $this->pass;

                $str .= '@';
            }

            if(isset($this->host))
                $str .= $this->host;
        }

        if(isset($this->path))
        {
            $str .= $this->path;

            if(isset($this->args) && count($this->args))
            {
                if($encode)
                {
                    $str .= '?' . http_build_query($this->args, '', $sep);
                }
                else
                {
                    $str .= '?';

                    $n = (int) 0;
                    foreach($this->args as $key => $value)
                    {
                        if($n != 0)
                            $str .= "&$key=$value";
                        else
                            $str .= "$key=$value";

                        $n++;
                    }
                }
            }

            if(isset($this->fragment))
            {
                if($encode)
                    $str .= '#' . urlencode($this->fragment);
                else
                    $str .= '#' . $this->fragment;
            }
        }

        return $str;
    }
}
