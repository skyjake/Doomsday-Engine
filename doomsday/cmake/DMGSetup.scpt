on run argv
  set image_name to item 1 of argv

  tell application "Finder"
  tell disk image_name

    -- open the image the first time and save a DS_Store with just
    -- background and icon setup
    open
      set current view of container window to icon view
      set theViewOptions to the icon view options of container window
      set background picture of theViewOptions to file ".background:background.jpg"
      set arrangement of theViewOptions to not arranged
      set icon size of theViewOptions to 128
      delay 1
    close

    -- next setup the position of the app and Applications symlink
    -- plus hide all the window decoration
    open
      update without registering applications
      tell container window
        set sidebar width to 0
        set statusbar visible to false
        set toolbar visible to false
        set the bounds to { 250, 100, 1010, 540 }
        set position of item "Doomsday.app" to { 130, 200 }
        set position of item "Doomsday Shell.app" to { 300, 200 }
        set position of item "Applications" to { 470, 200 }
        set position of item "Read Me.rtf" to { 640, 200 }
        set position of item "include" to { 215, 500 }
        set position of item "lib" to { 385, 500 }
        set position of item "share" to { 555, 500 }
      end tell
      update without registering applications
      delay 1
    close

    -- one last open and close so you can see everything looks correct
    open
      delay 5
    close

  end tell
  delay 1
end tell
end run