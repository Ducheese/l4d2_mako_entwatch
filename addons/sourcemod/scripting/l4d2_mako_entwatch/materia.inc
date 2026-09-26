#if defined _l4d2_mako_entwatch_materia_included
 #endinput
#endif
#define _l4d2_mako_entwatch_materia_included

// ── 施法与冷却计算 ─────────────────────────────────────
public void Output_ButtonTimeUp(const char[] output, int caller, int activator, float delay)
{
    if (!g_bSupportedMap)
        return;

    int item = ItemFromEntity(caller, false);
    if (item < 0)
        return;

    if (item == Item_Haste)
    {
        g_flReadyAt[item] = GetGameTime() + 120.0;
        PrintToServer("[MakoEntWatch] %s cast! (CD: 120s)", g_sShort[Item_Haste]);
    }
    else if (item == Item_Ultima)
    {
        g_bUltimaUsed = true;
        PrintToServer("[MakoEntWatch] %s cast! (Single-use consumed)", g_sShort[Item_Ultima]);
        // 保持 g_iHolder 记录，使 HUD 可显示 "[终极] [E]: 玩家名"
        // 地图 FreeUser() 允许该玩家再拾取新神器，届时由 Timer_CheckPickup 顶替
    }
}

public void Output_RelayTrigger(const char[] output, int caller, int activator, float delay)
{
    if (!g_bSupportedMap)
        return;

    int callerIndex = EntRefToEntIndex(caller);
    if (callerIndex == INVALID_ENT_REFERENCE || !IsValidEntity(callerIndex))
        return;

    char name[64];
    if (!EntityName(callerIndex, name, sizeof(name)))
        return;

    if (StrEqual(name, "M_Void_Logic", false))
    {
        g_flReadyAt[Item_Void] = GetGameTime() + 110.0;
        PrintToServer("[MakoEntWatch] %s cast! (CD: 110s)", g_sShort[Item_Void]);
        return;
    }

    if (StrEqual(name, "M_Thunder_Entimer", false))
    {
        g_flThunderReadyAt = GetGameTime() + 50.0;
        return;
    }

    int tier = -1;
    int item = ItemFromRelay(name, tier);
    if (item >= 0)
    {
        if (tier < 0 || tier > 2)
            tier = g_iLevel - 1;
        if (tier < 0) tier = 0;
        else if (tier > 2) tier = 2;

        float now = GetGameTime();
        if (g_iCooldown[item][tier] > 0)
            g_flReadyAt[item] = now + float(g_iCooldown[item][tier]);

        PrintToServer("[MakoEntWatch] %s cast! (Tier %d, CD: %ds)", g_sShort[item], tier + 1, g_iCooldown[item][tier]);
    }
}

// ── 雷电充能计时器 ───────────────────────────────────────
public void Output_Timer(const char[] output, int caller, int activator, float delay)
{
    if (!g_bSupportedMap)
        return;

    int callerIndex = EntRefToEntIndex(caller);
    if (callerIndex == INVALID_ENT_REFERENCE || !IsValidEntity(callerIndex))
        return;

    char name[64];
    if (!EntityName(callerIndex, name, sizeof(name)))
        return;

    if (StrEqual(name, "M_Thunder_Regeneration", false))
    {
        int current, maximum;
        int counter = EntRefToEntIndex(g_iThunderCounter);
        if (counter != INVALID_ENT_REFERENCE && IsValidEntity(counter)
            && ReadThunder(counter, current, maximum) && current < maximum)
        {
            g_flThunderReadyAt = GetGameTime() + 50.0;
        }
        else
        {
            g_flThunderReadyAt = 0.0;
        }
    }
}

// ── 等级判定（MateriaMasterCase）──────────────────────
public void Output_Level1(const char[] output, int caller, int activator, float delay)
{
    if (!g_bSupportedMap)
        return;

    char name[64];
    if (EntityName(caller, name, sizeof(name)) && StrEqual(name, "MateriaMasterCase", false))
    {
        g_iLevel = 1;
        PrintToServer("[MakoEntWatch] Materia level updated to 1");
    }
}

public void Output_Level2(const char[] output, int caller, int activator, float delay)
{
    if (!g_bSupportedMap)
        return;

    char name[64];
    if (EntityName(caller, name, sizeof(name)) && StrEqual(name, "MateriaMasterCase", false))
    {
        g_iLevel = 2;
        PrintToServer("[MakoEntWatch] Materia level updated to 2");
    }
}

public void Output_Level3(const char[] output, int caller, int activator, float delay)
{
    if (!g_bSupportedMap)
        return;

    char name[64];
    if (EntityName(caller, name, sizeof(name)) && StrEqual(name, "MateriaMasterCase", false))
    {
        g_iLevel = 3;
        PrintToServer("[MakoEntWatch] Materia level updated to 3");
    }
}

// ── 雷电 math_counter 充能 ─────────────────────────────
public void Output_ThunderCounter(const char[] output, int caller, int activator, float delay)
{
    if (!g_bSupportedMap)
        return;

    int counter = EntRefToEntIndex(caller);
    int target = EntRefToEntIndex(g_iThunderCounter);
    if (counter == INVALID_ENT_REFERENCE || counter != target)
        return;

    int current, maximum;
    if (!ReadThunder(counter, current, maximum))
        return;

    if (current >= maximum)
    {
        g_flThunderReadyAt = 0.0;
    }
    else if (current < g_iLastThunder || (g_iLastThunder == -1 && current < maximum))
    {
        g_flThunderReadyAt = GetGameTime() + 50.0;
    }
    g_iLastThunder = current;
}

public void Output_ThunderFull(const char[] output, int caller, int activator, float delay)
{
    if (!g_bSupportedMap)
        return;

    int counter = EntRefToEntIndex(caller);
    int target = EntRefToEntIndex(g_iThunderCounter);
    if (counter != INVALID_ENT_REFERENCE && counter == target)
    {
        g_flThunderReadyAt = 0.0;
    }
}

void UpdateThunder(float now)
{
    int counter = EntRefToEntIndex(g_iThunderCounter);
    int current, maximum;
    if (counter == INVALID_ENT_REFERENCE || !IsValidEntity(counter) || !ReadThunder(counter, current, maximum))
        return;

    if (current >= maximum)
    {
        g_flThunderReadyAt = 0.0;
    }
    else if (current < g_iLastThunder && g_iLastThunder != -1)
    {
        g_flThunderReadyAt = now + 50.0;
    }
    g_iLastThunder = current;
}
