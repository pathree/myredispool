
RPMBUILD_TOPDIR:=  $(CURDIR)/rpmbuild
SPEC_FILE:=	myredispool.spec
DIST_FILES:=	Makefile src

TAG:=		$(shell git describe --tags --long 2>/dev/null)
VERSION:=	$(shell echo $(TAG) | sed -e 's/\(.*\)-\(.*\)-\(.*\)/\1/' -e 's/^v//' -e 's/[-]/./g' -e 's/.rc/~rc/')
RELEASE:=	$(shell echo $(TAG) | sed -e 's/\(.*\)-\(.*\)-\(.*\)/\2/g')
COMMIT:=	$(shell echo $(TAG) | sed -e 's/\(.*\)-\(.*\)-\(.*\)/\3/g')

.PHONY: all
all: build

.PHONY: build
build:
	cd src && $(MAKE)

.PHONY: install
install:
	cd src && $(MAKE) install

.PHONY: rpm
rpm:
	rm -rf $(RPMBUILD_TOPDIR)
	install  -d $(RPMBUILD_TOPDIR)/BUILD
	cp -ar $(DIST_FILES) $(RPMBUILD_TOPDIR)/BUILD
	rpmbuild -bb -v \
		--define='_topdir $(RPMBUILD_TOPDIR)' \
		--define='_version $(VERSION)' \
		--define='_release $(RELEASE)' \
		--define='_commit $(COMMIT)' \
		$(SPEC_FILE)
	
.PHONY: clean
clean:
	rm -rf $(RPMBUILD_TOPDIR)
