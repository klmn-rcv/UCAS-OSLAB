#include <os/string.h>
#include <os/fs.h>
#include <os/kernel.h>
#include <pgtable.h>
#include <printk.h>
#include <assert.h>

static fdesc_t fdesc_array[NUM_FDESCS] __attribute__((unused));

// Basic on-disk layout (simple, single-block maps)
#define FS_START_SECTOR 4096U            // leave space for kernel/tasks
#define FS_SECTOR_COUNT 1048576U         // 512MiB filesystem space
#define FS_BLOCK_SIZE   4096U            // 8 sectors per block
#define FS_INODE_COUNT  1024U
#define FS_MAX_PATH     256
#define FS_NDIRECT      12
#define FS_INDIRECT_ENTRIES (FS_BLOCK_SIZE / sizeof(uint32_t))
#define FS_MAX_BMAP_BLOCKS 8             // supports up to ~2Mi blocks (8 * 4096 * 8 bits)

// Cached metadata
static superblock_t sb;
static int fs_mounted;
static uint32_t cwd_ino;
static char cwd_path[FS_MAX_PATH] = "/";
static uint32_t imap_blocks;
static uint32_t bmap_blocks;
static uint32_t inode_table_blocks;
static uint8_t imap_cache[FS_BLOCK_SIZE];
static uint8_t bmap_cache[FS_MAX_BMAP_BLOCKS * FS_BLOCK_SIZE];
static uint8_t block_cache[FS_BLOCK_SIZE];
static uint8_t block_cache2[FS_BLOCK_SIZE];
static uint8_t zero_block[FS_BLOCK_SIZE];

extern uint32_t last_nonempty_sector;

// ----------------------------------------------------------
// File descriptor helpers
// ----------------------------------------------------------
static int alloc_fd(void)
{
    for (int i = 0; i < NUM_FDESCS; i++) {
        if (!fdesc_array[i].used) {
            fdesc_array[i].used = 1;
            fdesc_array[i].offset = 0;
            fdesc_array[i].ino = 0;
            fdesc_array[i].mode = 0;
            return i;
        }
    }
    return -1;
}

static int validate_fd(int fd)
{
    return fd >= 0 && fd < NUM_FDESCS && fdesc_array[fd].used;
}

// ----------------------------------------------------------
// Helpers: bitmaps, block IO, inode read/write
// ----------------------------------------------------------
static inline uint32_t sectors_per_block(void)
{
    return sb.block_size / SECTOR_SIZE;
}

static inline int bitmap_test(uint8_t *bm, uint32_t idx)
{
    return (bm[idx >> 3] >> (idx & 7)) & 1U;
}

static inline void bitmap_set(uint8_t *bm, uint32_t idx)
{
    bm[idx >> 3] |= (uint8_t)(1U << (idx & 7));
}

static inline void bitmap_clear(uint8_t *bm, uint32_t idx)
{
    bm[idx >> 3] &= (uint8_t)~(1U << (idx & 7));
}

static int disk_read_block(uint32_t block_id, void *buf)
{
    uintptr_t pa = kva2pa((uintptr_t)buf);
    uint32_t sec = sb.sector_start + block_id * sectors_per_block();
    return bios_sd_read(pa, sectors_per_block(), sec);
}

static int disk_write_block(uint32_t block_id, const void *buf)
{
    uintptr_t pa = kva2pa((uintptr_t)buf);
    uint32_t sec = sb.sector_start + block_id * sectors_per_block();
    return bios_sd_write(pa, sectors_per_block(), sec);
}

static void flush_superblock(void)
{
    memset(block_cache, 0, FS_BLOCK_SIZE);
    memcpy(block_cache, &sb, sizeof(sb));
    disk_write_block(0, block_cache);
}

static void flush_imap(void)
{
    for (uint32_t i = 0; i < imap_blocks; i++) {
        disk_write_block(sb.imap_offset + i, imap_cache + i * FS_BLOCK_SIZE);
    }
}

static void flush_bmap(void)
{
    for (uint32_t i = 0; i < bmap_blocks; i++) {
        disk_write_block(sb.bmap_offset + i, bmap_cache + i * FS_BLOCK_SIZE);
    }
}

static int load_superblock(void)
{
    if (!disk_read_block(0, block_cache)) {
        return -1;
    }
    memcpy(&sb, block_cache, sizeof(sb));
    if (sb.magic != SUPERBLOCK_MAGIC) {
        return -1;
    }

    imap_blocks = (sb.inode_count + sb.block_size * 8 - 1) / (sb.block_size * 8);
    bmap_blocks = (sb.data_offset + sb.block_count + sb.block_size * 8 - 1) /
                  (sb.block_size * 8);
    inode_table_blocks = sb.data_offset - sb.inode_offset;

    if (sb.block_size != FS_BLOCK_SIZE || imap_blocks == 0 || bmap_blocks == 0 ||
        bmap_blocks > FS_MAX_BMAP_BLOCKS) {
        return -1;
    }
    if (imap_blocks > 1) {
        return -1;
    }

    disk_read_block(sb.imap_offset, imap_cache);
    for (uint32_t i = 0; i < bmap_blocks; i++) {
        disk_read_block(sb.bmap_offset + i, bmap_cache + i * FS_BLOCK_SIZE);
    }
    fs_mounted = 1;
    cwd_ino = sb.root_ino;
    strcpy(cwd_path, "/");

    // Prevent swap from stealing filesystem sectors
    uint32_t fs_end = sb.sector_start + sb.sector_count;
    if (fs_end > last_nonempty_sector) {
        last_nonempty_sector = fs_end;
    }
    return 0;
}

