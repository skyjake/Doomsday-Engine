<?php
include 'common.php';
page_header( "Doomsday Engine Development :: Source Code" );

$pagedir = array( "packages" => "Source Packages vs. CVS",
		  "modules" => "CVS Modules",
		  "checkout" => "Checking Out From CVS" );

page_dir( "Source Code", $pagedir );
begin_section( "packages", $pagedir );
?>

<p>Under construction.</p>

<?php
end_section();
begin_section( "modules", $pagedir );
?>

<p>Under construction.</p>

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
