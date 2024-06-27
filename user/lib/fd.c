// 实现了文件描述符，允许用户程序使用统一的接口，抽象地操作磁盘文件系统中的文件，以及控制台和管道等虚拟的文件
#include <env.h>
#include <fd.h>
#include <lib.h>
#include <mmu.h>

static struct Dev *devtab[] = {&devfile, &devcons,
#if !defined(LAB) || LAB >= 6
			       &devpipe,
#endif
			       0};

// 根据设备序号 dev_id 找到对应的设备
int dev_lookup(int dev_id, struct Dev **dev) {
	for (int i = 0; devtab[i]; i++) {
		if (devtab[i]->dev_id == dev_id) {
			*dev = devtab[i];
			return 0;
		}
	}
	*dev = NULL;
	debugf("[%08x] unknown device type %d\n", env->env_id, dev_id);
	return -E_INVAL;
}

// Overview:
//  Find the smallest i from 0 to MAXFD-1 that doesn't have its fd page mapped.
//
// Post-Condition:
//   Set *fd to the fd page virtual address.
//   (Do not allocate any pages: It is up to the caller to allocate
//    the page, meaning that if someone calls fd_alloc twice
//    in a row without allocating the first page we returned, we'll
//    return the same page at the second time.)
//   Return 0 on success, or an error code on error.
// 遍历所有文件描述符编号（共有MAXFD个），找到其中还
// 没有被使用过的最小的一个，返回该文件描述符对应的地址。
int fd_alloc(struct Fd **fd) {
	u_int va;
	u_int fdno;

	for (fdno = 0; fdno < MAXFD - 1; fdno++) {
		// 通过文件描述符编号获取虚拟地址
		va = INDEX2FD(fdno);
		// 通过查看页目录项和页表项是否有效来得知文件描述符是否被使用
		if ((vpd[va / PDMAP] & PTE_V) == 0) {
			*fd = (struct Fd *)va;
			return 0;
		}

		if ((vpt[va / PTMAP] & PTE_V) == 0) { // the fd is not used
			*fd = (struct Fd *)va;
			return 0;
		}
	}
	// 这里只是返回了可以作为文件描述符的地址，还没有实际的文件描述符数据

	return -E_MAX_OPEN;
}

void fd_close(struct Fd *fd) {
	panic_on(syscall_mem_unmap(0, fd));
}

// Overview:
//  Find the 'Fd' page for the given fd number.
//
// Post-Condition:
//  Return 0 and set *fd to the pointer to the 'Fd' page on success.
//  Return -E_INVAL if 'fdnum' is invalid or unmapped.
// 根据序号fdnum找到文件描述符
int fd_lookup(int fdnum, struct Fd **fd) {
	u_int va;

	if (fdnum >= MAXFD) {
		return -E_INVAL;
	}

	va = INDEX2FD(fdnum);

	if ((vpt[va / PTMAP] & PTE_V) != 0) { // the fd is used
		*fd = (struct Fd *)va;
		return 0;
	}

	return -E_INVAL;
}

// 获取文件内容应该映射到的地址
void *fd2data(struct Fd *fd) {
	return (void *)INDEX2DATA(fd2num(fd));
}

int fd2num(struct Fd *fd) {
	return ((u_int)fd - FDTABLE) / PTMAP;
}

int num2fd(int fd) {
	return fd * PTMAP + FDTABLE; // 实际就是INDEX2FD(i)
}

int close(int fdnum) {
	int r;
	struct Dev *dev = NULL;
	struct Fd *fd;

	if ((r = fd_lookup(fdnum, &fd)) < 0 || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0) {
		return r;
	}

	r = (*dev->dev_close)(fd);
	fd_close(fd);
	return r;
}

void close_all(void) {
	int i;

	for (i = 0; i < MAXFD; i++) {
		close(i);
	}
}

/* Overview:
 *   Duplicate the file descriptor.
 *
 * Post-Condition:
 *   Return 'newfdnum' on success.
 *   Return the corresponding error code on error.
 *
 * Hint:
 *   Use 'fd_lookup' or 'INDEX2FD' to get 'fd' to 'fdnum'.
 *   Use 'fd2data' to get the data address to 'fd'.
 *   Use 'syscall_mem_map' to share the data pages.
 */
