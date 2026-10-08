/*
 * Native DAIMOS KCC phase driver.
 *
 * Keep orchestration out of the compiler phases: on a PDP-6 every resident
 * word matters, and KGEN is the high-water process.  This driver therefore
 * creates bounded temporary files, RUNs one phase at a time, waits for it,
 * and removes intermediates before returning.
 */
#include <stdio.h>
#include <string.h>

#include "u.h"

#define KDRV_REC_WORDS      18U
#define KDRV_ARG_MAX        SYS_RUN_ARG_MAX
#define KDRV_BLOCK_WORDS \
    (SYS_RUN_V2_FIXED_WORDS + KDRV_REC_WORDS + \
     KDRV_ARG_MAX * KDRV_REC_WORDS + 2U)

static char kcpp_path[]   = "/OPTION/BASE/LIBEXEC/KCC/KCPP";
static char kparse_path[] = "/OPTION/BASE/LIBEXEC/KCC/KPARSE";

static int
pack_record(kword_t *dst, unsigned int *used, char *text)
{
    kword_t rec[KDRV_REC_WORDS];
    unsigned int words;
    unsigned int i;

    if (u_s6_pack(rec, KDRV_REC_WORDS, text) != 0)
        return -1;
    words = 1U + ((unsigned int)rec[0] + 5U) / 6U;
    if (*used > KDRV_BLOCK_WORDS - words)
        return -1;
    for (i = 0U; i < words; ++i)
        dst[(*used)++] = rec[i];
    return 0;
}

static int
run_child(char *path, char **argv, unsigned int argc)
{
    kword_t block[KDRV_BLOCK_WORDS];
    struct sys_run_v2 *run;
    kword_t status;
    unsigned int used;
    unsigned int i;
    int pid;

    if (argc == 0U || argc > KDRV_ARG_MAX)
        return 126;
    used = SYS_RUN_V2_FIXED_WORDS;
    if (pack_record(block, &used, path) != 0)
        return 126;
    for (i = 0U; i < argc; ++i)
        if (pack_record(block, &used, argv[i]) != 0)
            return 126;

    /* Preserve stderr explicitly.  RUN inherits ordinary descriptors, but an
     * explicit map keeps compiler diagnostics stable if that policy changes. */
    block[used++] = SYS_RUN_FD_MAP(2U, 2U);
    run = (struct sys_run_v2 *)block;
    run->version_words = SYS_RUN_HEADER(SYS_RUN_VERSION_2, used);
    run->flags = SYS_RUN_PGRP_INHERIT;
    run->pgrp = 0UL;
    run->fdmap_count = 1UL;
    run->argc = (kword_t)argc;
    run->envc = 0UL;

    pid = dsys_run(run);
    if (pid < 0)
        return 126;
    status = 0UL;
    if (dsys_wait((unsigned int)pid, &status, 0U) != pid ||
        SYS_WAIT_STATUS_KIND(status) != SYS_WAIT_EXITED)
        return 126;
    return (int)SYS_WAIT_STATUS_VALUE(status);
}

static int
exec_child(char *path, char **argv, unsigned int argc)
{
    kword_t block[KDRV_BLOCK_WORDS];
    struct sys_exec_v1 *exec;
    unsigned int used;
    unsigned int i;

    if (argc == 0U || argc > KDRV_ARG_MAX)
        return 126;
    used = SYS_EXEC_V1_FIXED_WORDS;
    if (pack_record(block, &used, path) != 0)
        return 126;
    for (i = 0U; i < argc; ++i)
        if (pack_record(block, &used, argv[i]) != 0)
            return 126;
    exec = (struct sys_exec_v1 *)block;
    exec->version_words = SYS_RUN_HEADER(SYS_EXEC_VERSION_1, used);
    exec->argc = (kword_t)argc;
    exec->envc = 0UL;
    i = (unsigned int)dsys_exec(exec);
    return 126;
}

static void
append_octal(char *dst, unsigned int *pos, unsigned int value)
{
    unsigned int shift;
    unsigned int digit;

    for (shift = 15U; ; shift -= 3U) {
        digit = (value >> shift) & 07U;
        dst[(*pos)++] = (char)('0' + digit);
        if (shift == 0U)
            break;
    }
}

static void
temp_name(char *dst, char *suffix)
{
    unsigned int pos;
    unsigned int i;

    /* Compiler IR is cold serialized data, often several thousand words.
     * Keep it on the secondary D6FS drum scratch set instead of charging
     * scarce physical core to MEMFS or churning the root filesystem.  Each
     * consumed phase file is unlinked by the continuation chain. */
    strcpy(dst, "/SCRATCH/KCC");
    pos = (unsigned int)strlen(dst);
    append_octal(dst, &pos, (unsigned int)dsys_getpid() & 077777U);
    for (i = 0U; suffix[i] != 0; ++i)
        dst[pos++] = suffix[i];
    dst[pos] = 0;
}

