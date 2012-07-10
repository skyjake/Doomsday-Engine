<?php
/**
 * @file plugins.class.php
 * Plugins collection.
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

includeGuard('Plugins');

require_once(DIR_CLASSES.'/plugin.class.php');

class Plugins implements Iterator, Countable
{
    private $_rootDir;
    private $_plugins;
    private $_position = 0;

    public function __construct($pluginDir)
    {
        // Add a trailing backslash if necessary.
        if(substr($pluginDir, -1, 1) != '/')
            $pluginDir = $pluginDir .'/';

        $this->_rootDir = $pluginDir;
        $this->detect();
    }

    private function detect()
    {
        $plugins = array();
        $handle = opendir($this->_rootDir);

        while($name = readdir($handle))
        {
            if($name != '.' && $name != '..')
            {
                if(is_dir($this->_rootDir.$name))
                {
                    /**
                     * Rudimentary mechanism used to dictate the order in which
                     * plugins are asked to interpret HTTP requests.
                     *
                     * Anything before the first hash character in the plugin
                     * directory name is ignored, therefore due to the alpha-numeric
                     * order in which PHP's readdir returns files, the directory
                     * name can be used to specify the order.
                     */
                    if(strrchr($name, "#") != false)
                        $fileName = substr(strrchr($name, "#"), 1);
                    else
                        $fileName = $name;

                    $file = $this->_rootDir.$name.'/'.$fileName.'.php';

                    if(file_exists($file) && is_readable($file))
                    {
                        include_once($file);

                        $className = $fileName . 'Plugin';

                        $plugin = new $className;

                        $plugins[$name] = array('file' => $file, 'object' => $plugin);
                    }
                }
            }
        }
        closedir($handle);

        ksort($plugins);
        $this->_plugins = $plugins;
    }

    public function load($pluginName)
    {
        if(is_string($pluginName) && strlen($pluginName) > 0)
        foreach($this->_plugins as $plugin => $record)
        {
            if(!strcasecmp($plugin, $pluginName))
            {
                if(!isset($record['object']))
                {
                    require($record['file']);

                    $className = $pluginName . 'Plugin';

                    $record['object'] = new $className;
                }

                return $record['object'];
            }
        }

        throw new Exception(sprintf('Unknown plugin %s.', $pluginName));
    }

    public function interpretRequest($Request)
    {
        foreach($this->_plugins as $plugin => $record)
        {
            if(isset($record['object']) &&
               $record['object'] instanceof RequestInterpreter)
            {
                if($record['object']->InterpretRequest($Request))
                    return $record['object'];
            }
        }

        throw new Exception('Unhandled request');
    }

    /**
     * Lookup a plugin by name. If no plugin is found by the name specified
     * Exception is thrown. Plugin is loaded if it is not already.
     *
     * @param pluginName  (string) Name of the plugin to locate.
     * @return (object) Found plugin object.
     */
    public function find($pluginName)
    {
        return $this->load($pluginName);
    }

    /**
     * Implements Countable.
     */
    public function count()
    {
        return sizeof($this->_plugins);
    }

    /**
     * Implements Iterator
     */
    public function rewind()
    {
        $this->_position = key($this->_plugins);
    }

    public function current()
    {
        return $this->_plugins[$this->_position];
    }

    public function key()
    {
        return $this->_position;
    }

    public function next()
    {
        next($this->_plugins);
        $this->_position = key($this->_plugins);
    }

    public function valid()
    {
        return isset($this->_plugins[$this->_position]);
    }
}
