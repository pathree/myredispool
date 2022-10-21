
RPMBUILD_TOPDIR:=  $(CURDIR)/rpmbuild
SPEC_FILE:=	myredispool.spec
DIST_FILES:=	Makefile src
VERSION:=	0.0.1
RELEASE:=	$(shell git rev-list HEAD --count)

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
	mkdir -p $(RPMBUILD_TOPDIR)/BUILD
	cp -ar $(DIST_FILES) $(RPMBUILD_TOPDIR)/BUILD
	rpmbuild -bb -v \
		--define='_topdir $(RPMBUILD_TOPDIR)' \
		--define='_version $(VERSION)' \
		--define='_release $(RELEASE)' \
		$(SPEC_FILE)
	
.PHONY: clean
clean:
	rm -rf $(RPMBUILD_TOPDIR)
