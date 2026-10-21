# CIVITAS PSP

**V0.1 开发版** · PSP 1000 回合制 4X 策略游戏

[中文](README.md) | [English](README.en.md)

CIVITAS 使用 PSPSDK 和 PSP GU 2D API 开发，运行于 480×272 屏幕，采用固定 64×48 地图，面向 PSP 的短局 4X 体验。

![CIVITAS V0.1 主菜单](docs/screenshots/v0.1_main.png)

## 目录

- [功能](#功能)
- [操作](#操作)
- [构建与运行](#构建与运行)
- [发布包](#发布包)
- [项目结构](#项目结构)
- [文档](#文档)
- [测试](#测试)
- [已知限制](#已知限制)

## 功能

- **完整 4X 流程**：探索 64×48 地图、建城、扩张边界、研究 30 项科技、组建军队，通过征服、科技、外交或分数获胜。
- **四个文明与蛮族**：玩家奥蕾莉亚，以及博瑞亚、赛琳、德雷文和敌对蛮族。单位与城市使用文明色贴图变体和底部归属色条。
- **城市管理**：5×5 工作范围、市民分配/锁定、食物成长与饥荒、生产队列、金币加速、9 种建筑、边界扩张和地块购买。
- **军事系统**：定居者、战士、侦察兵、剑士、骑手、工人；地形/城市防御、反击、治疗和城市占领。
- **外交**：关系、赠礼、贸易、互不侵犯、开放边境、同盟、AI 提案和外交胜利。
- **两种模式**：标准模式 200 回合计分结算；限时模式 30/60/90 分钟；3 个手动存档位和自动存档。
- **中英文界面**：默认中文，可在选项切换 English；主菜单和暂停菜单均可进入 12 页游戏手册。
- **探索小地图**：右下角面板，默认收起，按 `L + R` 打开；显示已探索地形、当前可见区域、城市、单位、光标和镜头框。
- **音频**：Python 生成的 8 秒环境音和 8 个音效，可在选项中开关音乐。
- **PSP 表现**：自定义 XMB 图标/背景、菜单淡入/滑入/选中辉光，以及文明颜色单位。

## 操作

| 输入 | 功能 |
|---|---|
| 方向键 | 移动地图光标 |
| 摇杆 | 平移镜头 |
| × | 选择 / 确认 / 行动 |
| ○ | 返回 / 暂停 |
| △ | 单位行动菜单 |
| □ | 地块信息 |
| L / R | 切换单位 |
| L + R | 打开/关闭小地图 |
| START | 外交 |
| SELECT | 结束回合 |

城市界面：`L/R` 切换标签，`×` 选择生产/市民操作，`△` 购买/锁定，`□` 自动分配市民。

## 构建与运行

### 依赖

- [pspdev](https://github.com/pspdev/pspdev) 工具链（加入 `PATH`）
- GNU Make
- Python 3 + Pillow（重新生成资源时需要）
- Noto Sans CJK / DejaVu 字体（重新生成中文字库和 XMB 美术时需要）

### 构建

```bash
export PATH=$PATH:/home/zhang/pspdev/bin
make clean
make -j4
```

构建后在项目根目录生成 `EBOOT.PBP` 和 `civpsp.elf`。

重新生成字符串、字体、音频、XMB 美术和地形图集：

```bash
make assets
```

### 测试

```bash
make test
```

当前测试门槛为 **13 个 host 测试套件**，外加中英文/字库检查和生成音频检查。

### PPSSPP 运行

```bash
PPSSPP --windowed --graphics=software civpsp.elf
```

也可以直接在 PPSSPP 中打开 `EBOOT.PBP`。PSP 安装路径：

```text
ms0:/PSP/GAME/CIVPSP/EBOOT.PBP
```

`ICON0.PNG` 和 `PIC1.PNG` 会由构建过程打包进 EBOOT。

## 发布包

V0.1 开发版位于：

```text
release/v0.1/
├── EBOOT.PBP
├── civpsp-v0.1.zip
└── SHA256SUMS
```

`civpsp-v0.1.zip` 内含可直接复制到 PSP 的 EBOOT。

## 项目结构

```text
.
├── src/                 PSP 游戏代码与生成资源
│   ├── app.c            状态机、菜单、存读档、过渡动画
│   ├── map.c            地图生成、迷雾、连通性
│   ├── mapview.c        地图渲染、光标、小地图、单位操作
│   ├── city.c           城市产出、成长、生产、边界
│   ├── cityview.c       城市界面
│   ├── tech.c           30 项科技树与效果
│   ├── world.c          文明、回合、经济、计分
│   ├── combat.c         伤害、防御、占领
│   ├── diplomacy.c      关系、条约、贸易
│   ├── save.c           version-2 存档格式
│   ├── audio.c          音频线程与混音
│   ├── text.c           中英文文本渲染
│   └── gen/             生成的字符串、字库、PCM、图集
├── assets/
│   ├── strings.csv      中英文 UI 字符串
│   └── src/             源 PNG/WAV 资源
├── tools/               Python 资源/字符串/音频生成器
├── tests/               主机逻辑测试
├── docs/                状态、交接、决策、数据表、截图
├── release/             本地发布包（二进制文件由 Git 忽略）
├── Makefile
├── ICON0.PNG / PIC1.PNG
├── UPDATE.md
├── UPDATE.en.md
├── PROJECT_PLAN.md
└── README.md / README.en.md
```

## 文档

- [UPDATE.md](UPDATE.md) — V0.1 发布说明与变更记录
- [UPDATE.en.md](UPDATE.en.md) — English release notes
- [docs/GAME_DATA.md](docs/GAME_DATA.md) — 完整单位/建筑/科技/经济数据
- [docs/HANDOFF.md](docs/HANDOFF.md) — 开发交接与真机检查清单
- [docs/STATUS.md](docs/STATUS.md) — 详细里程碑与验证记录
- [docs/DECISIONS.md](docs/DECISIONS.md) — 设计决策与简化说明
- [PROJECT_PLAN.md](PROJECT_PLAN.md) — 原始范围与里程碑规划
- [docs/README_MILESTONES.md](docs/README_MILESTONES.md) — 归档的里程碑 README

## 测试

- 13 个 host 测试套件：地图、单位、城市、科技、改良、战斗、AI、外交、摇杆、存档、M7 计分、M8 科技/胜利、M9 i18n
- `tools/check_i18n.py`：翻译覆盖、格式化占位符、字形覆盖、行宽
- `tools/check_audio.py`：WAV 格式和非静音峰值

PPSSPP 已检查主菜单、手册、地图、城市标签、外交、存档、小地图、分数结算以及中英文切换。最新 V0.1 音频和发布包仍建议在 PSP 1000 上做最终验收。

## 已知限制

- V0.1 是开发快照，PSP 1000 最新包仍在最终验收中；主要验证目标为 PPSSPP。
- 没有海军单位；海岸和海洋不可通行。
- 贸易仅为金币交易；开放边境共享视野，但暂不限制单位穿越。
- 边界只通过影响力或购买扩张，暂无文化/边境移动门槛。
- 科技页为平面滚动列表，未做时代分组树。
- 暂无难度选择、重叠音效和设置菜单内的小地图开关。
- 存档格式为 version 2；M7/M8/M9 开发版存档应可加载。

## 版本

**V0.1** 是首个 GitHub 开发版，整合 M0–M9.12 开发线。历史里程碑标签保留在仓库中。
