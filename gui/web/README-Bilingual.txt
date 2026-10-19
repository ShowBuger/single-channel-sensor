FIELD 内部版 / Internal Edition - 中文 / English

解压完整压缩包，双击“启动上位机.cmd”、Start FIELD.cmd 或 FIELD.exe。
程序会自动在浏览器打开界面。使用期间保留服务控制台，关闭它可退出服务。
右上角“中文 / English”开关切换语言，默认中文，自动记住选择。
切换不刷新页面，不主动断开串口或清空采集数据。
配置导入、设备读回、日志导出和所有内部版功能均保留。
运行无需 Python、Node.js 或联网；需要 Windows .NET Framework 4.x 和 Edge/Chrome。
FIELD.exe、index.html 和全部 JavaScript 文件（包括 i18n.js）必须保持在同一目录。

Extract the entire ZIP and launch Start FIELD.cmd or FIELD.exe.
Keep the service console open. Use the top-right Chinese / English switch.
Chinese is the default. Your language selection is saved locally.
Switching preserves the active page, serial session, samples and JSON editor.
Configuration import, device readback and log export remain available.
No Python, Node.js or internet connection is required at runtime.
Requires Windows .NET Framework 4.x and a current Edge/Chrome browser.
Keep FIELD.exe, index.html and all JavaScript files including i18n.js together.

串口默认波特率 / Default baud rate: 921600.
原 MA01 分支实际上默认 MA02/TMAG3001；此行为保留，MA01 模板仍可选择。
The upstream MA01 branch defaults to MA02/TMAG3001; this behavior is preserved.
Select the correct product configuration for your hardware.
No firmware is included or flashed. Physical hardware operations require verification.
设备原始回复、用户 JSON 内容及协议命令不作为界面文案翻译。
Raw firmware replies, user JSON and protocol commands are device/user data.
刷新或关闭页面前请导出需要保留的数据和日志。
Export data and logs before refreshing or closing the page.

Source: https://github.com/ShowBuger/single-channel-sensor (branch: MA01)
Rebuild: powershell.exe -NoProfile -File gui/web/package-bilingual.ps1
