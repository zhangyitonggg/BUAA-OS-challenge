#include <lib.h>

int flag_r;
int flag_f;

void rm(char *path) {
    int fd;
    struct Stat st;
    if ((fd = open(path, O_RDONLY)) < 0) {
        if (!flag_f) {
            user_panic("rm: cannot remove '%s': No such file or directory\n", path);
        }
        return;
    }
    close(fd);
    stat(path, &st);
    if (st.st_isdir && !flag_r) {
        user_panic("rm: cannot remove '%s': Is a directory\n", path);
    }
    remove(path);
}

int main(int argc, char **argv) {
    char s_r[5] = "-r";
    char s_rf[5] = "-rf";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], s_r) == 0) {
            argv[i] = 0;
            flag_r = 1;
        } else if (strcmp(argv[i], s_rf) == 0) {
            argv[i] = 0;
            flag_f = 1;
            flag_r = 1;
        }
    }

    if (argc < 2) {
        user_panic("nothing to rm\n");
    } else {
        for (int i = 1; i < argc; ++i) {
            if (argv[i] == 0) {
                continue;
            }
            rm(argv[i]);
        }
    }
}