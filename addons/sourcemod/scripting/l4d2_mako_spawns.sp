//========================================================================================
// l4d2_mako_spawns - Materia Spawn Locations Visual Marker for l4d2_ffvii_makoreactor
//
// 功能特性：
//   1. 完整逆向全图 19 个可能的神器刷新点（City 市区 10 处 + RC 反应堆/撤离 9 处）；
//   2. 采用纯 TempEnt 网络广播（DLight 动态光源 + Beam 垂直冲天光柱 + BeamRing 地面能量环），
//      零实体消耗（0 Entity Count），不占用 2048 实体字典，杜绝崩服与实体泄漏；
//   3. 智能状态辨别：
//      - 【真神器在场】（未被拾取）：金黄色高亮闪烁光柱与大半径光晕；
//      - 【候选点（为空/已被拿走）】：经典魔晄青碧色（Mako Cyan-Green）常亮光柱与地灯；
//   4. 客户端自主控制指令（!makolights / !makospawns 自由开关）；
//   5. 管理员调试指令（sm_mako_spawns_list 打印坐标，sm_mako_tp_spawn 快速巡查传送）。
//========================================================================================

#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sdktools>

#define PLUGIN_VERSION "1.0.0"
#define SPAWN_COUNT 19
#define MATERIA_COUNT 8

public Plugin myinfo =
{
    name = "Mako Materia Spawn Points Marker（半成品有问题）",
    author = "Ducheese",
    description = "Highlights all 19 potential Materia spawn locations in l4d2_ffvii_makoreactor with beacon lights",
    version = PLUGIN_VERSION,
    url = "https://github.com/Ducheese"
};

// ── 刷新点数据结构 ─────────────────────────────────────────────────────────────
enum struct SpawnPoint
{
    char code[16];          // 编号代号（City-01 ~ City-10, RC-01 ~ RC-09）
    char description[64];   // 场景地标中文描述
    float origin[3];        // 刷新点三维坐标
    int region;             // 区域（0: City 市区街道, 1: RC 反应堆核心/撤离通道）
}

SpawnPoint g_Spawns[SPAWN_COUNT];

// 8 种神器的触发器名称，用于探测当前是否有真实神器滞留在刷新点
char g_sMateriaTriggers[MATERIA_COUNT][32] =
{
    "M_Fire_Trigger",
    "M_Ice_Trigger",
    "M_Heal_Trigger",
    "M_Thunder_Trigger",
    "M_Shield_Trigger",
    "M_Haste_Trigger",
    "M_Void_Trigger",
    "M_Ultima_Trigger"
};

char g_sMateriaShort[MATERIA_COUNT][16] =
{
    "[火]", "[冰]", "[奶]", "[电]", "[盾]", "[速]", "[黑洞]", "[终极]"
};

// ── 全局状态与资源 ─────────────────────────────────────────────────────────────
bool g_bSupportedMap = false;
int g_iBeamSprite = -1;
int g_iGlowSprite = -1;

// 每个刷新点当前滞留的神器索引（-1: 无神器, 0~7: 对应神器）与真实实体世界坐标
int g_iActiveMateriaAtSpawn[SPAWN_COUNT];
float g_fActiveMateriaOrigin[SPAWN_COUNT][3];

// 客户端偏好与 ConVar
ConVar g_cvEnabled;
ConVar g_cvBeacon;
ConVar g_cvDLight;
ConVar g_cvInterval;
ConVar g_cvOnlyActive;
ConVar g_cvMaxDist;

bool g_bClientEnabled[MAXPLAYERS + 1];

