<?php
require_once('include/template.inc.php');
$page_title = 'Linux Downloads';
generate_page_header($page_title);
?>
<body>
    <?php 
    include('include/topbar.inc.php'); 
    generate_page_title($page_title);
    ?>
    <div id='content'>
        <div id='page-content'>

<div class="block">
    <article>
        <h1>Stable</h1>
        <?php 
        generate_badges('ubuntu16-x86_64', BT_CANDIDATE);
        generate_badges('fedora23-x86_64', BT_CANDIDATE);
        ?>
    </article>
</div>
<div class="block">
    <article>
        <h1>Unstable</h1>
        <?php 
        generate_badges('ubuntu16-x86_64', BT_UNSTABLE);
        generate_badges('fedora23-x86_64', BT_UNSTABLE);
        ?>
    </article>
</div>
<div class="block">
    <article>
        <h1>Other Builds</h1>
        <p>Binary packages are available for some Linux distributions. You can also <a href="/manual/devel/compile">compile manually from source</a> (requires CMake 3.1 and Qt 5).</p>

        <h2><img src="/images/ubuntu.png" alt="Ubuntu" class="distro-icon">Ubuntu</h2>
        <p><a class="link-external" href="https://launchpad.net/~sjke/+archive/ubuntu/doomsday/">skyjake's PPA</a> has Doomsday builds for a number of versions of Ubuntu. The <a class="link-external" href="http://packages.ubuntu.com/xenial/games/doomsday">Ubuntu repositories</a> should also have stable releases of Doomsday.</p>

        <h2><img src="/images/debian.png" alt="Debian" class="distro-icon">Debian</h2>
        <p>Check Debian's repositories for binary packages.</p>

        <h2><img src="/images/gentoo.png" alt="Gentoo" class="distro-icon">Gentoo</h2>
        <p><a href="https://packages.gentoo.org/packages/games-fps/doomsday" class="link-external">Doomsday Engine package for Gentoo</a></p>

        <h2><img src="/images/opensuse.png" alt="openSUSE" class="distro-icon">openSUSE</h2>
        <p>Binary packages for Doomsday are available in the <a href="https://software.opensuse.org/package/doomsday" class="link-external">openSUSE repositories.</a></p>
    </article>
</div>

        </div>
        <?php generate_sidebar(); ?>
    </div>
    <?php generate_sitemap(); ?>
</body>
