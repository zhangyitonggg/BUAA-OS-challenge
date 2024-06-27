# shell增强文档
在本文档中，我将按照自己实现各个要求的顺序来进行记录。

### 实现不带 `.b` 后缀指令

这个任务非常容易，实际上只需要修改spawn。在open失败后加个.b再open一次。那么这样的话如果有.b后缀的话再第一次open时就可以成功，否则的话在第二次open时就可以成功了。

```c
int open_again_with_b(char *prog) {
	char temp[256];
	int i;
	for (i = 0; prog[i] != '\0'; ++i) {
		temp[i] = prog[i];
	}
	temp[i++] = '.';
	temp[i++] = 'b';
	temp[i] = '\0';
	return open(temp, O_RDONLY);
}
```

```c
	if (((fd = open(prog, O_RDONLY)) < 0) && ((fd = open_again_with_b(prog)) < 0)) {
		return fd;
	}
```

### 实现一行多指令

这里实际上和管道功能的实现非常类似，不同的是我们没必要进行信息的传输了，只需要fork出一个进程，让子进程执行分号左边部分，然后父进程在子进程执行完毕后执行分号右边部分。

```c
		case ';':;
			int my_env_id = fork();
			if (my_env_id < 0) {
				debugf("failed to fork in sh.c\n");
				exit();
			} else if (my_env_id == 0) { // 子进程执行左边
				return argc; 
			} else { // 父进程执行右边
				wait(my_env_id); // 好像不wait也可以
				return parsecmd(argv, rightpipe); 
			}
			break;
```

### 实现引号支持

这里是让我们将双引号间的部分全部看成字符串，由于没有嵌套引号，所以我们在gettoken中识别到双引号时，其实可以下一个引号之前的部分全部读取出来。

```c
	if (*s == '\"') {
		*p1 = s + 1; // 从双引号之后开始begin
		do {
			s++;
		} while(*s && *s != '\"');
		*s = 0;
		s++;
		*p2 = s;
		return 'w';
	}
```

### 实现注释功能

感觉这个甚至要比第一个任务还要简单，这个只需要在main函数里加如下几行即可，也就是在识别到#后将#所在的位置标志成0即可。

```c
		for (int i = 0; i < strlen(buf); ++i) {
			if (buf[i] == '#') {
				buf[i] = '\0';
			}
		}
```

### touch

核心部分就是调用open函数，然后传入O_RDONLY

```c
#include <lib.h>

void touch(char *path) {
    int fd;
    if ((fd = open(path, O_RDONLY)) >= 0) {
        close(fd);
        return;
    }
    fd = open(path, O_CREAT);
    if (fd == -10) {
        user_panic("touch: cannot touch '%s': No such file or directory\n", path);
    } else if (fd < 0) {
        user_panic("other error when touch %s, error code is %d\n", path, fd);
    } else {
        close(fd);
    }
    return;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        user_panic("nothing to touch\n");
    } else {
        for (int i = 1; i < argc; ++i) {
            touch(argv[i]);
        }
    }
    return 0;
}
```

### mkdir

这里比较复杂的就是如何处理-p选项，首先目录已存在的情况非常容易处理，只需要直接return即可；而创建目录的父目录不存在的情况处理起来就稍微有点复杂了，我是从头到尾扫描path，扫描到`/`时就open一下，并传入参数O_MKDIR。虽然这种方法有点低效，不过实现起来比较方便就是了。

```c
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
```

### rm

这里需要实现-r和-rf选项，但都没有啥难度。唯一需要注意的时即使是删除文件，最后也要调用close(fd)。

```c
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
```

### 实现追加重定向

我是新增了一个文件操作请求类型O_APPEND，然后修改了serve_open函数，当识别到传入了O_APPEND时，则将文件描述符的offset修改成文件的大小，这样就实现了追加的功能。

```c
#define O_APPEND 0x0004

if (rq->req_omode & O_APPEND) {
    ff->f_fd.fd_offset = ff->f_file.f_size;
    // debugf("offset: %d\n", ff->f_fd.fd_offset);
}
```

### 实现反引号

