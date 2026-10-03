ROOTDIR := $(shell pwd)
SRCDIR := $(ROOTDIR)/source
PROJECTS := $(SRCDIR)/projects
PKG_NAME = softkut
MAX_VERSIONS := 8 9

# print a section header from a recipe: $(call section,"text")
section = @echo "==> "$(1)

.PHONY: all build test clean setup update-submodules link maxhelp

all: build


build:
	@mkdir -p build && cd build && \
		cmake .. && \
		cmake --build . --config Release

test: build
	@cd build && ctest --output-on-failure

# regenerate help/softkut~.maxhelp (py2max, via uv)
maxhelp:
	@uv run python scripts/make_help.py

clean:
	@rm -rf externals build

setup: update-submodules link
	$(call section,"setup complete")

update-submodules:
	$(call section,"updating git submodules")
	@git submodule init && git submodule update

link:
	$(call section,"symlink to Max 'Packages' Directories")
	@for MAX_VERSION in $(MAX_VERSIONS); do \
		MAX_DIR="Max $${MAX_VERSION}" ; \
		PACKAGES="$(HOME)/Documents/$${MAX_DIR}/Packages" ; \
		PY_PACKAGE="$${PACKAGES}/$(PKG_NAME)" ; \
		if [ -d "$${PACKAGES}" ]; then \
			echo "symlinking to $${PY_PACKAGE}" ; \
			if ! [ -L "$${PY_PACKAGE}" ]; then \
				ln -s "$(ROOTDIR)" "$${PY_PACKAGE}" ; \
				echo "... symlink created" ; \
			else \
				echo "... symlink already exists" ; \
			fi \
		fi \
	done