// ─────────────────────────────────────────────────────────────────────────────
public void OnPluginStart()
{
    CreateConVar("l4d2_mako_spawns_version", PLUGIN_VERSION, "Plugin version", FCVAR_NOTIFY | FCVAR_DONTRECORD);

    g_cvEnabled    = CreateConVar("sm_mako_spawns_enable", "1", "是否启用魔晄炉神器刷新点光效标记 (0=关闭, 1=开启)", FCVAR_NOTIFY);
    g_cvBeacon     = CreateConVar("sm_mako_spawns_beacon", "1", "是否启用冲天光柱与地面能量环 (0=关闭, 1=开启)", FCVAR_NOTIFY);
    g_cvDLight     = CreateConVar("sm_mako_spawns_dlight", "1", "是否启用地面动态光源光晕效果 (0=关闭, 1=开启)", FCVAR_NOTIFY);
    g_cvInterval   = CreateConVar("sm_mako_spawns_interval", "1.0", "光效刷新间隔 (秒)", FCVAR_NOTIFY, true, 0.5, true, 5.0);
    g_cvOnlyActive = CreateConVar("sm_mako_spawns_only_active", "0", "是否仅标记当前真正有神器的点位 (0=标记全部19处, 1=仅标记有神器的点)", FCVAR_NOTIFY);
    g_cvMaxDist    = CreateConVar("sm_mako_spawns_max_dist", "0.0", "光效渲染最大可见距离 (0.0=全图无限制可见)", FCVAR_NOTIFY);

    RegConsoleCmd("sm_makolights", Command_ToggleLights, "切换个人神器刷新点光效显示");
    RegConsoleCmd("sm_makospawns", Command_ToggleLights, "切换个人神器刷新点光效显示");
    RegConsoleCmd("sm_mako_spawns_list", Command_ListSpawns, "在控制台列出全图 19 个神器可能刷新点状态");

    RegAdminCmd("sm_mako_tp_spawn", Command_TeleportSpawn, ADMFLAG_GENERIC, "传送自己到指定的刷新点进行核对 (参数: 1-19)");

    HookEvent("round_start", Event_RoundStart, EventHookMode_PostNoCopy);

    InitSpawnData();

    // 默认所有客户端开启光效
    for (int i = 1; i <= MaxClients; i++)
        g_bClientEnabled[i] = true;

    // 周期渲染计时器
    CreateTimer(g_cvInterval.FloatValue, Timer_RenderSpawns, _, TIMER_REPEAT);

    // 启动热检测当前地图并预缓存资源
    CheckMap();
    if (g_bSupportedMap)
    {
        g_iBeamSprite = PrecacheModel("materials/sprites/laserbeam.vmt");
        g_iGlowSprite = PrecacheModel("materials/sprites/glow01.vmt");
    }
}

public void OnClientPutInServer(int client)
{
    g_bClientEnabled[client] = true;
}

public void OnClientDisconnect(int client)
{
    g_bClientEnabled[client] = true;
}

// ── 初始化 19 个刷新点坐标与地标信息 ──────────────────────────────────────────
void InitSpawnData()
{
    // ── City 区域（市区街道，共 10 个候选点）────────────────────────────────
    SetSpawn(0,  "City-01", "市区开局火车站外大街道中央",             32.0,  -8100.0, -1224.0, 0);
    SetSpawn(1,  "City-02", "市区左侧 AK47 刷新点与装饰门角落",     -1344.0,  -7696.0, -1184.0, 0);
    SetSpawn(2,  "City-03", "市区右侧电锯/散弹枪/药丸室内",          1584.0,  -8152.0, -1456.0, 0);
    SetSpawn(3,  "City-04", "市区建筑屋顶西南侧(垃圾箱爬梯空调旁)",  1592.0,  -5384.0, -1080.0, 0);
    SetSpawn(4,  "City-05", "市区深处绿植花坛/窗台土制炸弹旁",        304.0,  -4496.0, -1440.0, 0);
    SetSpawn(5,  "City-06", "市区最右侧连喷/机械楼梯火花通道",       3472.0,  -7264.0, -1316.0, 0);
    SetSpawn(6,  "City-07", "市区核心通道 Materia_loc17(步枪旁)",     96.0,  -5360.0, -1344.0, 0);
    SetSpawn(7,  "City-08", "市区左侧建筑 Materia_loc18(马格南旁)",  -352.0,  -5280.0, -1424.0, 0);
    SetSpawn(8,  "City-09", "市区核心通道 Materia_loc19(下层凹槽)",   112.0,  -5328.0, -1488.0, 0);
    SetSpawn(9,  "City-10", "市区建筑屋顶东北侧(旋转门通风管平台)",  1668.0,  -5176.0, -1080.0, 0);

    // ── RC 区域（Reactor Core 反应堆炉心与撤离路线，共 9 个候选点）───────────
    SetSpawn(10, "RC-01",   "反应堆核心入口走廊大门通道",           -1312.0,  -3680.0, -1672.0, 1);
    SetSpawn(11, "RC-02",   "萨菲罗斯大桥桥面正中央 (Seph_Bridge)", -5224.0,  -5440.0, -1460.0, 1);
    SetSpawn(12, "RC-03",   "电梯前机房走廊/通风管道口 (Vent/Server)", -4816.0,   -896.0, -1400.0, 1);
    SetSpawn(13, "RC-04",   "下炉子1: 反应堆炉心最底部大弹药堆旁",   -4792.0,  -1536.0, -3760.0, 1);
    SetSpawn(14, "RC-05",   "撤离路线第 5 区域安全门前死角通道",     -5936.0,   -768.0, -4800.0, 1);
    SetSpawn(15, "RC-06",   "下炉子2: 大空洞黄色大工字钢梁上方",     -4912.0,    600.0, -4800.0, 1);
    SetSpawn(16, "RC-07",   "最深处大爬梯底部牢房 (大货梯按钮旁)",   -5656.0,    208.0, -6160.0, 1);
    SetSpawn(17, "RC-08",   "电梯与反应堆防守大门前 (Button1/变压器)", -1308.0,  -6268.0, -1400.0, 1);
    SetSpawn(18, "RC-09",   "下炉子3: 大空洞中层集装箱护栏平台",     -4992.0,  -1312.0, -2990.0, 1);
}

