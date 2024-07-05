# experimental "cpp-pdk" stuff -- humbletim 2024.07.02

SHELL := bash

dummy:
	echo hi

#################################################################
# emscripten -- note: update EMSDK location below
#################################################################
emsdk/README.md:
	git clone https://github.com/emscripten-core/emsdk.git
emsdk/upstream/emscripten/em++: emsdk/README.md
	./emsdk/emsdk install latest
	./emsdk/emsdk activate latest

EMSDK ?= ./emsdk
emsdk_clang = $(EMSDK)/upstream/emscripten/em++
emsdk_flags = -Dusing_emscripten_toolchain
emsdk_flags += "-D__xx_toolchain__=\"$$($(emsdk_clang) --version|head -1)\""
emsdk_flags += -sMALLOC=emmalloc
emsdk_ldflags =
emsdk_ldflags += -sSTANDALONE_WASM=1

emsdk_cxx = $(EMSDK)/upstream/emscripten/em++ $(emsdk_flags) $(emsdk_ldflags)


#################################################################
# vanilla clang/llvm -- note: update clang++ version used below
#################################################################
llvm_clang := clang++
### llvm_flags = -target wasm32-unknown-unknown
llvm_flags = -target wasm32-wasi
llvm_flags += -Dusing_llvm_clang_toolchain
llvm_flags += "-D__xx_toolchain__=\"$$($(llvm_clang) --version|head -1)\""
llvm_ldflags =

llvm_cxx = $(llvm_clang) $(llvm_flags) $(llvm_ldflags)

#################################################################
# wasi-sdk -- note: update WASK_SDK_PATH below
#################################################################
wasi-sdk-22.0/VERSION:
	wget -nv -c https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-22/wasi-sdk-22.0-linux.tar.gz
	tar xf wasi-sdk-22.0-linux.tar.gz

WASI_SDK_PATH ?= wasi-sdk-22.0
wasi_sdk_clang = $(WASI_SDK_PATH)/bin/clang++
wasi_sdk_flags = -Dusing_wasi_sdk_toolchain=1
wasi_sdk_flags += "-D__xx_toolchain__=\"$$($(wasi_sdk_clang) --version|head -1)\""
wasi_sdk_ldflags =
wasi_sdk_cxx = $(wasi_sdk_clang) $(wasi_sdk_flags) $(wasi_sdk_ldflags)

#################################################################
# ... common flags ...
CFLAGS =
#CFLAGS += -Oz
#CFLAGS += -fcolor-diagnostics
CFLAGS += -fno-exceptions            # no try/catch support yet, sorry
LDFLAGS =
LDFLAGS += -nostartfiles             # prevent clang crt startup routines
LDFLAGS += -Wl,--no-entry            # prevent clang _start/main assumptions

#################################################################
#################################################################

extism: 
	wget -nv -c https://github.com/extism/cli/releases/download/v1.5.2/extism-v1.5.2-linux-amd64.tar.gz
	tar xf extism-v1.5.2-linux-amd64.tar.gz extism
extism-sdk = ./extism-sdk
extism-sdk/include/extism.h: extism
	./extism lib install --prefix=extism-sdk
include/extism.h: extism-sdk/include/extism.h
	cp -av $< $@

# for now use a local C++ compatible version of extism c-pdk header
include/extism-pdk.h:
	cp -av wip/extism-pdk.h $@
#	wget -nv -c https://github.com/extism/c-pdk/archive/refs/tags/v1.0.1.zip
#	unzip -q -d include -j v1.0.1.zip c-pdk-1.0.1/extism-pdk.h


include/glm/glm.hpp:
	mkdir -p include || true
	wget -nv -c https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip
	unzip -q -d include glm-1.0.1-light.zip

#################################################################
DEPS =

# COMMON
DEPS += include/glm/glm.hpp

# PDK
DEPS += include/extism.h
DEPS += include/extism-pdk.h

# TOOLCHAINS
DEPS += emsdk/upstream/emscripten/em++
DEPS += wasi-sdk-22.0/VERSION
DEPS += emsdk/README.md

fetch-linux-sdks: $(DEPS)
	@$(wasi_sdk_cxx) --version | head -1
	@$(emsdk_cxx) --version | head -1
	@$(llvm_cxx) --version | head -1
	ls -l $^

CFLAGS += -DXX_USE_LIBCPP_ONLY  # use WASI polyfills but no runtime dependencies
CFLAGS += -O3
CFLAGS += -Iinclude -Iwip

LDFLAGS += -Wl,--export-if-defined=hello

wasm-dis = $(EMSDK)/upstream/bin/wasm-dis
dis-filtered = grep -E '[(](export|import)' | grep -v extism:

emscripten-plugin.wasm: plugin.cpp
	cd $(EMSDK) && . ./emsdk_env.sh
	$(emsdk_cxx) $(CFLAGS) $(LDFLAGS) $< -o $@ $(EXPORTS) 2>&1
	ls -l $@
	$(wasm-dis) $@ | $(dis-filtered)

wasi-sdk-plugin.wasm: plugin.cpp
	$(wasi_sdk_cxx) $(CFLAGS) $(LDFLAGS) $< -o $@ $(EXPORTS) 2>&1
	ls -l $@
	$(wasm-dis) $@ | $(dis-filtered)

llvm-clang-plugin.wasm: plugin.cpp
	$(llvm_cxx) $(CFLAGS) $(LDFLAGS) $< -o $@ $(EXPORTS) 2>&1
	ls -l $@
	$(wasm-dis) $@ | $(dis-filtered)

ALL =
ALL += wasi-sdk-plugin.wasm
ALL += emscripten-plugin.wasm
# ALL += llvm-clang-plugin.wasm # broken on stock clang/gha ubuntu runners

all: $(ALL)
	ls -l *.wasm

test: $(ALL)
	for x in $^ ; do echo "--------"; ls -l $$x ; \
	  ./extism call $$x hello --config foo=bar --config user=make --input 0123456789ab0123456789ab0123456 --log-level info ;\
         echo "" ; done

test-wasi: $(ALL)
	for x in $^ ; do echo "--------"; ls -l $$x ; ./extism call --wasi $$x hello --input 0123456789ab0123456789ab0123456 --log-level info ; echo "" ; done

report: $(ALL)
	@for x in $^ ; do \
	echo "### $$(du -h $$x)" ; \
	echo '```' ; $(EMSDK)/upstream/bin/wasm-dis $$x | $(dis-filtered) ; echo '```' ; \
        echo '```' ; ./extism call $$x hello --config foo=bar --config user=make --input 0123456789ab0123456789ab0123456 --log-level info | grep -E 'TOOLCHAIN|error|glm_vec3_test' 2>&1 | c++filt -t ; echo '```' ; \
	echo '' ;  \
	done

