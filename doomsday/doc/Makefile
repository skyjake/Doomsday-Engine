sources := $(wildcard *.ame)

doomsday.6: $(sources)
	amethyst -dMAN -dUNIX -odoomsday.6 readme.ame

Readme.rtf: $(sources)
	amethyst -dRTF -dMACOSX -oReadme.rtf readme.ame

readme.txt: $(sources)
	amethyst -dTXT -dWIN32 -oreadme.txt readme.ame
