# CMake 工作流配置

# 预设配置
CMakePresets.json 提供了常用的构建配置

## 可用预设

### Configure Presets (配置预设)
- `debug` - Debug 构建配置
- `release` - Release 构建配置

### Build Presets (构建预设)
- `debug` - 构建 Debug 版本
- `release` - 构建 Release 版本

## 使用方法

### 命令行

```bash
# 配置 Debug
cmake --preset debug

# 构建 Debug
cmake --build --preset debug

# 配置 Release
cmake --preset release

# 构建 Release
cmake --build --preset release
```

### VSCode CMake Tools

VSCode 会自动检测并使用这些预设。

在状态栏选择预设配置即可。