static void write_inode(uint32_t ino, const inode_t *inode)
{
    uint32_t offset = ino * sizeof(inode_t);
    uint32_t blk = sb.inode_offset + offset / sb.block_size;
    uint32_t off = offset % sb.block_size;

    disk_read_block(blk, block_cache);
    memcpy(block_cache + off, inode, sizeof(inode_t));
    disk_write_block(blk, block_cache);
}

static void read_inode(uint32_t ino, inode_t *inode)
{
    uint32_t offset = ino * sizeof(inode_t);
    uint32_t blk = sb.inode_offset + offset / sb.block_size;
    uint32_t off = offset % sb.block_size;

    disk_read_block(blk, block_cache);
    memcpy(inode, block_cache + off, sizeof(inode_t));
}

static int alloc_inode(uint32_t *ino)
{
    for (uint32_t i = 0; i < sb.inode_count; i++) {
        if (!bitmap_test(imap_cache, i)) {
            bitmap_set(imap_cache, i);
            sb.free_inodes--;
            *ino = i;
            flush_imap();
            flush_superblock();
            return 0;
        }
    }
    return -1;
}

static void free_inode(uint32_t ino)
{
    bitmap_clear(imap_cache, ino);
    sb.free_inodes++;
    flush_imap();
    flush_superblock();
}

// Forward declarations for block allocation helpers
static int alloc_block(uint32_t *blk);
static void free_block(uint32_t blk);

// Map a logical block number to a physical block, allocating metadata/data
// blocks on demand when 'create' is non-zero. Supports direct, single-,
// double-, and triple-indirect addressing.
static int get_data_block(inode_t *inode, uint32_t lbn, int create, uint32_t *blk_out)
{
    uint32_t blk = 0;

    // Direct
    if (lbn < FS_NDIRECT) {
        if (inode->direct[lbn] == 0 && create) {
            if (alloc_block(&inode->direct[lbn]) != 0) return -1;
        }
        blk = inode->direct[lbn];
        *blk_out = blk;
        return blk ? 0 : -1;
    }

    lbn -= FS_NDIRECT;
    uint32_t cap1 = FS_INDIRECT_ENTRIES;
    uint32_t cap2 = FS_INDIRECT_ENTRIES * FS_INDIRECT_ENTRIES;
    uint32_t cap3 = cap2 * FS_INDIRECT_ENTRIES;

    // Single indirect
    if (lbn < cap1) {
        if (inode->indirect1 == 0) {
            if (!create) return -1;
            if (alloc_block(&inode->indirect1) != 0) return -1;
            disk_write_block(inode->indirect1, zero_block);
        }
        disk_read_block(inode->indirect1, block_cache);
        uint32_t *table = (uint32_t *)block_cache;
        if (table[lbn] == 0 && create) {
            uint32_t new_blk;
            if (alloc_block(&new_blk) != 0) return -1;
            disk_read_block(inode->indirect1, block_cache);
            table = (uint32_t *)block_cache;
            table[lbn] = new_blk;
            disk_write_block(inode->indirect1, block_cache);
            disk_write_block(new_blk, zero_block);
        }
        blk = table[lbn];
        *blk_out = blk;
        return blk ? 0 : -1;
    }

    // Double indirect
    lbn -= cap1;
    if (lbn < cap2) {
        uint32_t idx1 = lbn / FS_INDIRECT_ENTRIES;
        uint32_t idx0 = lbn % FS_INDIRECT_ENTRIES;

        if (inode->indirect2 == 0) {
            if (!create) return -1;
            if (alloc_block(&inode->indirect2) != 0) return -1;
            disk_write_block(inode->indirect2, zero_block);
        }

        // Level 2 table
        disk_read_block(inode->indirect2, block_cache);
        uint32_t *lvl2 = (uint32_t *)block_cache;
        if (lvl2[idx1] == 0) {
            if (!create) return -1;
            uint32_t mid;
            if (alloc_block(&mid) != 0) return -1;
            disk_read_block(inode->indirect2, block_cache);
            lvl2 = (uint32_t *)block_cache;
            lvl2[idx1] = mid;
            disk_write_block(inode->indirect2, block_cache);
            disk_write_block(mid, zero_block);
        }

        uint32_t mid_blk = lvl2[idx1];
        disk_read_block(mid_blk, block_cache2);
        uint32_t *lvl1 = (uint32_t *)block_cache2;
        if (lvl1[idx0] == 0 && create) {
            uint32_t data_blk;
            if (alloc_block(&data_blk) != 0) return -1;
            disk_read_block(mid_blk, block_cache2);
            lvl1 = (uint32_t *)block_cache2;
            lvl1[idx0] = data_blk;
            disk_write_block(mid_blk, block_cache2);
            disk_write_block(data_blk, zero_block);
        }

        blk = lvl1[idx0];
        *blk_out = blk;
        return blk ? 0 : -1;
    }

    // Triple indirect
    lbn -= cap2;
    if (lbn >= cap3) {
        return -1; // beyond supported size
    }

    uint32_t idx2 = lbn / cap2;
    uint32_t rem = lbn % cap2;
    uint32_t idx1 = rem / FS_INDIRECT_ENTRIES;
    uint32_t idx0 = rem % FS_INDIRECT_ENTRIES;

    if (inode->indirect3 == 0) {
        if (!create) return -1;
        if (alloc_block(&inode->indirect3) != 0) return -1;
        disk_write_block(inode->indirect3, zero_block);
    }

    disk_read_block(inode->indirect3, block_cache);
    uint32_t *lvl3 = (uint32_t *)block_cache;
    if (lvl3[idx2] == 0) {
        if (!create) return -1;
        uint32_t mid2;
        if (alloc_block(&mid2) != 0) return -1;
        disk_read_block(inode->indirect3, block_cache);
        lvl3 = (uint32_t *)block_cache;
        lvl3[idx2] = mid2;
        disk_write_block(inode->indirect3, block_cache);
        disk_write_block(mid2, zero_block);
    }

    uint32_t mid2_blk = lvl3[idx2];
    disk_read_block(mid2_blk, block_cache2);
    uint32_t *lvl2 = (uint32_t *)block_cache2;
    if (lvl2[idx1] == 0) {
        if (!create) return -1;
        uint32_t mid1;
        if (alloc_block(&mid1) != 0) return -1;
        disk_read_block(mid2_blk, block_cache2);
        lvl2 = (uint32_t *)block_cache2;
        lvl2[idx1] = mid1;
        disk_write_block(mid2_blk, block_cache2);
        disk_write_block(mid1, zero_block);
    }

    uint32_t mid1_blk = lvl2[idx1];
    disk_read_block(mid1_blk, block_cache);
    uint32_t *lvl1 = (uint32_t *)block_cache;
    if (lvl1[idx0] == 0 && create) {
        uint32_t data_blk;
        if (alloc_block(&data_blk) != 0) return -1;
        disk_read_block(mid1_blk, block_cache);
        lvl1 = (uint32_t *)block_cache;
        lvl1[idx0] = data_blk;
        disk_write_block(mid1_blk, block_cache);
        disk_write_block(data_blk, zero_block);
    }

    blk = lvl1[idx0];
    *blk_out = blk;
    return blk ? 0 : -1;
}

