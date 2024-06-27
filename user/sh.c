// 不断读取用户的命令输入，根据命令创建对应的进程，并实现进程间的通信
#include <args.h>
#include <lib.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"
#define UP 'A'
#define DOWN 'B'
#define OTHER -1

int flag_next_is_condition;
int flag_is_in_back;


char outbuf[20000];
char now_cmd_buf[1025];

int all_line_count; // 表示指令总数量，上限20
int now_line_index; // 表示光标当前所在指令索引
char all_lines[25][1025];

void tackle_dir(char *buf);
char read_dir();
/* Overview:
 *   Parse the next token from the string at s.
 *
 * Post-Condition:
 *   Set '*p1' to the beginning of the token and '*p2' to just past the token.
 *   Return:
 *     - 0 if the end of string is reached.
 *     - '<' for < (stdin redirection).
 *     - '>' for > (stdout redirection).
 *     - '|' for | (pipe).
 *     - 'w' for a word (command, argument, or file name).
 *
 *   The buffer is modified to turn the spaces after words into zero bytes ('\0'), so that the
 *   returned token is a null-terminated string.
 */
int _gettoken(char *s, char **p1, char **p2) {
	*p1 = 0;
	*p2 = 0;
	if (s == 0) {
		return 0;
	}

	while (strchr(WHITESPACE, *s)) {
		*s++ = 0;
	}
	if (*s == 0) {
		return 0;
	}

	if (*s == '"') {
		*p1 = s; // 从双引号开始begin
		do {
			s++;
		} while(*s && *s != '"');
		*s = 0;
		s++;
		*p2 = s;
		return 'w';
	}

	if (*s == '`') {
		*p1 = s; // 从反引号开始begin
		do {
			s++;
		} while(*s && *s != '`');
		*s = 0;
		s++;
		*p2 = s;
		return 'w';
	}

	if (*s == '>' && *(s + 1) == '>') {
		*p1 = s;
		*s++ = 0;
		*s++ = 0;
		*p2 = s;
		return 'a';
	}

	if (*s == '|' && *(s + 1) == '|') {
		*p1 = s;
		*s++ = 0;
		*s++ = 0;
		*p2 = s;
		return 'O';
	}

	if (*s == '&' && *(s + 1) == '&') {
		*p1 = s;
		*s++ = 0;
		*s++ = 0;
		*p2 = s;
		return 'A';
	}

	if (strchr(SYMBOLS, *s)) {
		int t = *s;
		*p1 = s;
		*s++ = 0;
		*p2 = s;
		return t;
	}

	*p1 = s;
	while (*s && !strchr(WHITESPACE SYMBOLS, *s)) {
		s++;
	}
	*p2 = s;
	return 'w';
}

int gettoken(char *s, char **p1) {
	static int c, nc;
	static char *np1, *np2;

	if (s) {
		nc = _gettoken(s, &np1, &np2);
		return 0;
	}
	c = nc;
	// 这里np1会在gettoken中改变
	*p1 = np1;
	nc = _gettoken(np2, &np1, &np2);
	return c;
}

#define MAXARGS 128

