# l4d2_mako_entwatch（魔晶石 HUD 监控插件）

专为 Left 4 Dead 2 僵尸逃跑（Zombie Escape）经典地图 `l4d2_ffvii_makoreactor` 定制的非侵入式神器 HUD（EntWatch 风格）插件。

通过被动挂钩地图底层 Output、Timer 及 VScript 状态机，实现 8 种魔晶石持有者与冷却/充能状态的无感精准追踪。

---

## 功能特性

- **EMS HUD 视口直绘**：基于 L4D2 原生 EMS HUD（通过 `GameRules` NetProps 直接驱动），零实体依赖、零 VScript 冲突，全面支持**生存模式（Survival）**与战役模式。
- **双 Slot 纵向全量展示**：
  - Slot 8 (`HUD_FAR_RIGHT`)：负责渲染前 4 种神器（火、冰、奶、电）
  - Slot 9 (`HUD_MID_BOX` 重定位)：紧贴 Slot 8 下方，负责渲染后 4 种神器（盾、速、黑洞、终极）
  - 彻底规避引擎网络属性单 Slot 128 字节限制，全部 8 行一屏尽览，与左侧伤害显示（Slot 7 `HUD_FAR_LEFT`）互不重叠。
- **8 种魔晶石全覆盖**：
  - `[火]` (Fire)：等级动态冷却（T1: 70s / T2: 110s / T3: 150s）
  - `[冰]` (Ice)：等级动态冷却（T1: 60s / T2: 90s / T3: 120s）
  - `[奶]` (Heal)：等级动态冷却（T1: 90s / T2: 150s / T3: 210s）
  - `[电]` (Thunder)：`math_counter` 实时点数镜像（如 1级 `1/1`，高阶 `2/2`、`3/3`）与 50s 回充倒计时（`0/1|48`）
  - `[盾]` (Shield)：等级动态冷却（T1: 90s / T2: 150s / T3: 180s）
  - `[速]` (Haste)：120s 冷却倒计时
  - `[黑洞]` (Void)：110s 冷却倒计时
  - `[终极]` (Ultima)：就绪 `[R]` / 一次性施法消耗后常驻展示 `[E]`
- **0.03s 归属判定**：精准抵御地图 VScript 改名时序竞争（`Carrier<X>` / `MateriaCarrier`），彻底杜绝假阳性。
- **纯被动事件挂钩**：所有挂钩 Output 均为无感监听，不拦截、不修改地图原始 Entity I/O。
- **静默自适应**：无人持有时面板自动隐藏，不破坏游戏画面沉浸感；仅在检测到持有者时点亮。

---

## 目录结构

```
l4d2_mako_entwatch/
├── .gitignore
├── README.md
├── mako_materia_hud_spec.html              # 地图解析
└── addons/
    └── sourcemod/
        ├── plugins/
        │   └── l4d2_mako_entwatch.smx      # 编译输出产物
        └── scripting/
            ├── l4d2_mako_entwatch.sp       # 主插件入口与生命周期
            └── l4d2_mako_entwatch/
                ├── constants.inc           # 常量、数据结构、实体名表与全局状态
                ├── entity.inc              # 实体引用安全封装、名称比对与属性读取
                ├── materia.inc             # 施法 Relay/Button 挂钩、冷却计算与雷电充能
                ├── pickup.inc              # 拾取触发器挂钩与 0.03s 延迟归属验证
                ├── hud.inc                 # EMS HUD 双 Slot 渲染引擎与清屏
                └── client.inc              # 回合事件、断线清理与测试/调试指令
```

---

## 控制台指令

| 指令 | 权限 | 说明 |
| :--- | :--- | :--- |
| `sm_mako_debug` / `sm_mako_status` | 所有人 | 打印当前地图、等级、各神器持有者/CD/实体引用等完整调试状态 |
| `sm_makohud` | 所有人 | 模拟 8 种神器全显示与倒计时（持续 30 秒后自动清空，供排版测试） |
