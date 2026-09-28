# Git Commit Message Convention

本项目的Git提交信息规范，用于保持提交历史的清晰和一致性。

## 提交信息格式

```
<type>(<scope>): <subject>

<body>

<footer>
```

### 必需部分

- **type**: 提交类型（必需）
- **subject**: 简短描述（必需，不超过50个字符）

### 可选部分

- **scope**: 影响范围（可选）
- **body**: 详细描述（可选）
- **footer**: 备注信息（可选，如关联issue）

---

## Type 提交类型

| 类型 | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | feat(sensor): 添加MLX90393传感器驱动 |
| `fix` | 修复Bug | fix(i2c): 修复地址计算错误 |
| `docs` | 文档更新 | docs: 更新README中的使用说明 |
| `style` | 代码格式调整（不影响功能） | style: 格式化sensor_manager.c |
| `refactor` | 重构代码（不改变功能） | refactor(filter): 优化滑动平均滤波算法 |
| `perf` | 性能优化 | perf(sensor): 减少I2C读取延迟 |
| `test` | 添加或修改测试 | test: 添加传感器校准测试用例 |
| `build` | 构建系统或依赖项修改 | build: 更新Keil工程配置 |
| `ci` | CI/CD配置修改 | ci: 添加自动构建脚本 |
| `chore` | 其他不修改src或test的更改 | chore: 更新.gitignore |
| `revert` | 回退之前的提交 | revert: 回退"feat: 添加新功能" |

---

## Scope 影响范围

根据项目模块划分，常用scope包括：

### 硬件驱动层
- `i2c` - I2C通信相关
- `sensor` - 传感器驱动
- `mlx90393` - MLX90393传感器
- `tmag3001` - TMAG3001传感器
- `tca9548a` - I2C多路复用器
- `serial` - 串口通信
- `gpio` - GPIO控制

### 算法层
- `filter` - 滤波算法
- `calibration` - 校准算法
- `compensation` - 补偿算法
- `mapping` - 映射算法

### 应用层
- `main` - 主程序
- `command` - 串口命令处理
- `config` - 配置管理

### 其他
- `all` - 影响整个项目
- `docs` - 文档相关
- `build` - 构建相关

---

## Subject 主题

### 规则
1. **使用动词开头**：添加、修改、删除、修复、优化等
2. **使用现在时态**：使用"添加"而不是"添加了"
3. **首字母小写**（中文无此要求）
4. **不超过50个字符**
5. **结尾不加句号**

### 示例
✅ 好的例子：
- `修改MLX90393基地址为0x10`
- `添加参考传感器数据输出`
- `修复死区滤波阈值计算错误`

❌ 不好的例子：
- `修改了一些代码。` （使用过去时，有句号）
- `更新` （过于简单，没有说明更新了什么）
- `修复传感器读取、校准、滤波和输出等多个模块的各种问题` （太长）

---

## Body 详细描述

### 何时需要Body
- 复杂的修改需要解释原因和实现方式
- 多个文件的修改需要说明改动内容
- 需要说明修改的上下文和背景

### 格式要求
1. 与subject之间空一行
2. 每行不超过72个字符
3. 说明"是什么"和"为什么"，而不仅是"怎么做"
4. 可以使用列表格式

### 示例
```
feat(sensor): 支持双传感器差分测量

添加实际传感器和参考传感器的配置：
- 实际传感器地址: 0x10 (A1=0, A0=0)
- 参考传感器地址: 0x11 (A1=0, A0=1)

通过差分测量消除环境磁场干扰，提高测量精度。
```

---

## Footer 备注信息

### 常用场景

#### 1. 破坏性变更（Breaking Changes）
```
BREAKING CHANGE: 传感器地址配置方式改变

需要更新硬件接线：
- MLX90393 实际传感器 A0接GND
- MLX90393 参考传感器 A0接VCC
```

#### 2. 关闭Issue
```
Closes #123
Fixes #456
```

#### 3. 关联提交
```
Related to: a3f2b1c
```

### ⚠️ 不使用的内容

