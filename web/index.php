<?php
include 'common.php';
page_header('Doomsday Engine Development :: About');

$pagedir = array( "what" => "What Is This?",
		  "join" => "Getting Involved" );

page_dir( "About", $pagedir );
begin_section( "what", $pagedir );
?>

<p>This is the <strong>Doomsday Engine Development</strong>
website. The site provides information and instructions for Doomsday
Engine developers.
</p>

<p><center>
  <a href="http://www.doomsdayhq.com/">
  <img border="0" src="images/mainshot.jpg" />
  </a>
</center></p>

<p>The <a href="http://www.doomsdayhq.com/">Doomsday Engine</a> is a
Doom source port for the Windows platform. It is based on the source
code of <a href="http://www.idsoftware.com/">id Software</a>'s Doom
and <a href="http://www.ravensoft.com/">Raven Software</a>'s Heretic
and Hexen. The Doomsday Engine and the associated ports of Doom,
Heretic and Hexen have been under development since 1999. The first
versions were released in late 1999 and early 2000. Most of the
development work has been done by Jaakko Keränen (<a
href="http://sourceforge.net/users/skyjake/">skyjake</a>).
</p>

<p>The purpose of the project is to create versions of Doom, Heretic
and Hexen that feel the same as the original games but are implemented
using modern techniques such as 3D graphics and client/server
networking. A lot of emphasis is placed on good-looking graphics.
</p>

<p>The engine uses a modular structure to separate game logic from the
renderer, sound, network and other such subsystems. This allows the
engine to be used with different game logic modules. The currently
available ones are called jDoom, jHeretic and jHexen, and are ports of
Doom, Heretic and Hexen, respectively. A secondary goal of the project
is to make it possible to write new game logic modules with minimal
effort, either completely from scratch or based on the existing
modules.</p>

<p>The Doomsday Engine has been written in C and C++. It uses <a
href="http://www.opengl.org/">OpenGL</a> and <a
href="http://www.microsoft.com/directx/">Direct3D</a> to render
graphics with 3D objects, particle effects and dynamic lights. It
supports DirectSound3D and A3D for 3D sound effects and environmental
effects, and <a href="http://fmod.org/">FMOD</a> for playing MP3/OGG
music. It uses DirectPlay for client/server networking, where players
may join games already in progress. Microsoft DirectX is also used for
controller input.
</p>

<?php
end_section();
begin_section( "join", $pagedir );
?>

<p>If you would like to join the project as a developer, become a <a
href="http://sourceforge.net/">SourceForge.net</a> user and contact
the <a href="http://sourceforge.net/projects/deng/">deng project</a>
manager.
</p>

<p>Patches and other contributions are also welcome.
</p>

<?php
end_section();
page_footer();
?>