static void free_data_blocks(inode_t *inode)
{
    // Direct blocks
    for (int i = 0; i < FS_NDIRECT; i++) {
        if (inode->direct[i]) {
            free_block(inode->direct[i]);
            inode->direct[i] = 0;
        }
    }

    // Single indirect
    if (inode->indirect1) {
        disk_read_block(inode->indirect1, block_cache);
        uint32_t *table = (uint32_t *)block_cache;
        for (uint32_t i = 0; i < FS_INDIRECT_ENTRIES; i++) {
            if (table[i]) {
                free_block(table[i]);
                table[i] = 0;
            }
        }
        free_block(inode->indirect1);
        inode->indirect1 = 0;
    }

    // Double indirect
    if (inode->indirect2) {
        disk_read_block(inode->indirect2, block_cache);
        uint32_t *lvl2 = (uint32_t *)block_cache;
        for (uint32_t i = 0; i < FS_INDIRECT_ENTRIES; i++) {
            if (lvl2[i]) {
                uint32_t mid = lvl2[i];
                disk_read_block(mid, block_cache2);
                uint32_t *lvl1 = (uint32_t *)block_cache2;
                for (uint32_t j = 0; j < FS_INDIRECT_ENTRIES; j++) {
                    if (lvl1[j]) {
                        free_block(lvl1[j]);
                        lvl1[j] = 0;
                    }
                }
                free_block(mid);
                lvl2[i] = 0;
            }
        }
        free_block(inode->indirect2);
        inode->indirect2 = 0;
    }

    // Triple indirect
    if (inode->indirect3) {
        disk_read_block(inode->indirect3, block_cache);
        uint32_t *lvl3 = (uint32_t *)block_cache;
        for (uint32_t i = 0; i < FS_INDIRECT_ENTRIES; i++) {
            if (lvl3[i]) {
                uint32_t mid2 = lvl3[i];
                disk_read_block(mid2, block_cache2);
                uint32_t *lvl2 = (uint32_t *)block_cache2;
                for (uint32_t j = 0; j < FS_INDIRECT_ENTRIES; j++) {
                    if (lvl2[j]) {
                        uint32_t mid1 = lvl2[j];
                        disk_read_block(mid1, block_cache);
                        uint32_t *lvl1 = (uint32_t *)block_cache;
                        for (uint32_t k = 0; k < FS_INDIRECT_ENTRIES; k++) {
                            if (lvl1[k]) {
                                free_block(lvl1[k]);
                                lvl1[k] = 0;
                            }
                        }
                        free_block(mid1);
                        lvl2[j] = 0;
                    }
                }
                free_block(mid2);
                lvl3[i] = 0;
            }
        }
        free_block(inode->indirect3);
        inode->indirect3 = 0;
    }
}

static int alloc_block(uint32_t *blk)
{
    uint32_t total_blocks = sb.data_offset + sb.block_count;
    for (uint32_t i = sb.data_offset; i < total_blocks; i++) {
        if (!bitmap_test(bmap_cache, i)) {
            bitmap_set(bmap_cache, i);
            sb.free_blocks--;
            *blk = i;
            flush_bmap();
            flush_superblock();
            disk_write_block(i, zero_block);
            return 0;
        }
    }
    return -1;
}

static void free_block(uint32_t blk)
{
    bitmap_clear(bmap_cache, blk);
    sb.free_blocks++;
    flush_bmap();
    flush_superblock();
}

