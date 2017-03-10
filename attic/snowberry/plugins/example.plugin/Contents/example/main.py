# Example plugin.

import ui

def init():
    print "Example init called!"

    area = ui.getArea(ui.COMMAND)
    #area.createButton('quit')


print "Module init in example.plugin"
