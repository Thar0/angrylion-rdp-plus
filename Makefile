TARGET := build/angrylion-plus.dll
INSTALL_DIR := $(PJ64_DIR)/Plugin/GFX

CC := clang.exe
CXX := clang++.exe

OPTFLAGS := -march=x86-64-v4 -msse4.2 -mavx512f -mavx512bw -mavx512vl -O3 -ffast-math -flto
CFLAGS   := -x c   -fno-PIC -std=gnu17 -m32 -target i386-windows-pc -fvisibility=hidden
CXXFLAGS := -x c++ -fno-PIC -std=c++20 -m32 -target i386-windows-pc -fvisibility=hidden -fvisibility-inlines-hidden
WARNFLAGS := -Wall -Wextra
DEFS := -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS
INCLUDES := -Isrc
DEPFLAGS = -MMD -MP -MF $(@:.o=.d)

LDFLAGS := -fuse-ld=lld-link -shared -m32 -target i386-windows-pc -Wl,/machine:x86
LDLIBS := -luser32 -lshlwapi -lopengl32 -lgdi32 -lmsvcrt

ARFLAGS := -fuse-ld=llvm-lib

CORE_DIRS := $(shell find src/core -type d)
CORE_C_FILES   := $(foreach dir, $(CORE_DIRS), $(wildcard $(dir)/*.c))
CORE_CXX_FILES := $(foreach dir, $(CORE_DIRS), $(wildcard $(dir)/*.cpp))
CORE_O_FILES := $(foreach f, $(CORE_C_FILES), build/$(f:.c=.o)) $(foreach f, $(CORE_CXX_FILES), build/$(f:.cpp=.o))
CORE_LIB := build/alcore.lib

OUTPUT_DIRS := $(shell find src/output -type d)
OUTPUT_C_FILES   := $(foreach dir, $(OUTPUT_DIRS), $(wildcard $(dir)/*.c))
OUTPUT_CXX_FILES := $(foreach dir, $(OUTPUT_DIRS), $(wildcard $(dir)/*.cpp))
OUTPUT_O_FILES := $(foreach f, $(OUTPUT_C_FILES), build/$(f:.c=.o)) $(foreach f, $(OUTPUT_CXX_FILES), build/$(f:.cpp=.o))
OUTPUT_LIB := build/aloutput.lib

PJ64_DIRS := $(shell find src/plugin/zilmar -type d)
PJ64_C_FILES   := $(foreach dir, $(PJ64_DIRS), $(wildcard $(dir)/*.c))
PJ64_CXX_FILES := $(foreach dir, $(PJ64_DIRS), $(wildcard $(dir)/*.cpp))
PJ64_O_FILES := $(foreach f, $(PJ64_C_FILES), build/$(f:.c=.o)) $(foreach f, $(PJ64_CXX_FILES), build/$(f:.cpp=.o))
PJ64_LIB := build/plugin-zilmar.lib

DEP_FILES := $(CORE_O_FILES:.o=.d) $(OUTPUT_O_FILES:.o=.d) $(PJ64_O_FILES:.o=.d)

# fixpaths is a dreadful hack for converting windows paths to unix paths in dep files output by clang++.exe
$(shell python3 tools/fixpaths.py build)
$(shell mkdir -p build $(foreach dir, $(CORE_DIRS) $(OUTPUT_DIRS) $(PJ64_DIRS), build/$(dir)))

.PHONY: all clean install

all: $(TARGET)

clean:
	$(RM) -r build

install: all
	cp $(TARGET) $(INSTALL_DIR)

$(TARGET): $(CORE_LIB) $(OUTPUT_LIB) $(PJ64_LIB)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(CORE_LIB): $(CORE_O_FILES)
	$(CXX) $(ARFLAGS) $^ -o $@

$(OUTPUT_LIB): $(OUTPUT_O_FILES)
	$(CXX) $(ARFLAGS) $^ -o $@

$(PJ64_LIB): $(PJ64_O_FILES)
	$(CXX) $(ARFLAGS) $^ -o $@

build/src/%.o: src/%.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $(INCLUDES) $(DEPFLAGS) $(WARNFLAGS) $(DEFS) -c $< -o $@

build/src/%.o: src/%.cpp
	$(CC) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) $(DEPFLAGS) $(WARNFLAGS) $(DEFS) -c $< -o $@

# Dependencies

-include $(DEP_FILES)
