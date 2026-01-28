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

RUNTIME_NAMES = pdp6rt.s ka10rt.s ks10rt.s
RUNTIME_SPLIT_NAMES = \
	pdp6rt-adjbp.s pdp6rt-kdfad.s pdp6rt-kdfsb.s pdp6rt-kdfmp.s pdp6rt-kdfdv.s \
	ka10rt-adjbp.s ka10rt-kdfad.s ka10rt-kdfsb.s ka10rt-kdfmp.s ka10rt-kdfdv.s \
	ks10rt-adjbp.s ks10rt-kdfad.s ks10rt-kdfsb.s ks10rt-kdfmp.s ks10rt-kdfdv.s \
	kccrt-zero.s kccrt-dimode-div.s
RUNTIME = \
	runtime/pdp6rt.s runtime/ka10rt.s runtime/ks10rt.s
RUNTIME_SPLIT = \
	runtime/pdp6rt-adjbp.s runtime/pdp6rt-kdfad.s runtime/pdp6rt-kdfsb.s \
	runtime/pdp6rt-kdfmp.s runtime/pdp6rt-kdfdv.s \
	runtime/ka10rt-adjbp.s runtime/ka10rt-kdfad.s runtime/ka10rt-kdfsb.s \
	runtime/ka10rt-kdfmp.s runtime/ka10rt-kdfdv.s \
	runtime/ks10rt-adjbp.s runtime/ks10rt-kdfad.s runtime/ks10rt-kdfsb.s \
	runtime/ks10rt-kdfmp.s runtime/ks10rt-kdfdv.s \
	runtime/kccrt-zero.s runtime/kccrt-dimode-div.s

all: kcc runtime

test: kcc
	@set -e; \
	./kcc -S tests/oldstyle-proto-compat.c -o tests/oldstyle-proto-compat.s; \
	if ./kcc -S tests/oldstyle-proto-conflict.c -o tests/oldstyle-proto-conflict.s >/dev/null 2>&1; then \
		echo "KCC regression: incompatible old-style prototype accepted" >&2; \
		exit 1; \
	fi; \
	rm -f oldstyle-proto-compat.s oldstyle-proto-conflict.s

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
	@for f in $(RUNTIME_NAMES) $(RUNTIME_SPLIT_NAMES); do \
		$(RM) "$(DESTDIR)$(KCCLIBDIR)/$$f"; \
	done

.SUFFIXES: .c .s
.c.s:
	$(KCC) $(KCC_SELF_FLAGS) -S $<

clean:
	$(RM) $(ASMS) $(OBJS) kcc

distclean: clean
	$(RM) *~ *.rej runtime/*~ runtime/*.rej $(DISTNAME).tar.gz

DISTNAME ?= kcc-source
source-tar: distclean
	git archive --format=tar --prefix=$(DISTNAME)/ HEAD | gzip -9 > $(DISTNAME).tar.gz

.PHONY: all test asm self-asm runtime install install-runtime uninstall clean distclean source-tar
