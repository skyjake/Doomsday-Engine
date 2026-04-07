readme_sources := $(wildcard ../readme/*.ame)
readme_main := ../readme/readme.ame

shell_sources := $(wildcard ../shell-text/*.ame)
shell_main := ../shell-text/shell-text.ame

sv_sources := $(wildcard ../server/*.ame)
sv_main := ../server/server.ame

.PHONY: wiki

all: wiki

wiki:
	python /Users/jaakko/Dropbox/Scripts/readme_to_wiki.py