int parsecmd(char **argv, int *rightpipe) {
	int argc = 0;
	while (1) {
		flag_next_is_condition = 0;
		flag_is_in_back = 0;
		char *t;
		int fd, r;
		int c = gettoken(0, &t);
		int my_env_id;
		switch (c) {
		case 0:
			return argc;
		case 'w':
			if (argc >= MAXARGS) {
				debugf("too many arguments\n");
				exit();
			}
			argv[argc++] = t;
			break;
		case '<':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: < not followed by word\n");
				exit();
			}
			// Open 't' for reading, dup it onto fd 0, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (1/3) */
			if((fd = open(t, O_RDONLY)) < 0) {
				debugf("failed to open %s\n", t);
				exit();
			}
			if((r = dup(fd, 0)) < 0) {
				debugf("failed to duplicate file to <stdin>\n");
				exit();
			}
			close(fd);
			// user_panic("< redirection not implemented");

			break;
		case '>':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: > not followed by word\n");
				exit();
			}
			// Open 't' for writing, create it if not exist and trunc it if exist, dup
			// it onto fd 1, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (2/3) */
			if((fd = open(t, O_WRONLY)) < 0) {
				fd = open(t, O_CREAT);
			    if (fd < 0) {
        			debugf("error in open %s\n", t);
					exit();
				}
			}
			close(fd);
			if ((fd = open(t, O_TRUNC)) < 0) {
        		debugf("error in open %s\n", t);
				exit();
			}
			close(fd);
			if ((fd = open(t, O_WRONLY)) < 0) {
        		debugf("error in open %s\n", t);
				exit();				
			}

			if((r = dup(fd, 1)) < 0) {
				debugf("failed to duplicate file to <stdout>\n");
				exit();
			}
			close(fd);

			break;
		case 'a':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: > not followed by word\n");
				exit();
			}

			// 建文件
			if((fd = open(t, O_RDONLY)) < 0) {
				fd = open(t, O_CREAT);
			    if (fd < 0) {
        			debugf("error in open %s\n", t);
					exit();
				}
			}
			close(fd);
			// >>
			if ((fd = open(t, O_WRONLY | O_APPEND)) < 0) {
        		debugf("error in open %s\n", t);
				exit();				
			}

			if((r = dup(fd, 1)) < 0) {
				debugf("failed to duplicate file to <stdout>\n");
				exit();
			}
			close(fd);

			break;
		case '|':;
			/*
			 * First, allocate a pipe.
			 * Then fork, set '*rightpipe' to the returned child envid or zero.
			 * The child runs the right side of the pipe:
			 * - dup the read end of the pipe onto 0
			 * - close the read end of the pipe
			 * - close the write end of the pipe
			 * - and 'return parsecmd(argv, rightpipe)' again, to parse the rest of the
			 *   command line.
			 * The parent runs the left side of the pipe:
			 * - dup the write end of the pipe onto 1
			 * - close the write end of the pipe
			 * - close the read end of the pipe
			 * - and 'return argc', to execute the left of the pipeline.
			 */
			int p[2];
			/* Exercise 6.5: Your code here. (3/3) */
			if((r = pipe(p)) < 0) {
				debugf("failed to create pipe\n");
				exit();
			}
			if((*rightpipe = fork()) == 0) {
				dup(p[0], 0);
				close(p[0]);
				close(p[1]);
				return parsecmd(argv, rightpipe);
			} else {
				dup(p[1], 1);
				close(p[1]);
				close(p[0]);
				return argc;
			}
			// user_panic("| not implemented");
			break;
		// ';'已经在SYMBOLS中
		case ';':;
			my_env_id = fork();
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
		case 'O':;
			my_env_id = fork();
			if (my_env_id < 0) {
				debugf("failed to fork in sh.c\n");
				exit();
			} else if (my_env_id == 0) { // 子进程
				flag_next_is_condition = 1;
				return argc;
			} else {
				u_int caller;
				int res = ipc_recv(&caller, 0, 0);
				if (res == 0) {
					while(1) {
						int op = gettoken(0, &t);
						if (op == 0) {
							return 0;
						} else if (op == 'A') {
							return parsecmd(argv, rightpipe);
						}
					}
				} else {
					return parsecmd(argv, rightpipe);
				}
				// return res == 0 ? 0 : parsecmd(argv, rightpipe);
			}
			// argv[argc++] = "||";
			break;
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
	}

	return argc;
}

void runcmd(char *s) {
	char *s_begin = s;
	char s_copy[1025];
	strcpy(s_copy, s);
	// 首先调用一次 gettoken，这将把 s 设定为要解析的字符串
	gettoken(s, 0);

	char *argv[MAXARGS];
	int rightpipe = 0;
	// 调用 parsecmd 将完整的字符串解析。解析的参数返回到 argv，参数的数量返回为 argc
	int argc = parsecmd(argv, &rightpipe);
	if (argc == 0) {
		return;
	}
	argv[argc] = 0;

	if (strcmp(argv[0], "echo") == 0) {
		for (int i = 1; i < argc; ++i) {
			// 处理双引号
			if (argv[i][0] == '"') {
				for (int j = 0; argv[i][j] != '\0'; j++) {
					argv[i][j] = argv[i][j + 1];
				}
			}
			// 处理反引号
			else if (argv[i][0] == '`') {
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
			}
		}
	}

	if (strcmp(argv[0] , "history") == 0) {
		int fd, r, n;
		char hist_buf[1025];
		if ((fd = open("/.mosh_history", O_RDONLY)) < 0) {
		    printf("error in history.b\n");
		    exit();
		}
		while((n = read(fd, hist_buf, 1024)) > 0) {
		    hist_buf[n] = '\0';
		    printf("%s", hist_buf); 
		}
		
		close(fd);

		return;
	}


	if (strcmp(argv[0] , "jobs") == 0) {
		syscall_print_jobs();
	
		close_all();
		if (rightpipe) {
			wait(rightpipe);
		}
		exit();
	} else if (strcmp(argv[0], "kill") == 0) {
		// syscall_try_done_job_status()
		int job_id = 0;
		for (int i = 0; argv[1][i]; i++) {
			job_id = 10 * job_id + argv[1][i] - '0';
		}
		// printf("1111111111");
		syscall_kill_job(job_id);
	} else if (strcmp(argv[0], "fg") == 0) {
		int job_id = 0;
		for (int i = 0; argv[1][i]; i++) {
			job_id = 10 * job_id + argv[1][i] - '0';
		}
		u_int envid = syscall_get_envid_by_job(job_id);
		if (envid >= 0) {
			wait(envid);
		}
	}

	char tmp[1000];
	if (flag_is_in_back > 0) {
		int begin = argv[0] - s_begin;
		
		int end = 1024;
		for (int i = argv[argc - 1] - s_begin + strlen(argv[argc - 1]); i < 1024; i++) {
			if (s_copy[i] == '&') {
				end = i + 1;
				break;
			}
		}

		strcpy(tmp, s_copy + begin);
		tmp[end - begin] = 0;
	}

	int child = spawn(argv[0], argv);
	if (flag_is_in_back > 0) {
		syscall_add_job(child, tmp);
	}
	
	// 关闭当前进程的所有文件
	close_all();
	if (child >= 0) {
		u_int caller;
		int res = ipc_recv(&caller, 0, 0);
		if (flag_next_is_condition) {
			ipc_send(syscall_get_env_parent_id(0), res, 0, 0);
		}
		// wait(child);
	} else {
		debugf("spawn %s: %d\n", argv[0], child);
	}
	if (rightpipe) {
		wait(rightpipe);
	}
	exit();
}

