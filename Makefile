
RPMBUILD_DIR?=  $(CURDIR)/rpmbuild
SPEC_FILE=	myredispool.spec
DIST_FILES=	src


.PHONY: all
all:
	@echo
	@echo 'make rpm to build RPM package'
	@echo


.PHONY: rpm
rpm:
	rm -rf $(RPMBUILD_DIR)
	mkdir -p $(RPMBUILD_DIR)/BUILD
	cp -ar $(DIST_FILES) $(RPMBUILD_DIR)/BUILD
	rpmbuild -bb -v \
		--define='_topdir $(RPMBUILD_DIR)' \
		$(SPEC_FILE)
	
.PHONY: clean
clean:
	rm -rf $(RPMBUILD_DIR)
