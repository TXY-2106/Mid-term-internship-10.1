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
        s.pred_occ   = 9u;
        s.pred_n     = 4u;
        s.beams      = 5u;   /* 入口A + 队列50% 同时遮挡 */

        int n = qsm_frame_encode(&s, 1024u, "2026-10-28T12:31:04", buf, sizeof(buf));
        CHECK(n > 0);
        CHECK(buf[0] == '$');
        const char *expect = "$QSM,1024,3735000,2026-10-28T12:31:04,37,29,8,6100,0,2,3,9,4,5*";
        CHECK(strncmp(buf, expect, strlen(expect)) == 0);

        /* CRC 必须是两位大写十六进制 */
        {
            const char *star = strchr(buf, '*');
            char hex[8];
            uint8_t c = qsm_crc8((const uint8_t *)&buf[1], (size_t)(star - buf) - 1u);
            (void)snprintf(hex, sizeof(hex), "%02X", (unsigned)c);
            CHECK(star != NULL);
            CHECK(strncmp(star + 1, hex, 2u) == 0);
            CHECK(star[3] == '\n');
        }

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
        CHECK_EQ(r.pred_occ, 9u);
        CHECK_EQ(r.pred_n, 4u);
        CHECK_EQ(r.beams, 5u);
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
        s.beams = 0u;

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
        s.pred_occ = 0u; s.pred_n = 0u; s.beams = 0u;
        CHECK(qsm_frame_encode(&s, 1u, NULL, buf, sizeof(buf)) > 0);
        CHECK(strstr(buf, ",-,") != NULL);
    }

    /* 6) 老格式（11 个字段、没有 pred）仍能解析，预测字段按 0 处理 */
    {
        qsm_state_t r;
        char buf[128];
        char dt_out[32];
        uint32_t seq_out = 0u;
        const char body[] = "QSM,1024,3735000,2026-10-28T12:31:04,37,29,8,6100,0,2,3";
        uint8_t c = qsm_crc8((const uint8_t *)body, strlen(body));

        (void)snprintf(buf, sizeof(buf), "$%s*%02X\n", body, (unsigned)c);
        CHECK_EQ(qsm_frame_parse(buf, &r, &seq_out, dt_out, sizeof(dt_out)), 0);
        CHECK_EQ(seq_out, 1024u);
        CHECK_EQ(r.flags, (QSM_FLAG_RTC_OK | QSM_FLAG_SD_OK));
        CHECK_EQ(r.pred_occ, 0u);
        CHECK_EQ(r.pred_n, 0u);
        CHECK_EQ(r.beams, 0u);
    }

    /* 7) 探头位图字段：合法值解析、越界值忽略 */
    {
        qsm_state_t r;
        char buf[128];
        char dt_out[32];
        uint32_t seq_out = 0u;
        const char body15[] = "QSM,1024,3735000,2026-10-28T12:31:04,37,29,8,6100,0,2,3,9,4,15";
        uint8_t c15 = qsm_crc8((const uint8_t *)body15, strlen(body15));
        (void)snprintf(buf, sizeof(buf), "$%s*%02X\n", body15, (unsigned)c15);
        CHECK_EQ(qsm_frame_parse(buf, &r, &seq_out, dt_out, sizeof(dt_out)), 0);
        CHECK_EQ(r.beams, 15u);
        CHECK_EQ(r.pred_occ, 9u);
        CHECK_EQ(r.pred_n, 4u);

        const char body16[] = "QSM,1024,3735000,2026-10-28T12:31:04,37,29,8,6100,0,2,3,9,4,16";
        uint8_t c16 = qsm_crc8((const uint8_t *)body16, strlen(body16));
        (void)snprintf(buf, sizeof(buf), "$%s*%02X\n", body16, (unsigned)c16);
        CHECK_EQ(qsm_frame_parse(buf, &r, &seq_out, dt_out, sizeof(dt_out)), 0);
        CHECK_EQ(r.beams, 0u);
    }

    /* 8) 下行命令帧：编码、往返、CRC、非法命令、老帧不误判 */
    {
        char buf[32];
        uint8_t cmd = 0u;

        int n = qsm_cmd_encode(QSM_CMD_RESET_COUNTS, buf, sizeof(buf));
        CHECK_EQ(n, 10);
        CHECK(strcmp(buf, "$QSR,1*69\n") == 0);
        CHECK_EQ(qsm_cmd_parse(buf, (size_t)n, &cmd), 0);
        CHECK_EQ(cmd, QSM_CMD_RESET_COUNTS);

        n = qsm_cmd_encode(QSM_CMD_RESET_HISTORY, buf, sizeof(buf));
        CHECK(strcmp(buf, "$QSR,2*60\n") == 0);
        CHECK_EQ(qsm_cmd_parse(buf, (size_t)n, &cmd), 0);
        CHECK_EQ(cmd, QSM_CMD_RESET_HISTORY);

        n = qsm_cmd_encode(QSM_CMD_RESET_ALL, buf, sizeof(buf));
        CHECK(strcmp(buf, "$QSR,3*67\n") == 0);
        CHECK_EQ(qsm_cmd_parse(buf, (size_t)n, &cmd), 0);
        CHECK_EQ(cmd, QSM_CMD_RESET_ALL);

        /* 不带结尾换行也能解析 */
        CHECK_EQ(qsm_cmd_parse("$QSR,1*69", 10u, &cmd), 0);
        CHECK_EQ(cmd, QSM_CMD_RESET_COUNTS);

        /* CRC 错 -> -2 */
        CHECK_EQ(qsm_cmd_parse("$QSR,1*6A\n", 10u, &cmd), -2);
        /* 命令号越界 -> -1（CRC 必须正确，否则先报 -2） */
        {
            char bad[32];
            const char b4[] = "QSR,4";
            const char b0[] = "QSR,0";
            uint8_t c4 = qsm_crc8((const uint8_t *)b4, strlen(b4));
            uint8_t c0 = qsm_crc8((const uint8_t *)b0, strlen(b0));
            (void)snprintf(bad, sizeof(bad), "$%s*%02X\n", b4, (unsigned)c4);
            CHECK_EQ(qsm_cmd_parse(bad, strlen(bad), &cmd), -1);
            (void)snprintf(bad, sizeof(bad), "$%s*%02X\n", b0, (unsigned)c0);
            CHECK_EQ(qsm_cmd_parse(bad, strlen(bad), &cmd), -1);
        }
        /* 上行帧不会被当成命令 */
        {
            char up[128];
            char dt_out2[32];
            uint32_t seq2 = 0u;
            qsm_state_t st;
            const char body[] = "QSM,1,2,2026-10-28T12:31:04,1,0,1,1,0,1,3";
            uint8_t c = qsm_crc8((const uint8_t *)body, strlen(body));
            (void)snprintf(up, sizeof(up), "$%s*%02X\n", body, (unsigned)c);
            CHECK_EQ(qsm_frame_parse(up, &st, &seq2, dt_out2, sizeof(dt_out2)), 0);
            CHECK_EQ(qsm_cmd_parse(up, strlen(up), &cmd), -1);
        }
        /* 缓冲区太小 -> -1 */
        CHECK_EQ(qsm_cmd_encode(QSM_CMD_RESET_ALL, buf, 8u), -1);
        /* NULL 参数 */
        CHECK_EQ(qsm_cmd_parse(NULL, 10u, &cmd), -1);
        CHECK_EQ(qsm_cmd_parse("$QSR,1*69\n", 10u, NULL), -1);
    }
}
