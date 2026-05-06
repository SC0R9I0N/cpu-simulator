# Variables
SRC_DIR := src
OBJ_DIR := obj
EXE := sim
SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Compiler Settings
CC := g++
OPENXLSX_DIR := OpenXLSX
OPENXLSX_BUILD_DIR := $(OPENXLSX_DIR)/build
OPENXLSX_LIB := $(OPENXLSX_BUILD_DIR)/libOpenXLSX.a
OPENXLSX_EXPORT_HDR := $(OPENXLSX_BUILD_DIR)/OpenXLSX-Exports.hpp
CFLAGS := -fsanitize=address -g -c -Wall -Wextra -std=c++17 -Iinclude/ `pkg-config --cflags glib-2.0` -I$(OPENXLSX_DIR) -I$(OPENXLSX_DIR)/headers -I$(OPENXLSX_BUILD_DIR)
LIBS := -fsanitize=address `pkg-config --libs glib-2.0`

CONFS = $(wildcard configs/*.cfg)
PROGRAMS = $(wildcard programs/*.dat)
OUTPUTS := $(foreach conf,$(CONFS),$(foreach program, $(PROGRAMS), outputs/$(program:programs/%.dat=%).$(conf:configs/%.cfg=%).out))
OUTPUTS_XLSX := $(foreach conf,$(CONFS),$(foreach program, $(PROGRAMS), outputs_xlsx/$(program:programs/%.dat=%).$(conf:configs/%.cfg=%).xlsx))
DIFFS := $(foreach conf,$(CONFS),$(foreach program, $(PROGRAMS), diffs/$(program:programs/%.dat=%).$(conf:configs/%.cfg=%).diff))

.PHONY: all clean xlsx outputs_xlsx

all: $(EXE)

$(EXE): $(OBJ) $(OPENXLSX_LIB)
	$(CC) $^ $(LIBS) -o $@

$(OPENXLSX_LIB):
	cmake -S $(OPENXLSX_DIR) -B $(OPENXLSX_BUILD_DIR) -DOPENXLSX_LIBRARY_TYPE=STATIC
	cmake --build $(OPENXLSX_BUILD_DIR) --target OpenXLSX

$(OPENXLSX_EXPORT_HDR): $(OPENXLSX_LIB)
	@true

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(OPENXLSX_EXPORT_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(OBJ:.o=.d)

run: $(OUTPUTS) $(DIFFS)
xlsx: $(OUTPUTS_XLSX)

define run_rules
outputs/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).out: $(EXE) $(1) $(2)
	@echo "Running ./$(EXE) -p $(1) -c $(2) -j outputs/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).json -d > $$@"
	-@./$(EXE) -p $(1) -c $(2) -j outputs/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).json -d > $$@ 2>&1
endef
$(foreach program,$(PROGRAMS),$(foreach conf, $(CONFS), $(eval $(call run_rules,$(program),$(conf)))))

define xlsx_rules
outputs_xlsx/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).xlsx: $(EXE) $(1) $(2) | outputs_xlsx
	@echo "Running ./$(EXE) -p $(1) -c $(2) -x outputs_xlsx/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).xlsx"
	-@./$(EXE) -p $(1) -c $(2) -x outputs_xlsx/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).xlsx 2>&1
endef
$(foreach program,$(PROGRAMS),$(foreach conf, $(CONFS), $(eval $(call xlsx_rules,$(program),$(conf)))))

define diff_rules
diffs/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).diff: outputs/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).out
	@echo "Running diff -dw $$< outputs_solution/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).out > $$@"
	-@diff -dw $$< outputs_solution/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).out > $$@
endef
$(foreach program,$(PROGRAMS),$(foreach conf, $(CONFS), $(eval $(call diff_rules,$(program),$(conf)))))

clean:
	@$(RM) -rf $(OBJ_DIR)/* $(EXE) outputs/* outputs_xlsx/* diffs/*
