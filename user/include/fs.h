#ifndef _FS_H_
#define _FS_H_ 1

#include <stdint.h>

// File nodes (both in-memory and on-disk)

// Bytes per file system block - same as page size
#define BLOCK_SIZE PAGE_SIZE
#define BLOCK_SIZE_BIT (BLOCK_SIZE * 8)

// Maximum size of a filename (a single path component), including null
#define MAXNAMELEN 128

// Maximum size of a complete pathname, including null
#define MAXPATHLEN 1024

// Number of (direct) block pointers in a File descriptor
#define NDIRECT 10
#define NINDIRECT (BLOCK_SIZE / 4)

#define MAXFILESIZE (NINDIRECT * BLOCK_SIZE)

#define FILE_STRUCT_SIZE 256

// 对于普通的文件，其指向的磁盘块存储着文件内容，而对于目录文件来说，
// 其指向的磁盘块存储着该目录下各个文件对应的文件控制块
struct File {
	char f_name[MAXNAMELEN]; // filename 文件名，最大长度为128
	uint32_t f_size;	 // file size in bytes 文件大小，单位为字节
	uint32_t f_type;	 // file type 文件类型，有普通文件和目录两种
	// 文件的直接指针，每个文件控制块设有10个直接指针，用来记录文件的数据块在磁盘上的位置,其实就是编号
	uint32_t f_direct[NDIRECT]; 
	uint32_t f_indirect; // 文件的间接指针，指向一个间接磁盘块，其实就是存着间接磁盘块的编号
	// 间接磁盘块上存有文件的数据库在磁盘上的位置，其实就是许多个间接磁盘块的编号

	// 自己所在的目录
	struct File *f_dir; // the pointer to the dir where this file is in, valid only in memory.
	// f_pad是为了让整数个文件结构体占用一个磁盘块，填充结构体中剩下的字节
	char f_pad[FILE_STRUCT_SIZE - MAXNAMELEN - (3 + NDIRECT) * 4 - sizeof(void *)];
} __attribute__((aligned(4), packed));

#define FILE2BLK (BLOCK_SIZE / sizeof(struct File))

// File types
#define FTYPE_REG 0 // Regular file
#define FTYPE_DIR 1 // Directory

// File system super-block (both in-memory and on-disk)

#define FS_MAGIC 0x68286097 // Everyone's favorite OS class

struct Super {
	uint32_t s_magic;   // Magic number: FS_MAGIC; 用于验证文件系统的幻数
	uint32_t s_nblocks; // Total number of blocks on disk; 磁盘块总数
	struct File s_root; // Root directory node; 根目录文件节点
};

#endif // _FS_H_