**本项目不使用以下内容**：

- ❌ `Co-Authored-By:` - 不添加合著者信息
- ❌ `Signed-off-by:` - 不添加签名信息

保持提交信息简洁，只包含必要的内容。

---

## 完整示例

### 示例1：简单修改
```
fix(i2c): 修正MLX90393传感器I2C地址
```

### 示例2：新功能
```
feat(filter): 添加尖峰滤波算法

实现基于标准差的尖峰检测和过滤：
- 检测阈值：10倍标准差
- 检测到尖峰时使用上一次有效值
- 分别处理实际传感器和参考传感器

可有效过滤偶发的数据突变，提高测量稳定性。
```

### 示例3：重大修改
```
refactor(sensor): 重构传感器地址配置

修改内容：
- MLX90393基地址从0x0C改为0x10
- 实际传感器：A1=0, A0=0 → 地址0x10
- 参考传感器：A1=0, A0=1 → 地址0x11

文件修改：
- Software/Hardware/sensor_driver/mlx90393.h
- Software/Hardware/sensor_manager.c

BREAKING CHANGE: 硬件接线需要相应调整传感器A0/A1引脚配置
```

### 示例4：修复Bug
```
fix(calibration): 修复校准数据保存失败问题

问题：校准完成后数据未正确保存到Flash

原因：Config_Save函数返回值判断错误

解决：修正返回值判断逻辑，确保数据正确写入

Fixes #23
```

---

## 提交频率建议

### 推荐的提交粒度
- ✅ 完成一个独立的功能或修复
- ✅ 代码可以编译通过
- ✅ 不破坏现有功能
- ✅ 一次提交只做一件事

### 避免
- ❌ 将多个不相关的修改放在一起
- ❌ 提交编译失败的代码
- ❌ 提交过于细碎的修改（如每改一行提交一次）
- ❌ 长时间不提交累积大量修改

---

## 多语言支持

### 中文示例
```
feat(传感器): 添加MLX90393驱动支持
fix(串口): 修复命令解析错误
docs(文档): 更新API使用说明
```

### 英文示例
```
feat(sensor): add MLX90393 driver support
fix(serial): fix command parsing error
docs: update API documentation
```

**建议**：在同一项目中保持语言一致性，本项目推荐使用**中文**。

---

## 工具支持

### Commitizen
可以使用commitizen工具辅助生成规范的commit message：

```bash
npm install -g commitizen
git cz
```

### Git Hooks
配合commit-msg hook自动检查提交信息格式：

```bash
# .git/hooks/commit-msg
#!/bin/sh
commit_msg=$(cat "$1")
if ! echo "$commit_msg" | grep -qE '^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\(.+\))?: .+'; then
    echo "Error: Invalid commit message format"
    echo "Expected: <type>(<scope>): <subject>"
    exit 1
fi
```

---

## AI生成Commit Message指南

当使用AI生成commit message时，请遵循以下原则：

1. **分析代码变更**：理解修改的本质和目的
2. **选择准确的type**：根据变更类型选择最合适的类型
3. **明确scope**：指明影响的模块或组件
4. **简洁的subject**：用一句话说清楚做了什么
5. **必要时添加body**：复杂修改需要详细说明
6. **标注破坏性变更**：API变更或配置变更需要特别说明

### AI生成示例

对于本次传感器地址修改，AI应生成：

```
refactor(sensor): 修改MLX90393传感器I2C地址配置

- 基地址从0x0C改为0x10
- 实际传感器地址：0x10 (A1=0, A0=0)
- 参考传感器地址：0x11 (A1=0, A0=1)

修改文件：
- mlx90393.h: 更新I2C_BASE_ADDR定义
- sensor_manager.c: 调整传感器初始化参数
```

---

## 参考资源

- [Conventional Commits](https://www.conventionalcommits.org/)
- [Angular Commit Guidelines](https://github.com/angular/angular/blob/main/CONTRIBUTING.md#commit)
- [Git Commit Message Best Practices](https://chris.beams.io/posts/git-commit/)