// ----------------------------------------------------------
// Path helpers
// ----------------------------------------------------------
static int dir_lookup(uint32_t dir_ino, const char *name, dentry_t *out,
                      uint32_t *blk_idx, uint32_t *entry_idx)
{
    inode_t dir;
    read_inode(dir_ino, &dir);
    if (dir.type != FS_TYPE_DIR) {
        return -1;
    }

    uint32_t entries = dir.size / sizeof(dentry_t);
    uint32_t per_block = sb.block_size / sizeof(dentry_t);

    for (uint32_t i = 0; i < entries; i++) {
        uint32_t blk = i / per_block;
        uint32_t off = i % per_block;
        uint32_t phys;
        if (get_data_block(&dir, blk, 0, &phys) != 0 || phys == 0) {
            continue;
        }
        disk_read_block(phys, block_cache);
        dentry_t *ent = (dentry_t *)(block_cache + off * sizeof(dentry_t));
        if (ent->valid && strcmp(ent->name, name) == 0) {
            if (out) memcpy(out, ent, sizeof(dentry_t));
            if (blk_idx) *blk_idx = blk;
            if (entry_idx) *entry_idx = off;
            return 0;
        }
    }
    return -1;
}

static int dir_add_entry(uint32_t dir_ino, const char *name, uint32_t ino,
                         uint8_t type)
{
    inode_t dir;
    read_inode(dir_ino, &dir);
    uint32_t per_block = sb.block_size / sizeof(dentry_t);
    uint32_t entries = dir.size / sizeof(dentry_t);

    // Try to reuse an invalid slot
    for (uint32_t i = 0; i < entries; i++) {
        uint32_t blk = i / per_block;
        uint32_t off = i % per_block;
        uint32_t phys;
        if (get_data_block(&dir, blk, 0, &phys) != 0 || phys == 0) {
            continue;
        }
        disk_read_block(phys, block_cache);
        dentry_t *ent = (dentry_t *)(block_cache + off * sizeof(dentry_t));
        if (!ent->valid) {
            ent->valid = 1;
            ent->type = type;
            ent->ino = ino;
            strncpy(ent->name, name, FS_MAX_NAME_LEN - 1);
            ent->name[FS_MAX_NAME_LEN - 1] = '\0';
            disk_write_block(phys, block_cache);
            return 0;
        }
    }

    // Need to append
    uint32_t blk_idx = entries / per_block;
    uint32_t off = entries % per_block;
    uint32_t phys;
    if (get_data_block(&dir, blk_idx, 1, &phys) != 0 || phys == 0) {
        return -1;
    }
    disk_read_block(phys, block_cache);
    dentry_t *ent = (dentry_t *)(block_cache + off * sizeof(dentry_t));
    ent->valid = 1;
    ent->type = type;
    ent->ino = ino;
    strncpy(ent->name, name, FS_MAX_NAME_LEN - 1);
    ent->name[FS_MAX_NAME_LEN - 1] = '\0';
    disk_write_block(phys, block_cache);

    dir.size += sizeof(dentry_t);
    write_inode(dir_ino, &dir);
    return 0;
}

static int dir_is_empty(uint32_t dir_ino)
{
    inode_t dir;
    read_inode(dir_ino, &dir);
    if (dir.type != FS_TYPE_DIR) {
        return 0;
    }
    uint32_t entries = dir.size / sizeof(dentry_t);
    uint32_t per_block = sb.block_size / sizeof(dentry_t);
    for (uint32_t i = 0; i < entries; i++) {
        uint32_t blk = i / per_block;
        uint32_t off = i % per_block;
        uint32_t phys;
        if (get_data_block(&dir, blk, 0, &phys) != 0 || phys == 0) {
            continue;
        }
        disk_read_block(phys, block_cache);
        dentry_t *ent = (dentry_t *)(block_cache + off * sizeof(dentry_t));
        if (ent->valid && strcmp(ent->name, ".") && strcmp(ent->name, "..")) {
            return 0;
        }
    }
    return 1;
}

static uint32_t parent_from_dir(uint32_t dir_ino)
{
    inode_t dir;
    read_inode(dir_ino, &dir);
    uint32_t per_block = sb.block_size / sizeof(dentry_t);
    uint32_t entries = dir.size / sizeof(dentry_t);
    for (uint32_t i = 0; i < entries; i++) {
        uint32_t blk = i / per_block;
        uint32_t off = i % per_block;
        uint32_t phys;
        if (get_data_block(&dir, blk, 0, &phys) != 0 || phys == 0) {
            continue;
        }
        disk_read_block(phys, block_cache);
        dentry_t *ent = (dentry_t *)(block_cache + off * sizeof(dentry_t));
        if (ent->valid && strcmp(ent->name, "..") == 0) {
            return ent->ino;
        }
    }
    return dir_ino;
}

static int resolve_parent(const char *path, uint32_t *parent_ino, char *leaf)
{
    if (!path || !path[0]) {
        return -1;
    }

    uint32_t cur = (path[0] == '/') ? sb.root_ino : cwd_ino;
    const char *p = path;
    char component[FS_MAX_NAME_LEN];

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        int len = 0;
        while (p[len] && p[len] != '/') {
            if (len < FS_MAX_NAME_LEN - 1) {
                component[len] = p[len];
            }
            len++;
        }
        component[len < FS_MAX_NAME_LEN ? len : FS_MAX_NAME_LEN - 1] = '\0';

        const char *next = p + len;
        while (*next == '/') next++;
        if (*next == '\0') {
            strncpy(leaf, component, FS_MAX_NAME_LEN - 1);
            leaf[FS_MAX_NAME_LEN - 1] = '\0';
            *parent_ino = cur;
            return 0;
        }

        if (strcmp(component, ".") == 0) {
            // stay
        } else if (strcmp(component, "..") == 0) {
            cur = parent_from_dir(cur);
        } else {
            dentry_t ent;
            if (dir_lookup(cur, component, &ent, NULL, NULL) != 0 || ent.type != FS_TYPE_DIR) {
                return -1;
            }
            cur = ent.ino;
        }
        p = next;
    }
    return -1;
}

