/* Runtime localization. Chinese remains the source language; language changes preserve the live session. */
(function(root){
'use strict';
const pairs=[
  [
    "配置无效：标志位需为 0–255，映射系数需为有限 float32 数值。",
    "Invalid configuration: flags must be 0–255; mapping factors must be finite float32 values."
  ],
  [
    "连接",
    "Connect"
  ],
  [
    "断开",
    "Disconnect"
  ],
  [
    "串口已断开",
    "Serial port disconnected"
  ],
  [
    "﻿FIELD 串口通信日志\r\n时间戳：UTC（ISO 8601）；RX 为设备原文，TX 为发送命令。\r\n",
    "﻿FIELD serial communication log\r\nTimestamp: UTC(ISO 8601); RX contains raw device text; TX contains sent commands.\r\n"
  ],
  [
    "设备已拔出",
    "Device unplugged"
  ],
  [
    "当前浏览器未提供 Web Serial，请用桌面版 Edge / Chrome 打开此页面。",
    "This browser does not provide Web Serial. Use desktop Edge / Chrome to open this page."
  ],
  [
    "连接正在使用，请先断开。",
    "Connection is in use. Disconnect first."
  ],
  [
    "串口文本行过长，已丢弃并重新同步。",
    "Serial line too long; discarded and resynchronized."
  ],
  [
    "已取消选择串口",
    "Serial port selection canceled"
  ],
  [
    "串口数据流已关闭",
    "Serial data stream closed"
  ],
  [
    "串口读取中断：",
    "Serial read interrupted: "
  ],
  [
    "串口已断开",
    "Serial port disconnected"
  ],
  [
    "关闭串口：",
    "Closing serial port: "
  ],
  [
    "丢弃格式无效的采样行：",
    "Discarded malformed sample: "
  ],
  [
    "设备在命令执行期间重启，请重新连接。",
    "Device restarted during a command. Please reconnect."
  ],
  [
    "设备已重启",
    "Device restarted"
  ],
  [
    "串口未连接或连接已改变。",
    "Serial port is not connected or the session changed."
  ],
  [
    "串口未连接。",
    "Serial port is not connected."
  ],
  [
    "无效的串口命令。",
    "Invalid serial command."
  ],
  [
    "收到采样数据，但未收到命令确认。",
    "Samples received, but no command acknowledgement."
  ],
  [
    "等待期间未收到完整串口数据，请检查端口、波特率及设备状态。",
    "No complete serial data received. Check the port, baud rate and device state."
  ],
  [
    "串口发送失败：",
    "Serial transmission failed: "
  ],
  [
    "串口发送失败",
    "Serial transmission failed"
  ],
  [
    "设备不支持完整配置读取，请更新固件。",
    "Full configuration readback is unsupported. Update the firmware."
  ],
  [
    "配置回包重复开始。",
    "Duplicate configuration start marker."
  ],
  [
    "配置回包缺少开始标记。",
    "Configuration response is missing its start marker."
  ],
  [
    "设备返回的配置缺少字段：",
    "Device configuration is missing fields: "
  ],
  [
    "设备配置回包过长。",
    "Device configuration response is too long."
  ],
  [
    "设备使用旧版配置回包，请更新固件后读取完整配置。",
    "Device uses an older configuration response. Update the firmware for full readback."
  ],
  [
    "完整配置读取失败：",
    "Full configuration read failed: "
  ],
  [
    "当前固件不支持配置导入，请更新固件。",
    "The firmware does not support configuration import. Update the firmware."
  ],
  [
    "Flash 保存失败。",
    "Flash save failed."
  ],
  [
    "Flash 保存失败，设备内存参数可能已改变；请读取配置核对。",
    "Flash save failed. Device memory may have changed; read back the configuration to verify."
  ],
  [
    "设备未确认 Flash 保存，参数可能仅在内存中改变。",
    "Device did not confirm Flash save; parameters may have changed only in memory."
  ],
  [
    "设备读回配置与期望值不一致，请重新读取并核对。",
    "Device readback differs from the expected configuration. Read and verify again."
  ],
  [
    "配置传输已取消，未提交应用。",
    "Configuration transfer canceled before applying."
  ],
  [
    "配置读回不一致：product。",
    "Configuration readback mismatch: product."
  ],
  [
    "；退出导入状态失败：",
    "; failed to exit import mode: "
  ],
  [
    "设备已确认应用并保存，但后续启动或校验未完成：",
    "Device confirmed application and save, but startup or verification is incomplete: "
  ],
  [
    "已提交应用，设备状态需核对：",
    "Application submitted; verify the device state: "
  ],
  [
    "product 产品型号必须是 MA01（MLX90393）或 MA02（TMAG3001）。",
    "Product model must be MA01(MLX90393) or MA02(TMAG3001)."
  ],
  [
    "传感器类型",
    "Sensor type"
  ],
  [
    "增益档位",
    "Gain setting"
  ],
  [
    "过采样档位",
    "Oversampling setting"
  ],
  [
    "数字滤波等级",
    "Digital filter level"
  ],
  [
    "X 轴分辨率",
    "X axis resolution"
  ],
  [
    "Y 轴分辨率",
    "Y axis resolution"
  ],
  [
    "Z 轴分辨率",
    "Z axis resolution"
  ],
  [
    "温度补偿",
    "Temperature compensation"
  ],
  [
    "磁场量程",
    "Magnetic field range"
  ],
  [
    "工作模式",
    "Operating mode"
  ],
  [
    "磁场测量通道",
    "Magnetic measurement channels"
  ],
  [
    "传感器版本",
    "Sensor version"
  ],
  [
    "算法功能标志",
    "Algorithm flags"
  ],
  [
    "X 轴映射系数",
    "X axis mapping factor"
  ],
  [
    "Y 轴映射系数",
    "Y axis mapping factor"
  ],
  [
    "Z 轴映射系数",
    "Z axis mapping factor"
  ],
  [
    "死区倍数",
    "Dead-zone multiplier"
  ],
  [
    "尖峰过滤倍数",
    "Spike filter multiplier"
  ],
  [
    "稳定性阈值",
    "Stability threshold"
  ],
  [
    "日志等级",
    "Log level"
  ],
  [
    "配置文件不能超过 64 KB。",
    "Configuration file must not exceed 64 KB."
  ],
  [
    "JSON 格式错误：",
    "JSON syntax error: "
  ],
  [
    "配置顶层必须是 JSON 对象。",
    "Configuration root must be a JSON object."
  ],
  [
    "不支持的 $schema。",
    "Unsupported $schema."
  ],
  [
    "product 产品型号须为 1–7 个字母、数字、下划线、点或短横线。",
    "Product model must contain 1–7 letters, digits, underscores, dots or hyphens."
  ],
  [
    "未知配置分组：",
    "Unknown configuration section: "
  ],
  [
    "配置分组重复：",
    "Duplicate configuration section: "
  ],
  [
    " 必须是对象。",
    " must be an object."
  ],
  [
    "未知配置字段：",
    "Unknown configuration field: "
  ],
  [
    "同一字段使用了重复别名：",
    "Duplicate aliases for the same field: "
  ],
  [
    " 必须是有限 float32 数值。",
    " must be a finite float32 value."
  ],
  [
    "配置文件未包含任何可用参数。",
    "Configuration contains no usable parameters."
  ],
  [
    "MA01 的 sensor.gain 必须为 0–7，sensor.osr 必须为 0–3。",
    "MA01 sensor.gain must be 0–7, sensor.osr must be 0–3."
  ],
  [
    "规范化配置超过 1023 字节，无法导入。",
    "Normalized configuration exceeds 1023 bytes; import unavailable."
  ],
  [
    "浏览器无法保存本地设置，本次会话内仍然有效。",
    "Browser could not save settings. They remain valid for this session."
  ],
  [
    "系统",
    "System"
  ],
  [
    "错误",
    "Error"
  ],
  [
    "zh-CN",
    "en-US"
  ],
  [
    "演示模式",
    "Demo mode"
  ],
  [
    "等待数据",
    "Waiting for data"
  ],
  [
    "设备连接",
    "Device connected"
  ],
  [
    "已断开",
    "Disconnected"
  ],
  [
    "断开串口",
    "Disconnect serial"
  ],
  [
    "连接串口",
    "Connect serial"
  ],
  [
    "暂停演示",
    "Pause demo"
  ],
  [
    "继续演示",
    "Resume demo"
  ],
  [
    "启动采集",
    "Start acquisition"
  ],
  [
    "停止采集",
    "Stop acquisition"
  ],
  [
    "演示采样",
    "Demo sampling"
  ],
  [
    "接收速率",
    "Receive rate"
  ],
  [
    "错误",
    "Error"
  ],
  [
    "未检测到串口",
    "No serial ports detected"
  ],
  [
    "未检测到串口，请插入设备后刷新。",
    "No serial ports detected. Connect the device and refresh."
  ],
  [
    "串口服务不可用",
    "Serial service unavailable"
  ],
  [
    "演示模式：模拟 10 组采样进度，不改变设备。",
    "Demo mode: simulate 10 sampling steps without changing the device."
  ],
  [
    "向设备发送 ML CALIB，等待真实标定结果。请先启动采集。",
    "Send ML CALIB and wait for actual calibration results. Start acquisition first."
  ],
  [
    "开始模拟标定",
    "Start simulated calibration"
  ],
  [
    "开始设备标定",
    "Start device calibration"
  ],
  [
    "检测到旧版固件：可接收采样数据，完整配置导入与读取需要更新固件。",
    "Older firmware detected. Sampling is available; full configuration import and readback require a firmware update."
  ],
  [
    "兼容",
    "Compatibility"
  ],
  [
    "连接",
    "Connect"
  ],
  [
    "已切换到演示模式，开始新的数据记录。",
    "Switched to demo mode; started a new recording."
  ],
  [
    "已继续演示采样。",
    "Demo sampling resumed."
  ],
  [
    "已暂停演示采样。",
    "Demo sampling paused."
  ],
  [
    "采集",
    "Acquisition"
  ],
  [
    "设备已确认启动采集（启动过程包含标定）。",
    "Device confirmed acquisition started (including calibration)."
  ],
  [
    "采集",
    "Acquisition"
  ],
  [
    "设备已确认停止采集。",
    "Device confirmed acquisition stopped."
  ],
  [
    "采集",
    "Acquisition"
  ],
  [
    "自动加载已开启：连接时读取设备完整配置。",
    "Auto-load enabled. Full device configuration will be read on connection."
  ],
  [
    "自动加载已关闭，可在导入配置中手动读取设备。",
    "Auto-load disabled. Read the device manually in Import configuration."
  ],
  [
    "配置",
    "Configuration"
  ],
  [
    "正在读取设备完整配置…",
    "Reading full device configuration..."
  ],
  [
    "已读取设备完整配置：传感器参数、算法参数与日志等级。",
    "Full device configuration loaded: sensor parameters, algorithm parameters and log level."
  ],
  [
    "配置",
    "Configuration"
  ],
  [
    "设备配置已加载",
    "Device configuration loaded"
  ],
  [
    "模拟标定完成 · 设备未改变",
    "Simulated calibration complete · Device unchanged"
  ],
  [
    "模拟标定完成。",
    "Simulated calibration complete."
  ],
  [
    "标定",
    "Calibration"
  ],
  [
    "设备正在标定，等待回执…",
    "Device is calibrating; waiting for acknowledgement..."
  ],
  [
    "设备已确认标定完成",
    "Device confirmed calibration complete"
  ],
  [
    "设备标定完成。",
    "Device calibration complete."
  ],
  [
    "标定",
    "Calibration"
  ],
  [
    "标定未完成：",
    "Calibration incomplete: "
  ],
  [
    "暂无采样数据可导出。",
    "No samples available to export."
  ],
  [
    "导出",
    "Export"
  ],
  [
    "CSV 数据已保存",
    "CSV data saved"
  ],
  [
    "暂无串口日志，请先连接设备。",
    "No serial log available. Connect a device first."
  ],
  [
    "串口日志已导出（.log）",
    "Serial log exported (.log)"
  ],
  [
    "暂无数据或日志可清空。",
    "No data or logs to clear."
  ],
  [
    "清空数据前已自动停止采集。",
    "Acquisition automatically stopped before clearing data."
  ],
  [
    "系统",
    "System"
  ],
  [
    "系统",
    "System"
  ],
  [
    "数据和日志已清空",
    "Data and logs cleared"
  ],
  [
    "请先连接设备。",
    "Connect a device first."
  ],
  [
    "设备未连接。",
    "Device is not connected."
  ],
  [
    "确定要复位设备吗？设备将重新启动，当前连接会断开。",
    "Reset the device? It will restart and the current connection will close."
  ],
  [
    "复位前已停止采集。",
    "Acquisition stopped before reset."
  ],
  [
    "系统",
    "System"
  ],
  [
    "已发送复位命令，设备将重启。",
    "Reset command sent. The device will restart."
  ],
  [
    "系统",
    "System"
  ],
  [
    "设备正在复位...",
    "Device resetting..."
  ],
  [
    "设备复位完成，连接已断开。请重新连接设备。",
    "Device reset complete; disconnected. Please reconnect the device."
  ],
  [
    "系统",
    "System"
  ],
  [
    "连接",
    "Connect"
  ],
  [
    "发送",
    "Send"
  ],
  [
    "设备",
    "Device"
  ],
  [
    "检测到设备重启，请重新读取配置。",
    "Device restart detected. Read the configuration again."
  ],
  [
    "系统",
    "System"
  ],
  [
    "警告",
    "Warning"
  ],
  [
    "警告",
    "Warning"
  ],
  [
    "示意形变 · 箭头沿坡面指向低处",
    "Illustrative deformation · Arrows point downhill"
  ],
  [
    "示意形变 · 方向箭头已隐藏",
    "Illustrative deformation · Direction arrows hidden"
  ],
  [
    "最新",
    "Now"
  ],
  [
    "等待采样数据",
    "Waiting for samples"
  ],
  [
    "设备模式已就绪，请连接串口开始采集。",
    "Device mode ready. Connect a serial port to start acquisition."
  ],
  [
    "本地设置不可读取，本次使用默认配置。",
    "Local settings unavailable. Using defaults for this session."
  ],
  [
    "关闭",
    "Off"
  ],
  [
    "错误",
    "Error"
  ],
  [
    "警告",
    "Warning"
  ],
  [
    "信息",
    "Info"
  ],
  [
    "调试",
    "Debug"
  ],
  [
    "启用",
    "On"
  ],
  [
    "关闭",
    "Off"
  ],
  [
    "待机",
    "Standby"
  ],
  [
    "睡眠",
    "Sleep"
  ],
  [
    "连续测量",
    "Continuous measurement"
  ],
  [
    "唤醒与睡眠",
    "Wake and sleep"
  ],
  [
    "关闭",
    "Off"
  ],
  [
    "X 轴",
    "X axis"
  ],
  [
    "Y 轴",
    "Y axis"
  ],
  [
    "X、Y 轴",
    "X, Y axis"
  ],
  [
    "Z 轴",
    "Z axis"
  ],
  [
    "X、Z 轴",
    "X, Z axis"
  ],
  [
    "Y、Z 轴",
    "Y, Z axis"
  ],
  [
    "X、Y、Z 轴",
    "X, Y, Z axis"
  ],
  [
    "文件值",
    "File value"
  ],
  [
    "文件值",
    "File value"
  ],
  [
    "文件超过 64 KB，请选择传感器 JSON 配置文件。",
    "File exceeds 64 KB. Select a sensor JSON configuration file."
  ],
  [
    "编辑值",
    "Edited value"
  ],
  [
    "已编辑 · 待应用配置",
    "Edited · Pending configuration"
  ],
  [
    "内容已修改，请重新校验。",
    "Content changed. Please validate again."
  ],
  [
    "等待当前分块回执后取消…",
    "Canceling after the current chunk acknowledgement..."
  ],
  [
    "设备读回",
    "Device readback"
  ],
  [
    "设备配置 · ",
    "Device configuration · "
  ],
  [
    "zh-CN",
    "en-US"
  ],
  [
    "设备完整配置已读取，未修改设备。可编辑后应用，或导出配置。",
    "Full device configuration read without modifying the device. Edit and apply, or export a backup."
  ],
  [
    "已连接 · 可应用到设备",
    "Connected · Ready to apply to device"
  ],
  [
    "未连接 · 可导入预览，连接后应用",
    "Not connected · Preview available; connect to apply"
  ],
  [
    "修正配置后重新校验。",
    "Correct the configuration and validate again."
  ],
  [
    "产品信息",
    "Product information"
  ],
  [
    "产品型号",
    "Product model"
  ],
  [
    "传感器寄存器参数",
    "Sensor register parameters"
  ],
  [
    "算法参数",
    "Algorithm parameters"
  ],
  [
    "系统日志",
    "System log"
  ],
  [
    "默认补全",
    "Default value"
  ],
  [
    " · 仅存储，不生效",
    " · Stored only; inactive"
  ],
  [
    "这是完整配置。 ",
    "Configuration is complete. "
  ],
  [
    "应用可能重新标定，请保持传感器静止。",
    "Applying may recalibrate the sensor. Keep it still."
  ],
  [
    "准备导入",
    "Preparing import"
  ],
  [
    "正在发送",
    "Sending"
  ],
  [
    "设备应用与保存中",
    "Applying and saving on device"
  ],
  [
    "启动采集与标定中",
    "Starting acquisition and calibration"
  ],
  [
    "读取参数校验中",
    "Reading back and verifying"
  ],
  [
    "应用完成",
    "Applied"
  ],
  [
    "设备已确认应用并保存；全部配置字段读回一致。",
    "Device confirmed application and save; all fields match the readback."
  ],
  [
    "配置已按产品型号应用并保存，全部字段读回一致。",
    "Configuration applied and saved for the product; all fields match the readback."
  ],
  [
    "配置",
    "Configuration"
  ],
  [
    "配置已保存，运行参数已核对",
    "Configuration saved; runtime parameters verified"
  ],
  [
    "请双击“启动上位机.cmd”，由本地程序自动打开界面并识别 COM 端口。",
    "Double-click \"Start FIELD.cmd\" to open the interface and detect COM ports."
  ],
  [
    "当前为静态预览。请双击“启动上位机.cmd”启用串口。",
    "Static preview. Double-click \"Start FIELD.cmd\" to enable serial communication."
  ],
  [
    "本地通信服务不匹配。",
    "Local communication service mismatch."
  ],
  [
    "本地通信失败",
    "Local communication failed"
  ],
  [
    "请选择检测到的串口。",
    "Select a detected serial port."
  ],
  [
    "串口已断开",
    "Serial port disconnected"
  ],
  [
    "不支持的操作。",
    "Unsupported operation."
  ],
  [
    "页面停止响应，串口已自动释放。",
    "Page stopped responding. Serial port released automatically."
  ],
  [
    "串口正在使用，请在原页面断开后重试。",
    "Serial port is in use. Disconnect it on the original page and retry."
  ],
  [
    "端口已移除，请刷新端口列表。",
    "Port removed. Refresh the port list."
  ],
  [
    "不支持的波特率。",
    "Unsupported baud rate."
  ],
  [
    "接收缓存溢出，连接已关闭，请重新连接。",
    "Receive buffer overflow. Connection closed; please reconnect."
  ],
  [
    "串口读取中断：",
    "Serial read interrupted: "
  ],
  [
    "串口会话已改变，请重新连接。",
    "Serial session changed. Please reconnect."
  ],
  [
    "无效的设备命令。",
    "Invalid device command."
  ],
  [
    "串口已断开。",
    "Serial port disconnected."
  ],
  [
    "串口被其他程序占用或访问被拒绝，请先关闭原上位机。",
    "Serial port is busy or access was denied. Close the other sensor application first."
  ],
  [
    "传感上位机",
    "FIELD Sensor Monitor"
  ],
  [
    "实时监测",
    "Live monitor"
  ],
  [
    "导入配置",
    "Import configuration"
  ],
  [
    "数据记录",
    "Recordings"
  ],
  [
    "连接串口",
    "Connect serial"
  ],
  [
    "设备模式",
    "Device mode"
  ],
  [
    "演示模式",
    "Demo mode"
  ],
  [
    "启动标定",
    "Calibrate"
  ],
  [
    "保存数据",
    "Save data"
  ],
  [
    "信号总览 ",
    "Signal overview "
  ],
  [
    "接收速率",
    "Receive rate"
  ],
  [
    "数据点 ",
    "Data points "
  ],
  [
    "时长 ",
    "Duration "
  ],
  [
    "三维传感平面",
    "3D sensor surface"
  ],
  [
    "箭头",
    "Arrows"
  ],
  [
    "透视",
    "Perspective"
  ],
  [
    "俯视",
    "Top"
  ],
  [
    "MLX90393 · 单点形变示意",
    "MLX90393 · Single-point deformation model"
  ],
  [
    "拖动旋转 · 滚轮缩放",
    "Drag to rotate · Scroll to zoom"
  ],
  [
    "弱",
    "Low"
  ],
  [
    "|Z| 强度 · 强",
    "|Z| Intensity · High"
  ],
  [
    "X 轴输出",
    "X axis output"
  ],
  [
    "Y 轴输出",
    "Y axis output"
  ],
  [
    "Z 轴输出",
    "Z axis output"
  ],
  [
    "单点数据示意 · 非实测空间形貌",
    "Single-point illustration · Not a measured spatial profile"
  ],
  [
    "合成幅值 ",
    "Magnitude "
  ],
  [
    "X 轴曲线",
    "X axis chart"
  ],
  [
    "最近 30 s",
    "Last 30 s"
  ],
  [
    "峰峰值 ",
    "Peak-to-peak "
  ],
  [
    "Y 轴曲线",
    "Y axis chart"
  ],
  [
    "最近 30 s",
    "Last 30 s"
  ],
  [
    "峰峰值 ",
    "Peak-to-peak "
  ],
  [
    "Z 轴曲线",
    "Z axis chart"
  ],
  [
    "最近 30 s",
    "Last 30 s"
  ],
  [
    "峰峰值 ",
    "Peak-to-peak "
  ],
  [
    "未连接",
    "Not connected"
  ],
  [
    "设备模式已就绪，请连接串口开始采集。",
    "Device mode ready. Connect a serial port to start acquisition."
  ],
  [
    "查看记录",
    "View recordings"
  ],
  [
    "串口连接",
    "Serial connection"
  ],
  [
    "端口号",
    "Port"
  ],
  [
    "正在扫描串口…",
    "Scanning serial ports..."
  ],
  [
    "波特率",
    "Baud rate"
  ],
  [
    "取消",
    "Cancel"
  ],
  [
    "连接",
    "Connect"
  ],
  [
    "零点标定",
    "Zero calibration"
  ],
  [
    "请保持传感器静止，并移开外部磁场干扰。设备标定将重新计算实际与参考传感器的偏移及标准差。",
    "Keep the sensor still and remove external magnetic interference. Calibration recalculates offsets and standard deviations for the actual and reference sensors."
  ],
  [
    "请连接设备后执行标定。",
    "Connect a device before calibrating."
  ],
  [
    "准备采集",
    "Preparing acquisition"
  ],
  [
    "关闭",
    "Close"
  ],
  [
    "开始设备标定",
    "Start device calibration"
  ],
  [
    "数据记录",
    "Recordings"
  ],
  [
    "当前缓存数据点",
    "Buffered samples"
  ],
  [
    "串口日志记录 · LOG",
    "Serial log entries · LOG"
  ],
  [
    "最多保留最近 12,000 个采样点。切换数据来源、重新连接或刷新页面将开始新的记录。",
    "Retains up to 12,000 samples. Switching sources, reconnecting or refreshing starts a new recording."
  ],
  [
    "串口日志包含本页连接后的全部收发文本（含采样行）、连接状态和错误；断开、重连后仍可导出。刷新或关闭页面会清空，请及时保存。下方仅预览最近 150 条事件。",
    "Serial logs include all sent and received text (including samples), connection states and errors since connecting on this page. Logs remain available after disconnecting or reconnecting. Save before refreshing or closing the page. Preview shows the latest 150 events."
  ],
  [
    "关闭",
    "Close"
  ],
  [
    "清空数据",
    "Clear data"
  ],
  [
    "复位设备",
    "Reset device"
  ],
  [
    "导出数据 CSV",
    "Export data CSV"
  ],
  [
    "导出日志 .log",
    "Export log .log"
  ],
  [
    "导入配置",
    "Import configuration"
  ],
  [
    "选择配置文件",
    "Choose configuration file"
  ],
  [
    "读取设备配置",
    "Read device configuration"
  ],
  [
    "模板型号",
    "Template model"
  ],
  [
    "下载默认模板",
    "Download defaults"
  ],
  [
    "导出配置",
    "Export configuration"
  ],
  [
    "自动加载配置",
    "Auto-load configuration"
  ],
  [
    "支持固件示例配置，也可粘贴 JSON。",
    "Open a firmware example configuration or paste JSON."
  ],
  [
    "配置内容",
    "Configuration JSON"
  ],
  [
    "校验配置",
    "Validate configuration"
  ],
  [
    "导入文件只生成预览，点击“应用到设备”才会发送。",
    "Import creates a preview. Select Apply to device to send it."
  ],
  [
    "选择文件后可查看参数、默认补全项和传输大小。",
    "Select a file to preview parameters, default values and transfer size."
  ],
  [
    "应用后启动采集（执行标定）",
    "Start acquisition after applying (includes calibration)"
  ],
  [
    "取消传输",
    "Cancel transfer"
  ],
  [
    "应用到设备",
    "Apply to device"
  ],
  [
    "主导航",
    "Main navigation"
  ],
  [
    "采集工具栏",
    "Acquisition toolbar"
  ],
  [
    "切换来源将开始新的数据记录",
    "Switching sources starts a new recording"
  ],
  [
    "数据来源",
    "Data source"
  ],
  [
    "停止采集",
    "Stop acquisition"
  ],
  [
    "停止采集",
    "Stop acquisition"
  ],
  [
    "视角切换",
    "View controls"
  ],
  [
    "三维形变曲面，可拖动旋转，方向键调整视角",
    "3D deformation surface; drag to rotate or use arrow keys"
  ],
  [
    "重置视角",
    "Reset view"
  ],
  [
    "重置视角",
    "Reset view"
  ],
  [
    "放大",
    "Zoom in"
  ],
  [
    "放大",
    "Zoom in"
  ],
  [
    "缩小",
    "Zoom out"
  ],
  [
    "缩小",
    "Zoom out"
  ],
  [
    "三轴实时曲线",
    "Live XYZ charts"
  ],
  [
    "X 轴最近 30 秒输出曲线",
    "X axis: last 30 seconds of output"
  ],
  [
    "Y 轴最近 30 秒输出曲线",
    "Y axis: last 30 seconds of output"
  ],
  [
    "Z 轴最近 30 秒输出曲线",
    "Z axis: last 30 seconds of output"
  ],
  [
    "关闭",
    "Close"
  ],
  [
    "刷新端口",
    "Refresh ports"
  ],
  [
    "刷新端口",
    "Refresh ports"
  ],
  [
    "关闭",
    "Close"
  ],
  [
    "关闭",
    "Close"
  ],
  [
    "关闭",
    "Close"
  ],
  [
    "连接时读取设备配置，已连接时开启将立即读取。",
    "Read device configuration on connection. Enabling while connected reads it immediately."
  ],
  [
    "配置预览",
    "Configuration preview"
  ],
  [
    "配置传输进度",
    "Configuration transfer progress"
  ]
];
const forward=new Map(),reverse=new Map();
function register(zh,en){if(zh===en)return;if(!forward.has(zh))forward.set(zh,en);if(!reverse.has(en))reverse.set(en,zh);}
for(const pair of pairs)register(...pair);
let language='zh-CN';try{if(localStorage.getItem('field-language')==='en-US')language='en-US';}catch{}
let revision=0,cachedRevision=-1,zhPattern,enPattern;
const escape=s=>s.replace(/[.*+?^${}()|[\]\\]/g,'\\$&');
function patterns(){if(cachedRevision===revision)return;const make=map=>new RegExp([...map.keys()].filter(Boolean).sort((a,b)=>b.length-a.length).map(escape).join('|'),'g');zhPattern=make(forward);enPattern=make(reverse);cachedRevision=revision;}
function translate(text,lang=language){if(typeof text!=='string'||!text)return text;const map=lang==='en-US'?forward:reverse;if(map.has(text))return map.get(text);patterns();return text.replace(lang==='en-US'?zhPattern:enPattern,part=>map.get(part));}
function t(zh,en){if(!forward.has(zh)){register(zh,en);revision++;}return language==='en-US'?en:zh;}
const originals=new WeakMap();
function ignored(node){return node.parentElement?.closest('script,style,textarea,input,[data-i18n-ignore]');}
function renderValue(owner,key,value,setter){let records=originals.get(owner);if(!records){records=new Map();originals.set(owner,records);}let record=records.get(key);if(!record||value!==record.rendered){record={zh:language==='en-US'?translate(value,'zh-CN'):value,rendered:value};records.set(key,record);}const closeControl=(owner.nodeType===3?owner.parentElement:owner)?.hasAttribute?.('data-close');const next=language==='en-US'?(record.zh==='??'&&closeControl?'Close':translate(record.zh,'en-US')):record.zh;if(next!==value)setter(next);record.rendered=next;}
function render(){
 const walker=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT);let node;
 while((node=walker.nextNode())){if(!ignored(node)&&node.nodeValue.trim())renderValue(node,'text',node.nodeValue,value=>node.nodeValue=value);}
 for(const el of document.querySelectorAll('[title],[aria-label],[placeholder]')){if(el.closest('[data-i18n-ignore],textarea,input'))continue;for(const key of ['title','aria-label','placeholder'])if(el.hasAttribute(key))renderValue(el,key,el.getAttribute(key),value=>el.setAttribute(key,value));}
 document.documentElement.lang=language;
 const control=document.getElementById('languageSwitch');if(control)control.checked=language==='en-US';
 const name=document.getElementById('languageName');if(name)name.textContent=language==='en-US'?'English':'中文';
}
let observer;
function refresh(){observer?.disconnect();render();observer?.observe(document.body,{subtree:true,childList:true,characterData:true,attributes:true,attributeFilter:['title','aria-label','placeholder']});}
function setLanguage(next){language=next==='en-US'?'en-US':'zh-CN';try{localStorage.setItem('field-language',language);}catch{}refresh();root.dispatchEvent(new CustomEvent('field-language-change',{detail:language}));}
root.FieldI18n={t,translate,setLanguage,get language(){return language;}};
if(typeof document!=='undefined'){
 // Capture the original Chinese DOM before any English rendering occurs.
 const chosen=language;language='zh-CN';render();language=chosen;
 observer=new MutationObserver(refresh);refresh();
 document.getElementById('languageSwitch')?.addEventListener('change',event=>setLanguage(event.target.checked?'en-US':'zh-CN'));
}
})(globalThis);
