readme_sources := $(wildcard ../readme/*.ame)
readme_main := ../readme/readme.ame

shell_sources := $(wildcard ../shell-text/*.ame)
shell_main := ../shell-text/shell-text.ame

sv_sources := $(wildcard ../server/*.ame)
sv_main := ../server/server.ame

.PHONY: wiki

all: doomsday.6 doomsday-server.6 doomsday-shell-text.6 Read\ Me.rtf readme.txt wiki

doomsday.6: $(readme_sources)
	amethyst -dMAN -dUNIX -odoomsday.6 $(readme_main)

Read\ Me.rtf: $(readme_sources)
	amethyst -dRTF -dMACOSX -oRead\ Me.rtf $(readme_main)

readme.txt: $(readme_sources)
	amethyst -dTXT -dWIN32 -dCR_NL -oreadme.txt $(readme_main)

doomsday-server.6: $(sv_sources)
	amethyst -dMAN -dUNIX -odoomsday-server.6 $(sv_main)

doomsday-shell-text.6: $(shell_sources)
	amethyst -dMAN -dUNIX -odoomsday-shell-text.6 $(shell_main)

wiki:
	python /Users/jaakko/Dropbox/Scripts/readme_to_wiki.py

clean:
	rm -f doomsday.6 Read\ Me.rtf readme.txt
