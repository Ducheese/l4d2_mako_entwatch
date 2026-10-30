#if defined _l4d2_mako_entwatch_hud_included
 #endinput
#endif
#define _l4d2_mako_entwatch_hud_included

// ── EMS HUD 渲染引擎（VScript 后端，基于 smHud.inc 与 HUDSetLayout 架构）──────
// 参照 L4d2-MoreInfo 核心渲染机制，彻底解决 Dedicated Server 非突变模式下的 HUD 频闪
// 频道严格错开：
//   - 0..6: L4d2-MoreInfo (Boss / Messages / Ranking)
//   - 7   : hitsound 伤害统计 (HUD_FAR_LEFT)
//   - 8   : 魔晶石上半区 (HUD_FAR_RIGHT) -> 火/冰/奶/电
#if !defined _smHud_included
 #include "l4d2_mako_entwatch/smHud.inc"
#endif

// slot A：前 4 个神器（火/冰/奶/电），右侧上半
#define MAKO_SLOT_A     HUD_FAR_RIGHT   // 8
#define MAKO_X          0.72
#define MAKO_Y_A        0.50
#define MAKO_W          0.26
#define MAKO_H          0.22

// slot B：后 4 个神器（盾/速/黑洞/终极），紧接 A 下方（微调避开 Slot A 文本下沿）
#define MAKO_SLOT_B     HUD_MID_BOX     // 9，重定位到右侧下半
#define MAKO_Y_B        0.62

// flags = HUD_FLAG_ALIGN_LEFT(1<<8) | HUD_FLAG_NOBG(1<<6) | HUD_FLAG_TEXT(1<<13)
#define MAKO_FLAGS      (HUD_FLAG_ALIGN_LEFT | HUD_FLAG_NOBG | HUD_FLAG_TEXT)

static char g_sSlotText[15][128];
static bool g_bSlotOwned[15];
static bool g_bHudDirty;
static float g_flHudSent[15];
static float g_flNextWatch;

