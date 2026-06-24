## Top-level convenience Makefile.
##
## The Haiku makefile-engine builds one binary per Makefile, so each binary has
## its own Makefile under src/. This recurses into them.

SUBDIRS = src/server src/cli src/gui

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