static int resolve_path(const char *path, uint32_t *ino_out)
{
    if (!path || !path[0]) {
        *ino_out = cwd_ino;
        return 0;
    }
    uint32_t cur = (path[0] == '/') ? sb.root_ino : cwd_ino;
    const char *p = path;
    char component[FS_MAX_NAME_LEN];

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        int len = 0;
        while (p[len] && p[len] != '/') {
            if (len < FS_MAX_NAME_LEN - 1) {
                component[len] = p[len];
            }
            len++;
        }
        component[len < FS_MAX_NAME_LEN ? len : FS_MAX_NAME_LEN - 1] = '\0';
        const char *next = p + len;
        while (*next == '/') next++;

        if (strcmp(component, ".") == 0) {
            // stay
        } else if (strcmp(component, "..") == 0) {
            cur = parent_from_dir(cur);
        } else {
            dentry_t ent;
            if (dir_lookup(cur, component, &ent, NULL, NULL) != 0) {
                return -1;
            }
            cur = ent.ino;
        }
        p = next;
    }
    *ino_out = cur;
    return 0;
}

static void ensure_mounted(void)
{
    if (!fs_mounted) {
        if (load_superblock() != 0) {
            // Auto-format when no valid superblock is found
            do_mkfs();
        }
    }
}

void fs_init(void)
{
    // Ensure filesystem is ready during kernel boot.
    ensure_mounted();
}

// ----------------------------------------------------------
// Task1 APIs
// ----------------------------------------------------------
int do_mkfs(void)
{
    printk("[FS] Start initialize filesystem!\n");

    // Layout
    sb.magic = SUPERBLOCK_MAGIC;
    sb.sector_start = FS_START_SECTOR;
    sb.sector_count = FS_SECTOR_COUNT;
    sb.block_size = FS_BLOCK_SIZE;
    sb.inode_count = FS_INODE_COUNT;

    imap_blocks = (sb.inode_count + sb.block_size * 8 - 1) / (sb.block_size * 8);
    sb.imap_offset = 1; // block 0 holds superblock

    bmap_blocks = (sb.sector_count * SECTOR_SIZE / sb.block_size + sb.block_size * 8 - 1) /
                  (sb.block_size * 8);
    sb.bmap_offset = sb.imap_offset + imap_blocks;

    inode_table_blocks = (sb.inode_count * sizeof(inode_t) + sb.block_size - 1) / sb.block_size;
    sb.inode_offset = sb.bmap_offset + bmap_blocks;

    sb.data_offset = sb.inode_offset + inode_table_blocks;
    uint32_t total_blocks = sb.sector_count * SECTOR_SIZE / sb.block_size;
    sb.block_count = total_blocks - sb.data_offset;

    if (imap_blocks != 1 || bmap_blocks == 0 || bmap_blocks > FS_MAX_BMAP_BLOCKS) {
        printk("[FS] mkfs: unsupported bitmap sizing\n");
        return -1;
    }

    memset(imap_cache, 0, imap_blocks * FS_BLOCK_SIZE);
    memset(bmap_cache, 0, bmap_blocks * FS_BLOCK_SIZE);

    // Reserve metadata blocks
    for (uint32_t i = 0; i < sb.data_offset; i++) {
        bitmap_set(bmap_cache, i);
    }

    sb.free_inodes = sb.inode_count;
    sb.free_blocks = sb.block_count;

    // Allocate root directory inode and its data block
    uint32_t root_block;
    alloc_block(&root_block);
    bitmap_set(imap_cache, 0);
    sb.root_ino = 0;
    sb.free_inodes = sb.inode_count - 1;
    sb.free_blocks = sb.block_count - 1;

    inode_t root;
    memset(&root, 0, sizeof(root));
    root.ino = sb.root_ino;
    root.type = FS_TYPE_DIR;
    root.links = 2;
    root.size = 2 * sizeof(dentry_t);
    root.direct[0] = root_block;
    write_inode(root.ino, &root);

    // Write root directory entries
    memset(block_cache, 0, FS_BLOCK_SIZE);
    dentry_t *ents = (dentry_t *)block_cache;
    ents[0].ino = root.ino;
    ents[0].type = FS_TYPE_DIR;
    ents[0].valid = 1;
    strcpy(ents[0].name, ".");
    ents[1].ino = root.ino;
    ents[1].type = FS_TYPE_DIR;
    ents[1].valid = 1;
    strcpy(ents[1].name, "..");
    disk_write_block(root_block, block_cache);

    flush_imap();
    flush_bmap();
    flush_superblock();

    fs_mounted = 1;
    cwd_ino = sb.root_ino;
    strcpy(cwd_path, "/");

    uint32_t fs_end = sb.sector_start + sb.sector_count;
    if (fs_end > last_nonempty_sector) {
        last_nonempty_sector = fs_end;
    }
        uint32_t spb = sectors_per_block();

        printk("[FS] Setting superblock...\n");
        printk("    magic : 0x%x\n", sb.magic);
        printk("    num sector : %u, start sector : %u\n", sb.sector_count, sb.sector_start);
        printk("    inode map offset : %u (%u)\n", sb.imap_offset, sb.imap_offset * spb);
        printk("    sector map offset : %u (%u)\n", sb.bmap_offset, sb.bmap_offset * spb);
        printk("    inode offset : %u (%u)\n", sb.inode_offset, sb.inode_offset * spb);
        printk("    data offset : %u (%u)\n", sb.data_offset, sb.data_offset * spb);
        printk("    inode entry size : %luB, dir entry size : %luB\n",
            (unsigned long)sizeof(inode_t), (unsigned long)sizeof(dentry_t));

        printk("[FS] Setting inode-map...\n");
        printk("[FS] Setting sector-map...\n");
        printk("[FS] Setting inode...\n");
        printk("[FS] Initialize filesystem finished!\n");
    return 0;
}

