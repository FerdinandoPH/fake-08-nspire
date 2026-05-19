TARGET		:= FAKE08
BUILD		:= build

GCC		= nspire-gcc
GXX		= nspire-g++
LD		= nspire-ld
AS		= nspire-as
GENZEHN		= genzehn

APP_VERSION	?= v0.0.0.0
# Keep the Zehn name alphanumeric only — some genzehn builds silently fail
# (exit 0 with no output file) when given names containing dashes/spaces.
ZEHNFLAGS	:= --name "FAKE08"

ROOT		:= ../..

SOURCES		:= source $(ROOT)/source $(ROOT)/libs/z8lua $(ROOT)/libs/lodepng $(ROOT)/libs/simpleini $(ROOT)/libs/miniz
INCLUDES	:= $(SOURCES)

CFLAGS		:= -Wall -ffunction-sections -fdata-sections -O2 -MMD -MP \
		   -DVER_STR=\"$(APP_VERSION)\" -D_NSPIRE -DMINIZ_NO_TIME \
		   -DSYSCALL_DECLS
# -DSYSCALL_DECLS pre-defines the include guard of <syscall-decls.h>, making
# it a no-op everywhere except in nspire_glue.c (which strips this flag).
# The SDK's Lua stubs in that header have signatures incompatible with z8lua;
# we never call them from C++ anyway.
CFLAGS		+= $(foreach dir,$(INCLUDES),-I$(dir))

CXXFLAGS	:= $(CFLAGS) -fno-rtti -std=gnu++17

LDFLAGS		:= -Wl,--gc-sections -Wl,--defsym,__stack_size=0x80000

CFILES		:= $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:= $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))

OBJS		:= $(CFILES:%.c=$(BUILD)/%.o) $(CPPFILES:%.cpp=$(BUILD)/%.o)
DEPS		:= $(OBJS:.o=.d)

vpath %.c   $(SOURCES)
vpath %.cpp $(SOURCES)

.PHONY: all clean

all: $(TARGET).tns

$(BUILD):
	@mkdir -p $(BUILD)

# Default: compile .c files as C++ (matches what other fake-08 platform
# Makefiles do — z8lua and friends need the C++ frontend for fix32).
$(BUILD)/%.o: %.c | $(BUILD)
	$(GXX) $(CXXFLAGS) -x c++ -c $< -o $@

# Exception: nspire_glue.c is the bridge to the Ndless SDK and MUST be built
# as plain C, since libndls.h/dirent.h pull in <syscall-decls.h> whose Lua
# stubs clash with z8lua under C++. We also strip the z8lua include path so
# the SDK's `#include <lauxlib.h>` resolves to its own (C-only) Lua stubs
# rather than z8lua's C++ headers.
GLUE_CFLAGS	:= -Wall -ffunction-sections -fdata-sections -O2 -MMD -MP \
		   -DVER_STR=\"$(APP_VERSION)\" -D_NSPIRE
$(BUILD)/nspire_glue.o: source/nspire_glue.c | $(BUILD)
	$(GCC) $(GLUE_CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp | $(BUILD)
	$(GXX) $(CXXFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(LD) $(OBJS) -o $@ $(LDFLAGS)

$(TARGET).tns: $(TARGET).elf
	$(GENZEHN) --input $< --output $@.zehn $(ZEHNFLAGS)
	@test -s $@.zehn || { echo "ERROR: $(GENZEHN) exited 0 but produced no $@.zehn"; exit 1; }
	make-prg $@.zehn $@
	rm -f $@.zehn
	cp $@ $(DEPLOYDIR)/$@

clean:
	rm -rf $(BUILD) $(TARGET).tns $(TARGET).elf $(TARGET).tns.zehn

-include $(DEPS)

DEPLOYDIR = /mnt/d/Users/perez/Desktop/Hacks/calc/tns
send: $(TARGET).tns
	./firebird-send.sh -p 3334 $(DEPLOYDIR)/$(TARGET).tns /ndless/