# Note: This is all BS. I don't actually know make.
CC:=gcc
SRCD:=src
OBJD:=obj
INC:=$(shell find -L $(SRCD) -type d | sed s/^/-I/)
HDRS:=$(shell find -L $(SRCD) -type f -name *.h)
SRCS_C:=$(shell find -L $(SRCD) -type f -name *.c)
SRCS_S:=$(shell find -L $(SRCD) -type f -name *.S)
SRCS:=$(SRCS_C) $(SRCS_S)
OBJS:=$(subst $(SRCD),$(OBJD),$(patsubst %.c,%.o,$(patsubst %.S,%.o,$(SRCS))))
DIRS:=$(shell echo $(dir $(OBJS)) | tr ' ' '\n' | sort -u | tr '\n' ' ')
CFLAGS:=$(INC)\
	-Og \
	-fuse-linker-plugin\
	-flto=jobserver\
	-g\
	-D_GNU_SOURCE\
	-Wall \
	-Wextra \
	-Werror \
	-Wcast-align\
	-Wno-missing-field-initializers \
	-Wno-ignored-qualifiers \
	-Wno-missing-braces \
	-Wno-unused-parameter \
	-Wno-unused-function\
	-Wno-unused-value\
	-Wno-address\
	-fplan9-extensions\
	-Wno-unused-variable\
	-std=gnu11\
	-pthread\
	-fno-omit-frame-pointer\
	-include "dialect.h"\
	-m64\
	-mcx16
LD:=$(CC)
LDFLAGS:=-fvisibility=hidden $(CFLAGS)

all: test ref

test: $(DIRS) $(SRCD)/TAGS $(OBJS) Makefile
		+ $(LD) $(LDFLAGS) -o $@ $(OBJS)

ifndef REF
ref: test
	$(MAKE) ref OBJD:=obj/fake CFLAGS='$(CFLAGS) -DFAKELOCKFREE' REF=1
else
ref: $(DIRS) $(SRCD)/TAGS $(OBJS) Makefile
	+ $(LD) $(LDFLAGS) -o $@ $(OBJS)
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
	rm $(SRCD)/TAGS
	rm -f test;

define \n


endef
info::
	$(foreach v, \
		$(filter-out $(BUILTIN_VARS) BUILTIN_VARS \n, $(.VARIABLES)), \
		$(info $(v) = $($(v)) ${\n}))