void SetSpawn(int index, const char[] code, const char[] desc, float x, float y, float z, int region)
{
    strcopy(g_Spawns[index].code, sizeof(g_Spawns[].code), code);
    strcopy(g_Spawns[index].description, sizeof(g_Spawns[].description), desc);
    g_Spawns[index].origin[0] = x;
    g_Spawns[index].origin[1] = y;
    g_Spawns[index].origin[2] = z;
    g_Spawns[index].region = region;
    g_iActiveMateriaAtSpawn[index] = -1;
    g_fActiveMateriaOrigin[index][0] = x;
    g_fActiveMateriaOrigin[index][1] = y;
    g_fActiveMateriaOrigin[index][2] = z;
}

// ── 地图生命周期与资源缓存 ─────────────────────────────────────────────────────
public void OnMapStart()
{
    CheckMap();
    if (g_bSupportedMap)
    {
        g_iBeamSprite = PrecacheModel("materials/sprites/laserbeam.vmt");
        g_iGlowSprite = PrecacheModel("materials/sprites/glow01.vmt");
    }
}

public void OnMapEnd()
{
    g_bSupportedMap = false;
}

public void Event_RoundStart(Event event, const char[] name, bool dontBroadcast)
{
    if (g_bSupportedMap)
    {
        for (int i = 0; i < SPAWN_COUNT; i++)
            g_iActiveMateriaAtSpawn[i] = -1;
    }
}

void CheckMap()
{
    char map[64];
    GetCurrentMap(map, sizeof(map));
    g_bSupportedMap = (StrContains(map, "ffvii_makoreactor", false) != -1 || StrContains(map, "mako", false) != -1);
}

// ── 刷新点神器探测（每秒核对实体当前坐标）─────────────────────────────────────
void ProbeActiveMaterias()
{
    for (int i = 0; i < SPAWN_COUNT; i++)
        g_iActiveMateriaAtSpawn[i] = -1;

    if (!g_bSupportedMap)
        return;

    int trigger = -1;
    while ((trigger = FindEntityByClassname(trigger, "trigger_multiple")) != -1)
    {
        char targetname[64];
        if (GetEntPropString(trigger, Prop_Data, "m_iName", targetname, sizeof(targetname)) <= 0)
            continue;

        for (int m = 0; m < MATERIA_COUNT; m++)
        {
            if (StrEqual(targetname, g_sMateriaTriggers[m], false))
            {
                // 如果已经被玩家捡起（父级是玩家），跳过
                int parent = GetEntPropEnt(trigger, Prop_Data, "m_hMoveParent");
                if (parent > 0 && parent <= MaxClients)
                    continue;

                float mOrigin[3];
                GetEntPropVector(trigger, Prop_Send, "m_vecOrigin", mOrigin);

                // 遍历 19 个刷新点匹配距离（< 150 单位）
                for (int s = 0; s < SPAWN_COUNT; s++)
                {
                    if (GetVectorDistance(mOrigin, g_Spawns[s].origin) < 150.0)
                    {
                        g_iActiveMateriaAtSpawn[s] = m;
                        g_fActiveMateriaOrigin[s][0] = mOrigin[0];
                        g_fActiveMateriaOrigin[s][1] = mOrigin[1];
                        g_fActiveMateriaOrigin[s][2] = mOrigin[2];
                        break;
                    }
                }
                break;
            }
        }
    }
}

