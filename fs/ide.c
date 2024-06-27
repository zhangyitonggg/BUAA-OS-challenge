// 通过系统调用与磁盘镜像进行交互
/*
 * operations on IDE disk.
 */

#include "serv.h"
#include <lib.h>
#include <malta.h>
#include <mmu.h>

/* Overview:
 *   Wait for the IDE device to complete previous requests and be ready
 *   to receive subsequent requests.
 */
// 等待IDE设备准备好服务当前请求
static uint8_t wait_ide_ready() {
	uint8_t flag;
	while (1) {
		panic_on(syscall_read_dev(&flag, MALTA_IDE_STATUS, 1));
		if ((flag & MALTA_IDE_BUSY) == 0) {
			break;
		}
		syscall_yield();
	}
	return flag;
}

/* Overview:
 *  read data from IDE disk. First issue a read request through
 *  disk register and then copy data from disk buffer
 *  (512 bytes, a sector) to destination array.
 *
 * Parameters:
 *  diskno: disk number.
 *  secno: start sector number.
 *  dst: destination for data read from IDE disk.
 *  nsecs: the number of sectors to read.
 *
 * Post-Condition:
 *  Panic if any error occurs. (you may want to use 'panic_on')
 *
 * Hint: Use syscalls to access device registers and buffers.
 * Hint: Use the physical address and offsets defined in 'include/malta.h'.
 */
// 从IDE磁盘读取数据
void ide_read(u_int diskno, u_int secno, void *dst, u_int nsecs) {
	uint8_t temp;
	u_int offset = 0, max = nsecs + secno;
	panic_on(diskno >= 2);

	// Read the sector in turn
	while (secno < max) {
		// 等待IDE设备准备好服务当前请求
		temp = wait_ide_ready();
		// Step 1: Write the number of operating sectors to NSECT register
		temp = 1;
		// 首先，设置操作扇区的数目，这里我们只操作一个扇区，因此设置为 1。
		panic_on(syscall_write_dev(&temp, MALTA_IDE_NSECT, 1));

		// Step 2: Write the 7:0 bits of sector number to LBAL register
		temp = secno & 0xff;
		// 依次设置操作扇区号
		panic_on(syscall_write_dev(&temp, MALTA_IDE_LBAL, 1));

		// Step 3: Write the 15:8 bits of sector number to LBAM register
		/* Exercise 5.3: Your code here. (1/9) */
		temp = (secno >> 8) & 0xff;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_LBAM, 1));

		// Step 4: Write the 23:16 bits of sector number to LBAH register
		/* Exercise 5.3: Your code here. (2/9) */
		temp = (secno >> 16) & 0xff;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_LBAH, 1));

		// Step 5: Write the 27:24 bits of sector number, addressing mode
		// and diskno to DEVICE register
		// 在设置操作扇区号的 [27:24] 位时，还需要同时设置扇区寻址模式和磁盘编号
		temp = ((secno >> 24) & 0x0f) | MALTA_IDE_LBA | (diskno << 4);
		panic_on(syscall_write_dev(&temp, MALTA_IDE_DEVICE, 1));

		// Step 6: Write the working mode to STATUS register
		temp = MALTA_IDE_CMD_PIO_READ;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_STATUS, 1));

		// Step 7: Wait until the IDE is ready
		temp = wait_ide_ready();

		// Step 8: Read the data from device
		for (int i = 0; i < SECT_SIZE / 4; i++) {
			panic_on(syscall_read_dev(dst + offset + i * 4, MALTA_IDE_DATA, 4));
		}

		// Step 9: Check IDE status
		panic_on(syscall_read_dev(&temp, MALTA_IDE_STATUS, 1));

		// 更新offset
		offset += SECT_SIZE;
		// 更新扇区号
		secno += 1;
	}
}

/* Overview:
 *  write data to IDE disk.
 *
 * Parameters:
 *  diskno: disk number.
 *  secno: start sector number.
 *  src: the source data to write into IDE disk.
 *  nsecs: the number of sectors to write.
 *
 * Post-Condition:
 *  Panic if any error occurs.
 *
 * Hint: Use syscalls to access device registers and buffers.
 * Hint: Use the physical address and offsets defined in 'include/malta.h'.
 */
// 将数据写入IDE磁盘
void ide_write(u_int diskno, u_int secno, void *src, u_int nsecs) {
	uint8_t temp;
	u_int offset = 0, max = nsecs + secno;
	panic_on(diskno >= 2);

	// Write the sector in turn
	while (secno < max) {
		temp = wait_ide_ready();
		// Step 1: Write the number of operating sectors to NSECT register
		/* Exercise 5.3: Your code here. (3/9) */
		temp = 1;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_NSECT, 1));

		// Step 2: Write the 7:0 bits of sector number to LBAL register
		/* Exercise 5.3: Your code here. (4/9) */
		temp = secno & 0xff;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_LBAL, 1));
		
		// Step 3: Write the 15:8 bits of sector number to LBAM register
		/* Exercise 5.3: Your code here. (5/9) */
		temp = (secno >> 8) & 0xff;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_LBAM, 1));
		
		// Step 4: Write the 23:16 bits of sector number to LBAH register
		/* Exercise 5.3: Your code here. (6/9) */
		temp = (secno >> 16) & 0xff;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_LBAH, 1));
		
		// Step 5: Write the 27:24 bits of sector number, addressing mode
		// and diskno to DEVICE register
		/* Exercise 5.3: Your code here. (7/9) */
		// 在设置操作扇区号的 [27:24] 位时，还需要同时设置扇区寻址模式和磁盘编号
		temp = ((secno >> 24) & 0x0f) | MALTA_IDE_LBA | (diskno << 4);
		panic_on(syscall_write_dev(&temp, MALTA_IDE_DEVICE, 1));
		
		// Step 6: Write the working mode to STATUS register
		/* Exercise 5.3: Your code here. (8/9) */
		temp = MALTA_IDE_CMD_PIO_WRITE;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_STATUS, 1));

		// Step 7: Wait until the IDE is ready
		temp = wait_ide_ready();

		// Step 8: Write the data to device
		for (int i = 0; i < SECT_SIZE / 4; i++) {
			/* Exercise 5.3: Your code here. (9/9) */
			panic_on(syscall_write_dev(src + offset + i * 4, MALTA_IDE_DATA, 4));
		}

		// Step 9: Check IDE status
		panic_on(syscall_read_dev(&temp, MALTA_IDE_STATUS, 1));

		offset += SECT_SIZE;
		secno += 1;
	}
}