int dup(int oldfdnum, int newfdnum) {
	int i, r;
	void *ova, *nva;
	u_int pte;
	struct Fd *oldfd, *newfd;

	/* Step 1: Check if 'oldnum' is valid. if not, return an error code, or get 'fd'. */
	if ((r = fd_lookup(oldfdnum, &oldfd)) < 0) {
		return r;
	}

	/* Step 2: Close 'newfdnum' to reset content. */
	close(newfdnum);
	/* Step 3: Get 'newfd' reffered by 'newfdnum'. */
	newfd = (struct Fd *)INDEX2FD(newfdnum);
	/* Step 4: Get data address. */
	ova = fd2data(oldfd);
	nva = fd2data(newfd);
	/* Step 5: Dunplicate the data and 'fd' self from old to new. */
	// 6.3
	// 共享文件描述符与文件内容
	if (vpd[PDX(ova)]) {
		for (i = 0; i < PDMAP; i += PTMAP) {
			pte = vpt[VPN(ova + i)];

			if (pte & PTE_V) {
				// should be no error here -- pd is already allocated
				if ((r = syscall_mem_map(0, (void *)(ova + i), 0, (void *)(nva + i),
							 pte & (PTE_D | PTE_LIBRARY))) < 0) {
					goto err;
				}
			}
		}
	}

	// 为 newfd 设置与 oldfd 相同的可写可共享权限
	if ((r = syscall_mem_map(0, oldfd, 0, newfd, vpt[VPN(oldfd)] & (PTE_D | PTE_LIBRARY))) <
	    0) {
		goto err;
	}

	return newfdnum;

err:
	/* If error occurs, cancel all map operations. */
	panic_on(syscall_mem_unmap(0, newfd));

	for (i = 0; i < PDMAP; i += PTMAP) {
		panic_on(syscall_mem_unmap(0, (void *)(nva + i)));
	}

	return r;
}

// Overview:
//  Read at most 'n' bytes from 'fd' at the current seek position into 'buf'.
//
// Post-Condition:
//  Update seek position.
//  Return the number of bytes read successfully.
//  Return < 0 on error.
// 从fd中读取至多n个byte，存储到buf中，返回读取字节个数
int read(int fdnum, void *buf, u_int n) {
	int r;

	// Similar to the 'write' function below.
	// Step 1: Get 'fd' and 'dev' using 'fd_lookup' and 'dev_lookup'.
	struct Dev *dev;
	struct Fd *fd;
	/* Exercise 5.10: Your code here. (1/4) */
	try(fd_lookup(fdnum, &fd));
	try(dev_lookup(fd->fd_dev_id, &dev));

	// Step 2: Check the open mode in 'fd'.
	// Return -E_INVAL if the file is opened for writing only (O_WRONLY).
	/* Exercise 5.10: Your code here. (2/4) */
	if ((fd->fd_omode & O_ACCMODE) == O_WRONLY) {
		// 文件是只写的
		return -E_INVAL;
	}

	// Step 3: Read from 'dev' into 'buf' at the seek position (offset in 'fd').
	/* Exercise 5.10: Your code here. (3/4) */
	r = dev->dev_read(fd, buf, n, fd->fd_offset); // 对于devfile而言，dev_read其实就是file_read

	// Step 4: Update the offset in 'fd' if the read is successful.
	/* Hint: DO NOT add a null terminator to the end of the buffer!
	 *  A character buffer is not a C string. Only the memory within [buf, buf+n) is safe to
	 *  use. */
	/* Exercise 5.10: Your code here. (4/4) */
	// 更新文件的fd_offset
	if (r > 0) {
		fd->fd_offset += r;
	}
	
	// 返回读到的数据的字节数
	return r;
}

int readn(int fdnum, void *buf, u_int n) {
	int m, tot;

	for (tot = 0; tot < n; tot += m) {
		m = read(fdnum, (char *)buf + tot, n - tot);

		if (m < 0) {
			return m;
		}

		if (m == 0) {
			break;
		}
	}

	return tot;
}

int write(int fdnum, const void *buf, u_int n) {
	int r;
	struct Dev *dev;
	struct Fd *fd;

	if ((r = fd_lookup(fdnum, &fd)) < 0 || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0) {
		return r;
	}

	if ((fd->fd_omode & O_ACCMODE) == O_RDONLY) {
		return -E_INVAL;
	}

	r = dev->dev_write(fd, buf, n, fd->fd_offset);
	if (r > 0) {
		fd->fd_offset += r;
	}

	return r;
}

int seek(int fdnum, u_int offset) {
	int r;
	struct Fd *fd;

	if ((r = fd_lookup(fdnum, &fd)) < 0) {
		return r;
	}

	fd->fd_offset = offset;
	return 0;
}

int fstat(int fdnum, struct Stat *stat) {
	int r;
	struct Dev *dev = NULL;
	struct Fd *fd;

	if ((r = fd_lookup(fdnum, &fd)) < 0 || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0) {
		return r;
	}

	stat->st_name[0] = 0;
	stat->st_size = 0;
	stat->st_isdir = 0;
	stat->st_dev = dev;
	return (*dev->dev_stat)(fd, stat);
}

int stat(const char *path, struct Stat *stat) {
	int fd, r;

	if ((fd = open(path, O_RDONLY)) < 0) {
		return fd;
	}

	r = fstat(fd, stat);
	close(fd);
	return r;
}
