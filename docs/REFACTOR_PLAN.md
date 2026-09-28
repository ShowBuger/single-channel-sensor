# 仓库目录重构方案

## 目标

创建清晰、模块化、易维护的目录结构，符合嵌入式项目最佳实践。

## 新目录结构

```
single_channel_sensor/
├── docs/                           # 📚 文档目录
│   ├── README.md                   # 项目主文档
│   ├── COMMIT_CONVENTION.md        # Git提交规范
│   ├── API.md                      # API文档
│   ├── 串口命令.md                  # 串口命令说明
│   └── hardware/                   # 硬件相关文档
│       └── 原理图和PCB设计文件
│
├── firmware/                       # 💾 固件代码（原Software目录）
│   ├── src/                        # 源代码
│   │   ├── main.c                  # 主程序
│   │   ├── app/                    # 应用层
│   │   │   ├── sensor_manager.c/h  # 传感器管理
│   │   │   ├── command_handler.c/h # 命令处理（从main.c提取）
│   │   │   └── config_manager.c/h  # 配置管理
│   │   │
│   │   ├── drivers/                # 驱动层
│   │   │   ├── bsp/                # 板级支持包
│   │   │   │   ├── bsp_i2c.c/h     # I2C驱动封装
│   │   │   │   ├── bsp_uart.c/h    # 串口驱动封装
│   │   │   │   └── bsp_delay.c/h   # 延时驱动
│   │   │   │
│   │   │   └── sensors/            # 传感器驱动
│   │   │       ├── mlx90393.c/h
│   │   │       ├── tmag3001.c/h
│   │   │       └── tca9548a.c/h    # I2C多路复用器
│   │   │
│   │   ├── algorithm/              # 算法层
│   │   │   ├── filter.c/h          # 滤波算法
│   │   │   ├── calibration.c/h     # 校准算法
│   │   │   └── compensation.c/h    # 补偿算法
│   │   │
│   │   └── utils/                  # 工具函数
│   │       └── storage.c/h         # Flash存储
│   │
│   ├── libs/                       # 第三方库
│   │   ├── STM32F10x_StdPeriph_Driver/  # 标准外设库
│   │   └── CMSIS/                  # CMSIS库
│   │
│   ├── build/                      # 构建输出（.gitignore）
│   │   ├── obj/
│   │   └── bin/
│   │
│   └── project/                    # 项目文件
│       ├── keil/                   # Keil工程文件
│       └── debug/                  # 调试配置
│
├── gui/                            # 🖥️ 上位机GUI
│   ├── display/                    # 显示版本
│   │   ├── src/
│   │   │   ├── main.py
│   │   │   ├── gui/
│   │   │   ├── core/
│   │   │   └── config/
│   │   ├── resources/
│   │   └── requirements.txt
│   │
│   └── test/                       # 测试版本
│       └── （同上结构）
│
├── hardware/                       # ⚡ 硬件设计文件
│   ├── pcb/                        # PCB设计
│   │   └── 单路串口通信板.epro2
│   ├── schematic/                  # 原理图
│   └── bom/                        # 物料清单
│
├── tools/                          # 🔧 开发工具
│   ├── flash.sh                    # 烧录脚本
│   ├── serial_monitor.py           # 串口监控工具
│   └── hooks/                      # Git hooks备份
│
├── tests/                          # 🧪 测试文件
│   ├── unit/                       # 单元测试
│   └── integration/                # 集成测试
│
├── .clang-format                   # 代码格式配置
├── .gitignore                      # Git忽略规则
└── README.md                       # 项目说明（精简版）
```

## 重构步骤

### 阶段1: 文档整理
1. 创建 `docs/` 目录
2. 移动所有 `.md` 文件到 `docs/`
3. 移动 `Hardware/` 内容到 `hardware/pcb/`