int do_statfs(void)
{
    ensure_mounted();
    if (!fs_mounted) {
        printk("[FS] statfs: filesystem not found\n");
        return -1;
    }
        uint32_t used_blocks = sb.data_offset + (sb.block_count - sb.free_blocks);
        uint32_t used_sectors = used_blocks * sectors_per_block();
        uint32_t used_inodes = sb.inode_count - sb.free_inodes;
        uint32_t spb = sectors_per_block();

        printk("magic : 0x%x (KFS)\n", sb.magic);
        printk("used sector : %u/%u, start sector : %u (0x%x)\n",
            used_sectors, sb.sector_count, sb.sector_start, sb.sector_start * SECTOR_SIZE);
        printk("inode map offset : %u, occupied sector : %u, used : %u/%u\n",
            sb.imap_offset, imap_blocks * spb, used_inodes, sb.inode_count);
        printk("sector map offset : %u, occupied sector : %u\n",
            sb.bmap_offset, bmap_blocks * spb);
        printk("inode offset : %u, occupied sector : %u\n",
            sb.inode_offset, inode_table_blocks * spb);
        printk("data offset : %u, occupied sector : %u\n",
            sb.data_offset, sb.block_count * spb);
        printk("inode entry size : %luB, dir entry size : %luB\n",
            (unsigned long)sizeof(inode_t), (unsigned long)sizeof(dentry_t));
    return 0;
}

int do_cd(char *path)
{
    ensure_mounted();
    if (!fs_mounted) {
        return -1;
    }
    uint32_t ino;
    if (resolve_path(path, &ino) != 0) {
        printk("[FS] cd: path not found\n");
        return -1;
    }
    inode_t target;
    read_inode(ino, &target);
    if (target.type != FS_TYPE_DIR) {
        printk("[FS] cd: not a directory\n");
        return -1;
    }
    cwd_ino = ino;
    strcpy(cwd_path, path && path[0] ? path : "/");
    return 0;
}

int do_mkdir(char *path)
{
    ensure_mounted();
    if (!fs_mounted) {
        return -1;
    }
    char leaf[FS_MAX_NAME_LEN];
    uint32_t parent_ino;
    if (resolve_parent(path, &parent_ino, leaf) != 0 || leaf[0] == '\0') {
        printk("[FS] mkdir: invalid path\n");
        return -1;
    }
    if (strlen(leaf) >= FS_MAX_NAME_LEN) {
        printk("[FS] mkdir: name too long\n");
        return -1;
    }
    if (dir_lookup(parent_ino, leaf, NULL, NULL, NULL) == 0) {
        printk("[FS] mkdir: %s exists\n", leaf);
        return -1;
    }

    uint32_t ino;
    uint32_t blk;
    if (alloc_inode(&ino) != 0 || alloc_block(&blk) != 0) {
        printk("[FS] mkdir: no space\n");
        return -1;
    }

    inode_t node;
    memset(&node, 0, sizeof(node));
    node.ino = ino;
    node.type = FS_TYPE_DIR;
    node.links = 2;
    node.size = 2 * sizeof(dentry_t);
    node.direct[0] = blk;
    write_inode(ino, &node);

    memset(block_cache, 0, FS_BLOCK_SIZE);
    dentry_t *ents = (dentry_t *)block_cache;
    ents[0].ino = ino;
    ents[0].type = FS_TYPE_DIR;
    ents[0].valid = 1;
    strcpy(ents[0].name, ".");
    ents[1].ino = parent_ino;
    ents[1].type = FS_TYPE_DIR;
    ents[1].valid = 1;
    strcpy(ents[1].name, "..");
    disk_write_block(blk, block_cache);

    if (dir_add_entry(parent_ino, leaf, ino, FS_TYPE_DIR) != 0) {
        free_block(blk);
        free_inode(ino);
        return -1;
    }
    return 0;
}

int do_rmdir(char *path)
{
    ensure_mounted();
    if (!fs_mounted) {
        return -1;
    }
    char leaf[FS_MAX_NAME_LEN];
    uint32_t parent;
    if (resolve_parent(path, &parent, leaf) != 0 || leaf[0] == '\0') {
        printk("[FS] rmdir: invalid path\n");
        return -1;
    }
    if (strcmp(leaf, ".") == 0 || strcmp(leaf, "..") == 0) {
        printk("[FS] rmdir: cannot remove . or ..\n");
        return -1;
    }

    dentry_t ent;
    uint32_t blk_idx, ent_idx;
    if (dir_lookup(parent, leaf, &ent, &blk_idx, &ent_idx) != 0) {
        printk("[FS] rmdir: not found\n");
        return -1;
    }

    uint32_t target = ent.ino;
    if (target == sb.root_ino) {
        printk("[FS] rmdir: cannot remove root\n");
        return -1;
    }

    inode_t node;
    read_inode(target, &node);
    if (node.type != FS_TYPE_DIR) {
        printk("[FS] rmdir: not a dir\n");
        return -1;
    }
    if (!dir_is_empty(target)) {
        printk("[FS] rmdir: directory not empty\n");
        return -1;
    }

    inode_t par;
    read_inode(parent, &par);
    uint32_t phys;
    if (get_data_block(&par, blk_idx, 0, &phys) != 0 || phys == 0) {
        return -1;
    }
    disk_read_block(phys, block_cache);
    dentry_t *slot = (dentry_t *)(block_cache + ent_idx * sizeof(dentry_t));
    slot->valid = 0;
    disk_write_block(phys, block_cache);

    free_data_blocks(&node);
    free_inode(target);
    return 0;
}

