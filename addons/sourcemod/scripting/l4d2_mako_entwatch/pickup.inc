#if defined _l4d2_mako_entwatch_pickup_included
 #endinput
#endif
#define _l4d2_mako_entwatch_pickup_included

// ── 拾取与归属判定 ─────────────────────────────────────
public void Output_TriggerTouch(const char[] output, int caller, int activator, float delay)
{
    if (!g_bSupportedMap)
        return;

    int callerIndex = EntRefToEntIndex(caller);
    if (callerIndex == INVALID_ENT_REFERENCE || !IsValidEntity(callerIndex))
        return;

    char name[64];
    if (!EntityName(callerIndex, name, sizeof(name)))
        return;

    int item = ItemFromTrigger(name);
    if (item >= 0)
    {
        if (g_iTriggerRef[item] == INVALID_ENT_REFERENCE)
            g_iTriggerRef[item] = SafeEntRef(caller);

        // 过滤器是 SurvivorHuman，只允许存活幸存者玩家
        if (!IsSurvivor(activator) || IsFakeClient(activator))
            return;

        // 记入 pending，0.03 秒后复查验证（避开地图脚本 0.01 秒的时序竞争）
        g_iPendingItem[activator] = item;
        DataPack pack;
        CreateDataTimer(0.03, Timer_CheckPickup, pack, TIMER_FLAG_NO_MAPCHANGE);
        pack.WriteCell(GetClientUserId(activator));
        pack.WriteCell(item);
        pack.WriteCell(0);
        return;
    }
}

public Action Timer_CheckPickup(Handle timer, DataPack pack)
{
    pack.Reset();
    int userid = pack.ReadCell();
    int item = pack.ReadCell();
    int attempt = pack.ReadCell();

    int client = GetClientOfUserId(userid);
    if (!IsSurvivor(client) || item < 0 || item >= ITEM_COUNT)
        return Plugin_Stop;

    char targetname[64];
    bool isCarrier = false;
    if (GetEntPropString(client, Prop_Data, "m_iName", targetname, sizeof(targetname)) > 0)
    {
        // TryParentMateria 在 0.01s 时将玩家改名为 Carrier<X>，0.25s 后才改成通用的 MateriaCarrier
        // 如果玩家处于 Carrier<X> 窗口期直接命中；
        // 若因服务器卡顿调度延迟导致名字已翻转为 MateriaCarrier：
        // 唯当玩家此前未持有其他神器（或终极已使用）且 pending 匹配时才认可，彻底防范多玩家假阳性
        bool canPickup = (g_iClientItem[client] == -1 || (g_iClientItem[client] == Item_Ultima && g_bUltimaUsed));
        if (StrEqual(targetname, g_sCarrier[item], false)
            || (StrEqual(targetname, "MateriaCarrier", false) && canPickup && g_iPendingItem[client] == item))
        {
            isCarrier = true;
        }
    }

    if (isCarrier)
    {
        // 若该玩家之前拿着别的神器，先清空旧神器的持有者
        int previous = g_iClientItem[client];
        if (previous >= 0 && previous != item)
        {
            PrintToServer("[MakoEntWatch] %N dropped %s (swapped for %s)", client, g_sShort[previous], g_sShort[item]);
            g_iHolder[previous] = 0;
        }

        // 若该神器之前在别人手里，清空前任
        int oldHolder = g_iHolder[item];
        if (oldHolder != 0 && oldHolder != userid)
        {
            int oldClient = GetClientOfUserId(oldHolder);
            if (oldClient > 0 && oldClient <= MaxClients)
            {
                PrintToServer("[MakoEntWatch] %s transferred from %N to %N", g_sShort[item], oldClient, client);
                g_iClientItem[oldClient] = -1;
            }
        }

        g_iClientItem[client] = item;
        g_iHolder[item] = userid;
        g_iPendingItem[client] = -1;
        PrintToServer("[MakoEntWatch] %N picked up %s!", client, g_sShort[item]);
        return Plugin_Stop;
    }

    if (++attempt < PICKUP_RETRIES)
    {
        DataPack retry;
        CreateDataTimer(0.03, Timer_CheckPickup, retry, TIMER_FLAG_NO_MAPCHANGE);
        retry.WriteCell(userid);
        retry.WriteCell(item);
        retry.WriteCell(attempt);
    }
    else
    {
        LogError("[MakoEntWatch] Pickup verification timed out for client %N on item %s (targetname was '%s')",
            client, g_sShort[item], targetname);
        g_iPendingItem[client] = -1;
    }
    return Plugin_Stop;
}
