<?php
include 'common.php';
page_header( "Doomsday Engine Development :: Source Code" );

$pagedir = array( "overview" => "Overview",
		  "modules" => "CVS Modules",
		  "checkout" => "Checking Out From CVS" );

page_dir( "Source Code", $pagedir );
begin_section( "overview", $pagedir );
?>

<p>The source code is distributed under the
<a href="LICENSE">GNU General Public License</a>.</p>

<p>For an overview of the current code base, see the
<a href="status.php">Current Status</a> page.</p>

<?php
end_section();
begin_section( "modules", $pagedir );
?>

<table class="tab">

<tr>
<td><tt>distrib</tt>
<td>Scripts (DOS batch files) for creating distribution packages.
Uses <a href="http://www.rarsoft.com/">WinRAR</a> and
<a href="http://www.winzip.com/">WinZip</a> command line tools (wzzip).

<tr>
<td><tt>doomsday</tt>
<td>Visual C++ 6.0 project files and source code for
the engine, jDoom, jHeretic, jHexen, jtNet2 (soon deprecated), OpenGL
and Direct3D renderer, DEH reader plugin and A3D, DirectSound 6 and
OpenAL (not working at the moment) sound interfaces.

<tr>
<td><tt>web</tt>
<td>PHP sources of this website.

</table>

<?php
end_section();
begin_section( "checkout", $pagedir );
?>

<p>See the <a href="http://sourceforge.net/cvs/?group_id=74815">deng CVS
page</a> at SourceForge.</p>

<?php
end_section();
page_footer();
?>