// ── TempEnt 动态光源封装 ─────────────────────────────────────────────────────
stock void TE_SetupDynamicLight(const float vecOrigin[3], int r, int g, int b, int iExponent, float flRadius, float flTime, float flDecay)
{
    TE_Start("Dynamic Light");
    TE_WriteVector("m_vecOrigin", vecOrigin);
    TE_WriteNum("r", r);
    TE_WriteNum("g", g);
    TE_WriteNum("b", b);
    TE_WriteNum("exponent", iExponent);
    TE_WriteFloat("m_fRadius", flRadius);
    TE_WriteFloat("m_fTime", flTime);
    TE_WriteFloat("m_fDecay", flDecay);
}

// ── 周期渲染光效（纯 TempEnt 网络广播，0 实体开销）────────────────────────────
public Action Timer_RenderSpawns(Handle timer)
{
    if (!g_bSupportedMap || !g_cvEnabled.BoolValue)
        return Plugin_Continue;

    ProbeActiveMaterias();

    bool doBeacon   = g_cvBeacon.BoolValue && (g_iBeamSprite != -1);
    bool doDLight   = g_cvDLight.BoolValue;
    bool onlyActive = g_cvOnlyActive.BoolValue;
    float maxDist   = g_cvMaxDist.FloatValue;
    float interval  = g_cvInterval.FloatValue;

    for (int i = 0; i < SPAWN_COUNT; i++)
    {
        int materia = g_iActiveMateriaAtSpawn[i];
        bool hasMateria = (materia != -1);

        if (onlyActive && !hasMateria)
            continue;

        float pos[3];
        if (hasMateria)
        {
            // 真实神器在场：优先动态吸附到实际实体的精确世界坐标
            pos[0] = g_fActiveMateriaOrigin[i][0];
            pos[1] = g_fActiveMateriaOrigin[i][1];
            pos[2] = g_fActiveMateriaOrigin[i][2];
        }
        else
        {
            // 空候选点：对齐视觉魔晶石小球水平中心 (X+30.0, Y, Z)
            pos[0] = g_Spawns[i].origin[0] + 30.0;
            pos[1] = g_Spawns[i].origin[1];
            pos[2] = g_Spawns[i].origin[2];
        }

        // 颜色定义：
        //   - 有真实神器滞留：耀眼金黄色 (255, 215, 0)
        //   - 普通可能刷新点：经典魔晄青碧色 (0, 230, 180)
        int r = hasMateria ? 255 : 0;
        int g = hasMateria ? 215 : 230;
        int b = hasMateria ? 0   : 180;
        int a = hasMateria ? 230 : 160;

        float lightRadius  = hasMateria ? 350.0 : 260.0;
        float pillarHeight = hasMateria ? 240.0 : 160.0;

        // 向所有启用的客户端分发
        for (int client = 1; client <= MaxClients; client++)
        {
            if (!IsClientInGame(client) || IsFakeClient(client) || !g_bClientEnabled[client])
                continue;

            float eyePos[3];
            GetClientEyePosition(client, eyePos);
            float dist = GetVectorDistance(eyePos, pos);

            if (maxDist > 0.0 && dist > maxDist)
                continue;

            // 视距分流保护（彻底解决 19 点 * 4 TE = 76 TE 挤爆客户端队列导致后半段RC点被全部丢弃的引擎Bug）：
            // 只有距离 < 2200 码或真神器在场时，才发送近距离地面 DLight、能量环与悬浮光球；
            // 远距离只发送冲天光柱，既省带宽又杜绝丢包！
            bool isNearby = (dist < 2200.0) || hasMateria;

            // 1. 地面动态光源（点灯效果，照亮地面与周围环境）
            if (doDLight && isNearby)
            {
                TE_SetupDynamicLight(pos, r, g, b, 8, lightRadius, interval + 0.3, 0.0);
                TE_SendToClient(client);
            }

            // 2. 悬浮发光魔晶石光球（Glow Sprite，对齐小球中心 +40.0）
            if (g_iGlowSprite != -1 && isNearby)
            {
                float glowPos[3];
                glowPos[0] = pos[0];
                glowPos[1] = pos[1];
                glowPos[2] = pos[2] + 40.0; // 悬浮在视觉小球正中心
                TE_SetupGlowSprite(glowPos, g_iGlowSprite, interval + 0.3, hasMateria ? 1.0 : 0.65, 220);
                TE_SendToClient(client);
            }

            // 3. 冲天垂直引导光柱（全图超视距拔地而起，必见！）
            if (doBeacon)
            {
                int color[4];
                color[0] = r; color[1] = g; color[2] = b; color[3] = a;

                // 垂直光柱
                float start[3], end[3];
                start = pos;
                end = pos;
                end[2] += pillarHeight;
                TE_SetupBeamPoints(start, end, g_iBeamSprite, g_iGlowSprite, 0, 30, interval + 0.3, 3.5, 3.5, 1, 0.0, color, 10);
                TE_SendToClient(client);

                // 地面能量波纹光圈（仅近身可见）
                if (isNearby)
                {
                    float ringPos[3];
                    ringPos = pos;
                    ringPos[2] += 6.0; // 稍抬高避开刷子面
                    TE_SetupBeamRingPoint(ringPos, 10.0, 90.0, g_iBeamSprite, g_iGlowSprite, 0, 15, interval, 4.0, 0.0, color, 10, 0);
                    TE_SendToClient(client);
                }
            }
        }
    }

    return Plugin_Continue;
}

