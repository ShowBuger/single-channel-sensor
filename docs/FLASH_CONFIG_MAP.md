# Flash 配置映射（TMAG3001 分支）

配置保持统一的56字节结构，MA01与MA02的同名字段位置相同。
本分支的采集驱动固定为TMAG3001，只接受MA02/type=1的硬件配置。
MA01/type=0可在网页预览、导出，但应由MLX固件应用；型号与类型冲突会被拒绝。

## 固定存储布局

- Flash页：0x0800FC00，1024字节。
- 魔数：0x52454C57，占页偏移0–3。
- Schema：5；本次没有修改版本、魔数或字段布局。
- Config_TypeDef为56字节，StoredConfig_TypeDef为60字节。
- checksum为配置偏移0–51的字节和；头文件静态断言检查关键偏移。
- 全部字段保存和完整读回，MLX特有字段在MA02中仅存储、不写入传感器。
- 旧Flash若通过魔数、版本和校验和检查，且曾被错误标为MA01/type=1，
  加载时仅将型号迁移为MA02，保留用户参数。新导入的冲突配置仍报错。

| 配置偏移 | Flash 地址 | 字段 / JSON 键 | 长度 |
| --- | --- | --- | --- |
| 0 | 0x0800FC04 | schema_ver | 1 |
| 1 | 0x0800FC05 | sensor_type / sensor.type | 1 |
| 2 | 0x0800FC06 | FLAG_RES / algo.flag | 1 |
| 3 | 0x0800FC07 | gain_sel / sensor.gain | 1 |
| 4 | 0x0800FC08 | osr / sensor.osr | 1 |
| 5 | 0x0800FC09 | dig_filt / sensor.filt | 1 |
| 6 | 0x0800FC0A | res_x / sensor.resx | 1 |
| 7 | 0x0800FC0B | res_y / sensor.resy | 1 |
| 8 | 0x0800FC0C | res_z / sensor.resz | 1 |
| 9 | 0x0800FC0D | tcmp_en / sensor.tcmp | 1 |
| 10 | 0x0800FC0E | range / sensor.range | 1 |
| 11 | 0x0800FC0F | op_mode / sensor.opmode | 1 |
| 12 | 0x0800FC10 | mag_ch / sensor.magch | 1 |
| 13 | 0x0800FC11 | sensor_version / sensor.version | 1 |
| 14 | 0x0800FC12 | log_level / log.level | 1 |
| 15 | 0x0800FC13 | 编译器对齐填充 | 1 |
| 16 | 0x0800FC14 | X_MAPPING_VALUE / algo.xmap | 4 |
| 20 | 0x0800FC18 | Y_MAPPING_VALUE / algo.ymap | 4 |
| 24 | 0x0800FC1C | Z_MAPPING_VALUE / algo.zmap | 4 |
| 28 | 0x0800FC20 | dead_value / algo.dead | 4 |
| 32 | 0x0800FC24 | spike_multiplier / algo.spike | 4 |
| 36 | 0x0800FC28 | stability_threshold / algo.stab | 4 |
| 40 | 0x0800FC2C | product_model / product | 8 |
| 48 | 0x0800FC34 | _pad | 1 |
| 49–51 | 0x0800FC35 | 编译器对齐填充 | 3 |
| 52 | 0x0800FC38 | checksum | 4 |


## 默认配置

MA02/type=1，gain=5，osr=4（16x平均），range=0，opmode=2（连续），
magch=7（XYZ），version=0，日志4，算法flag=0，XYZ映射系数均为1。
`firmware/config/config_ma02.json`及网页MA02默认模板使用这些值。
`config_example.json`保留已有的自定义算法/增益参数，并补齐字段。

硬件仍使用0x35实际传感器和0x34参考传感器。芯片A1/A2由Device_ID自动识别，
磁场换算使用识别出的版本；sensor.version是保存的版本标记，不能改变物理芯片版本。

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
