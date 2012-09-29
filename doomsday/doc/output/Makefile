sources := $(wildcard ../*.ame)
main_source := ../readme.ame

.PHONY: wiki

all: doomsday.6 Read\ Me.rtf readme.txt wiki

doomsday.6: $(sources)
	amethyst -dMAN -dUNIX -odoomsday.6 $(main_source)

Read\ Me.rtf: $(sources)
	amethyst -dRTF -dMACOSX -oRead\ Me.rtf $(main_source)

readme.txt: $(sources)
	amethyst -dTXT -dWIN32 -dCR_NL -oreadme.txt $(main_source)

wiki:
	python /Users/jaakko/Dropbox/Scripts/readme_to_wiki.py

clean:
	rm -f doomsday.6 Read\ Me.rtf readme.txt
