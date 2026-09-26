#if defined _l4d2_mako_entwatch_hud_included
 #endinput
#endif
#define _l4d2_mako_entwatch_hud_included

// ── EMS HUD 渲染引擎（双 slot，GameRules NetProps，零实体依赖）──────────────
// m_szScriptedHUDStringSet 单 slot 上限 128 字节，8 行中文昵称会溢出。
// 方案：前 4 个神器写 slot 8（HUD_FAR_RIGHT），后 4 个写 slot 9（HUD_MID_BOX 重定位到右下）
// hitsound 伤害 HUD 用 slot 7（HUD_FAR_LEFT），不冲突。
#include <l4d2_ems_hud>

// slot A：前 4 个神器（火/冰/奶/电），右侧上半
#define MAKO_SLOT_A     HUD_FAR_RIGHT   // 8
#define MAKO_X          0.72
#define MAKO_Y_A        0.50
#define MAKO_W          0.26
#define MAKO_H          0.22

// slot B：后 4 个神器（盾/速/黑洞/终极），紧接 A 下方
#define MAKO_SLOT_B     HUD_MID_BOX     // 9，重定位到右侧下半
#define MAKO_Y_B        0.60

// flags = HUD_FLAG_ALIGN_LEFT(1<<8) | HUD_FLAG_NOBG(1<<6) | HUD_FLAG_TEXT(1<<13)
#define MAKO_FLAGS      (HUD_FLAG_ALIGN_LEFT | HUD_FLAG_NOBG | HUD_FLAG_TEXT)

// 每个 slot 独立写，绕过 HUDSetLayout 的函数内部 128 字节局部 buffer
// 必须每次无条件调用 HUDPlace，确保任何时候进入游戏或换回合的客户端都能收到坐标覆盖，
// 杜绝客户端回退至 hudscriptedmode.res 默认的右上角 (540, 14, 65x21) 引发飞屏与截断。
static void SlotWrite(int slot, float y, const char[] text)
{
    HUDPlace(slot, MAKO_X, y, MAKO_W, MAKO_H);
    GameRules_SetProp("m_iScriptedHUDFlags", MAKO_FLAGS, _, slot, true);
    GameRules_SetPropString("m_szScriptedHUDStringSet", text, true, slot);
}

static void SlotClear(int slot)
{
    GameRules_SetProp("m_iScriptedHUDFlags", HUD_FLAG_NOTVISIBLE, _, slot, true);
    GameRules_SetPropString("m_szScriptedHUDStringSet", "", true, slot);
}

void MakoHUD_Enable()
{
    EnableHUD();
}

void MakoHUD_Clear()
{
    SlotClear(MAKO_SLOT_A);
    SlotClear(MAKO_SLOT_B);
}

// ─────────────────────────────────────────────────────────────────────────────
// SafeTruncateUtf8：安全截断 UTF-8 字符串到指定字节数，避免破坏多字节序列造成中文乱码
stock void SafeTruncateUtf8(char[] str, int maxBytes)
{
    int len = strlen(str);
    if (len <= maxBytes)
        return;

    int i = 0;
    while (i < len)
    {
        int c = str[i] & 0xFF;
        int charLen = 1;
        if ((c & 0x80) == 0x00)
            charLen = 1;
        else if ((c & 0xE0) == 0xC0)
            charLen = 2;
        else if ((c & 0xF0) == 0xE0)
            charLen = 3;
        else if ((c & 0xF8) == 0xF0)
            charLen = 4;
        else
            charLen = 1;

        if (i + charLen > maxBytes)
        {
            str[i] = '\0';
            return;
        }
        i += charLen;
    }

    str[maxBytes] = '\0';
}

// ─────────────────────────────────────────────────────────────────────────────
// ItemState：生成单条神器状态字符串
void ItemState(int item, float now, char[] state, int maxlen)
{
    if (item == Item_Ultima)
    {
        strcopy(state, maxlen, g_bUltimaUsed ? "E" : "R");
        return;
    }

    if (item == Item_Thunder)
    {
        int counter = EntRefToEntIndex(g_iThunderCounter);
        int current, maximum;
        if (counter != INVALID_ENT_REFERENCE && IsValidEntity(counter) && ReadThunder(counter, current, maximum))
        {
            if (current < maximum && g_flThunderReadyAt > now)
                FormatEx(state, maxlen, "%d/%d|%d", current, maximum, RoundToCeil(g_flThunderReadyAt - now));
            else
                FormatEx(state, maxlen, "%d/%d", current, maximum);
        }
        else
        {
            strcopy(state, maxlen, "N/A");
        }
        return;
    }

    float left = g_flReadyAt[item] - now;
    FormatEx(state, maxlen, left > 0.0 ? "%d" : "R", RoundToCeil(left));
}

// ─────────────────────────────────────────────────────────────────────────────
// Timer_Hud：每秒拆分前 4 / 后 4 推送到两个 slot
// 单 slot 每行最大不超过 30 字节，4 行 ≤ 112 字节，安全收敛在 128 字节物理上限内
public Action Timer_Hud(Handle timer)
{
    if (!g_bSupportedMap)
        return Plugin_Continue;

    // 每个 slot 独立 buffer（最大 128 字节物理上限）
    char textA[128], textB[128];
    textA[0] = '\0';
    textB[0] = '\0';
    bool hasA = false, hasB = false;
    float now = GetGameTime();

    UpdateThunder(now);

    for (int item = 0; item < ITEM_COUNT; item++)
    {
        int userid = g_iHolder[item];
        if (userid == 0)
            continue;

        int client = GetClientOfUserId(userid);
        if (client <= 0 || !IsClientInGame(client) || GetClientTeam(client) != 2)
        {
            g_iHolder[item] = 0;
            continue;
        }

        char line[64], state[32], clientName[32];
        ItemState(item, now, state, sizeof(state));
        GetClientName(client, clientName, sizeof(clientName));
        SafeTruncateUtf8(clientName, 15);

        FormatEx(line, sizeof(line), "%s[%s] %s\n", g_sShort[item], state, clientName);

        // 前 4 个神器（0-3）→ slot A；后 4 个（4-7）→ slot B
        if (item < 4)
        {
            if (strlen(textA) + strlen(line) < sizeof(textA))
            {
                StrCat(textA, sizeof(textA), line);
                hasA = true;
            }
        }
        else
        {
            if (strlen(textB) + strlen(line) < sizeof(textB))
            {
                StrCat(textB, sizeof(textB), line);
                hasB = true;
            }
        }
    }

    if (hasA)
        SlotWrite(MAKO_SLOT_A, MAKO_Y_A, textA);
    else
        SlotClear(MAKO_SLOT_A);

    if (hasB)
        SlotWrite(MAKO_SLOT_B, MAKO_Y_B, textB);
    else
        SlotClear(MAKO_SLOT_B);

    return Plugin_Continue;
}
