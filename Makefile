CC:=clang
SRCD:=src
OBJD:=obj
INC:=$(shell find -L $(SRCD) -not -path "*/.*" -type d | sed s/^/-I/)
HDRS:=$(shell find -L $(SRCD) -type f -name "*.h")
SRCS_C:=$(shell find -L $(SRCD) -type f -name "*.c")
SRCS_S:=$(shell find -L $(SRCD) -type f -name "*.S")
SRCS:=$(SRCS_C) $(SRCS_S)
OBJS:=$(subst $(SRCD),$(OBJD),$(patsubst %.c,%.o,$(patsubst %.S,%.o,$(SRCS))))
DIRS:=$(shell echo $(dir $(OBJS)) | tr ' ' '\n' | sort -u | tr '\n' ' ')

override CFLAGS+=$(INC)\
	-O0 \
	-g\
	-fms-extensions\
	-std=gnu11\
	-pthread\
	-include "src/linux_dialect/dialect/dialect.h"\
	-D_GNU_SOURCE\
	-Wall \
	-Wextra \
	-Werror \
	-Wcast-align\
	-Wno-missing-field-initializers \
	-Wno-missing-braces \
	-Wno-unused-parameter \
	-Wno-unused-function\
	-Wno-unused-value\
	-march=native\
	-mtune=native\
	-mcx16

ifeq ($(CC),gcc)
override CFLAGS+=\
	-flto=jobserver\
	-fuse-linker-plugin\
	-fno-fat-lto-objects\
	-ftrack-macro-expansion=0\
	-Wdesignated-init\
	-Wno-misleading-indentation\

else
override CFLAGS+=\
	-flto\
	-Wno-microsoft-anon-tag\
	-Wno-unknown-pragmas\
	-Wno-cast-align\

endif


LD:=$(CC)
LDFLAGS:= $(CFLAGS)

all: test ref

test: $(DIRS) $(SRCD)/TAGS $(OBJS) Makefile
		$(LD) $(LDFLAGS) -o $@ $(OBJS)

ifndef REF
ref: test
	$(MAKE) ref OBJD:=obj/fake CFLAGS='-DFAKELOCKFREE' REF=1
else
ref: $(DIRS) $(SRCD)/TAGS $(OBJS) Makefile
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
endif

$(DIRS):
	mkdir -p $@

$(SRCD)/TAGS: $(SRCS) $(HDRS)
	etags -o $(SRCD)/TAGS $(HDRS) $(SRCS)

$(OBJS): Makefile

$(OBJD)/%.o: $(SRCD)/%.c
		$(CC) $(CFLAGS) -MM -MP -MT $(OBJD)/$*.o -o $(OBJD)/$*.dep $<
		$(CC) $(CFLAGS) -o $@ -c $<;

$(OBJD)/%.o: $(SRCD)/%.S
		$(CC) $(CFLAGS) -MM -MP -MT $(OBJD)/$*.o -o $(OBJD)/$*.dep $<
		$(CC) $(CFLAGS) -o $@ -c $<

-include $(OBJS:.o=.dep)

check-syntax:
		$(CC) $(CFLAGS) -c $(CHK_SOURCES) -o /dev/null

clean:
	rm -rf $(OBJD) 
	rm -f $(SRCD)/TAGS
	rm -f test;
	rm -f ref;

define \n


endef
info::
	$(foreach v, \
		$(filter-out $(BUILTIN_VARS) BUILTIN_VARS \n, $(.VARIABLES)), \
		$(info $(v) = $($(v)) ${\n}))
