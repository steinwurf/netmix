CXXFLAGS := -std=c++11 -g -Wall -O2 -ftree-vectorize -Wno-unused-local-typedefs -Wno-unknown-warning-option ${CXXFLAGS}

KODO_DIR  ?= ../kodo/src/
SAK_DIR   ?= ../kodo/bundle_dependencies/sak-602ce9/master/src/
FIFI_DIR  ?= ../kodo/bundle_dependencies/fifi-6ca972/master/src/
GAUGE_DIR ?= ../kodo/bundle_dependencies/gauge-d53326/master/src/
BOOST_DIR ?= ../kodo/bundle_dependencies/boost-e92f30/master/

INCLUDES = -Isrc/ -I $(KODO_DIR) -I $(SAK_DIR) -I $(FIFI_DIR) -I $(GAUGE_DIR) -I $(BOOST_DIR)

BUILD = build
BIN = $(BUILD)/$(shell $(CXX) -dumpmachine)
CACHE = $(BUILD)/$(shell $(CXX) -dumpmachine)/.cache
EXMPL = examples
TARGETS := rlnc_helper rlnc_recoder rlnc_dencoder \
           plain_entry plain_relay plain_client
EXAMPLES := tcp_client tcp_server udp_client udp_server tcping rlnc_multipath \
            rlnc_singlepath

V = 0
CXX_0 = @echo "$(CXX) $< -o $@"; $(CXX)
CXX_1 := $(CXX)
C = $(CXX_$(V))

all: $(TARGETS) $(EXAMPLES)

.PHONY: clean distclean

-include $(CACHE)/*.P

$(BIN)/%: src/%.cpp | $(BIN) $(CACHE)
	$(C) -MD -MP $(CXXFLAGS) $(LDFLAGS) $(INCLUDES) $< -o $@
	@mv $(BIN)/$*.d $(CACHE)/$*.P

$(BIN)/$(EXMPL)/%: $(EXMPL)/%.cpp | $(BIN)/$(EXMPL) $(CACHE)
	$(C) -MD -MP $(CXXFLAGS) $(LDFLAGS) $(INCLUDES) $< -o $@
	@mv $(BIN)/$(EXMPL)/$*.d $(CACHE)/$*.P

$(TARGETS): %: $(BIN)/%

$(EXAMPLES): %: $(BIN)/$(EXMPL)/%

$(BIN):
	@mkdir -p $(BIN)

$(BIN)/$(EXMPL):
	@mkdir -p $(BIN)/$(EXMPL)

$(CACHE):
	@mkdir -p $(CACHE)

clean:
	rm -rf $(BIN)

distclean:
	rm -rf $(BUILD)