void readline(char *buf, u_int n) {
	memset(buf, 0, 1024);
	int r;
	for (int i = 0; i < n; i++) {
		// printf("@%s@",buf);
		if ((r = read(0, buf + i, 1)) != 1) {
			if (r < 0) {
				debugf("read error: %d\n", r);
			}
			exit();
		}
		if (buf[i] == '\b' || buf[i] == 0x7f) {
			if (i > 0) {
				i -= 2;
			} else {
				i = -1;
			}
			if (buf[i] != '\b') {
				printf("\b");
			}
		}


		// hist
		if (buf[i] == 0x1b) {
			buf[i] = 0;
			i--;
			// printf("___(((%s))): %d___", buf, strlen(buf));
			tackle_dir(buf);
		}
		// hist

		if (buf[i] == '\r' || buf[i] == '\n') {
			buf[i] = 0;
			return;
		}
	}
	debugf("line too long\n");
	while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
		;
	}
	buf[0] = 0;
}

char buf[1024];

void usage(void) {
	printf("usage: sh [-ix] [script-file]\n");
	exit();
}

void print_hello_message();
void init_mosh_history();
void save_line(char *line);

int main(int argc, char **argv) {
	int r;
	int interactive = iscons(0);
	int echocmds = 0;
	print_hello_message();

	// 解析命令中的选项
	ARGBEGIN {
	case 'i':
		interactive = 1;
		break;
	case 'x':
		echocmds = 1;
		break;
	default:
		usage();
	}
	ARGEND

	if (argc > 1) {
		usage();
	}
	if (argc == 1) {
		close(0);
		if ((r = open(argv[0], O_RDONLY)) < 0) {
			user_panic("open %s: %d", argv[0], r);
		}
		user_assert(r == 0);
	}

	// 初始化.mosh_history
	init_mosh_history();

	for (;;) {
		// 交互式
		if (interactive) {
			printf("\n$ ");
		}

		// 读取一行
		readline(buf, sizeof buf);

		// 忽略空指令
		if (buf[0] == '\0') {
			continue;
		}

		//保存指令置.mosh_history与临时数组all_lines中
		save_line(buf);

		// 忽略以 # 开头的注释
		if (buf[0] == '#') {
			continue;
		}
		for (int i = 0; i < strlen(buf); ++i) {
			if (buf[i] == '#') {
				buf[i] = '\0';
			}
		}
		
		// 在 echocmds 模式下输出读入的命令
		if (echocmds) {
			printf("# %s\n", buf);
		}
		// 调用 fork 复制了一个 Shell 进程
		if ((r = fork()) < 0) {
			user_panic("fork: %d", r);
		}
		if (r == 0) {
			// 子进程执行 runcmd 函数来运行命令，然后死亡
			runcmd(buf);
			exit();
		} else {
			// 原本的 Shell 进程则等待该新复制的进程结束。
			wait(r);
		}
	}
	return 0;
}
/*-------------------------------------------------------------------------------------*/

void print_hello_message(void) {
	printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	printf("::                                                         ::\n");
	printf("::                     MOS Shell 2024                      ::\n");
	printf("::                                                         ::\n");
	printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
}

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
		// printf("sbup");
		// buf[buf_len - 1] = 0;

		if (now_line_index + 1 == all_line_count) {
			memset(now_cmd_buf, 0 ,1024);
			strcpy(now_cmd_buf, buf);
		}

		// for (int i = 0; i < buf_len - 1; i++)
		// 	printf("\b");
		//printf("@@@%s@@@", buf);

		if (now_line_index > -1) { 
			memset(buf, 0, 1024);
			int len = strlen(all_lines[now_line_index]); // 不能输出最后的'\n'
			strcpy(buf, all_lines[now_line_index]);
			buf[len - 1] = '\0';
			// printf("%s", buf);
			now_line_index--;
		}
	} else if (op == DOWN) {
		//printf("down");
	} 
}