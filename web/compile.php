<?php
include 'common.php';
page_header( "Doomsday Engine Development :: Compiling" );

$pagedir = array( "compiler" => "Compiler",
		  "deps" => "Library Dependencies" );

page_dir( "Compiling", $pagedir );
begin_section( "compiler", $pagedir );
?>

<p>On the Win32 platform, Microsoft Visual C++ 6.0 is used as the
compiler. The .NET version should work, too.</p>

<?php
end_section();
begin_section( "deps", $pagedir );
?>

<p>The following libraries and SDKs are needed:</p>

<ul>
<li><a href="http://www.microsoft.com/windows/directx/">DirectX</a>
SDK 8.0, or newer.

<li><a href="http://developer.creative.com/">EAX 2.0</a> SDK, for
environmental sound effects such as reverb. EAX 3.0 may work as well
but hasn't been tested.

<li>A3D 3.0 SDK, if you want to build the dsA3D.dll audio driver. <a
href="http://www.google.com/">Google</a> might find it, but with
Aureal gone A3D is on its way out, too.

<li><a href="http://fmod.org/">FMOD</a> SDK. It's used to play the
'fancy', 'newfangled' music files such as MP3 and OGG.
</ul>

<?php
end_section();
page_footer();
?>
