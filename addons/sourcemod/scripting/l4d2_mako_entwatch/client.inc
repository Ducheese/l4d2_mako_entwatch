#if defined _l4d2_mako_entwatch_client_included
 #endinput
#endif
#define _l4d2_mako_entwatch_client_included

// ── 事件监听与玩家状态维护 ───────────────────────────
public void Event_RoundStart(Event event, const char[] name, bool dontBroadcast)
{
    if (!g_bSupportedMap)
        return;

    // 回合重开：彻底复位神器归属与CD，清空HUD，重新激活视口并重新绑定新回合实体
    ResetState();
    MakoHUD_Clear();
    MakoHUD_Enable();
    RequestFrame(Frame_BindMap);
}

public void Event_PlayerDisconnect(Event event, const char[] name, bool dontBroadcast)
{
    int userid = event.GetInt("userid");
    int client = GetClientOfUserId(userid);
    for (int item = 0; item < ITEM_COUNT; item++)
    {
        if (g_iHolder[item] == userid)
        {
            g_iHolder[item] = 0;
            if (client > 0 && IsClientInGame(client))
                PrintToServer("[MakoEntWatch] Holder %N disconnected, %s lost (destroyed by map)!", client, g_sShort[item]);
            else
                PrintToServer("[MakoEntWatch] Holder disconnected, %s lost (destroyed by map)!", g_sShort[item]);
        }
    }
}

public void Event_BotReplace(Event event, const char[] name, bool dontBroadcast)
{
    int oldId, newId;
    if (StrEqual(name, "player_bot_replace", false))
    {
        oldId = event.GetInt("player");
        newId = event.GetInt("bot");
    }
    else // bot_player_replace
    {
        oldId = event.GetInt("bot");
        newId = event.GetInt("player");
    }

    // 玩家与 BOT 接管时平滑转移神器持有者映射，防止追踪断裂
    for (int item = 0; item < ITEM_COUNT; item++)
    {
        if (g_iHolder[item] == oldId)
        {
            g_iHolder[item] = newId;
            int oldClient = GetClientOfUserId(oldId);
            int newClient = GetClientOfUserId(newId);
            if (oldClient > 0 && oldClient <= MaxClients)
                g_iClientItem[oldClient] = -1;
            if (newClient > 0 && newClient <= MaxClients)
                g_iClientItem[newClient] = item;
            PrintToServer("[MakoEntWatch] %s transferred during bot takeover (old userid %d -> new userid %d).",
                g_sShort[item], oldId, newId);
        }
    }
}

public void OnClientDisconnect(int client)
{
    g_iClientItem[client] = -1;
    g_iPendingItem[client] = -1;
}

public void OnClientPutInServer(int client)
{
    g_iClientItem[client] = -1;
    g_iPendingItem[client] = -1;
}

// ── HUD 测试命令 ─────────────────────────────────────────────────────────────
// sm_mako_hudtest：模拟 8 种神器全部被持有 + 冷却倒计时，30 秒后自动清空
// 可在任意地图/模式执行，不依赖地图支持；结束后自动恢复原先支持状态

int g_iHudTestUserId = 0;       // 测试命令执行者 userid，用于 Cleanup 精准还原
bool g_bHudTestMapRestore = false; // Cleanup 后是否恢复 g_bSupportedMap = false

public Action Command_HudTest(int client, int args)
{
    if (client <= 0)
    {
        ReplyToCommand(client, "[MakoEntWatch] 该命令只能由玩家在游戏内执行");
        return Plugin_Handled;
    }

    // 若上一轮测试尚在倒计时，先提示已覆盖
    if (g_iHudTestUserId != 0)
        PrintToServer("[MakoEntWatch] HUD 测试：覆盖上一轮，重新计时 30s");

    // 强制开启 HUD 视口（若本来就不在支持地图则之后 Cleanup 会还原）
    g_bHudTestMapRestore = !g_bSupportedMap;
    g_bSupportedMap = true;
    MakoHUD_Enable();

    float now = GetGameTime();
    int userid = GetClientUserId(client);
    g_iHudTestUserId = userid;

    // 给所有 8 种神器设置假持有者（就是执行命令的玩家本人）+ 各自不同的冷却时长
    // 冷却倒计时差异化，方便看出多行效果
    int fakeCDs[ITEM_COUNT] = {55, 42, 87, 0, 135, 20, 95, 0};
    for (int item = 0; item < ITEM_COUNT; item++)
    {
        g_iHolder[item] = userid;
        g_flReadyAt[item] = (fakeCDs[item] > 0) ? (now + float(fakeCDs[item])) : 0.0;
    }

    // 雷电额外设回充倒计时（ItemState 中若 counter 无效会显示 N/A，无所谓）
    g_flThunderReadyAt = now + 33.0;

    // 终极标记为未使用 → 显示 "R"
    g_bUltimaUsed = false;

    ReplyToCommand(client, "[MakoEntWatch] HUD 测试启动，模拟 8 种神器显示 30 秒后自动清空");

    // 30 秒后清理；data=1 表示 Cleanup 时需要还原 g_bSupportedMap = false
    CreateTimer(30.0, Timer_HudTestCleanup, g_bHudTestMapRestore ? 1 : 0);
    return Plugin_Handled;
}

public Action Timer_HudTestCleanup(Handle timer, any data)
{
    // 仅清理由本次测试设置的 holder（精准还原，不干扰真实持有者）
    int userid = g_iHudTestUserId;
    if (userid != 0)
    {
        for (int item = 0; item < ITEM_COUNT; item++)
        {
            if (g_iHolder[item] == userid)
            {
                g_iHolder[item] = 0;
                g_flReadyAt[item] = 0.0;
            }
        }
        g_flThunderReadyAt = 0.0;
        g_iHudTestUserId = 0;
    }

    // 若原本不在支持地图，还原标记并清除 HUD
    if (data == 1)
    {
        g_bSupportedMap = false;
        MakoHUD_Clear();
    }

    PrintToServer("[MakoEntWatch] HUD 测试结束，已清空");
    return Plugin_Handled;
}
