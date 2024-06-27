#include <lib.h>

int flag;

void mkdir(char *path) {
    int fd;
    if (flag) {
        if ((fd = open(path, O_RDONLY)) >= 0) {
            close(fd);
            return;
        }
        int i = 0;
        char str[1024];
        for (int i = 0; path[i] != '\0'; ++i) {
            if (path[i] == '/') {
                str[i] = '\0';
                if ((fd = open(path, O_RDONLY)) >= 0) {
                    close(fd);
                } else {
                    break;
                } 
            }
            str[i] = path[i];
        }
        for (; path[i] != '\0'; ++i) {
            if (path[i] == '/') {
                str[i] = '\0';
                fd = open(str, O_MKDIR);
                if (fd >= 0) {
                    close(fd);
                } else {
                    user_panic("other error when mkdir %s, error code is %d\n", path, fd);
                }
            }
            str[i] = path[i];
        }
        str[i] = '\0';
        fd = open(str, O_MKDIR);
        if (fd >= 0) {
            close(fd);
        } else {
            user_panic("other error when mkdir %s, error code is %d\n", path, fd);
        }
    } else {
        if ((fd = open(path, O_RDONLY)) >= 0) {
            close(fd);
            user_panic("mkdir: cannot create directory '%s': File exists\n", path);
            return;
        }
        fd = open(path, O_MKDIR);
        if (fd == -10) {
            user_panic("mkdir: cannot create directory '%s': No such file or directory\n", path);
        } else if (fd < 0) {
            user_panic("other error when mkdir %s, error code is %d\n", path, fd);
        } else {
            close(fd);
        }
        return;
    }
}

int main(int argc, char **argv) {
    char s[5] = "-p";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], s) == 0) {
            argv[i] = 0;
            flag = 1;
            break;
        }
    }    

    if (argc < 2) {
        user_panic("nothing to mkdir\n");
    } else {
        for (int i = 1; i < argc; ++i) {
            if (argv[i] == 0) {
                continue;
            }
            mkdir(argv[i]);
        }
    }
    return 0;
}