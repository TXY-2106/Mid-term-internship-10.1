/*-----------------------------------------------------------------------*/
/*  FatFs 底层磁盘接口（本项目实现）                                     */
/*                                                                       */
/*  上游 FatFs R0.16 标准 diskio.c 模板的裁剪版：                        */
/*  - 只支持 1 个驱动器（pdrv 恒为 0），即 SPI1 上的 SD 卡               */
/*  - 扇区固定 512 字节                                                  */
/*  - 实际 SPI 时序在 BSP/bsp_sd_disk.c 里，这里只做转发与状态维护        */
/*-----------------------------------------------------------------------*/

#include "ff.h"        /* 先包含 ff.h：diskio.h 用到的 BYTE/LBA_t 由它定义 */
#include "diskio.h"
#include "bsp_sd_disk.h"

#define SD_DRIVE  0u

static DSTATUS s_stat = STA_NOINIT;

/*-----------------------------------------------------------------------*/
/* 初始化驱动器                                                          */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != SD_DRIVE) { return STA_NOINIT; }

    if (bsp_sd_disk_init()) {
        s_stat = 0u;
    } else {
        s_stat = STA_NOINIT;
    }
    return s_stat;
}

/*-----------------------------------------------------------------------*/
/* 取驱动器状态                                                          */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != SD_DRIVE) { return STA_NOINIT; }
    if (!bsp_sd_disk_ready()) { return s_stat | STA_NOINIT; }
    return s_stat;
}

/*-----------------------------------------------------------------------*/
/* 读扇区（count 恒为 1..n，逐个扇区读）                                 */
/*-----------------------------------------------------------------------*/
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    UINT i;

    if (pdrv != SD_DRIVE || (s_stat & STA_NOINIT)) { return RES_NOTRDY; }
    if (count == 0u) { return RES_PARERR; }

    for (i = 0u; i < count; i++) {
        if (!bsp_sd_disk_read(buff + (i * 512u), (uint32_t)(sector + i))) {
            return RES_ERROR;   /* 多扇区读到一半失败：FatFs 会重试整块 */
        }
    }
    return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* 写扇区                                                                */
/*-----------------------------------------------------------------------*/
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    UINT i;

    if (pdrv != SD_DRIVE || (s_stat & STA_NOINIT)) { return RES_NOTRDY; }
    if (count == 0u) { return RES_PARERR; }

    for (i = 0u; i < count; i++) {
        if (!bsp_sd_disk_write(buff + (i * 512u), (uint32_t)(sector + i))) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* 杂项控制                                                              */
/*-----------------------------------------------------------------------*/
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    DRESULT res = RES_ERROR;

    if (pdrv != SD_DRIVE || (s_stat & STA_NOINIT)) { return RES_NOTRDY; }

    switch (cmd) {
    case CTRL_SYNC:
        /* 本驱动的 disk_write 内部已经等到卡编程结束，无需额外刷缓存 */
        res = RES_OK;
        break;

    case GET_SECTOR_SIZE:
        *(WORD *)buff = 512u;
        res = RES_OK;
        break;

    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1u;       /* 擦除块大小未知 → 按 1 扇区处理 */
        res = RES_OK;
        break;

    default:
        res = RES_PARERR;
        break;
    }
    return res;
}
