<?php
/* Common routines: page header, footer. */

function page_header( $title )
{
  echo "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n";
  echo "<html>\n<head>\n";
  echo '  <meta http-equiv="Content-Type" content="text/html; ' .
    'charset=iso-8859-1">' . "\n";
  echo "  <title>$title</title>\n";
  echo '  <link rel="stylesheet" type="text/css" href="deng.css">' . "\n";
  echo "</head>\n<body>\n";

  echo "<table class=\"toptable\" cellspacing=\"0\">\n";
  echo "<tr><td class=\"titlebar\">\n";

  /* The title bar. */
  echo "<p><a name=\"top\"></a>d<span class=\"separator\">|</span>eng</p>";

  /* The link bar. */
  echo "<tr><td class=\"linkbar\">\n";
  linkbar();

  echo '<tr><td class="shadow">' . "\n";
  
  /* Contents of the page. */
  echo "<tr><td class=\"contentbar\">\n";

  /* Menu on the left. */
  echo "<table cellspacing=\"0\">\n";
  echo "<tr><td class=\"menu\">\n";
  menu();

  /* Actual contents on the right. */
  echo "<td class=\"content\">\n";
}

function linkbar()
{
  echo "<p>";
  echo '<a href="http://www.doomsdayhq.com/">DoomsdayHQ.com</a> | ';
  echo '<a href="http://sourceforge.net/projects/deng/">SF.net: ' .
    'deng Project</a> | ';
  echo '<a href="http://sourceforge.net/">SourceForge.net</a>';
  echo "</p>";
}

function menu()
{
  echo '<a href="index.php">About</a>';
  echo '<a href="source.php">Source Code</a>';
  echo '<a href="compile.php">Compiling</a>';
  echo '<a href="dir.php">Directories</a>';
  echo '<a href="status.php">Current Status (1.7)</a>';
  echo '<a href="goal.php">The Goal (2.0)</a>';

  echo '<a href="http://sourceforge.net">' .
    '<img src="http://sourceforge.net/sflogo.php?' .
    'group_id=74815&amp;type=2" width="125" height="37" border="0" ' .
    'alt="SourceForge.net Logo" /></a>' . "\n";
}

function bottom_linkbar()
{
  echo "<p><span class=\"smaller\">";
  echo 'Site by <a href="http://sourceforge.net/users/skyjake/">skyjake</a>'.
    ' :: 2003 :: Hosted by '.
    '<a href="http://sourceforge.net/">SourceForge.net</a>';
  echo "</span></p>\n";
}

function page_dir( $title, $links )
{
  echo '<table cellspacing="0" class="dir">' . "\n";
  echo "<tr><td><span class=\"topic\">$title:</span>\n";
  reset( $links );
  $isFirst = True;
  while( list($key, $val) = each($links) ) {
    if( !$isFirst ) echo ' <span class="separator">|</span> ';
    echo "<a href=\"#$key\">$val</a>";
    $isFirst = False;
  }
  echo "</table>\n";
}

function begin_section( $name, $links )
{
  echo "<a name=\"$name\"></a><h1>" . $links[$name] . "</h1>\n";
  
}

function end_section()
{
  echo "<p><a href=\"#top\">Back to top</a></p>\n";
}

function page_footer()
{
  /* End of contentbar. */
  echo "<td class=\"balance\">\n";
  echo "</table>\n";

  /* Bottom linkbar. */
  echo "<tr><td class=\"linkbar\">\n";
  bottom_linkbar();
  
  /* End of main table. */
  echo "</table>\n";
  echo "</body>\n</html>\n";
}

?>