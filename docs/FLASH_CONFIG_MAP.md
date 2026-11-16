# Flash 配置映射

配置保持统一的56字节结构，MA01、MA02与MA03的同名字段位置相同。
固件编入的传感器驱动由 `config_manager.h` 的 `CURRENT_SENSOR_TYPE` 决定（当前为
MA03/AK09974C），只接受 `sensor.type` 与之一致的配置（`config_parser.c` 返回 3 表示型号不符）。
其他型号的配置可在网页预览、导出，但应由对应型号的固件应用；型号与类型冲突会被拒绝。

## 固定存储布局

- 存储区：STM32H743 Bank2 扇区 7，起始 0x081E0000，扇区 128 KB（链接脚本已排除）。
  按 256 位 Flash 字（32 字节）编程，记录末尾以 0xFF 补齐；擦除约 1–2 s，期间喂狗。
  记录结构：偏移 0 为魔数（4 字节），偏移 4 起为下表的配置结构。地址由
  `firmware/src/board/<board>/board_pins.h` 定义，经 `plat_cfgstore_*` 访问。
- 魔数：0x52454C57，占页偏移0–3。
- Schema：5；本次没有修改版本、魔数或字段布局。
- Config_TypeDef为56字节，StoredConfig_TypeDef为60字节。
- checksum为配置偏移0–51的字节和；头文件静态断言检查关键偏移。
- 全部字段保存和完整读回，MLX特有字段在MA02中仅存储、不写入传感器。
- 旧Flash若通过魔数、版本和校验和检查，且曾被错误标为MA01/type=1，
  加载时仅将型号迁移为MA02，保留用户参数。新导入的冲突配置仍报错。

| 配置偏移 | Flash 地址 | 字段 / JSON 键 | 长度 |
| --- | --- | --- | --- |
| 0 | 0x081E0004 | schema_ver | 1 |
| 1 | 0x081E0005 | sensor_type / sensor.type | 1 |
| 2 | 0x081E0006 | FLAG_RES / algo.flag | 1 |
| 3 | 0x081E0007 | gain_sel / sensor.gain | 1 |
| 4 | 0x081E0008 | osr / sensor.osr | 1 |
| 5 | 0x081E0009 | dig_filt / sensor.filt | 1 |
| 6 | 0x081E000A | res_x / sensor.resx | 1 |
| 7 | 0x081E000B | res_y / sensor.resy | 1 |
| 8 | 0x081E000C | res_z / sensor.resz | 1 |
| 9 | 0x081E000D | tcmp_en / sensor.tcmp | 1 |
| 10 | 0x081E000E | range / sensor.range | 1 |
| 11 | 0x081E000F | op_mode / sensor.opmode | 1 |
| 12 | 0x081E0010 | mag_ch / sensor.magch | 1 |
| 13 | 0x081E0011 | sensor_version / sensor.version | 1 |
| 14 | 0x081E0012 | log_level / log.level | 1 |
| 15 | 0x081E0013 | 编译器对齐填充 | 1 |
| 16 | 0x081E0014 | X_MAPPING_VALUE / algo.xmap | 4 |
| 20 | 0x081E0018 | Y_MAPPING_VALUE / algo.ymap | 4 |
| 24 | 0x081E001C | Z_MAPPING_VALUE / algo.zmap | 4 |
| 28 | 0x081E0020 | dead_value / algo.dead | 4 |
| 32 | 0x081E0024 | spike_multiplier / algo.spike | 4 |
| 36 | 0x081E0028 | stability_threshold / algo.stab | 4 |
| 40 | 0x081E002C | product_model / product | 8 |
| 48 | 0x081E0034 | mcyc / sensor.mcyc（仅MA03使用，原填充字节；MA01/MA02为0） | 1 |
| 49–51 | 0x081E0035 | 编译器对齐填充 | 3 |
| 52 | 0x081E0038 | checksum | 4 |


## 默认配置

MA02/type=1，gain=5，osr=4（16x平均），range=0，opmode=2（连续），
magch=7（XYZ），version=0，日志4，算法flag=0，XYZ映射系数均为1。
`firmware/config/config_ma02.json`及网页MA02默认模板使用这些值。
`config_example.json`保留已有的自定义算法/增益参数，并补齐字段。

实际传感器使用0x36（ADDR接SDA），参考传感器使用0x35（ADDR接VCC）。芯片A1/A2由Device_ID自动识别，
磁场换算使用识别出的版本；sensor.version是保存的版本标记，不能改变物理芯片版本。

## MA03（AK09974C）字段复用

MA03沿用同一布局，不改Schema版本：`osr`存驱动模式DRI（0–3），`range`存WIDE位（0/1），
`op_mode`为0=单次触发/1=连续测量，`mcyc`使用原`_pad`字节。`gain`必须为0。
`ML GETCFG`仅在MA03时额外输出`sensor.mcyc`，MA01/MA02回包字段不变。

## 应用与错误回执

配置导入逐块接收，检查长度、语法、整数范围、有限浮点值与型号一致性。
所有值使用同一字段解析器，缺省值按产品补全，与JSON字段排列顺序无关。
`ML SET`也使用相同校验，不能通过整数截断绕过范围检查。

`ML GETCFG`完整返回product、全部sensor/algo/log字段；这是配置快照，
不是芯片寄存器读回。`ML APPLYCFG`及导入会重新扫描并配置两颗传感器。
运行状态下重新标定；停止状态下应用寄存器并保持停止。
配置已保存但硬件写入或校准失败时返回错误，不再报告IMPORT_OK。

主机采样支持opmode=0待机触发和opmode=2连续模式。
opmode=1/3允许存储、应用，但请求主机采样会报错，保持停止。
每轮轮询两颗传感器的Result_Status及滚动Set_Count，等待新的一组结果，
100ms超时。校准及等待期间喂狗，连续失败有重试上限；失败帧不输出旧数据。

## 与芯片协议对齐

依据TI TMAG3001数据手册SLYS053C：
https://www.ti.com/lit/ds/symlink/tmag3001.pdf

- Conv_AVG在Device_Config_1（0x00）bit4:2。
- XYZ统一量程使用Sensor_Config_2（0x03）bit1:0，保留其他位。
- 单次转换使用I2C控制字节bit7触发，不修改I2C_RD位。
- Conv_Status的bit0是Result_Status，bit1是Diag_Status，bit4是POR，
  bit7:5是只读转换计数。只对POR写1清除，诊断错误源在Device_Status清除。
- 初始化恢复标准I2C读格式，兼容旧触发代码误改过I2C_RD的设备。
