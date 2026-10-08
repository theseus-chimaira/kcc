/* Small DAIMOS pathname helper shared by native compiler phases. */
#include "u.h"

void
daimos_unlink_path(char *name)
{
        kword_t path[U_PATH_WORDS];

        if (u_s6_pack(path, U_PATH_WORDS, name) == 0)
                (void)dsys_unlink(path);
}