这个任务应该是第一个比较难的了。我是修改了runcmd的实现逻辑，当识别到是echo指令时，就特判一下有没有反引号，然后通过管道做出处理（感觉有点取巧了，不过只用考虑echo指令，所以足够通过评测了）。

```c
// 处理反引号
if (argv[i][0] == '`') {
    argv[i][0] = ' ';
    int p[2];
    if(pipe(p) < 0) {
        debugf("failed to create pipe\n");
        exit();
    }
    int my_env_id = fork();
    if (my_env_id < 0) {
        debugf("failed to fork in sh.c\n");
        exit();
    } else if (my_env_id == 0) { // 子进程执行反引号部分
        close(p[0]);
        dup(p[1], 1);
        close(p[1]);

        char temp_cmd[1024] = {0};	
        strcpy(temp_cmd, argv[i]);
        runcmd(temp_cmd);
        exit();		
    } else { // 父进程处理argv
        close(p[1]);
        memset(outbuf, 0, sizeof(outbuf));
        int offset = 0;
        int read_num = 0;
        while((read_num = read(p[0], outbuf + offset, sizeof(outbuf) - offset - 5)) > 0) {
            offset += read_num;
        }
        if (read_num < 0) {
            debugf("error in `\n");
            exit();
        }
        close(p[0]);
        argv[i] = outbuf;
    }
```

### 实现指令条件执行

这一个任务耗费了大概一天的时间，我认为是整个挑战性任务中最难的一个任务。主要思路是在sh中定义一个全局变量flag，如果识别到&&或者||，那么进程A就fork一个子进程B，在B中将flag设置为1，然后spawn一个此进程C，C结束时会无条件地将是否成功的信息通过ipc传给给其父进程，在这里就是B。然后B通过判断flag来决定是否要将其通过ipc返回给A。A通过传来的信息来判断是否还需要往后执行。

```c
// runcmd
if (child >= 0) {
    u_int caller;
    int res = ipc_recv(&caller, 0, 0);
    if (flag_next_is_condition) {
        ipc_send(syscall_get_env_parent_id(0), res, 0, 0);
    }
    // wait(child);
}

// parsecmd
case 'A':;
my_env_id = fork();
if (my_env_id < 0) {
    debugf("failed to fork in sh.c\n");
    exit();
} else if (my_env_id == 0) {
    flag_next_is_condition = 1;
    return argc;
} else {
    u_int caller;
    int res = ipc_recv(&caller, 0, 0);
    // wait(my_env_id);
    if (res != 0) {
        int op = gettoken(0, &t);
        if (op == 0) {
            return 0;
        } else if (op == 'O') {
            return parsecmd(argv, rightpipe);
        }
    } else {
        return parsecmd(argv, rightpipe);
    }
    // return res == 0 ? parsecmd(argv, rightpipe) : 0;
}
// argv[argc++] = "&&";
break;
case '&':;
my_env_id = fork();
if (my_env_id < 0) {
    debugf("failed to fork in sh.c\n");
    exit();
} else if (my_env_id == 0) {
    flag_is_in_back = 1;
    return argc;
} else {
    return parsecmd(argv, rightpipe);
}
break;
}
```

在这里列出我和我朋友的聊天记录，已证明我当时实现这个任务时的喜悦：

![image-20240625162225408](E:\Typora\typora-images\image-20240625162225408.png)

### 实现历史指令

这里主要是有两项小任务，第一是要将所有的指令保存到一个文件中，并在调用history时全部输出；第二则是监听键盘输入，以做到通过up和down来实现指令的切换：

```c
void init_mosh_history() {
	int fd;
	if ((fd = open("/.mosh_history", O_RDONLY | O_CREAT)) < 0) {
		user_panic("open ./mosh_history: %d", fd);		
	}	
	all_line_count = 0;
	
	while(1) {
		int offset = 0;
		int r;
		while((r = readn(fd, all_lines[all_line_count] + offset, 1)) == 1) {
			if (offset > 1024) {
				user_panic("too long in init_mosh_history");
				exit();
			}
			if (all_lines[all_line_count][offset++] == '\n') {
				all_lines[all_line_count][offset] = '\0';
				all_line_count++;
				break;
			}
		}
		if (r < 1) {
			break;
		}
	}
	
	now_line_index = all_line_count - 1;

	close(fd);
}


