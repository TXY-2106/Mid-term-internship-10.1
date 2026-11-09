/*-----------------------------------------------------------------------*/
/*  FatFs 配置文件（本项目裁剪版）                                        */
/*                                                                       */
/*  上游：ChaN FatFs R0.16 w/patch2（abbrev/fatfs 镜像）                  */
/*  裁剪原则：F103C8T6 只有 64 KB Flash / 20 KB RAM，只保留本工程用到的   */
/*            功能：挂载、开文件、追加写、读文件、f_gets 逐行读。          */
/*                                                                       */
/*  本项目实际文件名一律 8.3 短名（如 H261109.CSV），因此关闭长文件名。   */
/*-----------------------------------------------------------------------*/

#ifndef FFCONF_DEF
#define FFCONF_DEF 80386   /* 必须与 ff.h 中 FF_DEFINED 一致（R0.16 p2） */
#endif

/*-----------------------------------------------------------------------*/
/* 功能开关                                                              */
/*-----------------------------------------------------------------------*/

/* 只读模式：0 = 可读可写（本工程要写 CSV） */
#define FF_FS_READONLY  0

/* f_lseek 裁剪级别：0 = 完整 f_lseek（保留，便于按行回读/定位） */
#define FF_FS_MINIMIZE  0

/* f_findfirst / f_findnext：本工程不需要遍历目录 */
#define FF_USE_FIND     0

/* f_mkfs：卡在电脑上格式化成 FAT32，单片机不格式化 */
#define FF_USE_MKFS     0

/* 快速定位（给 f_lseek 用 FAT 缓存）：本工程按顺序读写，不需要 */
#define FF_USE_FASTSEEK 0

/* f_expand：按需扩展文件大小，不需要 */
#define FF_USE_EXPAND   0

/* f_chmod：不需要 */
#define FF_USE_CHMOD    0

/* f_setlabel / f_getlabel：不需要 */
#define FF_USE_LABEL    0

/* f_forward：不需要 */
#define FF_USE_FORWARD  0

/* f_printf / f_puts / f_putc / f_gets：1 = 打开。
 * 本工程用 f_gets() 逐行回读历史文件，因此必须为 1；
 * 但 f_printf 的浮点与 64 位整型支持关闭，省 Flash。 */
#define FF_USE_STRFUNC  1
#define FF_PRINT_LLI    0
#define FF_PRINT_FLOAT  0
#define FF_STRF_ENCODE  0

/* 代码页：437（ASCII，CSV 里只用数字、冒号、逗号、字母） */
#define FF_CODE_PAGE    437

/*-----------------------------------------------------------------------*/
/* 文件名                                                                */
/*-----------------------------------------------------------------------*/

/* 长文件名：0 = 关闭（8.3 短名），省 Flash 与 RAM */
#define FF_USE_LFN      0
#define FF_MAX_LFN      255
#define FF_LFN_UNICODE  0
#define FF_LFN_BUF      255
#define FF_SFN_BUF      12

/* 相对路径 f_chdir/f_getcwd：不需要 */
#define FF_FS_RPATH     0
#define FF_PATH_DEPTH   1

/* exFAT：不需要（SD 卡用电脑格式化成 FAT32） */
#define FF_FS_EXFAT     0

/*-----------------------------------------------------------------------*/
/* 时间戳                                                                */
/*-----------------------------------------------------------------------*/

/* 1 = 不取系统时间。本项目不用文件时间戳：时间记录在 CSV 每行里，
 *     由 DS3231 提供。若为 1，新建文件的日期固定为下面的值。 */
#define FF_FS_NORTC     1
#define FF_NORTC_MON    1
#define FF_NORTC_MDAY   1
#define FF_NORTC_YEAR   2020

/* FSINFO 的剩余空间缓存：0 = 正常使用（高速） */
#define FF_FS_NOFSINFO  0

/* 文件锁与重入：单线程裸机，全部关闭 */
#define FF_FS_LOCK      0
#define FF_FS_REENTRANT 0
#define FF_FS_TIMEOUT   1000

/*-----------------------------------------------------------------------*/
/* 物理驱动器 / 扇区大小                                                 */
/*-----------------------------------------------------------------------*/

/* 卷数：只用 1 张 SD 卡，驱动器号 0 */
#define FF_VOLUMES      1

/* 用字符串作卷标号（如 "SD:"）：不需要 */
#define FF_STR_VOLUME_ID 0
#define FF_VOLUME_STRS   "RAM","NAND","CF","SD","USB","USB2","USB3"

/* 多分区：0 = 按 MBR 引导区解析（常规 SD 卡都是这样） */
#define FF_MULTI_PARTITION 0
#define FF_MIN_GPT      1

/* 扇区大小：SD 卡一律 512 字节 */
#define FF_MIN_SS       512
#define FF_MAX_SS       512
#define FF_LBA64        0

/* 每个文件自带 512 B 扇区缓冲（FF_FS_TINY 0）。
 * 本工程同时打开 status.csv / events.csv / HYYMMDD.CSV，约 1.5 KB RAM。 */
#define FF_FS_TINY      0

/* 分区未用簇回收（f_mkfs 相关），保留默认 */
#define FF_USE_TRIM     0
