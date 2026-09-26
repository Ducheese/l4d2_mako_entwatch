#if defined _l4d2_mako_entwatch_entity_included
 #endinput
#endif
#define _l4d2_mako_entwatch_entity_included

// ── 实体引用安全封装 ──────────────────────────────────
// 在 SourceMod 中，非网络逻辑实体（如 math_counter、logic_script 等）由 FindEntityByClassname
// 或 HookEntityOutput 返回时，其值本身已经是负数实体引用 (bcompat reference)；
// 对负数引用调用 EntIndexToEntRef 会抛出 "Invalid entity index" 异常，因此使用此安全转换函数。
stock int SafeEntRef(int ent)
{
    if (ent == INVALID_ENT_REFERENCE || ent == -1)
        return INVALID_ENT_REFERENCE;
    if (ent < 0)
        return ent; // 负数说明已是实体引用，直接返回
    return EntIndexToEntRef(ent);
}

// ── 实体属性读取 ──────────────────────────────────────
bool ReadThunder(int entity, int &current, int &maximum)
{
    int index = (entity < 0) ? EntRefToEntIndex(entity) : entity;
    if (index == INVALID_ENT_REFERENCE || !IsValidEntity(index))
    {
        if (!IsValidEntity(entity))
            return false;
        index = entity;
    }

    if (!HasEntProp(index, Prop_Data, "m_OutValue")
        || !HasEntProp(index, Prop_Data, "m_flMin")
        || !HasEntProp(index, Prop_Data, "m_flMax"))
        return false;

    float min = GetEntPropFloat(index, Prop_Data, "m_flMin");
    current = RoundFloat(GetEntPropFloat(index, Prop_Data, "m_OutValue") - min);
    maximum = RoundFloat(GetEntPropFloat(index, Prop_Data, "m_flMax") - min);
    return true;
}

// ── 实体属性解析与名称映射 ───────────────────────────
int ItemFromTrigger(const char[] name)
{
    for (int item = 0; item < ITEM_COUNT; item++)
    {
        if (StrEqual(name, g_sTrigger[item], false))
            return item;
    }
    return -1;
}

int ItemFromButton(const char[] name)
{
    for (int item = 0; item < ITEM_COUNT; item++)
    {
        if (StrEqual(name, g_sButton[item], false))
            return item;
    }
    return -1;
}

int ItemFromEntity(int ent, bool trigger)
{
    int index = EntRefToEntIndex(ent);
    if (index == INVALID_ENT_REFERENCE || !IsValidEntity(index))
        return -1;

    for (int item = 0; item < ITEM_COUNT; item++)
    {
        int targetRef = trigger ? g_iTriggerRef[item] : g_iButtonRef[item];
        if (targetRef != INVALID_ENT_REFERENCE && EntRefToEntIndex(targetRef) == index)
            return item;
    }

    // 备选降级：若实体在运行期重置或重建，通过名称兜底
    char name[64];
    if (GetEntPropString(index, Prop_Data, "m_iName", name, sizeof(name)) > 0)
    {
        return trigger ? ItemFromTrigger(name) : ItemFromButton(name);
    }

    return -1;
}

int ItemFromRelay(const char[] name, int &tier)
{
    tier = -1;
    for (int item = 0; item < ITEM_COUNT; item++)
    {
        for (int i = 0; i < 3; i++)
        {
            if (g_sRelay[item][i][0] != '\0' && StrEqual(name, g_sRelay[item][i], false))
            {
                tier = i;
                return item;
            }
        }
    }
    return -1;
}

bool EntityName(int ent, char[] name, int maxlen)
{
    int index = EntRefToEntIndex(ent);
    return index != INVALID_ENT_REFERENCE && IsValidEntity(index) && GetEntPropString(index, Prop_Data, "m_iName", name, maxlen) > 0;
}

