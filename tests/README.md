# 测试文件

本目录包含项目的测试代码。

## 测试结构

- `unit/` - 单元测试
- `integration/` - 集成测试

## 运行测试

网页测试：`node --test gui/web/tests/*.test.cjs`。

TMAG固件回归：先执行 `cmake --build firmware/build`（Debug），安装
`python -m pip install unicorn`，再执行 `python tests/test_tmag_firmware.py`。
需要 ARM GNU 工具链的 `arm-none-eabi-nm` 及 Node.js 在 PATH 中。
可用 `FIRMWARE_BUILD_DIR` 指定其他Debug目录。

测试运行实际ARM二进制，以外设模型替代I2C和时间，验证寄存器应用、完整回包、
型号/数值校验、待机触发、连续模式10个新样本、超时喂狗、失败回执与Flash读写。
