#include "config_storage.h"
#include "stm32f10x_flash.h"
#include <string.h>

// STM32F103C8T6 Flash配置:
// - 总容量: 64KB
// - 页大小: 1KB (1024字节)
// - 起始地址: 0x08000000
// - 最后一页地址: 0x0800FC00 (64KB - 1KB)
// 使用最后一页存储配置参数

#define FLASH_PAGE_SIZE         1024                    // Flash页大小 1KB
#define CONFIG_FLASH_PAGE_ADDR  0x0800FC00             // 最后一页起始地址 (64KB - 1KB)
#define CONFIG_MAGIC_NUMBER     0x52454C54             // 魔数 "RELT" 用于验证数据有效性

// 完整的配置数据结构（包含魔数）
typedef struct {
    uint32_t magic;             // 魔数，用于判断是否已写入过配置
    Config_TypeDef config;      // 配置参数
} StoredConfig_TypeDef;

/**
 * 函数: calculate_checksum
 * 功能: 计算配置参数的校验和
 * 参数: config - 配置参数结构体指针
 * 返回: 计算得到的校验和
 */
static uint32_t calculate_checksum(Config_TypeDef *config)
{
    uint32_t sum = 0;
    uint8_t *data = (uint8_t *)config;
    int i;

    // 计算除校验和字段外的所有字节的和
    for(i = 0; i < (sizeof(Config_TypeDef) - sizeof(uint32_t)); i++)
    {
        sum += data[i];
    }

    return sum;
}

/**
 * 函数: Config_Init
 * 功能: 初始化配置存储模块
 * 参数: 无
 * 返回: 无
 * 说明: 系统启动时调用，解锁Flash
 */
void Config_Init(void)
{
    // 使能FLASH时钟（通常已经在系统初始化时使能）
    // 解锁FLASH以便后续写操作
    FLASH_Unlock();
}

/**
 * 函数: Config_Save
 * 功能: 保存配置参数到Flash
 * 参数: flag_res - FLAG_RES标志位
 *       x_map - X轴映射系数
 *       y_map - Y轴映射系数
 *       z_map - Z轴映射系数
 * 返回: 0-成功, 1-失败
 */
uint8_t Config_Save(uint8_t flag_res, float x_map, float y_map, float z_map)
{
    FLASH_Status status;
    StoredConfig_TypeDef stored_config;
    uint32_t *flash_ptr;
    uint32_t *data_ptr;
    int i;

    // 准备要写入的数据
    stored_config.magic = CONFIG_MAGIC_NUMBER;
    stored_config.config.FLAG_RES = flag_res;
    stored_config.config.reserved[0] = 0;
    stored_config.config.reserved[1] = 0;
    stored_config.config.reserved[2] = 0;
    stored_config.config.X_MAPPING_VALUE = x_map;
    stored_config.config.Y_MAPPING_VALUE = y_map;
    stored_config.config.Z_MAPPING_VALUE = z_map;
    stored_config.config.checksum = calculate_checksum(&stored_config.config);

    // 解锁Flash
    FLASH_Unlock();

    // 擦除配置页
    status = FLASH_ErasePage(CONFIG_FLASH_PAGE_ADDR);
    if(status != FLASH_COMPLETE)
    {
        FLASH_Lock();
        return 1;  // 擦除失败
    }

    // 写入配置数据（按半字写入，STM32F103要求）
    flash_ptr = (uint32_t *)CONFIG_FLASH_PAGE_ADDR;
    data_ptr = (uint32_t *)&stored_config;

    for(i = 0; i < sizeof(StoredConfig_TypeDef) / 4; i++)
    {
        status = FLASH_ProgramWord((uint32_t)&flash_ptr[i], data_ptr[i]);
        if(status != FLASH_COMPLETE)
        {
            FLASH_Lock();
            return 1;  // 写入失败
        }
    }

    // 锁定Flash
    FLASH_Lock();

    return 0;  // 保存成功
}

/**
 * 函数: Config_Load
 * 功能: 从Flash读取配置参数
 * 参数: flag_res - 存储FLAG_RES的指针
 *       x_map - 存储X轴映射系数的指针
 *       y_map - 存储Y轴映射系数的指针
 *       z_map - 存储Z轴映射系数的指针
 * 返回: 0-成功读取, 1-Flash未初始化/数据无效，已恢复默认值
 */
uint8_t Config_Load(uint8_t *flag_res, float *x_map, float *y_map, float *z_map)
{
    StoredConfig_TypeDef *stored_config = (StoredConfig_TypeDef *)CONFIG_FLASH_PAGE_ADDR;
    uint32_t checksum;

    // 检查魔数是否正确
    if(stored_config->magic != CONFIG_MAGIC_NUMBER)
    {
        // Flash未初始化，使用默认值
        *flag_res = 0x3B;
        *x_map = 0.1f;
        *y_map = 0.1f;
        *z_map = -0.1f;
        return 1;
    }

    // 验证校验和
    checksum = calculate_checksum(&stored_config->config);
    if(checksum != stored_config->config.checksum)
    {
        // 数据损坏，使用默认值
        *flag_res = 0x3B;
        *x_map = 0.1f;
        *y_map = 0.1f;
        *z_map = -0.1f;
        return 1;
    }

    // 读取有效数据
    *flag_res = stored_config->config.FLAG_RES;
    *x_map = stored_config->config.X_MAPPING_VALUE;
    *y_map = stored_config->config.Y_MAPPING_VALUE;
    *z_map = stored_config->config.Z_MAPPING_VALUE;
    return 0;
}

/**
 * 函数: Config_RestoreDefault
 * 功能: 恢复默认配置并保存到Flash
 * 参数: 无
 * 返回: 无
 */
void Config_RestoreDefault(void)
{
    Config_Save(0x3B, 0.1f, 0.1f, -0.1f);  // 保存默认值
}