// ─────────────────────────────────────────────────────────────────────────────
// ScriptEscape：转义为 Squirrel 字符串字面量，防止玩家名中包含 " 或 \ 导致 VScript 解析崩溃
void ScriptEscape(const char[] text, char[] out, int size)
{
    int n = 0;
    for (int i = 0; text[i] && n < size - 3; i++)
    {
        int c = text[i];
        if (c == '\r') continue;
        if (c == '\n') { out[n++] = '\\'; out[n++] = 'n'; continue; }
        if (c == '\\' || c == '"') out[n++] = '\\';
        out[n++] = c;
    }
    out[n] = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// ScriptSlot：将指定槽位安全挂载到全局 ::MyHUD 根表中（与外部插件共享 table，不粗暴覆盖）
void ScriptSlot(int slot, bool visible, const char[] text, float x, float y, float w, float h)
{
    if (!g_bHudDirty)
    {
        // ::MyHUD 位于 Squirrel 全局根表中；若不存在则创建，若已存在则直接共用，绝不清空外部插件槽位
        L4D2_RunScript("if (!(\"MyHUD\" in getroottable())) ::MyHUD <- { Fields = {} };");
        HUD.isInit = true;
        HUD.tableCreated = true;
        g_bHudDirty = true;
    }

    char escaped[260];
    if (visible)
    {
        ScriptEscape(text, escaped, sizeof(escaped));
        HUD.AddNewHUD_asTEXT(slot, escaped, MAKO_FLAGS);
        HUD.Place(slot, w, h, x, y);
    }
    else
    {
        HUD.AddNewHUD_asTEXT(slot, "", HUD_FLAG_NOTVISIBLE);
    }
    g_flHudSent[slot] = GetGameTime();
}

// ─────────────────────────────────────────────────────────────────────────────
// FlushScriptHud：原子应用 HUD 布局并强制保持 m_bChallengeModeActive 开启
void FlushScriptHud()
{
    if (!g_bHudDirty)
        return;

    g_bHudDirty = false;
    GameRules_SetProp("m_bChallengeModeActive", 1, 1, 0, true);
    HUD.Draw();
}

// ─────────────────────────────────────────────────────────────────────────────
// ScriptWatch：回读网络属性，检测是否被外部地图脚本/其他插件意外覆盖，被踩踏时自动修复
void ScriptWatch()
{
    float now = GetGameTime();
    bool any = false;
    int slots[2];
    slots[0] = MAKO_SLOT_A;
    slots[1] = MAKO_SLOT_B;

    for (int i = 0; i < 2; i++)
    {
        int s = slots[i];
        if (!g_bSlotOwned[s])
            continue;

        any = true;
        // HUDSetLayout 异步生效，至少等待 0.5s 后再比对
        if (now - g_flHudSent[s] < 0.5)
            continue;

        char actual[128];
        GameRules_GetPropString("m_szScriptedHUDStringSet", actual, sizeof(actual), s);
        if (!StrEqual(actual, g_sSlotText[s]))
        {
            ScriptSlot(s, true, g_sSlotText[s], MAKO_X, (s == MAKO_SLOT_A ? MAKO_Y_A : MAKO_Y_B), MAKO_W, MAKO_H);
            g_bHudDirty = true;
        }
    }

    if (any && !GameRules_GetProp("m_bChallengeModeActive", 1))
    {
        GameRules_SetProp("m_bChallengeModeActive", 1, 1, 0, true);
        g_bHudDirty = true;
    }

    FlushScriptHud();
}

// ─────────────────────────────────────────────────────────────────────────────
// PutHudSlot：槽位写入入口（核心防抖：文本完全相同时绝不重复刷新，杜绝重排频闪）
void PutHudSlot(int slot, float y, const char[] text)
{
    if (!text[0])
    {
        if (g_bSlotOwned[slot])
        {
            ScriptSlot(slot, false, "", MAKO_X, y, MAKO_W, MAKO_H);
            g_bSlotOwned[slot] = false;
            g_sSlotText[slot][0] = '\0';
            g_bHudDirty = true;
        }
        return;
    }

    char safe[128];
    strcopy(safe, sizeof(safe), text);
    SafeTruncateUtf8(safe, 127);

    // 关键防抖：文本内容未发生改变时直接跳过，零网络包、零重新排版！
    if (g_bSlotOwned[slot] && StrEqual(safe, g_sSlotText[slot]))
        return;

    strcopy(g_sSlotText[slot], sizeof(g_sSlotText[]), safe);
    ScriptSlot(slot, true, safe, MAKO_X, y, MAKO_W, MAKO_H);
    g_bSlotOwned[slot] = true;
    g_bHudDirty = true;
}

void MakoHUD_Enable()
{
    GameRules_SetProp("m_bChallengeModeActive", 1, 1, 0, true);
    g_bSlotOwned[MAKO_SLOT_A] = false;
    g_bSlotOwned[MAKO_SLOT_B] = false;
    g_sSlotText[MAKO_SLOT_A][0] = '\0';
    g_sSlotText[MAKO_SLOT_B][0] = '\0';
    g_bHudDirty = false;
}

void MakoHUD_Clear()
{
    PutHudSlot(MAKO_SLOT_A, MAKO_Y_A, "");
    PutHudSlot(MAKO_SLOT_B, MAKO_Y_B, "");
    FlushScriptHud();
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
// Timer_Hud：0.1 秒间隔轮询状态并推送双 slot
// 采用 PutHudSlot 文本比对防抖，只有在 CD 跳变或人员变动时才提交 VScript 布局更新
public Action Timer_Hud(Handle timer)
{
    if (!g_bSupportedMap)
        return Plugin_Continue;

    GameRules_SetProp("m_bChallengeModeActive", 1, 1, 0, true);

    char textA[128], textB[128];
    textA[0] = '\0';
    textB[0] = '\0';
    float now = GetGameTime();

    UpdateThunder(now);

    for (int item = 0; item < ITEM_COUNT; item++)
    {
        int userid = g_iHolder[item];
        if (userid == 0)
            continue;

        char clientName[32];
        if (!g_bHudTestActive)
        {
            int client = GetClientOfUserId(userid);
            if (client <= 0 || !IsClientInGame(client) || GetClientTeam(client) != 2)
            {
                g_iHolder[item] = 0;
                continue;
            }
            GetClientName(client, clientName, sizeof(clientName));
        }
        else
        {
            strcopy(clientName, sizeof(clientName), g_sHudTestNames[item]);
        }
        SafeTruncateUtf8(clientName, 12);

        char line[64], state[32];
        ItemState(item, now, state, sizeof(state));

        FormatEx(line, sizeof(line), "%s[%s] %s\n", g_sShort[item], state, clientName);

        // 前 4 个神器（0-3）→ slot A；后 4 个（4-7）→ slot B
        if (item < 4)
        {
            if (strlen(textA) + strlen(line) < sizeof(textA))
                StrCat(textA, sizeof(textA), line);
        }
        else
        {
            if (strlen(textB) + strlen(line) < sizeof(textB))
                StrCat(textB, sizeof(textB), line);
        }
    }

    PutHudSlot(MAKO_SLOT_A, MAKO_Y_A, textA);
    PutHudSlot(MAKO_SLOT_B, MAKO_Y_B, textB);
    FlushScriptHud();

    if (now >= g_flNextWatch)
    {
        g_flNextWatch = now + 0.5;
        ScriptWatch();
    }

    return Plugin_Continue;
}
