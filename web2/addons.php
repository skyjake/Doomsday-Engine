<?php
require_once('include/template.inc.php');
$page_title = 'Add-ons';
generate_page_header($page_title);

function generate_badge($info)
{
    cache_echo('<p><div class="package-badge">'
        ."<a class='package-dl' href='$info[download_url]' "
        ."title=\"Download $info[full_title]\"><div class='package-name'>$info[title]</div><div class='package-metadata'>"
        .$info['metadata']
        ."</div></a></div></p>\n");
    cache_echo("<div class='package-description'><p class='package-description'>".$info['description'].'</p>');
    if ($info['notes']) {
        cache_echo("<p class='package-notes'>".$info['notes'].'</p>');
    }
    cache_echo("</div>\n");
}

function is_featured($addon)
{
    return $addon['featured'] == 'true';
}

function addon_cmp($a, $b)
{
    if (is_featured($a) && !is_featured($b)) {
        return -1;
    }
    if (!is_featured($a) && is_featured($b)) {
        return 1;
    }
    return strcmp($a->title, $b->title);
}

function generate_game_addons($addons, $game_family)
{
    $found = [];
    foreach ($addons->addon as $addon) {
        // Looks for a matching family.
        foreach ($addon->games as $game) {
            if (starts_with($game_family, $game->game)) {
                array_push($found, $addon);
                break;
            }
        }
    }
    usort($found, "addon_cmp");
    foreach ($found as $addon) {
        $meta = $addon->version;
        if (stristr($addon->downloadUri, '.zip')) {
            if ($meta) $meta .= ' &middot; ';
            $meta .= 'ZIP';
        }
        else if (stristr($addon->downloadUri, '.pk3')) {
            if ($meta) $meta .= ' &middot; ';
            $meta .= 'PK3';
        }
        if (strstr($addon->downloadUri, '.torrent')) {
            if ($meta) $meta .= ' &middot; ';
            $meta .= 'Torrent';
        }
        $description = $addon->description;
        $notes = '';
        if (property_exists($addon, 'notes')) {
            $notes = $addon->notes;
        }
        $info = [
            'download_url' => $addon->downloadUri,
            'title'        => $addon->title,
            'full_title'   => $addon->title,
            'metadata'     => $meta,
            'description'  => $description,
            'notes'        => $notes,
        ];
        generate_badge($info);
    }        
}

function generate_addon_blocks()
{
    $ckey = cache_key('home', 'addons');    
    if (!cache_try_load($ckey)) {        
        // Load the XML file describing the packages.
        $addons = simplexml_load_file(__DIR__.'/data/addons.xml');        
        // Each game family gets a block.
        cache_echo("<div class='block'><article><h1>Doom</h1>");
        generate_game_addons($addons, 'doom');
        cache_echo("</article></div>\n");
        cache_echo("<div class='block'><article><h1>Heretic</h1>");
        generate_game_addons($addons, 'heretic');
        cache_echo("</article></div>\n");    
        cache_echo("<div class='block'><article><h1>Hexen</h1>");
        generate_game_addons($addons, 'hexen');
        cache_echo("</article></div>\n");
        cache_store($ckey);
    }
    cache_dump();
}

?>
<body>
    <?php 
    include('include/topbar.inc.php'); 
    generate_page_title($page_title);
    ?>
    <div id='content'>
        <div id='page-content'>
            <div class='block'>
                <article id="overview">
                    <h1>Add-ons</h1>
                    <p>This is a collection of featured resource packs and other add-ons.
                        You can find more on the <a href="/talk">discussions forums</a>.
                    </p>
                </article>
            </div>
            <?php generate_addon_blocks(); ?>
        </div>
        <?php generate_sidebar(); ?>
    </div>
    <?php generate_sitemap(); ?>
</body>
