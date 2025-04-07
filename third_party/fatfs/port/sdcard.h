#ifndef _SDCARD_H
#define _SDCARD_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stdint.h"
#include <stdbool.h>

// K210 specific defines
#define SD_SPI_NUM         SPI_DEVICE_0

// SD卡相关的GPIO和引脚定义
#define SD_CS_GPIO_NUM     7   // 使用GPIOHS7
#define SD_CS_PIN          20  // FPIOA pin 20 映射到SD卡片选

// SPI引脚定义
#define PIN_SPI_MOSI       28  // SPI MOSI引脚
#define PIN_SPI_MISO       26  // SPI MISO引脚
#define PIN_SPI_SCLK       27  // SPI SCLK引脚

// 一些K210特定的定义
#define SPI_WORK_FREQ      1000000  // SPI时钟频率(1MHz)
#define SD_CS_GPIONUM      (FUNC_GPIOHS0 + SD_CS_GPIO_NUM)  // GPIO高速接口功能号

/** 
  * @brief  Card Specific Data: CSD Register   
  */ 
typedef struct {
	uint8_t  CSDStruct;            /*!< CSD structure */
	uint8_t  SysSpecVersion;       /*!< System specification version */
	uint8_t  Reserved1;            /*!< Reserved */
	uint8_t  TAAC;                 /*!< Data read access-time 1 */
	uint8_t  NSAC;                 /*!< Data read access-time 2 in CLK cycles */
	uint8_t  MaxBusClkFrec;        /*!< Max. bus clock frequency */
	uint16_t CardComdClasses;      /*!< Card command classes */
	uint8_t  RdBlockLen;           /*!< Max. read data block length */
	uint8_t  PartBlockRead;        /*!< Partial blocks for read allowed */
	uint8_t  WrBlockMisalign;      /*!< Write block misalignment */
	uint8_t  RdBlockMisalign;      /*!< Read block misalignment */
	uint8_t  DSRImpl;              /*!< DSR implemented */
	uint8_t  Reserved2;            /*!< Reserved */
	uint32_t DeviceSize;           /*!< Device Size */
	uint8_t  MaxRdCurrentVDDMin;   /*!< Max. read current @ VDD min */
	uint8_t  MaxRdCurrentVDDMax;   /*!< Max. read current @ VDD max */
	uint8_t  MaxWrCurrentVDDMin;   /*!< Max. write current @ VDD min */
	uint8_t  MaxWrCurrentVDDMax;   /*!< Max. write current @ VDD max */
	uint8_t  DeviceSizeMul;        /*!< Device size multiplier */
	uint8_t  EraseGrSize;          /*!< Erase group size */
	uint8_t  EraseGrMul;           /*!< Erase group size multiplier */
	uint8_t  WrProtectGrSize;      /*!< Write protect group size */
	uint8_t  WrProtectGrEnable;    /*!< Write protect group enable */
	uint8_t  ManDeflECC;           /*!< Manufacturer default ECC */
	uint8_t  WrSpeedFact;          /*!< Write speed factor */
	uint8_t  MaxWrBlockLen;        /*!< Max. write data block length */
	uint8_t  WriteBlockPaPartial;  /*!< Partial blocks for write allowed */
	uint8_t  Reserved3;            /*!< Reserved */
	uint8_t  ContentProtectAppli;  /*!< Content protection application */
	uint8_t  FileFormatGrouop;     /*!< File format group */
	uint8_t  CopyFlag;             /*!< Copy flag (OTP) */
	uint8_t  PermWrProtect;        /*!< Permanent write protection */
	uint8_t  TempWrProtect;        /*!< Temporary write protection */
	uint8_t  FileFormat;           /*!< File Format */
	uint8_t  ECC;                  /*!< ECC code */
	uint8_t  CSD_CRC;              /*!< CSD CRC */
	uint8_t  Reserved4;            /*!< always 1*/
} SD_CSD;

/** 
  * @brief  Card Identification Data: CID Register   
  */
typedef struct {
	uint8_t  ManufacturerID;       /*!< ManufacturerID */
	uint16_t OEM_AppliID;          /*!< OEM/Application ID */
	uint32_t ProdName1;            /*!< Product Name part1 */
	uint8_t  ProdName2;            /*!< Product Name part2*/
	uint8_t  ProdRev;              /*!< Product Revision */
	uint32_t ProdSN;               /*!< Product Serial Number */
	uint8_t  Reserved1;            /*!< Reserved1 */
	uint16_t ManufactDate;         /*!< Manufacturing Date */
	uint8_t  CID_CRC;              /*!< CID CRC */
	uint8_t  Reserved2;            /*!< always 1 */
} SD_CID;

/** 
  * @brief SD Card information 
  */
typedef struct {
	SD_CSD SD_csd;
	SD_CID SD_cid;
	uint64_t CardCapacity;  /*!< Card Capacity */
	uint32_t CardBlockSize; /*!< Card Block Size */
} SD_CardInfo;

extern SD_CardInfo cardinfo;

// Function declarations
uint8_t sd_init(void);
uint8_t sd_read_sector(uint8_t *data_buff, uint32_t sector, uint32_t count);
uint8_t sd_write_sector(uint8_t *data_buff, uint32_t sector, uint32_t count);
uint8_t sd_read_sector_dma(uint8_t *data_buff, uint32_t sector, uint32_t count);
uint8_t sd_write_sector_dma(uint8_t *data_buff, uint32_t sector, uint32_t count);
void sd_power_control(bool on);

#ifdef __cplusplus
}
#endif

#endif
