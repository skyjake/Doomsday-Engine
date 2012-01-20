sources := $(wildcard *.ame)

all: doomsday.6 Read\ Me.rtf readme.txt

doomsday.6: $(sources)
	amethyst -dMAN -dUNIX -odoomsday.6 readme.ame

Read\ Me.rtf: $(sources)
	amethyst -dRTF -dMACOSX -oRead\ Me.rtf readme.ame

readme.txt: $(sources)
	amethyst -dTXT -dWIN32 -oreadme.txt readme.ame

clean:
	rm -f doomsday.6 Read\ Me.rtf readme.txt
