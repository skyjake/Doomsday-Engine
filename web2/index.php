<?php
require_once('include/template.inc.php');
generate_page_header();
?>
<body>
    <?php include('include/topbar.inc.php'); ?>
    <div id='content'>
        <div id='welcome'>
            <div class='main-logo'></div>
            <div class='intro'>
                <section>
                    <h1>WELCOME BACK TO THE 1990s</h1>
                    <p>Doomsday Engine is a Doom / Heretic / Hexen port with enhanced graphics</p>
                    <div class="download-button">
                        <?php generate_download_button(); ?>
                    </div>
                </section>
            </div>            
        </div>
        <div id='hero' class='block'>
            <article>
                <video id='hero-video' width="450" height="253" 
                    autoplay loop muted
                    src='http://files.dengine.net/doomsday_demo.mp4'
                    poster='theme/images/demo-thumb.jpg'>
                    <img src='theme/images/demo-thumb.jpg'>
                </video>
                <h1>A rejuvenation.</h1>
                <p><a href="https://en.wikipedia.org/wiki/Doom_(1993_video_game)">id 
                    Software's Doom</a> pioneered the modern first-person shooter genre.
                    Released in 1993, it was a quantum leap in game engine technology
                    with fluid and &mdash; at the time &mdash; incredibly realistic 
                    3D graphics.</p>
                <p>While you can still enjoy the original Doom and 
                    <a href="https://en.wikipedia.org/wiki/First-person_shooter#Rise_in_popularity:_1992.E2.80.931995">its progeny</a>
                    today in an emulator, modern games are held to higher standards of 
                    visual fidelity, usability, and multiplayer features. Doomsday 
                    Engine exists to refresh the technology of these classic games 
                    while retaining the core gameplay experience.
                </p>
            </article>
        </div>
        <div id='features' class='block'>
            <img id='feature-image' src='theme/images/home_ui.jpg'>
            <article>
                <h1>FEATURES</h1>
                <p><span class='heading'>UI</span> Game profiles and add-on selection. Multiplayer server browser. In-game overlay with configuration options and console prompt.</p>
                <p><span class='heading'>Graphics</span> Particle effects and dynamic lights. Bloom and vignette effects. Geometry-based ambient occlusion. Supports 3D models for objects (FBX, MD5) and sky boxes. Upscaling texture filter. Stereoscopic rendering modes.</p>
                <p><span class='heading'>Audio</span> 3D sound effects and reverb (FMOD plugin). Supports music add-ons (MP3) and FluidSynth for high-quality MIDI music (Unix only).</p>
                <p><span class='heading'>Multiplayer</span> Automatic discovery of LAN servers. Master server for public games. Tool for running servers in the background.</p>
                <p><span class='heading'>Technology</span> Portable code built on Qt 5 and OpenGL.</p>
            </article>
            <div style="clear:both;"></div>
        </div>
    </div>
    <?php generate_sitemap(); ?>
</body>
