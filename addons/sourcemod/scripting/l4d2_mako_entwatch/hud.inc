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
// （网络 prop 自身也是 128 字节，所以每次写入 < 128 字节即安全）
static void SlotWrite(int slot, float y, int idx, const char[] text)
{
    if (!g_bHudPlaced[idx])
    {
        HUDPlace(slot, MAKO_X, y, MAKO_W, MAKO_H);
        g_bHudPlaced[idx] = true;
    }
    GameRules_SetProp("m_iScriptedHUDFlags", MAKO_FLAGS, _, slot, true);
    GameRules_SetPropString("m_szScriptedHUDStringSet", text, true, slot);
}

static void SlotClear(int slot, int idx)
{
    GameRules_SetProp("m_iScriptedHUDFlags", HUD_FLAG_NOTVISIBLE, _, slot, true);
    GameRules_SetPropString("m_szScriptedHUDStringSet", "", true, slot);
    g_bHudPlaced[idx] = false;
}

void MakoHUD_Enable()
{
    EnableHUD();
    g_bHudPlaced[0] = false;
    g_bHudPlaced[1] = false;
}

void MakoHUD_Clear()
{
    SlotClear(MAKO_SLOT_A, 0);
    SlotClear(MAKO_SLOT_B, 1);
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
// 单 slot 每行约 22 字节，4 行 ≈ 88 字节，远低于 128 字节上限
public Action Timer_Hud(Handle timer)
{
    if (!g_bSupportedMap)
        return Plugin_Continue;

    // 每个 slot 独立 buffer（< 128 字节安全区间）
    char textA[120], textB[120];
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
        if (client <= 0 || !IsClientInGame(client))
        {
            g_iHolder[item] = 0;
            continue;
        }

        char line[64], state[32], clientName[24];
        ItemState(item, now, state, sizeof(state));
        GetClientName(client, clientName, sizeof(clientName));

        FormatEx(line, sizeof(line), "%s [%s]: %s\n", g_sShort[item], state, clientName);

        // 前 4 个神器（0-3）→ slot A；后 4 个（4-7）→ slot B
        if (item < 4)
        {
            if (strlen(textA) + strlen(line) < sizeof(textA) - 1)
            {
                StrCat(textA, sizeof(textA), line);
                hasA = true;
            }
        }
        else
        {
            if (strlen(textB) + strlen(line) < sizeof(textB) - 1)
            {
                StrCat(textB, sizeof(textB), line);
                hasB = true;
            }
        }
    }

    if (hasA)
        SlotWrite(MAKO_SLOT_A, MAKO_Y_A, 0, textA);
    else
        SlotClear(MAKO_SLOT_A, 0);

    if (hasB)
        SlotWrite(MAKO_SLOT_B, MAKO_Y_B, 1, textB);
    else
        SlotClear(MAKO_SLOT_B, 1);

    return Plugin_Continue;
}
