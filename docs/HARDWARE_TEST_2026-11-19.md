# 硬件实测与算法节点工具交付

## 实际板卡结果

按 `.agents/skills/hwtest/SKILL.md` 执行：

```powershell
python firmware/tools/hwtest.py suite --seconds 10 --expect-rate 100
```

板卡为 COM9 / MA03 / STM32H743 / AK09974C，921600 bps；设备报告固件 **1.1.0**，flag=0x3B、LogLevel=3。本轮没有烧录，下面数据不能作为新 1.2.0 算法或节点遥测的硬件验收。测试结束已停止采集，没有修改 Flash 配置。

| 测试 | 结果 | 关键数据 |
|---|---|---|
| 导入状态同步 | PASS | 63.69 ms |
| 版本查询 | PASS | proto=2 / fw=1.1.0 |
| 日志级别查询 | PASS | level=3，57.67 ms |
| 配置读取 | PASS | MA03，flag=0x3B |
| 停止 | PASS | 停止后无样本 |
| 启动 | PASS | 135.26 ms |
| 采样率 | PASS | 83.871 Hz；在脚本 100 Hz ±20% 容差内，尚未达到 100 Hz |
| 串口负载 | PASS（记录项） | 2075 B/s，链路占用 2.3%，每样本 1 行 |
| 噪声 | WARN | 算法处理后的输出：σX=0、σY=0、σZ=32.6473；不能视为原始噪声或精度 |
| 行完整性 | PASS | 无法解码的行 0 |
| 流中命令 | PASS | 20 次，丢失 0、粘连 0；中位数 45.9 ms，最大 72.6 ms |

报告：`tests/hw_reports/hwtest-20261119-163807.json`；原始字节：同名 `.raw`。该目录按项目规则忽略 Git，仅保留在本地。

与最早报告 `hwtest-20261112-155336.json` 对比：

| 指标 | 11 月 12 日基线 | 本次 |
|---|---:|---:|
| 采样率 | 9.085 Hz | 83.871 Hz |
| 主机接收间隔中位数 | 124.22 ms | 0 ms |
| 主机接收间隔 p95 / 最大 | 126.24 / 127.58 ms | 63.24 / 64.8 ms |
| 命令时延中位数 / 最大 | 95.1 / 166 ms | 45.9 / 72.6 ms |

主机间隔受到 ST-Link USB 批量传输影响；0 ms 不代表传感器同时采样。板上旧固件没有输出设备微秒时间戳，无法据此评价精确采样抖动。

首轮报告 `hwtest-20261119-162936.json` 的启动项为 FAIL，`line=null`；原始字节实际包含 `Sensor started successfully`（十六进制：`53 65 6e 73 6f 72 20 73 74 61 72 74 65 64 20 73 75 63 63 65 73 73 66 75 6c 6c 79`）。原因是脚本仅匹配 `Sensor started`，并非设备未启动。修正脚本兼容两种独占行回复、将启动等待放宽到 15 秒后取得上述复测报告，没有调整固件或设备配置。

## Web 算法工具

运行 `gui/web/Start FIELD.cmd`，在主页面点击“算法节点”；也可访问本地服务的 `/algorithm.html`。`?demo=1` 可直接查看演示。文件入口为 [algorithm.html](../gui/web/algorithm.html)。

- 左侧按实际执行顺序列出当前配置档的节点；选择节点查看 XYZ、实际/参考曲线和相对上一节点的差值。
- 开关等待设备确认并读取状态后更新。只修改 RAM；不发 `CHFLAG` 或 `SAVECFG`，切换会重置滤波历史、清空页面曲线。
- 可选择 legacy/v2；切换配置档会停止采集，需要点击开始采集重新标定。legacy 只显示其真实存在的 8 个节点；S0 强制开启。
- 参考数据是各阶段使用的参考工作向量，S5 之后不再修改这条补偿参考支路；它不是 REFOUT 的额外平滑调试副本。S8/S9 实际通道已应用标度，单位由映射配置决定，参考仍为 µT。
- 按同一 seq 和时间戳组装完整帧，不混合节点；无效帧显示错误，曲线断开。支持暂停显示和最近 300 帧 CSV 导出，导出不会停止采集。
- 遥测最多 10 帧/秒，不等同于传感器采样率；独立长度 1 的队列保留最新快照，由 T_TX 输出。显示帧率和不完整帧数不代表总采样丢包数。
- 演示模式使用明确标注的合成示意数据，不复现完整固件算法。桌面 1440 px 和窄屏 390 px 已做浏览器验证。

当前连接的 **1.1.0 不支持节点接口**，工具会提示更新并禁止节点操作；新固件已构建但尚未烧录，所以本轮节点通信验证使用 ARM 模拟器及模拟串口，没有声称完成真实板卡的节点链路验收。

## 新调试协议

```text
ML NODES
NODES profile=1 flags=255 bypass=0 calibrated=1 trace=0 running=1

ML TRACE 1
TRACE 1

ML NODE 4 0
NODE s=4 enabled=0

N seq=123 t=123456 p=1 s=4 run=0 valid=1 err=0 x=1 y=2 z=3 rx=4 ry=5 rz=6
```

`profile/p`：0=legacy、1=v2；`bypass` 按节点编号的位控制。启用节点同时开启对应运行时算法 flag，但仍受标定前提限制；`run` 是该帧真正执行情况，不能只根据用户开关推定。S0 不可关闭；legacy 的 S1/S3 不存在。状态与开关通过现有配置互斥锁保护。

`ML TRACE 0` 关闭遥测。正常点击断开会先关闭遥测；浏览器崩溃/设备拔出无法保证送达，重新连接后可关闭，设备重启默认关闭。协议输出格式和 REFOUT 开关保持独立。

## 验证方式

```powershell
$env:FIRMWARE_BUILD_DIR='firmware/build/emu'
python tests/test_algo_pipeline.py
python tests/test_legacy_golden.py
node --test gui/web/tests/*.test.cjs
# 可选浏览器验证，需要 Playwright 和本机 Edge
python tests/test_algorithm_ui.py
```

最终结果：算法测试 **28/28**、Web 测试 **55/55**、浏览器流程 **2/2**，legacy 黄金回放 **70 组 × 100 样本**逐位通过。算法测试覆盖遥测开启前后数值一致、节点旁路、无效帧不重放旧数据和 RAM 命令；黄金样本保持不变。GUI 测试包括分帧、配置档顺序、开关确认、暂停、CSV、窄屏布局，以及后端静态资源和命令白名单。浏览器截图输出在 `tests/hw_reports/algorithm-desktop.png`、`algorithm-mobile.png`。

三板编译通过：H743 Flash 77,276 B / RAM 36,576 B；L432 Flash 64,432 B / RAM 7,880 B；F103 Flash 63,128 B / RAM 7,808 B。F103 的 64 KB Flash 已用 96.33%。

后续真实板卡验收需在烧录新固件后进行：节点数据与最终输出一致性、开关/配置档切换、遥测开关对速率与命令时延的影响、RTOS 栈水位、长时间稳定性和真实精度。