int do_ls(char *path, int option)
{
    ensure_mounted();
    if (!fs_mounted) {
        return -1;
    }
    uint32_t ino;
    if (path && path[0]) {
        if (resolve_path(path, &ino) != 0) {
            printk("[FS] ls: path not found\n");
            return -1;
        }
    } else {
        ino = cwd_ino;
    }
    inode_t dir;
    read_inode(ino, &dir);
    if (dir.type != FS_TYPE_DIR) {
        printk("[FS] ls: not a directory\n");
        return -1;
    }
    uint32_t entries = dir.size / sizeof(dentry_t);
    uint32_t per_block = sb.block_size / sizeof(dentry_t);
    for (uint32_t i = 0; i < entries; i++) {
        uint32_t blk = i / per_block;
        uint32_t off = i % per_block;
        uint32_t phys;
        if (get_data_block(&dir, blk, 0, &phys) != 0 || phys == 0) continue;
        disk_read_block(phys, block_cache);
        dentry_t *ent = (dentry_t *)(block_cache + off * sizeof(dentry_t));
        if (ent->valid) {
            if (option == 1) {
                char name_copy[FS_MAX_NAME_LEN];
                strncpy(name_copy, ent->name, FS_MAX_NAME_LEN - 1);
                name_copy[FS_MAX_NAME_LEN - 1] = '\0';
                uint8_t type = ent->type;
                uint32_t ino = ent->ino;

                inode_t child;
                read_inode(ino, &child);
                printk("inode num: %u  link count: %u  size: %uB  %s%s\n",
                       ino, child.links, child.size,
                       name_copy, type == FS_TYPE_DIR ? "/" : "");
            } else {
                printk("%s%s   ", ent->name, ent->type == FS_TYPE_DIR ? "/" : "");
            }
        }
    }
    if (option != 1) {
        printk("\n");
    }
    return 0;
}

// ----------------------------------------------------------
// Task2 placeholders (left unimplemented)
// ----------------------------------------------------------
int do_open(char *path, int mode)
{
    ensure_mounted();
    if (!fs_mounted || !path || !path[0]) {
        return -1;
    }

    char leaf[FS_MAX_NAME_LEN];
    uint32_t parent;
    if (resolve_parent(path, &parent, leaf) != 0 || leaf[0] == '\0') {
        printk("[FS] open: invalid path\n");
        return -1;
    }
    if (strlen(leaf) >= FS_MAX_NAME_LEN) {
        printk("[FS] open: name too long\n");
        return -1;
    }

    dentry_t ent;
    int exists = (dir_lookup(parent, leaf, &ent, NULL, NULL) == 0);
    uint32_t ino;

    if (exists) {
        if (ent.type != FS_TYPE_FILE) {
            printk("[FS] open: not a file\n");
            return -1;
        }
        ino = ent.ino;
    } else {
        // Create new file
        if (mode == O_RDONLY) {
            printk("[FS] open: file not found\n");
            return -1;
        }
        if (alloc_inode(&ino) != 0) {
            printk("[FS] open: no inode\n");
            return -1;
        }
        inode_t node;
        memset(&node, 0, sizeof(node));
        node.ino = ino;
        node.type = FS_TYPE_FILE;
        node.links = 1;
        node.size = 0;
        write_inode(ino, &node);

        if (dir_add_entry(parent, leaf, ino, FS_TYPE_FILE) != 0) {
            free_inode(ino);
            printk("[FS] open: dentry add failed\n");
            return -1;
        }
    }

    int fd = alloc_fd();
    if (fd < 0) {
        printk("[FS] open: fd table full\n");
        return -1;
    }
    fdesc_array[fd].ino = ino;
    fdesc_array[fd].mode = mode;
    fdesc_array[fd].offset = 0;
    return fd;
}

int do_read(int fd, char *buff, int length)
{
    ensure_mounted();
    if (!fs_mounted || !validate_fd(fd) || !buff || length < 0) {
        return -1;
    }
    fdesc_t *f = &fdesc_array[fd];
    if (f->mode == O_WRONLY) {
        return -1;
    }

    inode_t node;
    read_inode(f->ino, &node);
    if (node.type != FS_TYPE_FILE) {
        return -1;
    }

    uint32_t pos = f->offset;
    uint32_t remaining = (node.size > pos && length > 0) ? (node.size - pos) : 0;
    if ((uint32_t)length < remaining) remaining = length;

    int read_total = 0;
    while (remaining > 0) {
        uint32_t blk_idx = pos / sb.block_size;
        uint32_t blk_off = pos % sb.block_size;
        uint32_t phys;
        int rc = get_data_block(&node, blk_idx, 0, &phys);
        uint32_t chunk = sb.block_size - blk_off;
        if (chunk > remaining) chunk = remaining;

        if (rc != 0 || phys == 0) {
            memset(buff + read_total, 0, chunk);
        } else {
            disk_read_block(phys, block_cache2);
            memcpy(buff + read_total, block_cache2 + blk_off, chunk);
        }
        pos += chunk;
        read_total += (int)chunk;
        remaining -= chunk;
    }

    f->offset = pos;
    return read_total;
}

