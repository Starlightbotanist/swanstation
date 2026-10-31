PYTHON ?= python3
HARNESS_CACHE ?=
ifeq ($(origin TOOLS),undefined)
TOOLS := $(shell "$(PYTHON)" fetch_harness.py $(if $(HARNESS_CACHE),--cache "$(HARNESS_CACHE)") --path)
harness:
	"$(PYTHON)" fetch_harness.py $(if $(HARNESS_CACHE),--cache "$(HARNESS_CACHE)") $(if $(filter 1,$(OFFLINE)),--offline)
else
harness:
	@test -f "$(TOOLS)/snapshot_smoke.c" -a -f "$(TOOLS)/run_profiles.py" || { echo 'TOOLS must name a snapshot harness directory'; exit 1; }
endif

.PHONY: harness

.DEFAULT_GOAL := check
CXX ?= c++
OUTPUT ?= build/results
HOST_BUILD := $(CURDIR)/build/host
ifeq ($(OS),Windows_NT)
EXE = .exe
LIB = .dll
else
LIB = .so
endif

fixture: harness
	$(MAKE) -B -C "$(TOOLS)" BUILD="$(HOST_BUILD)" all
	"$(HOST_BUILD)/snapshot_smoke$(EXE)" --generate-psx "$(CURDIR)/build"

check: fixture
	"$(PYTHON)" "$(TOOLS)/run_profiles.py" --host "$(HOST_BUILD)/snapshot_smoke$(EXE)" --core "$(CORE)" --content "$(CURDIR)/build/snapshot-history.exe" --suite "$(CURDIR)/suite.json" --output "$(abspath $(OUTPUT))" $(if $(SYSTEM),--system "$(SYSTEM)")

check-gpu: fixture
	$(MAKE) -B -C "$(TOOLS)" BUILD="$(HOST_BUILD)" gpu
	"$(PYTHON)" "$(TOOLS)/run_gpu_profiles.py" --frontend "$(FRONTEND)" --proxy "$(HOST_BUILD)/snapshot_proxy$(LIB)" --core "$(CORE)" --content "$(CURDIR)/build/snapshot-history.exe" --suite "$(CURDIR)/gpu-suite.json" --output "$(abspath $(OUTPUT))" $(if $(SYSTEM),--system "$(SYSTEM)")

.PHONY: fixture check check-gpu

build/pgxp_test$(EXE): pgxp_test.cpp ../../src/core/pgxp.cpp ../../src/core/pgxp.h
	mkdir -p build
	$(CXX) $(CXXFLAGS) -std=c++17 -O2 -ffunction-sections -fdata-sections -I../../src -I../../dep/libretro-common/include $< -Wl,--gc-sections -o $@

check-pgxp: build/pgxp_test$(EXE)
	./build/pgxp_test$(EXE)

.PHONY: check-pgxp