bool IsSurvivor(int client)
{
    return client >= 1 && client <= MaxClients && IsClientInGame(client) && GetClientTeam(client) == 2;
}

// ── 状态复位 ──────────────────────────────────────────
void ResetState()
{
    for (int item = 0; item < ITEM_COUNT; item++)
    {
        g_iHolder[item] = 0;
        g_flReadyAt[item] = 0.0;
    }
    for (int client = 1; client <= MaxClients; client++)
    {
        g_iClientItem[client] = -1;
        g_iPendingItem[client] = -1;
    }
    g_iThunderCounter = INVALID_ENT_REFERENCE;
    g_iLastThunder = -1;
    g_flThunderReadyAt = 0.0;
    g_bUltimaUsed = false;
    g_iLevel = 1;
}

// ── 缓存地图实体引用 ──────────────────────────────────
void Frame_BindMap(any data)
{
    if (!g_bSupportedMap)
        return;

    int triggersFound = 0;
    int buttonsFound = 0;

    for (int ent = MaxClients + 1; ent < GetMaxEntities(); ent++)
    {
        if (!IsValidEntity(ent))
            continue;

        char name[64];
        if (GetEntPropString(ent, Prop_Data, "m_iName", name, sizeof(name)) <= 0)
            continue;

        int item = ItemFromTrigger(name);
        if (item >= 0)
        {
            g_iTriggerRef[item] = SafeEntRef(ent);
            triggersFound++;
            continue;
        }

        item = ItemFromButton(name);
        if (item >= 0)
        {
            g_iButtonRef[item] = SafeEntRef(ent);
            buttonsFound++;
            continue;
        }
    }

    // 针对可能分配在高位实体区（>2048）的触发器与按钮，通过 FindEntityByClassname 遍历底层实体链表补漏
    int triggerEnt = -1;
    while ((triggerEnt = FindEntityByClassname(triggerEnt, "trigger_multiple")) != -1)
    {
        char name[64];
        if (GetEntPropString(triggerEnt, Prop_Data, "m_iName", name, sizeof(name)) > 0)
        {
            int item = ItemFromTrigger(name);
            if (item >= 0 && g_iTriggerRef[item] == INVALID_ENT_REFERENCE)
            {
                g_iTriggerRef[item] = SafeEntRef(triggerEnt);
                triggersFound++;
            }
        }
    }

    int buttonEnt = -1;
    while ((buttonEnt = FindEntityByClassname(buttonEnt, "func_button_timed")) != -1)
    {
        char name[64];
        if (GetEntPropString(buttonEnt, Prop_Data, "m_iName", name, sizeof(name)) > 0)
        {
            int item = ItemFromButton(name);
            if (item >= 0 && g_iButtonRef[item] == INVALID_ENT_REFERENCE)
            {
                g_iButtonRef[item] = SafeEntRef(buttonEnt);
                buttonsFound++;
            }
        }
    }

    // math_counter 属于非网络逻辑实体，在 L4D2 中常被分配在 2048~4096 高位实体区
    // 使用 FindEntityByClassname 遍历引擎内部实体链表，穿透 2048 索引上限
    int counter = -1;
    while ((counter = FindEntityByClassname(counter, "math_counter")) != -1)
    {
        char counterName[64];
        if (GetEntPropString(counter, Prop_Data, "m_iName", counterName, sizeof(counterName)) > 0
            && StrEqual(counterName, "M_Thunder_ChargCount", false))
        {
            g_iThunderCounter = SafeEntRef(counter);
            int current, maximum;
            if (ReadThunder(counter, current, maximum))
                g_iLastThunder = current;
            break;
        }
    }

    PrintToServer("[MakoEntWatch] Map entities bound. (Triggers: %d/8, Buttons: %d/8, ThunderCounter: %s)",
        triggersFound, buttonsFound, (g_iThunderCounter != INVALID_ENT_REFERENCE) ? "FOUND" : "NOT FOUND");
}
