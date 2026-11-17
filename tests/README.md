# 测试文件

本目录包含项目的测试代码。

## 测试结构

- `unit/` - 单元测试
- `integration/` - 集成测试

## 运行测试

网页测试：`node --test gui/web/tests/*.test.cjs`。

固件按板型构建：`firmware/tools/build.ps1 -Board <ma01_l432|ma02_f103|ma03_h743>`，
所有板型统一输出到 `firmware/build`；切换板型时脚本会清理旧构建目录。
板型同时决定 MCU 与编入的传感器驱动（MA01=MLX90393，MA02=TMAG3001，MA03=AK09974C）。

TMAG固件回归（MA02 / STM32F103）：先执行 `firmware/tools/build.ps1 -Board ma02_f103`，安装
`python -m pip install unicorn`，再执行
`set FIRMWARE_BUILD_DIR=firmware/build` 与 `python tests/test_tmag_firmware.py`。
需要 ARM GNU 工具链的 `arm-none-eabi-nm` 及 Node.js 在 PATH 中。

测试运行实际ARM二进制，以外设模型替代I2C和时间，验证寄存器应用、完整回包、
型号/数值校验、待机触发、连续模式10个新样本、超时喂狗、失败回执与Flash读写。

MA03（AK09974C）固件回归：使用下文的 H743 模拟构建（`-DBOARD=ma03_h743 -DFW_EMU_SP_FPU=ON`），
再执行 `python tests/test_ak09974c_firmware.py`（`FIRMWARE_BUILD_DIR` 指向该构建目录）。测试以寄存器级AK09974C模型替代I2C，验证
VIO→软复位→ID→设置→RUN 的初始化顺序、运行中禁止改设置、RUN单独写入、常规/宽量程换算、
连续模式等待新数据、单次触发、超时、`ML GETCFG` 与 `config_ma03.json` 模板。
自检/阈值等可选API需按测试文件说明的链接参数保留，否则该项跳过。

## H743 模拟构建与算法黄金样本（重构 P0）

Unicorn 的 Cortex-M7 模型不支持双精度 VFP 指令，H743 正式固件（`fpv5-d16`）中的
`strtod` 等会触发 `UC_ERR_INSN_INVALID`。模拟测试需使用单精度 FPU 的专用构建
（C 代码路径相同，double 由软件库实现），正式固件不得使用此选项：

```
cmake -B firmware/build/emu -S firmware -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFW_EMU_SP_FPU=ON
cmake --build firmware/build/emu
set FIRMWARE_BUILD_DIR=firmware/build/emu
python tests/test_ak09974c_firmware.py
python tests/test_legacy_golden.py
```

`test_legacy_golden.py` 在模拟器中运行现有 `data_process()`，用 7 组确定性激励
（静态噪声、阶跃、斜坡、尖峰、共模干扰、Z 符号变化、无效样本）× 10 组开关/标定组合，
与 `tests/golden/legacy_pipeline_ak09974c.json` 逐位比较。重构后的 legacy 配置档必须
保持逐位一致（见 `docs/REFACTOR_DESIGN.md` 第 17.4、17.9 节）。只有在基线提交上才允许
用 `--record` 重新录制。预测补偿依赖 `MEASURE_INTERVAL`，因此黄金样本按传感器型号区分，
当前只录制了 AK09974C。
