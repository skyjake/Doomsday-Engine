#!/bin/sh
. indent-typenames.sh

indent	--k-and-r-style \
	-ts 4 \
	--blank-lines-after-declarations \
	--blank-lines-after-procedures \
	--blank-lines-before-block-comments \
	--no-blank-lines-after-commas \
	--break-after-boolean-operator \
	--braces-on-struct-decl-line \
	--braces-after-if-line \
	--brace-indent0 \
	--comment-indentation33 \
	--declaration-comment-column33 \
	--format-first-column-comments \
	--comment-line-length79 \
	--declaration-indentation8 \
	--struct-brace-indentation0 \
	-ppi 2 \
	--indent-level4 \
	--no-tabs \
	--line-length79 \
	--continue-at-parentheses \
	--cuddle-do-while \
	--dont-cuddle-else \
	--space-after-cast \
	--no-space-after-parentheses \
	--no-space-after-if \
	--no-space-after-while \
	--no-space-after-for \
	--ignore-newlines \
	--swallow-optional-blank-lines \
	$TYPENAMES \
	$*
