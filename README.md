# 🌱 时光花园 Focus Garden

> 一棵住在你桌面角落、**随你的专注而成长**的陪伴型番茄钟。

**deepin Skills 开发大赛参赛作品** · 方向三：DTK 原生应用（`dtk-development`）
基于 deepin 25 / UOS v25，DTK6 + Qt6（C++/Qt Widgets），纯矢量自绘无外部素材。

![idle](docs/screenshots/01-idle-seed.png)

---

## ✨ 创意与亮点

- **养成驱动专注**：认真完成一段番茄钟，花盆里的“露露”就会长大——
  种子 → 幼苗 → 含苞 → 盛放（按累计专注时长进阶，进度实时显示）。
- **会累、会困、会开心**：专注时露露“认真脸 + 汗滴”，到了休息时间它会闭眼打盹（zZz），
  完成专注会撒花庆祝；它还会在你连续工作多轮后主动提醒你休息。
- **系统联动（真实可用）**：专注开始自动开启 **护眼模式（DDE 色温）**，
  结束/进入休息自动还原 —— 通过 `org.deepin.dde.Display1` 的 D-Bus 属性读写实现，
  已适配“用户原本开着夜间/自定义色温则不打扰”的场景。
- **低打扰常驻**：无边框毛玻璃小窗 + 系统托盘；按住露露即可拖动窗口，
  双击露露快速开始/暂停；关闭窗口收进托盘继续守护。
- **本地优先**：所有统计（今日/最近 7 天/累计专注、完成次数、成长阶段）
  保存在 `~/.config/focus-garden/`，无任何联网与隐私收集。

![focus](docs/screenshots/02-focus.png)

## 🎯 功能清单

| 模块 | 说明 |
| --- | --- |
| 番茄钟 | 15/25/45/60 分钟可选；开始/暂停/继续/放弃 |
| 劳逸节奏 | 短休息 5–15 分钟，长休息每 N 次触发（可配），自动进入休息可开关 |
| 养成系统 | 4 形态成长（按累计专注分钟），界面显示距下一形态还需几次 |
| 情绪动画 | 呼吸、眨眼、认真脸、困倦 zZz、撒花庆祝、成长演出 |
| 护眼联动 | 专注自动护眼色温，结束还原（D-Bus） |
| 统计 | 今日 / 累计 / 完成次数 / 成长阶段 / 最近 7 天柱状图 |
| 托盘 | 隐藏/显示、快速开始/暂停、成长记录、退出 |
| 数据 | QSettings 本地持久化，跨天自动重置“连续专注” |

## 🖥️ 运行环境

- deepin 25 / UOS v25（X11 已验证；Wayland 会话下核心功能正常，窗口装饰以合成器能力为准）
- amd64 / arm64（纯 C++ + Qt6/DTK6，跨架构无特殊代码）
- 运行时依赖：`libdtk6core libdtk6gui libdtk6widget libqt6core6 libqt6gui6 libqt6widgets6 libqt6dbus6`

## 🔧 构建

```bash
# 编译依赖
sudo apt install build-essential cmake ninja-build git \
  qt6-base-dev qt6-declarative-dev libdtk6widget-dev libdtk6core-dev libdtk6gui-dev

git clone <repo-url> focus-garden
cd focus-garden
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

## 📦 安装 / 卸载

```bash
# 方式一：直接安装到系统
sudo ninja -C build install        # 卸载: sudo ninja -C build uninstall

# 方式二：生成并安装 .deb（推荐，便于干净卸载）
cmake -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/usr
cpack -G DEB -C Release --config build/CPackConfig.cmake
sudo apt install ./focus-garden-1.0.0-amd64.deb   # 卸载: sudo apt remove focus-garden
```

安装后可在启动器搜索 **时光花园 / Focus Garden** 启动。

## 🎮 使用

1. 点击 **开始专注**：露露进入“守护”状态，倒计时环 + 自动护眼；
2. 专注结束自动进入休息，露露打盹（可跳过）；
3. 连续完成 N 轮触发长休息，帮助恢复精力；
4. 随时打开 **统计** 查看 7 天柱状图与成长进度；
5. 右键托盘图标 → 退出。

> 贴心小操作：**按住露露拖动**可以把它放到桌面上任意喜欢的位置，
> **双击露露**快速开始/暂停一段专注。

## 🏗️ 代码结构

```
src/
├── main.cpp          入口：DApplication、单实例逻辑组装
├── focusmanager.*    专注状态机：Idle/Focus/Pause/Break、成长与统计持久化
├── petwidget.*       露露：纯 QPainter 矢量绘制 + 呼吸/眨眼/庆祝粒子动画
├── mainwindow.*      无边框半透明主窗、托盘、按钮与文案联动、设置对话框
├── statsdialog.*     成长记录：汇总指标 + 最近 7 天柱状图
└── systemlinker.*    系统联动：护眼色温读写（org.deepin.dde.Display1）、桌面通知
resources/            SVG 应用图标（亦作为托盘/窗口图标）
data/                 .desktop 启动器入口
docs/screenshots/     演示截图
```

## 🧩 本项目如何使用 deepin Skills 开发

- 使用 **`dtk-development`** 技能获取 DTK6 工程模板（CMake / main / .desktop）、
  D-Bus（DDBusSender）、DConfig 权限边界、平台抽象层等规范文档；
- 按技能模板搭建 `Dtk6::Core/Gui/Widget` 标准工程，避免混用 DTK5/Qt5 与 DTK6/Qt6；
- 依据技能中“第三方应用不可写其他应用 DConfig”的结论，将 **勿扰（DND）自动切换**
  从 v1 移除并列为 Roadmap，保证功能可靠与安全；
- 依据 `dde 色温属性（ColorTemperatureEnabled/Manual）` 探测结果实现护眼联动。

## 🗺️ Roadmap（未来迭代）

- 勿扰联动：待 dde-shell 开放公开 D-Bus/权限后接入（v1 因第三方权限边界暂缓）
- 全局快捷键快速开始/暂停；专注时自动隐藏消息弹层
- 云同步/多端成长；更多露露形态与季节皮肤
- 与全局搜索、控制中心插件组合扩展

## 📄 License

[GPL-3.0](LICENSE)