int do_write(int fd, char *buff, int length)
{
    ensure_mounted();
    if (!fs_mounted || !validate_fd(fd) || !buff || length < 0) {
        return -1;
    }
    fdesc_t *f = &fdesc_array[fd];
    if (f->mode == O_RDONLY) {
        return -1;
    }

    inode_t node;
    read_inode(f->ino, &node);
    if (node.type != FS_TYPE_FILE) {
        return -1;
    }

    uint32_t pos = f->offset;
    int written = 0;
    while (written < length) {
        uint32_t blk_idx = pos / sb.block_size;
        uint32_t blk_off = pos % sb.block_size;
        uint32_t phys;
        if (get_data_block(&node, blk_idx, 1, &phys) != 0 || phys == 0) {
            break;
        }
        disk_read_block(phys, block_cache2);
        uint32_t chunk = sb.block_size - blk_off;
        int remain = length - written;
        if (chunk > (uint32_t)remain) chunk = (uint32_t)remain;
        memcpy(block_cache2 + blk_off, buff + written, chunk);
        disk_write_block(phys, block_cache2);

        pos += chunk;
        written += (int)chunk;
    }

    if (pos > node.size) {
        node.size = pos;
        write_inode(node.ino, &node);
    } else {
        write_inode(node.ino, &node);
    }
    f->offset = pos;
    return written;
}

int do_close(int fd)
{
    if (!validate_fd(fd)) {
        return -1;
    }
    fdesc_array[fd].used = 0;
    return 0;
}

int do_ln(char *src_path, char *dst_path)
{
    ensure_mounted();
    if (!fs_mounted || !src_path || !dst_path) {
        return -1;
    }

    uint32_t src_ino;
    if (resolve_path(src_path, &src_ino) != 0) {
        printk("[FS] ln: source not found\n");
        return -1;
    }
    inode_t src;
    read_inode(src_ino, &src);
    if (src.type != FS_TYPE_FILE) {
        printk("[FS] ln: cannot link directory\n");
        return -1;
    }

    char leaf[FS_MAX_NAME_LEN];
    uint32_t parent;
    if (resolve_parent(dst_path, &parent, leaf) != 0 || leaf[0] == '\0') {
        printk("[FS] ln: invalid destination\n");
        return -1;
    }
    if (strlen(leaf) >= FS_MAX_NAME_LEN) {
        printk("[FS] ln: name too long\n");
        return -1;
    }
    if (dir_lookup(parent, leaf, NULL, NULL, NULL) == 0) {
        printk("[FS] ln: destination exists\n");
        return -1;
    }

    if (dir_add_entry(parent, leaf, src_ino, FS_TYPE_FILE) != 0) {
        return -1;
    }
    src.links++;
    write_inode(src_ino, &src);
    return 0;
}

int do_rm(char *path)
{
    ensure_mounted();
    if (!fs_mounted || !path) {
        return -1;
    }

    char leaf[FS_MAX_NAME_LEN];
    uint32_t parent;
    if (resolve_parent(path, &parent, leaf) != 0 || leaf[0] == '\0') {
        printk("[FS] rm: invalid path\n");
        return -1;
    }
    if (strcmp(leaf, ".") == 0 || strcmp(leaf, "..") == 0) {
        printk("[FS] rm: cannot remove . or ..\n");
        return -1;
    }

    dentry_t ent;
    uint32_t blk_idx, ent_idx;
    if (dir_lookup(parent, leaf, &ent, &blk_idx, &ent_idx) != 0) {
        printk("[FS] rm: not found\n");
        return -1;
    }
    if (ent.type != FS_TYPE_FILE) {
        printk("[FS] rm: not a file\n");
        return -1;
    }

    inode_t inode;
    read_inode(ent.ino, &inode);

    inode_t dir;
    read_inode(parent, &dir);
    uint32_t phys;
    if (get_data_block(&dir, blk_idx, 0, &phys) != 0 || phys == 0) {
        return -1;
    }
    disk_read_block(phys, block_cache);
    dentry_t *slot = (dentry_t *)(block_cache + ent_idx * sizeof(dentry_t));
    slot->valid = 0;
    disk_write_block(phys, block_cache);

    if (inode.links > 0) {
        inode.links--;
        if (inode.links == 0) {
            free_data_blocks(&inode);
            free_inode(inode.ino);
        } else {
            write_inode(inode.ino, &inode);
        }
    }
    return 0;
}

int do_lseek(int fd, int offset, int whence)
{
    if (!validate_fd(fd)) {
        return -1;
    }
    fdesc_t *f = &fdesc_array[fd];
    inode_t node;
    read_inode(f->ino, &node);
    if (node.type != FS_TYPE_FILE) {
        return -1;
    }

    int64_t new_off = 0;
    if (whence == SEEK_SET) {
        new_off = offset;
    } else if (whence == SEEK_CUR) {
        new_off = (int64_t)f->offset + offset;
    } else if (whence == SEEK_END) {
        new_off = (int64_t)node.size + offset;
    } else {
        return -1;
    }

    if (new_off < 0) new_off = 0;
    f->offset = (uint32_t)new_off;
    return (int)f->offset;
}