static int
phase_arg(char *dst, unsigned int cap, int option, char *path)
{
    unsigned int n;

    n = (unsigned int)strlen(path);
    if (cap < n + 4U)
        return -1;
    dst[0] = '-';
    dst[1] = (char)option;
    dst[2] = '=';
    strcpy(dst + 3, path);
    return 0;
}

static void
remove_file(char *name)
{
    kword_t path[U_PATH_WORDS];

    if (u_s6_pack(path, U_PATH_WORDS, name) == 0)
        (void)dsys_unlink(path);
}

static int
compile_one(char *source, char *output, int optimize,
    char **cpp_options, unsigned int cpp_option_count)
{
    char kpt[40];
    char kir[40];
    char kp1[40];
    char kptarg[44];
    char kirarg[44];
    char kp1chain[SYS_RUN_ARG_MAX_CHARS + 1U];
    char outchain[SYS_RUN_ARG_MAX_CHARS + 1U];
    char *av[KDRV_ARG_MAX];
    unsigned int i;
    int rc;

    temp_name(kpt, ".KPT");
    temp_name(kir, ".KIR");
    temp_name(kp1, ".KP1");
    remove_file(kpt);
    remove_file(kir);
    remove_file(kp1);
    if (phase_arg(kptarg, sizeof(kptarg), 'R', kpt) != 0 ||
        phase_arg(kirarg, sizeof(kirarg), 'R', kir) != 0 ||
        phase_arg(kp1chain, sizeof(kp1chain), 'X', kp1) != 0 ||
        phase_arg(outchain, sizeof(outchain), 'Y', output) != 0)
        return 126;

    av[0] = "KCPP";
    av[1] = kptarg;
    if (cpp_option_count > KDRV_ARG_MAX - 3U)
        return 126;
    for (i = 0U; i < cpp_option_count; ++i)
        av[i + 2U] = cpp_options[i];
    av[2U + cpp_option_count] = source;
    rc = run_child(kcpp_path, av, 3U + cpp_option_count);
    if (rc != 0)
        goto done;

    av[0] = "KPARSE";
    av[1] = kirarg;
    av[2] = kp1chain;
    av[3] = outchain;
    if (optimize) {
        av[4] = kpt;
        rc = exec_child(kparse_path, av, 5U);
    } else {
        av[4] = "-Z";
        av[5] = kpt;
        rc = exec_child(kparse_path, av, 6U);
    }
    /* Successful EXEC never returns.  KPARSE chains through KGEN/KOPT. */

done:
    remove_file(kpt);
    remove_file(kir);
    remove_file(kp1);
    return rc;
}

int
main(int argc, char **argv)
{
    char *source;
    char *output;
    char *cpp_options[KDRV_ARG_MAX - 3U];
    unsigned int cpp_option_count;
    int optimize;
    int i;

    source = 0;
    output = 0;
    optimize = 1;
    cpp_option_count = 0U;
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-N") || !strcmp(argv[i], "-n")) {
            optimize = 0;
        } else if (!strcmp(argv[i], "-S")) {
            /* Compile-to-assembly is the native bootstrap's first complete
             * contract.  It is also the path used to rebuild KCC itself. */
        } else if ((!strcmp(argv[i], "-o") || !strcmp(argv[i], "-O") ||
            !strcmp(argv[i], "-R")) &&
            i + 1 < argc) {
            output = argv[++i];
        } else if (!strncmp(argv[i], "-D", 2) ||
            !strncmp(argv[i], "-I", 2) ||
            !strncmp(argv[i], "-H", 2) ||
            !strncmp(argv[i], "-P", 2) ||
            !strncmp(argv[i], "-x", 2) ||
            !strncmp(argv[i], "-m", 2)) {
            if (cpp_option_count >= KDRV_ARG_MAX - 3U) {
                fprintf(stderr, "kcc: too many preprocessor options\n");
                return 2;
            }
            cpp_options[cpp_option_count++] = argv[i];
        } else if (source == 0) {
            source = argv[i];
        } else {
            fprintf(stderr, "usage: kcc [-N] -S [-o output.s] source.c\n");
            return 2;
        }
    }
    if (source == 0) {
        fprintf(stderr, "usage: kcc [-N] -S [-o output.s] source.c\n");
        return 2;
    }
    if (output == 0)
        output = "/TEMP/KCCOUT.S";
    return compile_one(source, output, optimize,
        cpp_options, cpp_option_count);
}
