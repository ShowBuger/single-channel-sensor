---
name: commit
description: 分析暂存的 git 变更，按照项目提交规范生成 commit message 并执行提交
---

# Commit Message 生成规范

## 格式

```
<type>(<scope>): <subject>

<body>

<footer>
```

## Type（必选）

| type | 说明 |
|------|------|
| `feat` | 新功能 |
| `fix` | 修复 Bug |
| `docs` | 文档更新 |
| `style` | 代码格式调整（不影响功能） |
| `refactor` | 重构（不改变功能） |
| `perf` | 性能优化 |
| `test` | 添加或修改测试 |
| `build` | 构建系统或依赖项修改 |
| `ci` | CI/CD 配置修改 |
| `chore` | 其他不修改 src 或 test 的更改 |
| `revert` | 回退之前的提交 |

## Scope（可选）——本项目常用范围

**硬件驱动层**：`i2c` `sensor` `mlx90393` `tmag3001` `tca9548a` `serial` `gpio`

**算法层**：`filter` `calibration` `compensation` `mapping`

**应用层**：`main` `command` `config`

**其他**：`all` `docs` `build`

## Subject 规则

- 动词开头：添加 / 修改 / 删除 / 修复 / 优化
- 使用现在时：「添加」而不是「添加了」
- 不超过 50 个字符，结尾不加句号

## Body（复杂变更时使用）

- 与 subject 之间空一行
- 每行不超过 72 个字符
- 说明「是什么」和「为什么」，而不只是「怎么做」

## Footer（可选）

- 破坏性变更：`BREAKING CHANGE: <说明>`
- 关闭 Issue：`Closes #123` 或 `Fixes #456`

## 本项目不使用

- ❌ `Co-Authored-By:`
- ❌ `Signed-off-by:`

---

# 执行步骤

收到 `/commit` 指令后：

1. 运行 `git diff --cached` 查看暂存内容
2. 如果暂存区为空，运行 `git status` 提示用户先 `git add`
3. 根据变更内容选择最合适的 type 和 scope
4. 生成符合上述规范的 commit message
5. 展示给用户确认，确认后执行 `git commit -m "..."`
6. 多段 body 用 `-m` 多次传入，不使用 here-string

## 示例

### 简单修改
```
fix(i2c): 修正 MLX90393 传感器 I2C 地址
```

### 新功能带 body
```
feat(filter): 添加尖峰滤波算法

实现基于标准差的尖峰检测和过滤：
- 检测阈值：10 倍标准差
- 检测到尖峰时使用上一次有效值
- 分别处理实际传感器和参考传感器
```

### 破坏性变更
```
refactor(sensor): 重构传感器地址配置

- MLX90393 基地址从 0x0C 改为 0x10
- 实际传感器：A1=0, A0=0 → 地址 0x10
- 参考传感器：A1=0, A0=1 → 地址 0x11

BREAKING CHANGE: 硬件接线需要相应调整 A0/A1 引脚配置
```
