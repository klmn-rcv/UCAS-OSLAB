#ifndef __INCLUDE_OS_FS_H__
#define __INCLUDE_OS_FS_H__

#include <type.h>

/* macros of file system */
#define SUPERBLOCK_MAGIC 0xDF4C4459
#define NUM_FDESCS 16
#define FS_MAX_NAME_LEN 28

/* inode types */
#define FS_TYPE_FILE 1
#define FS_TYPE_DIR  2

/* data structures of file system */
typedef struct superblock {
    uint32_t magic;          // identify filesystem
    uint32_t sector_start;   // start sector on disk
    uint32_t sector_count;   // total sectors reserved for fs
    uint32_t block_size;     // bytes per block
    uint32_t inode_offset;   // block offset of inode table
    uint32_t data_offset;    // block offset of data area
    uint32_t imap_offset;    // block offset of inode bitmap
    uint32_t bmap_offset;    // block offset of block bitmap
    uint32_t inode_count;    // total inode entries
    uint32_t block_count;    // total data blocks
    uint32_t free_inodes;    // free inode count
    uint32_t free_blocks;    // free block count
    uint32_t root_ino;       // inode number of root directory
} superblock_t;

typedef struct dentry {
    uint32_t ino;                    // target inode number
    uint8_t  type;                   // FS_TYPE_FILE or FS_TYPE_DIR
    uint8_t  valid;                  // 1 if in use
    char     name[FS_MAX_NAME_LEN];  // null-terminated filename
} dentry_t;

typedef struct inode { 
    uint32_t ino;            // inode number
    uint8_t  type;           // FS_TYPE_FILE or FS_TYPE_DIR
    uint8_t  links;          // hard link count
    uint32_t size;           // bytes
    uint32_t direct[12];     // direct block indices
    uint32_t indirect1;      // single indirect block index
    uint32_t indirect2;      // double indirect block index
    uint32_t indirect3;      // triple indirect block index
} inode_t;

typedef struct fdesc {
    uint8_t  used;           // slot in use
    uint8_t  mode;           // O_RDONLY/O_WRONLY/O_RDWR
    uint32_t ino;            // inode number
    uint32_t offset;         // read/write offset
} fdesc_t;

/* modes of do_open */
#define O_RDONLY 1  /* read only open */
#define O_WRONLY 2  /* write only open */
#define O_RDWR   3  /* read/write open */

/* whence of do_lseek */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* fs function declarations */
extern void fs_init(void);
extern int do_mkfs(void);
extern int do_statfs(void);
extern int do_cd(char *path);
extern int do_mkdir(char *path);
extern int do_rmdir(char *path);
extern int do_ls(char *path, int option);
extern int do_open(char *path, int mode);
extern int do_read(int fd, char *buff, int length);
extern int do_write(int fd, char *buff, int length);
extern int do_close(int fd);
extern int do_ln(char *src_path, char *dst_path);
extern int do_rm(char *path);
extern int do_lseek(int fd, int offset, int whence);

#endif