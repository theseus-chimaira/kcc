/* CCKGEN.C - compact native KIR1 -> KPCODE1 driver. */
#include "cckir.h"
#include <stdlib.h>
#include <string.h>

extern void syminit(void), nodeinit(void), gencode(NODE *), outinit(void),
    outdone(int);

static void
init_pdp6_target(int optimize)
{
    tgcpu = TGCPU_PDP6;
    tgarch = TGARCH_PDP6;
    asmdialect = ASM_GAS;
    tgits = 0;
    tgmachuse.dmovx = 0;
    tgmachuse.adjsp = 0;
    tgmachuse.adjbp = 0;
    tgmachuse.fltr = 0;
    tgmachuse.fpimm = 0;
    tgmachuse.mapdbl = -1;
    optpar = optgen = optobj = optimize;
    delete = assemble = link = 0;
    longidents = 1;
    r_minnopreserve = R_MIN_NOPRESERVE;
    r_maxnopreserve = R_MAX_NOPRESERVE;
    r_preserve = XF4_call_spill = _reg_count = 0;
    outmsgs = stderr;
    nerrors = nwarns = err_waiting = 0;
}

int
main(int argc, char **argv)
{
    char *inname, *outname;
    NODE *root;
    int kind;
    int optimize;
    int i;

    inname = outname = NULL;
    optimize = 1;
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-n")) {
            optimize = 0;
        } else if (!strcmp(argv[i], "-N")) {
            optimize = 0;
        } else if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            outname = argv[++i];
        } else if (!strncmp(argv[i], "-R=", 3)) {
            outname = argv[i] + 3;
        } else if (inname == NULL) {
            inname = argv[i];
        } else {
            fprintf(stderr, "usage: kgen [-N] input.kir -R=output.kp1\n");
            return 2;
        }
    }
    if (inname == NULL || outname == NULL) {
        fprintf(stderr, "usage: kgen [-N] input.kir -R=output.kp1\n");
        return 2;
    }
    init_pdp6_target(optimize);
    if (strlen(inname) >= FNAMESIZE) {
        fprintf(stderr, "kgen: input filename too long\n");
        return 1;
    }
    strcpy(inpfname, inname);
    in = fopen(inname, "rb");
    if (in == NULL) {
        fprintf(stderr, "kgen: cannot open %s\n", inname);
        return 1;
    }
    out = fopen(outname, "wb");
    if (out == NULL) {
        fclose(in);
        fprintf(stderr, "kgen: cannot create %s\n", outname);
        return 1;
    }
    syminit();
    if (kir_read_header(in) != 0) {
        fprintf(stderr, "kgen: invalid KIR1 input\n");
        fclose(in);
        fclose(out);
        return 1;
    }
    outinit();
    for (;;) {
        if (kir_read_next(in, &kind, &root) != 0) {
            fprintf(stderr, "kgen: truncated KIR1 input\n");
            break;
        }
        if (kind == KIR_REC_EXTDEF) {
            nodeinit();
            gencode(root);
            kir_free_graph(root);
            continue;
        }
        if (kind == KIR_REC_MODULE_END) {
            outdone(kir_read_mainflag());
            fclose(in);
            fclose(out);
            kir_free_module();
            return nerrors == 0 ? 0 : 1;
        }
        fprintf(stderr, "kgen: unexpected KIR1 record\n");
        break;
    }
    fclose(in);
    fclose(out);
    kir_free_module();
    return 1;
}
