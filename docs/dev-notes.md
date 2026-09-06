# 开发过程说明 · 专注助手 Focus Assistant

> deepin Skills 开发大赛（10 亿 Token 奖池）· 方向三：DTK 原生应用
> 作品名：专注助手 Focus Assistant（focus-assistant） · License: GPL-3.0

## 一、用到的 Skill

- **`dtk-development`**（deepin Skills 内置）——核心开发依据：
  1. 工程模板：`assets/cmake/CMakeLists-DTK6.txt`、`assets/src/main.cpp`、`assets/desktop/myapp.desktop`；
  2. 模块文档：`app-dev-with-dtk.md`（CMake/依赖/头文件）、`platform-abstraction.md`（无边框/半透明/平台抽象）、
     `utilities/dbus.md`（DBus 调用）、`config/index.md`（DConfig 权限边界）；
  3. 关键技术决策来自 Skill 文档：
     - 使用 DTK6/Qt6 链路，不混用 DTK5/Qt5；
     - 半透明无边框窗口使用 `WA_TranslucentBackground` + `FramelessWindowHint` + 自绘圆角卡片；
     - 系统联动选择 **org.deepin.dde.Display1 色温属性**（护眼）而非改动他人 DConfig；
     - 依据 DConfig 权限边界：第三方应用**不可写 dde-shell 的勿扰配置**，因此 v1 将「自动勿扰」列为 Roadmap，保证可靠与安全；
     - 通知采用 `org.freedesktop.Notifications.Notify`（见 dbus.md 示例）。

> 备注：完整的 AI 编程对话记录截图（含调用 deepin Skills 的上下文）见提交帖附件。

## 二、实现要点

| 阶段 | 内容 |
| --- | --- |
| 1. 概念设计 | 桌面陪伴型番茄钟「露露」：专注 → 成长 → 情绪反馈 → 健康提醒 |
| 2. 工程搭建 | DTK6 标准 CMake + DApplication + 资源 + .desktop + 安装/卸载/CPack |
| 3. 核心逻辑 | FocusManager 状态机（Idle/Focus/Pause/Break）、成长阶段、跨天统计、QSettings 持久化 |
| 4. 视觉 | PetWidget 纯 QPainter 矢量绘制：花盆/茎叶/花盘/表情；呼吸、眨眼、粒子庆祝（低帧率低占用） |
| 5. 系统联动 | SystemLinker：护眼色温保存/恢复 + 桌面通知（D-Bus，X11 实测） |
| 6. 窗口 | 无边框半透明圆角卡片、按住露露拖动、双击开始/暂停、托盘常驻 |
| 7. 数据 | 今日/7 天柱状图/累计/阶段，跨天自动重置连续专注 |
| 8. 验证 | 构建 → 运行 → 点击开始/暂停/放弃/完成自动休息 → 统计弹窗 → 打包 .deb |

## 三、如何复现构建与运行

见根目录 README（构建 / 安装 / 卸载 / 使用）。

## 四、演示材料

- 截图：`docs/screenshots/*.png`（待机、专注、休息、统计、交互态）
- 视频：见提交帖（可现场演示：开始专注→露露守护→撒花庆祝→休息打盹）

## 五、安全与合规

- 全程无网络通信、无用户数据采集；仅本地 QSettings；
- 权限最小化：只读/写自身配置；护眼联动仅经系统公开 D-Bus 属性，并自动还原；
- 可独立构建、安装、卸载（make install / uninstall 与 .deb 双通道）。

## 六、v1.0.1 修复：护眼联动可靠性（2026-09-06）

实测发现“专注开始自动开护眼（DDE 色温）”存在三个问题并已修复：

1. **误写只读属性**：原实现直接 `setProperty(ColorTemperatureManual)`，但该属性为只读，
   正确做法是调用 daemon 方法 `SetColorTemperature(int)` 设置色温值后再开 `ColorTemperatureEnabled`。
2. **退出不还原**：托盘“退出”路径未还原色温，导致系统色温残留、桌面整体偏暖（表现为启动器等界面异常）。
   修复：`aboutToQuit` 兜底还原 + 强杀/崩溃场景用 QSettings 残留标记（`[eye]`），下次启动自动清理。
3. **默认开启太激进**：系统级色温会改变整个桌面观感，默认改为关闭（设置中可开启），
   并对“用户本已开启夜间/自定义色温”的场景保持不打扰、不改动其色温数值。

验证：开始专注→`ColorTemperatureEnabled=true`+写入残留标记；放弃/结束→`false`+清除标记；
`kill -9` 模拟崩溃后重启→自动还原色温并清除标记；默认无配置时不再开启色温。
