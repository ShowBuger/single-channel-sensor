# 单通道磁传感器系统

基于STM32F103的单通道磁传感器测量系统，支持MLX90393和TMAG3001传感器。

## 📁 项目结构

```
single_channel_sensor/
├── docs/                   # 📚 文档
│   ├── README.md           # 详细文档
│   ├── COMMIT_CONVENTION.md # Git提交规范
│   ├── 串口命令.md          # 串口命令说明
│   └── REFACTOR_PLAN.md    # 重构方案
│
├── firmware/               # 💾 固件代码
│   ├── src/                # 源代码
│   │   ├── app/            # 应用层
│   │   ├── drivers/        # 驱动层
│   │   └── main.c          # 主程序
│   ├── libs/               # 第三方库
│   └── project/            # 项目配置
│
├── gui/                    # 🖥️ 上位机GUI
│   ├── display/            # 显示版本
│   └── test/               # 测试版本
│
├── hardware/               # ⚡ 硬件设计
│   └── pcb/                # PCB设计文件
│
├── tools/                  # 🔧 开发工具
└── tests/                  # 🧪 测试文件
```

## 🚀 快速开始

### 固件开发

#### 开发环境
- **VSCode** + ARM工具链 (推荐)
- 或 Keil MDK-ARM

详见 [firmware/README.md](firmware/README.md) 和 [docs/README.md](docs/README.md)

#### 快速编译
```bash
cd firmware
make
```

### 上位机
- 显示版本: `gui/display/`
- 测试版本: `gui/test/`

## 📖 文档

- [完整文档](docs/README.md)
- [串口命令](docs/串口命令.md)
- [Git提交规范](docs/COMMIT_CONVENTION.md)

## 📝 更新日志

### 2026-09-29 - VSCode开发环境支持
- 添加Makefile构建系统
- 配置VSCode开发和调试环境
- 清理Keil残留文件
- 支持OpenOCD/J-Link/ST-Link调试

### 2024-09-28 - 目录重构
- 重构项目目录结构，建立清晰的分层架构
- 详见 [REFACTOR_PLAN.md](docs/REFACTOR_PLAN.md)
