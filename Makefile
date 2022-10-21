
RPMBUILD_TOPDIR:=  $(CURDIR)/rpmbuild
SPEC_FILE:=	myredispool.spec
DIST_FILES:=	src
VERSION:=	0.0.1
RELEASE:=	$(shell git rev-list HEAD --count)

.PHONY: all
all:
	@echo
	@echo 'make rpm to build RPM package'
	@echo


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
