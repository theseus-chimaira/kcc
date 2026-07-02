CC ?= gcc
KCC ?= ./kcc
KCC_SELF_FLAGS ?= -P=stdc+kcc -DHOST_UNIX=1 -Iself/include/ -Hself/include/

CFLAGS += -std=c99 -funsigned-char
LDFLAGS ?=
INSTALL ?= install
RM ?= rm -f

PREFIX ?= /usr/local
PDP10_PREFIX ?= $(PREFIX)
BINDIR ?= $(PDP10_PREFIX)/bin
KCCLIBDIR ?= $(PDP10_PREFIX)/lib/kcc
RUNTIMEDIR = runtime

SRCS = \
	cc.c ccasmb.c cccreg.c cccse.c cccode.c ccdata.c ccdbug.c ccdecl.c \
	ccerr.c cceval.c ccgen.c ccgen1.c ccgen2.c ccgswi.c ccjskp.c cclex.c \
	ccnode.c ccout.c ccpp.c ccreg.c ccstmt.c ccsym.c cctype.c ccopt.c
OBJS = $(SRCS:.c=.o)
ASMS = $(SRCS:.c=.s)

RUNTIME = \
	$(RUNTIMEDIR)/pdp6rt-adjbp.s $(RUNTIMEDIR)/pdp6rt-kdfad.s \
	$(RUNTIMEDIR)/pdp6rt-kdfsb.s $(RUNTIMEDIR)/pdp6rt-kdfmp.s \
	$(RUNTIMEDIR)/pdp6rt-kdfdv.s $(RUNTIMEDIR)/ka10rt-adjbp.s \
	$(RUNTIMEDIR)/ka10rt-kdfad.s $(RUNTIMEDIR)/ka10rt-kdfsb.s \
	$(RUNTIMEDIR)/ka10rt-kdfmp.s $(RUNTIMEDIR)/ka10rt-kdfdv.s \
	$(RUNTIMEDIR)/ks10rt-adjbp.s $(RUNTIMEDIR)/ks10rt-kdfad.s \
	$(RUNTIMEDIR)/ks10rt-kdfsb.s $(RUNTIMEDIR)/ks10rt-kdfmp.s \
	$(RUNTIMEDIR)/ks10rt-kdfdv.s $(RUNTIMEDIR)/kccrt-zero.s \
	$(RUNTIMEDIR)/kccrt-dimode-div.s

all: kcc runtime

.DELETE_ON_ERROR:

kcc: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

ccgen.o ccgen1.o ccgen2.o: cc.h ccgen.h

asm self-asm: $(ASMS)

runtime: $(RUNTIME)

install:
	$(MAKE) clean
	$(MAKE) all
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 kcc $(DESTDIR)$(BINDIR)/kcc
	cmp kcc $(DESTDIR)$(BINDIR)/kcc
	$(MAKE) install-runtime

install-runtime: runtime
	$(INSTALL) -d $(DESTDIR)$(KCCLIBDIR)
	$(INSTALL) -m 644 $(RUNTIME) $(DESTDIR)$(KCCLIBDIR)/

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/kcc
	@for f in $(RUNTIME); do \
		$(RM) "$(DESTDIR)$(KCCLIBDIR)/$${f##*/}"; \
	done

.SUFFIXES: .c .s
.c.s:
	$(KCC) $(KCC_SELF_FLAGS) -S $<

clean:
	$(RM) $(ASMS) $(OBJS) kcc

.PHONY: all asm self-asm runtime install install-runtime uninstall clean
