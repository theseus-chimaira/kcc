/*
 * Low-memory native KCC phase chaining for DAIMOS.
 *
 * RUN requires parent and child images to coexist. KGEN is large enough that
 * this is not reliable on a 96K PDP-6, while EXEC has a destructive low-memory
 * retry which replaces the current image. Keep launch-block construction in
 * this tiny object and link it only into KGEN.  The small public driver
 * reuses its existing launch-block packer for the first destructive EXEC.
 */
#include <stdio.h>
#include <string.h>
#include "u.h"

#define KCHAIN_REC_WORDS 18U
#define KCHAIN_ARG_MAX    8U
#define KCHAIN_BLOCK_WORDS \
    (SYS_EXEC_V1_FIXED_WORDS + KCHAIN_REC_WORDS + \
     KCHAIN_ARG_MAX * KCHAIN_REC_WORDS)

static int
pack_record(kword_t *dst, unsigned int *used, char *text)
{
        kword_t rec[KCHAIN_REC_WORDS];
        unsigned int words;
        unsigned int i;

        if (u_s6_pack(rec, KCHAIN_REC_WORDS, text) != 0)
                return -1;
        words = 1U + ((unsigned int)rec[0] + 5U) / 6U;
        if (*used > KCHAIN_BLOCK_WORDS - words)
                return -1;
        for (i = 0U; i < words; ++i)
                dst[(*used)++] = rec[i];
        return 0;
}

int
daimos_exec_image(char *path, char **argv, unsigned int argc)
{
        kword_t block[KCHAIN_BLOCK_WORDS];
        struct sys_exec_v1 *exec;
        unsigned int used;
        unsigned int i;

        if (argc == 0U || argc > KCHAIN_ARG_MAX)
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

int
daimos_exec_kgen(char *input, char *kp1, char *output, int optimize)
{
        static char kgen_path[] = "/OPTION/BASE/LIBEXEC/KCC/KGEN";
        char kp1arg[SYS_RUN_ARG_MAX_CHARS + 1U];
        char outarg[SYS_RUN_ARG_MAX_CHARS + 1U];
        char *av[5];
        unsigned int n;
        unsigned int argc;

        n = (unsigned int)strlen(kp1);
        if (n + 4U > sizeof(kp1arg))
                return 126;
        kp1arg[0] = '-';
        kp1arg[1] = 'R';
        kp1arg[2] = '=';
        strcpy(kp1arg + 3, kp1);

        n = (unsigned int)strlen(output);
        if (n + 4U > sizeof(outarg))
                return 126;
        outarg[0] = '-';
        outarg[1] = 'X';
        outarg[2] = '=';
        strcpy(outarg + 3, output);

        av[0] = "KGEN";
        argc = 4U;
        if (optimize) {
                av[1] = input;
                av[2] = kp1arg;
                av[3] = outarg;
        } else {
                av[1] = "-n";
                av[2] = input;
                av[3] = kp1arg;
                av[4] = outarg;
                argc = 5U;
        }
        return daimos_exec_image(kgen_path, av, argc);
}

int
daimos_exec_kopt(char *input, char *output)
{
        static char kopt_path[] = "/OPTION/BASE/LIBEXEC/KCC/KOPT";
        char outarg[SYS_RUN_ARG_MAX_CHARS + 1U];
        char *av[4];
        unsigned int n;

        n = (unsigned int)strlen(output);
        if (n + 4U > sizeof(outarg))
                return 126;
        outarg[0] = '-';
        outarg[1] = 'R';
        outarg[2] = '=';
        strcpy(outarg + 3, output);
        av[0] = "KOPT";
        av[1] = "-D";                 /* private: remove consumed KP1 */
        av[2] = input;
        av[3] = outarg;
        return daimos_exec_image(kopt_path, av, 4U);
}