### 阶段2: 固件代码重组
1. 重命名 `Software/` → `firmware/`
2. 创建新的子目录结构
3. 按功能分类移动源文件：
   - 驱动代码 → `firmware/src/drivers/`
   - 算法代码 → `firmware/src/algorithm/`
   - 应用代码 → `firmware/src/app/`
4. 重命名标准库目录
5. 整理项目文件

### 阶段3: GUI代码规范化
1. 重命名 `GUI/` → `gui/`（小写）
2. 规范子目录名称
3. 添加 `requirements.txt`

### 阶段4: 工具和测试
1. 创建 `tools/` 目录
2. 创建 `tests/` 目录骨架

## 文件映射表

### 应用层
```
Software/User/main.c                    → firmware/src/main.c
Software/Hardware/sensor_manager.c/h    → firmware/src/app/sensor_manager.c/h
Software/Hardware/config_storage.c/h    → firmware/src/app/config_manager.c/h
```

### 驱动层 - BSP
```
Software/Hardware/HardI2C.c/h           → firmware/src/drivers/bsp/bsp_i2c.c/h
Software/Hardware/SoftI2C.c/h           → firmware/src/drivers/bsp/bsp_i2c_soft.c/h
Software/Hardware/Serial.c/h            → firmware/src/drivers/bsp/bsp_uart.c/h
Software/System/Delay.c/h               → firmware/src/drivers/bsp/bsp_delay.c/h
Software/System/DMA.c/h                 → firmware/src/drivers/bsp/bsp_dma.c/h
```

### 驱动层 - 传感器
```
Software/Hardware/sensor_driver/mlx90393.c/h    → firmware/src/drivers/sensors/mlx90393.c/h
Software/Hardware/sensor_driver/tmag3001.c/h    → firmware/src/drivers/sensors/tmag3001.c/h
Software/Hardware/TCA9548A.c/h                  → firmware/src/drivers/sensors/tca9548a.c/h
```

### 库文件
```
Software/Library/                       → firmware/libs/STM32F10x_StdPeriph_Driver/
Software/Start/                         → firmware/libs/CMSIS/
```

### 项目文件
```
Software/*.uvprojx                      → firmware/project/keil/
Software/DebugConfig/                   → firmware/project/debug/
```

### 构建输出
```
Software/Objects/                       → firmware/build/obj/
Software/Listings/                      → firmware/build/listings/
```

### 文档
```
README.md                               → docs/README.md
COMMIT_CONVENTION.md                    → docs/COMMIT_CONVENTION.md
串口命令.md                              → docs/serial_commands.md
```

### 硬件
```
Hardware/单路串口通信板.epro2            → hardware/pcb/单路串口通信板.epro2
```

### GUI
```
GUI/DISPLAY/                            → gui/display/
GUI/TEST/                               → gui/test/
```

## 优势

### 1. 清晰的分层架构
- **应用层** (`app/`): 业务逻辑
- **驱动层** (`drivers/`): 硬件抽象
- **算法层** (`algorithm/`): 数据处理
- **库层** (`libs/`): 第三方代码

### 2. 易于维护
- 模块职责明确
- 文件命名统一
- 便于查找和修改

### 3. 符合行业规范
- 参考主流嵌入式项目结构
- 便于团队协作
- 利于项目交接

### 4. 可扩展性
- 添加新传感器驱动很容易
- 算法模块可独立开发测试
- 支持多种IDE工程

## 注意事项

### 构建系统更新
重构后需要更新：
1. Keil工程文件（.uvprojx）中的文件路径
2. 包含路径（Include Paths）
3. #include 语句中的相对路径

### Git历史保留
使用 `git mv` 命令移动文件，保留Git历史：
```bash
git mv old_path new_path
```

### 分阶段提交
每完成一个阶段就提交一次，便于回滚和追踪。

## 预期收益

- ✅ 代码结构清晰，易于理解
- ✅ 模块职责明确，便于维护
- ✅ 符合行业最佳实践
- ✅ 新成员快速上手
- ✅ 便于CI/CD集成
- ✅ 支持单元测试
