# Example plugin.

import ui

def init():
    print "Example init called!"

    area = ui.getArea(ui.Area.COMMAND)
    #area.createButton('quit')


print "Module init in example.plugin"
