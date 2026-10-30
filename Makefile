# Atari BBS client: build with the system cc65 tools.
# Override these settings here, in the environment, or on the make command line.
BUILD_MAKEFILE := $(lastword $(MAKEFILE_LIST))
CL65 ?= cl65
# Prefer cc65's own data-directory discovery. Fall back to common shared-data
# locations for packages whose tools cannot discover their installed data.
# Set CC65_HOME explicitly for a nonstandard installation (include/, lib/, etc.).
ifeq ($(origin CC65_HOME),undefined)
DETECTED_CC65_HOME := $(shell $(CL65) --print-target-path 2>/dev/null)
ifeq ($(strip $(DETECTED_CC65_HOME)),)
DETECTED_CC65_HOME := $(patsubst %/include/atari.h,%,$(firstword $(wildcard /usr/share/cc65/include/atari.h /usr/local/share/cc65/include/atari.h)))
endif
ifneq ($(strip $(DETECTED_CC65_HOME)),)
CC65_HOME := $(DETECTED_CC65_HOME)
endif
endif
ifneq ($(strip $(CC65_HOME)),)
export CC65_HOME
endif
FUJINET_LIB_VERSION ?= 5.0.0
FNLIB_CACHE ?= _cache/fujinet-lib
# Only the default managed cache is downloaded into; local overrides are read-only.
ifeq ($(origin FNLIB),undefined)
ifeq ($(origin FNLIB_LIB),undefined)
FNLIB_AUTO_DOWNLOAD := 1
endif
endif
FNLIB ?= $(FNLIB_CACHE)/$(FUJINET_LIB_VERSION)-atari
FNLIB_LIB ?= $(FNLIB)/fujinet-atari-$(FUJINET_LIB_VERSION).lib
FNLIB_URL := https://github.com/FujiNetWIFI/fujinet-lib/releases/download/v$(FUJINET_LIB_VERSION)/fujinet-lib-atari-$(FUJINET_LIB_VERSION).zip
BUILD_DIR ?= build/atari
TARGET ?= $(BUILD_DIR)/ataribbs.xex

CPPFLAGS ?=
CFLAGS ?= -O
ASFLAGS ?=
LDFLAGS ?=
LDLIBS ?=

C_SOURCES := $(wildcard src/*.c)
ASM_SOURCES := $(wildcard src/*.s)
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES)) \
           $(patsubst src/%.s,$(BUILD_DIR)/%.o,$(ASM_SOURCES))
DEPS := $(OBJECTS:.o=.d)
FNLIB_HEADERS := $(FNLIB)/fujinet-network.h $(FNLIB)/fujinet-fuji.h
HEADERS := $(wildcard src/*.h) $(wildcard $(FNLIB)/*.h) $(FNLIB_HEADERS)
FNLIB_CONFIG := $(BUILD_DIR)/.fnlib-config

.PHONY: all clean check-fnlib FORCE
.DELETE_ON_ERROR:
all: $(TARGET)

check-fnlib:
	@if [ ! -f "$(FNLIB_LIB)" ] || [ ! -f "$(FNLIB)/fujinet-network.h" ] || [ ! -f "$(FNLIB)/fujinet-fuji.h" ]; then \
	    if [ "$(FNLIB_AUTO_DOWNLOAD)" = 1 ]; then \
	        set -eu; \
	        mkdir -p "$(FNLIB_CACHE)"; \
	        tmpdir=$$(mktemp -d "$(FNLIB_CACHE)/.download-XXXXXX"); \
	        trap 'rm -rf "$$tmpdir"' 0; \
	        echo "Downloading Atari FujiNet-lib $(FUJINET_LIB_VERSION)"; \
	        curl --fail --location --retry 2 --connect-timeout 15 --max-time 120 \
	            -o "$$tmpdir/release.zip" "$(FNLIB_URL)"; \
	        unzip -q "$$tmpdir/release.zip" -d "$$tmpdir/unpacked"; \
	        test -f "$$tmpdir/unpacked/fujinet-atari-$(FUJINET_LIB_VERSION).lib"; \
	        test -f "$$tmpdir/unpacked/fujinet-network.h"; \
	        test -f "$$tmpdir/unpacked/fujinet-fuji.h"; \
	        mkdir -p "$(FNLIB)"; \
	        cp -R "$$tmpdir/unpacked/." "$(FNLIB)/"; \
	    fi; \
	fi
	@test -f "$(FNLIB)/fujinet-network.h" -a -f "$(FNLIB)/fujinet-fuji.h" || \
	    { echo "Set FNLIB to the directory containing the Atari FujiNet-lib headers." >&2; exit 1; }
	@test -n "$(FNLIB_LIB)" -a -f "$(FNLIB_LIB)" || \
	    { echo "Set FNLIB_LIB to the Atari FujiNet .lib file (normally inside FNLIB)." >&2; exit 1; }

$(BUILD_DIR):
	mkdir -p "$@"

# Explicit targets let a fresh checkout resolve prerequisites before downloading.
$(FNLIB_LIB) $(FNLIB_HEADERS): | check-fnlib
	@test -f "$@"

# Switching cached versions or local paths must rebuild even older libraries.
FORCE:
$(FNLIB_CONFIG): FORCE | $(BUILD_DIR)
	@printf '%s\n' "$(FUJINET_LIB_VERSION)" "$(abspath $(FNLIB))" "$(abspath $(FNLIB_LIB))" > "$@.tmp"
	@if ! cmp -s "$@.tmp" "$@"; then mv "$@.tmp" "$@"; else $(RM) "$@.tmp"; fi

$(BUILD_DIR)/%.o: src/%.c $(HEADERS) $(FNLIB_LIB) $(FNLIB_CONFIG) $(BUILD_MAKEFILE) | $(BUILD_DIR) check-fnlib
	$(CL65) -t atari --cpu 6502 $(CPPFLAGS) -I "$(FNLIB)" $(CFLAGS) \
	    --create-dep $(@:.o=.d) -c -o $@ $<

$(BUILD_DIR)/%.o: src/%.s $(BUILD_MAKEFILE) | $(BUILD_DIR)
	$(CL65) -t atari --cpu 6502 $(ASFLAGS) \
	    --create-dep $(@:.o=.d) -c -o $@ $<

$(TARGET): $(OBJECTS) src/atari.cfg $(FNLIB_LIB) $(BUILD_MAKEFILE) | check-fnlib
	mkdir -p "$(dir $@)"
	$(CL65) -t atari -C src/atari.cfg $(LDFLAGS) \
	    -m $(BUILD_DIR)/ataribbs.map -Ln $(BUILD_DIR)/ataribbs.lbl \
	    -o $@ $(OBJECTS) "$(FNLIB_LIB)" $(LDLIBS)

clean:
	$(RM) $(OBJECTS) $(DEPS) $(TARGET) $(BUILD_DIR)/ataribbs.map $(BUILD_DIR)/ataribbs.lbl $(FNLIB_CONFIG) $(FNLIB_CONFIG).tmp

-include $(DEPS)
