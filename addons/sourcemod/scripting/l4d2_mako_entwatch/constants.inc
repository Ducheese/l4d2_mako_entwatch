#if defined _l4d2_mako_entwatch_constants_included
 #endinput
#endif
#define _l4d2_mako_entwatch_constants_included

#define PLUGIN_VERSION "1.0.0"
#define ITEM_COUNT 8
#define HUD_INTERVAL 1.0
#define PICKUP_RETRIES 8

enum
{
    Item_Fire = 0,
    Item_Ice,
    Item_Heal,
    Item_Thunder,
    Item_Shield,
    Item_Haste,
    Item_Void,
    Item_Ultima
};

// 魔晶石短名称（国服 ZE 经典中文简称）
char g_sShort[ITEM_COUNT][16] =
{
    "[火]", "[冰]", "[奶]", "[电]", "[盾]", "[速]", "[黑洞]", "[终极]"
};

char g_sTrigger[ITEM_COUNT][32] =
{
    "M_Fire_Trigger", "M_Ice_Trigger", "M_Heal_Trigger", "M_Thunder_Trigger",
    "M_Shield_Trigger", "M_Haste_Trigger", "M_Void_Trigger", "M_Ultima_Trigger"
};

char g_sButton[ITEM_COUNT][32] =
{
    "M_Fire_Button", "M_Ice_Button", "M_Heal_Button", "M_Thunder_Button",
    "M_Shield_Button", "M_Haste_Button", "M_Void_Button", "M_Ultima_Button"
};

char g_sCarrier[ITEM_COUNT][32] =
{
    "CarrierFire", "CarrierIce", "CarrierHeal", "CarrierThunder",
    "CarrierShield", "CarrierHaste", "CarrierVoid", "CarrierUltima"
};

char g_sRelay[ITEM_COUNT][3][40] =
{
    {"M_Fire_Logic_T1", "M_Fire_Logic_T2", "M_Fire_Logic_T3"},
    {"M_Ice_Logic_T1", "M_Ice_Logic_T2", "M_Ice_Logic_T3"},
    {"M_Heal_Logic_T1", "M_Heal_Logic_T2", "M_Heal_Logic_T3"},
    {"", "", ""},
    {"M_Shield_Logic_T1", "M_Shield_Logic_T2", "M_Shield_Logic_T3"},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""}
};

int g_iCooldown[ITEM_COUNT][3] =
{
    {70, 110, 150},     // Fire (T1: 70s, T2: 110s, T3: 150s)
    {60, 90, 120},      // Ice (T1: 60s, T2: 90s, T3: 120s)
    {90, 150, 210},     // Heal (T1: 90s, T2: 150s, T3: 210s)
    {0, 0, 0},          // Thunder: math_counter
    {90, 150, 180},     // Shield (T1: 90s, T2: 150s, T3: 180s)
    {120, 120, 120},    // Haste: 120s
    {110, 110, 110},    // Void: 110s
    {0, 0, 0}           // Ultima: 一次性
};

// 运行时状态变量
int g_iTriggerRef[ITEM_COUNT];              // 拾取触发器实体引用
int g_iButtonRef[ITEM_COUNT];               // 施法按钮实体引用
int g_iHolder[ITEM_COUNT];                  // 当前持有者客户端 userid（0 = 无人持有）
int g_iClientItem[MAXPLAYERS + 1];          // 客户端当前持有神器索引（-1 = 无）
int g_iPendingItem[MAXPLAYERS + 1];         // 触碰触发器等待复查的神器索引
float g_flReadyAt[ITEM_COUNT];              // 技能冷却结束时刻（GetGameTime() 秒数）

int g_iThunderCounter = INVALID_ENT_REFERENCE; // 雷电充能计数器实体引用
int g_iLastThunder = -1;                    // 上一次读取的雷电充能值（用于判定减少并启动50s回充）
float g_flThunderReadyAt = 0.0;            // 下次 +1 充能预计就绪时刻

int g_iLevel = 1;                          // 地图 MateriaMasterCase 等级（1..3）
bool g_bUltimaUsed = false;                // 终极魔法是否已被消耗（一次性）

bool g_bSupportedMap = false;              // 是否在目标地图运行
bool g_bLateLoad = false;                  // 是否为运行中热加载（Late Load）
