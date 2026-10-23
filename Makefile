CXX ?= c++
CXXFLAGS ?= -O2 -g

crono_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
root_dir := $(abspath $(crono_dir)/../..)
llvm_root ?= $(root_dir)/dependencies/install/llvm21
clang := $(llvm_root)/bin/clang
wllvm ?= $(root_dir)/dependencies/install/wllvm/bin/wllvm
build_dir ?= build
ir_dir ?= $(build_dir)/ir

.DEFAULT_GOAL := all

# Keep required flags separate: scripts/build.sh supplies CXXFLAGS for capture.
compile_flags := -std=c++11 -Wall -pthread
bench_src ?= $(wildcard $(crono_dir)/apps/*/*.cc)
bench_names := $(notdir $(bench_src:.cc=))
bench_programs := $(addprefix $(build_dir)/,$(bench_names))
bench_objects := $(addsuffix .o,$(bench_programs))
ir_programs := $(addsuffix .ir,$(bench_names))

vpath %.cc $(sort $(dir $(bench_src)))

.PHONY: all benchmark ir test clean $(bench_names) $(ir_programs)

all: benchmark

benchmark: $(bench_programs)

$(bench_names): %: $(build_dir)/%

$(bench_objects): $(build_dir)/%.o: %.cc $(MAKEFILE_LIST)
	mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(compile_flags) \
	    -MMD -MP -c $< -o $@

$(bench_programs): %: %.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -pthread $(LDLIBS) -lrt -o $@

ir: $(ir_programs)

$(ir_programs): %.ir:
	"$(root_dir)/scripts/build.sh" --make-dir . \
	    --make-target "$*" --executable "wllvm/$*" --name "$*" \
	    --clang "$(clang)" --wllvm "$(wllvm)" --output-dir "$(ir_dir)"

test: benchmark
	python3 "$(crono_dir)/tests/test_runtime.py" "$(abspath $(build_dir))" $(bench_names)

clean:
	rm -rf "$(build_dir)"

-include $(addsuffix .d,$(bench_programs))
