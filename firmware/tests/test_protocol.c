/** @file test_protocol.c —— 帧编解码与 CRC */
#include <string.h>
#include "test_util.h"
#include "protocol.h"

void suite_protocol(void)
{
    /* 1) CRC-8/ATM 标准校验值：'123456789' → 0xF4 */
    {
        const uint8_t v[] = { '1','2','3','4','5','6','7','8','9' };
        CHECK_EQ(qsm_crc8(v, sizeof(v)), 0xF4);
    }

    /* 2) 编码 → 解析 往返一致 */
    {
        qsm_state_t s, r;
        char buf[128];
        char dt_out[32];
        uint32_t seq_out = 0u;

        s.t_ms       = 3735000u;
        s.count_in   = 37u;
        s.count_out  = 29u;
        s.occupancy  = 8;
        s.q50_occ_ms = 6100u;
        s.q100_occ_ms = 0u;
        s.grade      = QSM_GRADE_FULL;
        s.flags      = QSM_FLAG_RTC_OK | QSM_FLAG_SD_OK;

        int n = qsm_frame_encode(&s, 1024u, "2026-10-28T12:31:04", buf, sizeof(buf));
        CHECK(n > 0);
        CHECK(buf[0] == '$');
        const char *expect = "$QSM,1024,3735000,2026-10-28T12:31:04,37,29,8,6100,0,2,3*";
        CHECK(strncmp(buf, expect, strlen(expect)) == 0);

        CHECK_EQ(qsm_frame_parse(buf, &r, &seq_out, dt_out, sizeof(dt_out)), 0);
        CHECK_EQ(seq_out, 1024u);
        CHECK_EQ(r.t_ms, 3735000u);
        CHECK_EQ(r.count_in, 37u);
        CHECK_EQ(r.count_out, 29u);
        CHECK_EQ(r.occupancy, 8);
        CHECK_EQ(r.q50_occ_ms, 6100u);
        CHECK_EQ(r.q100_occ_ms, 0u);
        CHECK_EQ(r.grade, QSM_GRADE_FULL);
        CHECK_EQ(r.flags, (QSM_FLAG_RTC_OK | QSM_FLAG_SD_OK));
        CHECK(strcmp(dt_out, "2026-10-28T12:31:04") == 0);
    }

    /* 3) 篡改一个字符 → CRC 失败 */
    {
        qsm_state_t s, r;
        char buf[128];
        char dt_out[32];
        uint32_t seq_out = 0u;

        s.t_ms = 1000u; s.count_in = 1u; s.count_out = 0u; s.occupancy = 1;
        s.q50_occ_ms = 0u; s.q100_occ_ms = 0u;
        s.grade = QSM_GRADE_FEW; s.flags = 0u;

        CHECK(qsm_frame_encode(&s, 7u, "2026-10-28T00:00:00", buf, sizeof(buf)) > 0);
        buf[6] = (buf[6] == '9') ? '8' : '9';   /* 改坏负载中的一位 */
        CHECK_EQ(qsm_frame_parse(buf, &r, &seq_out, dt_out, sizeof(dt_out)), -2);
    }

    /* 4) 非帧数据被拒绝 */
    {
        qsm_state_t r;
        char dt_out[32];
        uint32_t seq_out = 0u;
        CHECK_EQ(qsm_frame_parse("hello world\n", &r, &seq_out, dt_out, sizeof(dt_out)), -1);
        CHECK_EQ(qsm_frame_parse("$QSM,1\n", &r, &seq_out, dt_out, sizeof(dt_out)), -1);
    }

    /* 5) RTC 未就绪时 dt 传 NULL → 写成 "-" */
    {
        qsm_state_t s;
        char buf[128];
        s.t_ms = 10u; s.count_in = 0u; s.count_out = 0u; s.occupancy = 0;
        s.q50_occ_ms = 0u; s.q100_occ_ms = 0u;
        s.grade = 0u; s.flags = 0u;
        CHECK(qsm_frame_encode(&s, 1u, NULL, buf, sizeof(buf)) > 0);
        CHECK(strstr(buf, ",-,") != NULL);
    }
}
