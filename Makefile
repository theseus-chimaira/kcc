CC ?= gcc
KCC ?= ./kcc
KCC_SELF_FLAGS ?= -P=stdc+kcc -DHOST_UNIX=1 -Iself/include/ -Hself/include/

CFLAGS += -std=c99 -funsigned-char \
	-Wall -Wextra -Wpedantic -Wstrict-prototypes -Wold-style-definition \
	-Werror
LDFLAGS ?=
INSTALL ?= install
RM ?= rm -f

PREFIX ?= /usr/local
ifdef PDP10_PREFIX
override PREFIX := $(PDP10_PREFIX)
endif
BINDIR ?= $(PREFIX)/bin
KCCLIBDIR ?= $(PREFIX)/lib/kcc
RUNTIMEDIR := runtime

SRCS := \
	cc.c ccasmb.c cccreg.c cccse.c cccode.c ccdata.c ccdbug.c ccdecl.c \
	ccerr.c cceval.c ccgen.c ccgen1.c ccgen2.c ccgswi.c ccjskp.c cclex.c \
	ccnode.c ccout.c ccpp.c ccreg.c ccstmt.c ccsym.c cctype.c ccopt.c
OBJS := $(SRCS:.c=.o)
ASMS := $(SRCS:.c=.s)

RUNTIME_NAMES := pdp6rt.s ka10rt.s ks10rt.s
RUNTIME_SPLIT_NAMES := \
	pdp6rt-adjbp.s pdp6rt-kdfad.s pdp6rt-kdfsb.s pdp6rt-kdfmp.s pdp6rt-kdfdv.s \
	ka10rt-adjbp.s ka10rt-kdfad.s ka10rt-kdfsb.s ka10rt-kdfmp.s ka10rt-kdfdv.s \
	ks10rt-adjbp.s ks10rt-kdfad.s ks10rt-kdfsb.s ks10rt-kdfmp.s ks10rt-kdfdv.s \
	kccrt-zero.s kccrt-dimode-div.s
RUNTIME := $(addprefix $(RUNTIMEDIR)/,$(RUNTIME_NAMES))
RUNTIME_SPLIT := $(addprefix $(RUNTIMEDIR)/,$(RUNTIME_SPLIT_NAMES))

all: kcc runtime

.DELETE_ON_ERROR:

kcc: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

ccgen.o ccgen1.o ccgen2.o: cc.h ccgen.h

asm self-asm: $(ASMS)

runtime: $(RUNTIME) $(RUNTIME_SPLIT)

install: kcc install-runtime
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 kcc $(DESTDIR)$(BINDIR)/kcc

install-runtime: runtime
	$(INSTALL) -d $(DESTDIR)$(KCCLIBDIR)
	$(INSTALL) -m 644 $(RUNTIME) $(RUNTIME_SPLIT) $(DESTDIR)$(KCCLIBDIR)/

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/kcc
	$(RM) $(addprefix $(DESTDIR)$(KCCLIBDIR)/,$(RUNTIME_NAMES) $(RUNTIME_SPLIT_NAMES))

%.s: %.c kcc
	$(KCC) $(KCC_SELF_FLAGS) -S $<

clean:
	$(RM) $(ASMS) $(OBJS) kcc

distclean: clean
	$(RM) *~ *.rej runtime/*~ runtime/*.rej $(DISTNAME).tar.gz

DISTNAME ?= kcc-source
source-tar: distclean
	git archive --format=tar --prefix=$(DISTNAME)/ HEAD | gzip -9 > $(DISTNAME).tar.gz

.PHONY: all asm self-asm runtime install install-runtime uninstall clean distclean source-tar