// ── 客户端交互与调试指令 ───────────────────────────────────────────────────────
public Action Command_ToggleLights(int client, int args)
{
    if (client <= 0 || !IsClientInGame(client))
        return Plugin_Handled;

    g_bClientEnabled[client] = !g_bClientEnabled[client];
    ReplyToCommand(client, "[MakoSpawns] 神器刷新点光效标记已 %s", g_bClientEnabled[client] ? "\x04【开启】" : "\x02【关闭】");
    return Plugin_Handled;
}

public Action Command_ListSpawns(int client, int args)
{
    ReplyToCommand(client, "[MakoSpawns] ===== FFVII Mako 全图 19 个神器刷新候选点 =====");
    ReplyToCommand(client, "序号 | 代号     | 区域 | 当前状态        | 场景地标描述");
    ReplyToCommand(client, "-------------------------------------------------------------");

    for (int i = 0; i < SPAWN_COUNT; i++)
    {
        int materia = g_iActiveMateriaAtSpawn[i];
        char status[32];
        if (materia >= 0 && materia < MATERIA_COUNT)
            FormatEx(status, sizeof(status), "★ %s 刷新于此", g_sMateriaShort[materia]);
        else
            strcopy(status, sizeof(status), "空 / 未刷新");

        ReplyToCommand(client, "#%02d | %-7s | %s | %-15s | %s",
            i + 1,
            g_Spawns[i].code,
            g_Spawns[i].region == 0 ? "City" : " RC ",
            status,
            g_Spawns[i].description
        );
    }
    ReplyToCommand(client, "[MakoSpawns] 共 19 处点位。输入 !makolights 切换个人视觉光效。");
    return Plugin_Handled;
}

public Action Command_TeleportSpawn(int client, int args)
{
    if (client <= 0 || !IsClientInGame(client))
        return Plugin_Handled;

    if (args < 1)
    {
        ReplyToCommand(client, "[MakoSpawns] 用法: sm_mako_tp_spawn <1-19>");
        return Plugin_Handled;
    }

    char arg[8];
    GetCmdArg(1, arg, sizeof(arg));
    int index = StringToInt(arg) - 1;

    if (index < 0 || index >= SPAWN_COUNT)
    {
        ReplyToCommand(client, "[MakoSpawns] 序号无效，请输入 1 到 19。");
        return Plugin_Handled;
    }

    float dest[3];
    dest[0] = g_Spawns[index].origin[0];
    dest[1] = g_Spawns[index].origin[1];
    dest[2] = g_Spawns[index].origin[2] + 10.0;

    TeleportEntity(client, dest, NULL_VECTOR, NULL_VECTOR);
    ReplyToCommand(client, "[MakoSpawns] 已传送至 #%02d [%s]：%s (%.0f, %.0f, %.0f)",
        index + 1, g_Spawns[index].code, g_Spawns[index].description, dest[0], dest[1], dest[2]);
    return Plugin_Handled;
}
