//========================================================================================
// l4d2_mako_entwatch - Non-intrusive ZE-style Materia HUD for l4d2_ffvii_makoreactor
//
// 模块化架构：
//   - l4d2_mako_entwatch/constants.inc : 常量、枚举定义、地图实体名称表与全局状态
//   - l4d2_mako_entwatch/entity.inc    : 实体属性读取、地图绑定 (Frame_BindMap) 与名称映射
//   - l4d2_mako_entwatch/materia.inc   : 施法 Relay/Button 挂钩、冷却计算、护盾与雷电充能
//   - l4d2_mako_entwatch/pickup.inc    : 拾取触发器挂钩与 0.03s 延迟归属验证 (Carrier<X>)
//   - l4d2_mako_entwatch/hud.inc       : EMS HUD 双 Slot 渲染引擎（GameRules NetProps，零实体依赖）与清屏
//   - l4d2_mako_entwatch/client.inc    : 回合事件、断线清理与测试/调试指令
//========================================================================================

#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sdktools>

#include "l4d2_mako_entwatch/constants"
#include "l4d2_mako_entwatch/entity"
#include "l4d2_mako_entwatch/materia"
#include "l4d2_mako_entwatch/pickup"
#include "l4d2_mako_entwatch/hud"
#include "l4d2_mako_entwatch/client"

public APLRes AskPluginLoad2(Handle myself, bool late, char[] error, int err_max)
{
    g_bLateLoad = late;
    return APLRes_Success;
}

public Plugin myinfo =
{
    name = "Mako Materia HUD",
    author = "Ducheese",
    description = "Non-intrusive ZE-style Materia HUD for l4d2_ffvii_makoreactor",
    version = PLUGIN_VERSION,
    url = "https://space.bilibili.com/1889622121"
};

public void OnPluginStart()
{
    HookEvent("round_start", Event_RoundStart, EventHookMode_PostNoCopy);
    HookEvent("player_disconnect", Event_PlayerDisconnect, EventHookMode_Pre);
    HookEvent("bot_player_replace", Event_BotReplace, EventHookMode_Pre);
    HookEvent("player_bot_replace", Event_BotReplace, EventHookMode_Pre);

    RegConsoleCmd("sm_mako_debug",   Command_DebugStatus, "Print Mako Materia HUD debug status");
    RegConsoleCmd("sm_mako_status",  Command_DebugStatus, "Print Mako Materia HUD debug status");
    RegConsoleCmd("sm_makohud",      Command_HudTest,     "Simulate all 8 materia HUD entries for 30s");

    // 全量被动挂钩地图实体 Output，签名一律返回 void，不拦截/不破坏地图原始逻辑
    HookEntityOutput("trigger_multiple", "OnStartTouch", Output_TriggerTouch); // 拾取
    HookEntityOutput("func_button_timed", "OnTimeUp", Output_ButtonTimeUp);    // 长按 E
    HookEntityOutput("logic_relay", "OnTrigger", Output_RelayTrigger);         // CD
    HookEntityOutput("logic_timer", "OnTimer", Output_Timer);                  // 雷电回充计时器
    HookEntityOutput("logic_case", "OnCase01", Output_Level1);                 // 难度变化
    HookEntityOutput("logic_case", "OnCase02", Output_Level2);
    HookEntityOutput("logic_case", "OnCase03", Output_Level3);
    HookEntityOutput("math_counter", "OutValue", Output_ThunderCounter);       // 雷电点数动态
    HookEntityOutput("math_counter", "OnHitMax", Output_ThunderFull);          // 雷电点数动态

    // 热加载
    if (g_bLateLoad)
    {
        char map[PLATFORM_MAX_PATH];
        GetCurrentMap(map, sizeof(map));
        if (StrContains(map, "makoreactor", false) != -1)
        {
            OnMapStart();
        }
    }
}

public void OnGameFrame()
{
    if (g_bSupportedMap)
    {
        MakoHUD_FrameUpdate();
    }
}

public void OnMapStart()
{
    char map[PLATFORM_MAX_PATH];
    GetCurrentMap(map, sizeof(map));
    g_bSupportedMap = (StrContains(map, "makoreactor", false) != -1);

    ResetState();

    if (!g_bSupportedMap)
        return;

    for (int item = 0; item < ITEM_COUNT; item++)
    {
        g_iTriggerRef[item] = INVALID_ENT_REFERENCE;
        g_iButtonRef[item] = INVALID_ENT_REFERENCE;
    }
    g_iThunderCounter = INVALID_ENT_REFERENCE;

    // 激活 EMS HUD 视口（m_bChallengeModeActive = true），生存模式下必须主动启用
    MakoHUD_Enable();

    RequestFrame(Frame_BindMap);
}

public void OnMapEnd()
{
    MakoHUD_Clear();
    g_bSupportedMap = false;
}

public Action Command_DebugStatus(int client, int args)
{
    char map[PLATFORM_MAX_PATH];
    GetCurrentMap(map, sizeof(map));

    ReplyToCommand(client, "[MakoEntWatch] === DEBUG STATUS ===");
    ReplyToCommand(client, "[MakoEntWatch] Map: '%s' (Supported: %s), Level: %d", map, g_bSupportedMap ? "YES" : "NO", g_iLevel);

    int counter = EntRefToEntIndex(g_iThunderCounter);
    int current, maximum;
    if (counter != INVALID_ENT_REFERENCE && ReadThunder(counter, current, maximum))
    {
        float left = (g_flThunderReadyAt > GetGameTime()) ? (g_flThunderReadyAt - GetGameTime()) : 0.0;
        ReplyToCommand(client, "[MakoEntWatch] Thunder: CounterEnt=%d, Points=%d/%d, RegenLeft=%.1fs", counter, current, maximum, left);
    }
    else
    {
        ReplyToCommand(client, "[MakoEntWatch] Thunder: CounterEnt=INVALID");
    }

    float now = GetGameTime();
    for (int i = 0; i < ITEM_COUNT; i++)
    {
        int userid = g_iHolder[i];
        int holderClient = (userid != 0) ? GetClientOfUserId(userid) : 0;
        char holderName[64];
        if (holderClient > 0 && IsClientInGame(holderClient))
            GetClientName(holderClient, holderName, sizeof(holderName));
        else
            strcopy(holderName, sizeof(holderName), "None");

        float cd = (g_flReadyAt[i] > now) ? (g_flReadyAt[i] - now) : 0.0;
        ReplyToCommand(client, "[MakoEntWatch] %s: Holder=%s (userid=%d, client=%d), CD=%.1fs, TrigRef=%d, BtnRef=%d",
            g_sShort[i], holderName, userid, holderClient, cd, g_iTriggerRef[i], g_iButtonRef[i]);
    }
    return Plugin_Handled;
}
