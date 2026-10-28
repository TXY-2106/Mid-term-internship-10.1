/**
 * @file  data_logger.c
 * @brief CSV 行组装与写卡
 *
 * 行格式（列顺序与表头严格一致）：
 *   status.csv : datetime,uptime_ms,in,out,occ,q50_ms,q100_ms,grade,flags
 *   events.csv : datetime,uptime_ms,beam,level
 */
#include "data_logger.h"
#include "app_config.h"

#if APP_SDCARD_ENABLE
#include "bsp_sdcard.h"

#define CSV_MAX 96u

static uint8_t s_ok;

static char *put_u32(char *p, uint32_t v)
{
    char tmp[10];
    uint8_t n = 0u;

    do {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v != 0u && n < sizeof(tmp));

    while (n > 0u) {
        *p++ = tmp[--n];
    }
    return p;
}

static char *put_str(char *p, const char *s)
{
    if (s == 0) { s = "-"; }
    while (*s != '\0') {
        *p++ = *s++;
    }
    return p;
}

uint8_t logger_init(void)
{
    s_ok = bsp_sdcard_init();
    return s_ok;
}

uint8_t logger_ok(void)
{
    return s_ok;
}

void logger_status(const qsm_state_t *s, const char *dt)
{
    char line[CSV_MAX];
    char *p = line;

    if (!s_ok || s == 0) { return; }

    p = put_str(p, dt);
    *p++ = ',';
    p = put_u32(p, s->t_ms);   *p++ = ',';
    p = put_u32(p, s->count_in);  *p++ = ',';
    p = put_u32(p, s->count_out); *p++ = ',';
    p = put_u32(p, (uint32_t)(s->occupancy < 0 ? 0 : s->occupancy)); *p++ = ',';
    p = put_u32(p, s->q50_occ_ms);   *p++ = ',';
    p = put_u32(p, s->q100_occ_ms);  *p++ = ',';
    p = put_u32(p, s->grade);        *p++ = ',';
    p = put_u32(p, s->flags);
    *p = '\0';

    bsp_sdcard_append_status(line);
}

void logger_event(const qsm_edge_t *e, const char *dt)
{
    char line[CSV_MAX];
    char *p = line;

    if (!s_ok || e == 0) { return; }

    p = put_str(p, dt);
    *p++ = ',';
    p = put_u32(p, e->t_ms);  *p++ = ',';
    p = put_u32(p, e->beam);  *p++ = ',';
    p = put_u32(p, e->level);
    *p = '\0';

    bsp_sdcard_append_event(line);
}

void logger_sync(void)
{
    if (s_ok) {
        bsp_sdcard_sync();
    }
}

#else  /* APP_SDCARD_ENABLE == 0：日志功能关闭，其余流程不受影响 */

uint8_t logger_init(void) { return 0u; }
uint8_t logger_ok(void)   { return 0u; }
void logger_status(const qsm_state_t *s, const char *dt) { (void)s; (void)dt; }
void logger_event(const qsm_edge_t *e, const char *dt)   { (void)e; (void)dt; }
void logger_sync(void) { }

#endif /* APP_SDCARD_ENABLE */