// 首先更新all_lines,如果all_line_count超过20，则删除最早的指令；之后将数组写到文件中
void save_line(char *line) {
	int fd;
	
	if ((fd = open("/.mosh_history", O_TRUNC | O_WRONLY)) < 0) { // 被删除了
		all_line_count = 0;
		now_line_index = 0;
		if ((fd = open("/.mosh_history", O_WRONLY | O_TRUNC | O_CREAT)) < 0) {
			user_panic("open ./mosh_history: %d", fd);		
		}	
	}

	if (all_line_count >= 20)  {
		all_line_count = 20;
		for (int i = 0; i < 19; i++) {
			strcpy(all_lines[i], all_lines[i + 1]);
		}
		strcpy(all_lines[all_line_count - 1], line);
	} else {
		strcpy(all_lines[all_line_count++], line);
	}

	now_line_index = all_line_count - 1;

	int len = strlen(all_lines[now_line_index]);
	all_lines[now_line_index][len++] = '\n';
	all_lines[now_line_index][len] = '\0';
	// printf("!!!%s",all_lines[now_line_index]);

	for (int i = 0; i < all_line_count; ++i) {
		len = strlen(all_lines[i]);
		// printf("len:%d\n", len);
		write(fd, all_lines[i], len);
	}

	close(fd);
}

char read_dir() {
	char ch;
	if (read(0, &ch, 1) < 0) {
		printf("error in read_id\n");
		exit();
	}
	if (ch == 91) {
		if (read(0, &ch, 1) < 0) {
			printf("error in read_id\n");
			exit();	
		}
		if (ch == 'A') {
			return UP;
		} else if (ch == 'B') {
			return DOWN;
		}
	}
	return OTHER;
}


void tackle_dir(char *buf) {
	char op = read_dir();
	int buf_len = strlen(buf);
	if (op == UP) {
		// do something
	} else if (op == DOWN) {
		// do something
	} 
}
```



### 实现前后台任务管理

这里我是在内核中定义一个后台任务状态记录表，然后所有操作都是通过新增系统调用来实现的。

具体而言，新增后台任务时，就是想记录表中新增一个记录即可；调用jobs时，就是将任务表按格式输出即可；调用fg时，就是让主进程wait一下后台任务的进行即可；调用kill时，就是直接销毁后台任务的进程的控制块即可。

```c
void sys_add_job(u_int envid, char *cmd) {
	struct Job* job = jobs + job_num;
	job->job_id = ++job_num;
	job->flag = 0;
	job->env_id = envid;
	strcpy(job->command, cmd);
	// printk("\n!%d!\n", job_num);
}

void sys_print_jobs() {
	for (int i = 0; i < job_num; i++) {
		struct Job* job = jobs + i;
		printk("[%d] %-10s 0x%08x %s\n", job->job_id, job->flag ? "Done" : "Running", job->env_id, job->command);
	}
}

void sys_try_done_job_status(u_int envid) {
	int id = curenv->env_id;
	for (int i = 0; i < job_num; i++) {
		struct Job* job = jobs + i;
		if (id == job->env_id && job->flag == 0) {
			job->flag = 1;
		} 
	}

	struct Env *e;
	envid2env(envid, &e, 1);

	printk("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return;
}

void sys_kill_job(u_int job_id) {
	// printk("\n%d\n\n", job_id);
	if (job_num < job_id) {
		printk("fg: job (%d) do not exist\n", job_id);
		return;
	} 
	struct Job* job = jobs + job_id - 1;
	if (job->flag == 1) {
		printk("fg: (0x%08x) not running\n", job->env_id);
		return;
	}

	job->flag = 1;
	
	struct Env *e;
	envid2env(job->env_id, &e, 0);

	printk("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return;
}

u_int sys_get_envid_by_job(u_int job_id) {
	if (job_num < job_id) {
		printk("fg: job (%d) do not exist\n", job_id);
		return -1;
	} 
	struct Job* job = jobs + job_id - 1;
	if (job->flag == 1) {
		printk("fg: (0x%08x) not running\n", job->env_id);
		return -2;
	}

	return job->env_id;
}
```























