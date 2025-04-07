#include "sdcard.h"
#include "sysctl.h"
#include "gpiohs.h"
#include "fpioa.h"
#include "dmac.h"
#include "spi.h"
#include <stdio.h>
#include "gpiohs.h"
#include "sleep.h"

/*
 * @brief  Start Data tokens:
 *         Tokens (necessary because at nop/idle (and CS active) only 0xff is
 *         on the data/command line)
 */
#define SD_START_DATA_SINGLE_BLOCK_READ    0xFE  /*!< Data token start byte, Start Single Block Read */
#define SD_START_DATA_MULTIPLE_BLOCK_READ  0xFE  /*!< Data token start byte, Start Multiple Block Read */
#define SD_START_DATA_SINGLE_BLOCK_WRITE   0xFE  /*!< Data token start byte, Start Single Block Write */
#define SD_START_DATA_MULTIPLE_BLOCK_WRITE 0xFC  /*!< Data token start byte, Start Multiple Block Write */

/*
 * @brief  Commands: CMDxx = CMD-number | 0x40
 */
#define SD_CMD0          0   /*!< CMD0 = 0x40 */
#define SD_CMD8          8   /*!< CMD8 = 0x48 */
#define SD_CMD9          9   /*!< CMD9 = 0x49 */
#define SD_CMD10         10  /*!< CMD10 = 0x4A */
#define SD_CMD12         12  /*!< CMD12 = 0x4C */
#define SD_CMD16         16  /*!< CMD16 = 0x50 */
#define SD_CMD17         17  /*!< CMD17 = 0x51 */
#define SD_CMD18         18  /*!< CMD18 = 0x52 */
#define SD_ACMD23        23  /*!< CMD23 = 0x57 */
#define SD_CMD24         24  /*!< CMD24 = 0x58 */
#define SD_CMD25         25  /*!< CMD25 = 0x59 */
#define SD_ACMD41        41  /*!< ACMD41 = 0x41 */
#define SD_CMD55         55  /*!< CMD55 = 0x55 */
#define SD_CMD58         58  /*!< CMD58 = 0x58 */
#define SD_CMD59         59  /*!< CMD59 = 0x59 */

SD_CardInfo cardinfo;

// 初始化SD卡的FPIOA引脚配置
static void sd_setup_pins(void)
{
    printf("Starting SD card pin configuration...\n");
    
    // 确保之前没有映射过这些管脚，先复位之前的映射
    printf("Resetting previous pin mappings...\n");
    fpioa_set_function(PIN_SPI_SCLK, FUNC_RESV0);
    fpioa_set_function(PIN_SPI_MOSI, FUNC_RESV0);
    fpioa_set_function(PIN_SPI_MISO, FUNC_RESV0);
    fpioa_set_function(SD_CS_PIN, FUNC_RESV0);
    
    // 延迟一小段时间让引脚状态稳定
    printf("Waiting for pins to stabilize (200ms)...\n");
    msleep(200);
    
    // 先配置MISO引脚，并设置上拉电阻
    printf("Configuring MISO pin with pull-up:\n");
    printf("  MISO: PIN %d -> FUNC_SPI0_D1\n", PIN_SPI_MISO);
    fpioa_set_function(PIN_SPI_MISO, FUNC_SPI0_D1);
    fpioa_set_io_pull(PIN_SPI_MISO, FPIOA_PULL_UP);
    msleep(20);
    
    // 配置SCLK引脚
    printf("  SCLK: PIN %d -> FUNC_SPI0_SCLK\n", PIN_SPI_SCLK);
    fpioa_set_function(PIN_SPI_SCLK, FUNC_SPI0_SCLK);
    msleep(20);
    
    // 配置MOSI引脚
    printf("  MOSI: PIN %d -> FUNC_SPI0_D0\n", PIN_SPI_MOSI);
    fpioa_set_function(PIN_SPI_MOSI, FUNC_SPI0_D0);
    msleep(20);
    
    // 配置CS引脚
    printf("Configuring CS pin:\n");
    printf("  CS: PIN %d -> GPIOHS%d\n", SD_CS_PIN, SD_CS_GPIO_NUM);
    fpioa_set_function(SD_CS_PIN, SD_CS_GPIONUM);
    
    // 初始化GPIO高速接口，设置为输出模式
    printf("Initializing GPIOHS for CS pin...\n");
    gpiohs_set_drive_mode(SD_CS_GPIO_NUM, GPIO_DM_OUTPUT);
    
    // 确保CS引脚为高电平
    printf("Setting CS pin HIGH...\n");
    gpiohs_set_pin(SD_CS_GPIO_NUM, GPIO_PV_HIGH);
    msleep(300);  // 等待CS稳定
    
    // 执行一次CS引脚复位序列
    printf("Performing CS pin reset sequence...\n");
    gpiohs_set_pin(SD_CS_GPIO_NUM, GPIO_PV_HIGH);
    msleep(200);
    gpiohs_set_pin(SD_CS_GPIO_NUM, GPIO_PV_LOW);
    msleep(200);
    gpiohs_set_pin(SD_CS_GPIO_NUM, GPIO_PV_HIGH);
    
    // 确保片选拉高后有足够的延时时间
    printf("Waiting for CS pin to stabilize (500ms)...\n");
    msleep(500);
    
    printf("SD card pin configuration complete\n");
}

void SD_CS_HIGH(void)
{
    gpiohs_set_pin(SD_CS_GPIO_NUM, GPIO_PV_HIGH);
    // 增加更长的延时确保CS引脚状态稳定
    msleep(1);
}

void SD_CS_LOW(void)
{
    gpiohs_set_pin(SD_CS_GPIO_NUM, GPIO_PV_LOW);
    // 增加更长的延时确保CS引脚状态稳定
    msleep(1);
}

void SD_HIGH_SPEED_ENABLE(void)
{
    // 启用低速SPI时钟 (1MHz而不是4MHz，以提高稳定性)
    spi_set_clk_rate(SPI_DEVICE_0, 1000000);
    printf("SD card set to low speed mode (1MHz)\n");
}

static void sd_lowlevel_init(uint8_t spi_index)
{
    printf("Starting SD card hardware initialization...\n");
    
    // 使能所需的时钟
    printf("Enabling clocks...\n");
    sysctl_clock_enable(SYSCTL_CLOCK_SPI0);
    sysctl_clock_enable(SYSCTL_CLOCK_GPIO);
    sysctl_clock_enable(SYSCTL_CLOCK_FPIOA);
    
    // 设置PLL频率为800MHz
    printf("Setting PLL frequency to 800MHz...\n");
    sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
    
    // 配置SD卡引脚
    printf("Configuring SD card pins...\n");
    sd_setup_pins();
    
    // 初始化SPI控制器，使用MODE0
    printf("Initializing SPI controller in MODE0...\n");
    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    
    // 设置更低的SPI时钟频率以提高兼容性 (100Hz)
    printf("Setting SPI clock rate to 100Hz for initialization...\n");
    spi_set_clk_rate(SPI_DEVICE_0, 100);
    
    // 等待SPI稳定
    printf("Waiting for SPI to stabilize (1000ms)...\n");
    msleep(1000);
    
    printf("SD card hardware initialization complete\n");
}

static void sd_write_data(uint8_t *data_buff, uint32_t length)
{
    printf("Writing %lu bytes to SPI...\n", (unsigned long)length);
    
    // 检查CS引脚状态
    if(gpiohs_get_pin(SD_CS_GPIO_NUM) != GPIO_PV_HIGH) {
        printf("Warning: CS pin is LOW before write, setting HIGH\n");
        gpiohs_set_pin(SD_CS_GPIO_NUM, GPIO_PV_HIGH);
        msleep(10);
    }
    
    // 发送数据
    spi_send_data_standard(SPI_DEVICE_0, SPI_CHIP_SELECT_3, NULL, 0, data_buff, length);
    
    // 再次检查CS引脚状态
    if(gpiohs_get_pin(SD_CS_GPIO_NUM) != GPIO_PV_HIGH) {
        printf("Warning: CS pin dropped LOW during write, setting HIGH\n");
        gpiohs_set_pin(SD_CS_GPIO_NUM, GPIO_PV_HIGH);
        msleep(10);
    }
    
    // 添加一个小的延迟，确保总线上的数据传输完成
    msleep(2);
}

static void sd_read_data(uint8_t *data_buff, uint32_t length)
{

    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    spi_receive_data_standard(SPI_DEVICE_0, SPI_CHIP_SELECT_3, NULL, 0, data_buff, length);

}

static void sd_write_data_dma(uint8_t *data_buff)
{
    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    spi_send_data_standard_dma(DMAC_CHANNEL0, SPI_DEVICE_0, SPI_CHIP_SELECT_3, NULL, 0, (uint8_t *)(data_buff), 128 * 4);
}

static void sd_read_data_dma(uint8_t *data_buff)
{
    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    spi_receive_data_standard_dma(-1, DMAC_CHANNEL0, SPI_DEVICE_0, SPI_CHIP_SELECT_3,NULL, 0, data_buff,128 * 4);
}

// 发送多个FF字节来产生额外的时钟脉冲
static void sd_send_dummy_bytes(int count)
{
    uint8_t dummy = 0xFF;
    for (int i = 0; i < count; i++) {
        sd_write_data(&dummy, 1);
    }
}

/*
 * @brief  Send 5 bytes command to the SD card.
 * @param  Cmd: The user expected command to send to SD card.
 * @param  Arg: The command argument.
 * @param  Crc: The CRC.
 * @retval Response code (0x00: No error, 0x01: Idle, other: Error)
 */
static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t frame[6];
    uint8_t response;
    uint8_t i;
    
    // 准备命令帧
    frame[0] = (cmd | 0x40);        // 命令字节
    frame[1] = (uint8_t)(arg >> 24); // 参数[31:24]
    frame[2] = (uint8_t)(arg >> 16); // 参数[23:16]
    frame[3] = (uint8_t)(arg >> 8);  // 参数[15:8]
    frame[4] = (uint8_t)(arg);       // 参数[7:0]
    frame[5] = (crc);                // CRC字节
    
    printf("Sending CMD%d (0x%02X) with arg=0x%08lX, crc=0x%02X\n", 
           cmd, cmd, (unsigned long)arg, crc);
    
    // 发送多个0xFF字节，确保之前的指令已完成
    uint8_t dummy = 0xFF;
    for (i = 0; i < 50; i++) {  // 增加到50个dummy字节
        sd_write_data(&dummy, 1);
    }
    
    // 发送命令帧
    printf("Sending command frame: ");
    for(i = 0; i < 6; i++) {
        printf("%02X ", frame[i]);
    }
    printf("\n");
    
    // 检查CS引脚状态
    if(gpiohs_get_pin(SD_CS_GPIO_NUM) != 0) {
        printf("Warning: CS pin is not low before sending command\n");
        SD_CS_LOW();
        msleep(20);
    }
    
    sd_write_data(frame, 6);
    
    // 对于CMD12，需要额外跳过一个字节
    if (cmd == SD_CMD12) {
        sd_read_data(&response, 1);
        printf("CMD12 extra byte: 0x%02X\n", response);
    }
    
    // 等待响应，最多尝试200次
    uint8_t n = 200;
    do {
        sd_read_data(&response, 1);
        if (!(response & 0x80)) {
            printf("Valid response received: 0x%02X (attempt %d)\n", response, 200-n);
            break; // 找到有效的响应（最高位为0）
        }
        n--;
        msleep(5);  // 增加延时
    } while (n > 0);
    
    if (n == 0) {
        printf("No valid response received after 200 attempts\n");
        printf("Last response was: 0x%02X\n", response);
        printf("Possible issues:\n");
        printf("1. Check MISO pin connection\n");
        printf("2. Verify SD card power supply\n");
        printf("3. Check SPI clock frequency\n");
        printf("4. Verify CS pin timing\n");
        printf("5. Check if SD card is properly inserted\n");
        printf("6. Verify voltage levels (should be 3.3V)\n");
        printf("7. Try disabling and re-enabling power to the SD card\n");
        printf("8. Check for signal integrity issues\n");
        printf("9. Verify SD card supports SPI mode\n");
        printf("10. Check for proper pull-up resistors\n");
    }
    
    return response;
}

/*
 * @brief  Send 5 bytes command to the SD card.
 * @param  Cmd: The user expected command to send to SD card.
 * @param  Arg: The command argument.
 * @param  Crc: The CRC.
 * @retval None
 */
static void sd_end_cmd(void)
{
    // 不需要调用这个函数了，我们已经在sd_init中完全重写了命令序列
    // 只是为了兼容性而保留
}

/*
 * @brief  Returns the SD response.
 * @param  None
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
static uint8_t sd_get_response(void)
{
	uint8_t result;
	uint16_t timeout = 0x0FFF;
	/*!< Check if response is got or a timeout is happen */
	while (timeout--) {
		sd_read_data(&result, 1);
		/*!< Right response got */
		if (result != 0xFF)
			return result;
	}
	/*!< After time out */
	return 0xFF;
}

/*
 * @brief  Get SD card data response.
 * @param  None
 * @retval The SD status: Read data response xxx0<status>1
 *         - status 010: Data accecpted
 *         - status 101: Data rejected due to a crc error
 *         - status 110: Data rejected due to a Write error.
 *         - status 111: Data rejected due to other error.
 */
static uint8_t sd_get_dataresponse(void)
{
	uint8_t response;
	/*!< Read resonse */
	sd_read_data(&response, 1);
	/*!< Mask unused bits */
	response &= 0x1F;
	if (response != 0x05)
		return 0xFF;
	/*!< Wait null data */
	sd_read_data(&response, 1);
	while (response == 0)
		sd_read_data(&response, 1);
	/*!< Return response */
	return 0;
}

/*
 * @brief  Read the CSD card register
 *         Reading the contents of the CSD register in SPI mode is a simple
 *         read-block transaction.
 * @param  SD_csd: pointer on an SCD register structure
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
static uint8_t sd_get_csdregister(SD_CSD *SD_csd)
{
	uint8_t csd_tab[18];
	uint8_t response;
	
	/*!< Send CMD9 (CSD register) or CMD10(CSD register) */
	response = sd_send_cmd(SD_CMD9, 0, 0);
	
	/*!< Wait for response in the R1 format (0x00 is no errors) */
	if (response != 0x00) {
		sd_end_cmd();
		return 0xFF;
	}
	
	/*!< Wait for the start block token */
	if (sd_get_response() != SD_START_DATA_SINGLE_BLOCK_READ) {
		sd_end_cmd();
		return 0xFF;
	}
	
	/*!< Store CSD register value on csd_tab */
	/*!< Get CRC bytes (not really needed by us, but required by SD) */
	sd_read_data(csd_tab, 18);
	sd_end_cmd();
	
	/*!< Byte 0 */
	SD_csd->CSDStruct = (csd_tab[0] & 0xC0) >> 6;
	SD_csd->SysSpecVersion = (csd_tab[0] & 0x3C) >> 2;
	SD_csd->Reserved1 = csd_tab[0] & 0x03;
	/*!< Byte 1 */
	SD_csd->TAAC = csd_tab[1];
	/*!< Byte 2 */
	SD_csd->NSAC = csd_tab[2];
	/*!< Byte 3 */
	SD_csd->MaxBusClkFrec = csd_tab[3];
	/*!< Byte 4 */
	SD_csd->CardComdClasses = csd_tab[4] << 4;
	/*!< Byte 5 */
	SD_csd->CardComdClasses |= (csd_tab[5] & 0xF0) >> 4;
	SD_csd->RdBlockLen = csd_tab[5] & 0x0F;
	/*!< Byte 6 */
	SD_csd->PartBlockRead = (csd_tab[6] & 0x80) >> 7;
	SD_csd->WrBlockMisalign = (csd_tab[6] & 0x40) >> 6;
	SD_csd->RdBlockMisalign = (csd_tab[6] & 0x20) >> 5;
	SD_csd->DSRImpl = (csd_tab[6] & 0x10) >> 4;
	SD_csd->Reserved2 = 0; /*!< Reserved */
	SD_csd->DeviceSize = (csd_tab[6] & 0x03) << 10;
	/*!< Byte 7 */
	SD_csd->DeviceSize = (csd_tab[7] & 0x3F) << 16;
	/*!< Byte 8 */
	SD_csd->DeviceSize |= csd_tab[8] << 8;
	/*!< Byte 9 */
	SD_csd->DeviceSize |= csd_tab[9];
	/*!< Byte 10 */
	SD_csd->EraseGrSize = (csd_tab[10] & 0x40) >> 6;
	SD_csd->EraseGrMul = (csd_tab[10] & 0x3F) << 1;
	/*!< Byte 11 */
	SD_csd->EraseGrMul |= (csd_tab[11] & 0x80) >> 7;
	SD_csd->WrProtectGrSize = (csd_tab[11] & 0x7F);
	/*!< Byte 12 */
	SD_csd->WrProtectGrEnable = (csd_tab[12] & 0x80) >> 7;
	SD_csd->ManDeflECC = (csd_tab[12] & 0x60) >> 5;
	SD_csd->WrSpeedFact = (csd_tab[12] & 0x1C) >> 2;
	SD_csd->MaxWrBlockLen = (csd_tab[12] & 0x03) << 2;
	/*!< Byte 13 */
	SD_csd->MaxWrBlockLen |= (csd_tab[13] & 0xC0) >> 6;
	SD_csd->WriteBlockPaPartial = (csd_tab[13] & 0x20) >> 5;
	SD_csd->Reserved3 = 0;
	SD_csd->ContentProtectAppli = (csd_tab[13] & 0x01);
	/*!< Byte 14 */
	SD_csd->FileFormatGrouop = (csd_tab[14] & 0x80) >> 7;
	SD_csd->CopyFlag = (csd_tab[14] & 0x40) >> 6;
	SD_csd->PermWrProtect = (csd_tab[14] & 0x20) >> 5;
	SD_csd->TempWrProtect = (csd_tab[14] & 0x10) >> 4;
	SD_csd->FileFormat = (csd_tab[14] & 0x0C) >> 2;
	SD_csd->ECC = (csd_tab[14] & 0x03);
	/*!< Byte 15 */
	SD_csd->CSD_CRC = (csd_tab[15] & 0xFE) >> 1;
	SD_csd->Reserved4 = 1;
	/*!< Return the reponse */
	return 0;
}

/*
 * @brief  Read the CID card register.
 *         Reading the contents of the CID register in SPI mode is a simple
 *         read-block transaction.
 * @param  SD_cid: pointer on an CID register structure
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
static uint8_t sd_get_cidregister(SD_CID *SD_cid)
{
	uint8_t cid_tab[18];
	uint8_t response;
	
	/*!< Send CMD10 (CID register) */
	response = sd_send_cmd(SD_CMD10, 0, 0);
	
	/*!< Wait for response in the R1 format (0x00 is no errors) */
	if (response != 0x00) {
		sd_end_cmd();
		return 0xFF;
	}
	
	/*!< Wait for the start block token */
	if (sd_get_response() != SD_START_DATA_SINGLE_BLOCK_READ) {
		sd_end_cmd();
		return 0xFF;
	}
	
	/*!< Store CID register value on cid_tab */
	/*!< Get CRC bytes (not really needed by us, but required by SD) */
	sd_read_data(cid_tab, 18);
	sd_end_cmd();
	
	/*!< Byte 0 */
	SD_cid->ManufacturerID = cid_tab[0];
	/*!< Byte 1 */
	SD_cid->OEM_AppliID = cid_tab[1] << 8;
	/*!< Byte 2 */
	SD_cid->OEM_AppliID |= cid_tab[2];
	/*!< Byte 3 */
	SD_cid->ProdName1 = cid_tab[3] << 24;
	/*!< Byte 4 */
	SD_cid->ProdName1 |= cid_tab[4] << 16;
	/*!< Byte 5 */
	SD_cid->ProdName1 |= cid_tab[5] << 8;
	/*!< Byte 6 */
	SD_cid->ProdName1 |= cid_tab[6];
	/*!< Byte 7 */
	SD_cid->ProdName2 = cid_tab[7];
	/*!< Byte 8 */
	SD_cid->ProdRev = cid_tab[8];
	/*!< Byte 9 */
	SD_cid->ProdSN = cid_tab[9] << 24;
	/*!< Byte 10 */
	SD_cid->ProdSN |= cid_tab[10] << 16;
	/*!< Byte 11 */
	SD_cid->ProdSN |= cid_tab[11] << 8;
	/*!< Byte 12 */
	SD_cid->ProdSN |= cid_tab[12];
	/*!< Byte 13 */
	SD_cid->Reserved1 |= (cid_tab[13] & 0xF0) >> 4;
	SD_cid->ManufactDate = (cid_tab[13] & 0x0F) << 8;
	/*!< Byte 14 */
	SD_cid->ManufactDate |= cid_tab[14];
	/*!< Byte 15 */
	SD_cid->CID_CRC = (cid_tab[15] & 0xFE) >> 1;
	SD_cid->Reserved2 = 1;
	/*!< Return the reponse */
	return 0;
}

/*
 * @brief  Returns information about specific card.
 * @param  cardinfo: pointer to a SD_CardInfo structure that contains all SD
 *         card information.
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
static uint8_t sd_get_cardinfo(SD_CardInfo *cardinfo)
{
	if (sd_get_csdregister(&(cardinfo->SD_csd)))
		return 0xFF;
	if (sd_get_cidregister(&(cardinfo->SD_cid)))
		return 0xFF;
	cardinfo->CardCapacity = (cardinfo->SD_csd.DeviceSize + 1) * 1024;
	cardinfo->CardBlockSize = 1 << (cardinfo->SD_csd.RdBlockLen);
	cardinfo->CardCapacity *= cardinfo->CardBlockSize;
	/*!< Returns the reponse */
	return 0;
}

// 检测SD卡是否存在的函数
static uint8_t sd_detect_card(void)
{
    uint8_t response;
    uint8_t dummy = 0xFF;
    uint8_t i;
    uint16_t retry = 0;
    
    printf("Detecting SD card...\n");
    
    // 确保CS引脚为高电平
    SD_CS_HIGH();
    msleep(200);
    
    // 发送多个时钟脉冲，让SD卡进入稳定状态
    for(i = 0; i < 20; i++) {
        sd_write_data(&dummy, 1);
    }
    
    // 尝试多次发送CMD0，直到收到正确的响应
    do {
        // 拉低CS引脚
        SD_CS_LOW();
        msleep(10);
        
        // 发送CMD0命令，检查是否有响应
        response = sd_send_cmd(SD_CMD0, 0, 0x95);
        printf("CMD0 response: 0x%02X\n", response);
        
        // 释放CS引脚
        SD_CS_HIGH();
        sd_send_dummy_bytes(10);
        msleep(10);
        
        // 如果收到0x01响应，说明SD卡存在
        if(response == 0x01) {
            printf("SD card detected\n");
            return 0;
        }
        
        retry++;
        if(retry > 10) {
            printf("SD card detection timeout\n");
            return 1;
        }
    } while(1);
    
    return 1;
}

/*
 * @brief  Initializes the SD/SD communication.
 * @param  None
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
uint8_t sd_init(void)
{
    uint8_t r1;
    uint8_t buff[10] = {0xFF};
    uint16_t retry;
    uint8_t i;
    bool is_high_capacity = false;
    
    printf("Initializing SD card...\n");
    
    // 硬件低级初始化
    sd_lowlevel_init(SD_SPI_NUM);
    
    // 延时至少1000ms以让SD卡完全上电
    printf("Waiting for SD card power stabilization (1000ms)...\n");
    msleep(1000);
    
    // 确保CS引脚为高电平状态
    SD_CS_HIGH();
    msleep(500); // 增加延时确保CS稳定
    
    // 发送更多的时钟脉冲，帮助SD卡进入SPI模式
    printf("Sending 400 clock pulses with CS HIGH...\n");
    uint8_t dummy = 0xFF;
    for(i = 0; i < 400; i++) {
        if(i % 20 == 0) {
            printf("  Sent %d clock pulses...\n", i);
            // 每20个脉冲检查一次CS状态
            if(gpiohs_get_pin(SD_CS_GPIO_NUM) != GPIO_PV_HIGH) {
                printf("Warning: CS pin dropped LOW during clock pulses\n");
                gpiohs_set_pin(SD_CS_GPIO_NUM, GPIO_PV_HIGH);
                msleep(20);  // 增加延时
            }
        }
        
        // 在发送每个脉冲前检查CS状态
        if(gpiohs_get_pin(SD_CS_GPIO_NUM) != GPIO_PV_HIGH) {
            printf("Warning: CS pin is LOW before pulse %d, setting HIGH\n", i);
            gpiohs_set_pin(SD_CS_GPIO_NUM, GPIO_PV_HIGH);
            msleep(10);
        }
        
        sd_write_data(&dummy, 1);
        msleep(2);  // 增加延时到2ms
        
        // 发送后再次检查CS状态
        if(gpiohs_get_pin(SD_CS_GPIO_NUM) != GPIO_PV_HIGH) {
            printf("Warning: CS pin dropped LOW after pulse %d, setting HIGH\n", i);
            gpiohs_set_pin(SD_CS_GPIO_NUM, GPIO_PV_HIGH);
            msleep(10);
        }
    }
    printf("  All 400 clock pulses sent\n");
    
    // 额外延时确保SD卡识别时钟信号
    printf("Waiting for SD card to recognize clock signals (1000ms)...\n");
    msleep(1000);  // 增加延时到1000ms
    
    // 尝试进入SPI模式，增加重试次数并放慢速度
    retry = 0;
    do {
        // 在每次尝试前都重置SPI和CS状态
        SD_CS_HIGH();
        msleep(500); // 增加延时
        sd_send_dummy_bytes(100);
        msleep(200);
        
        // CMD0: 复位SD卡进入SPI模式
        printf("Sending CMD0, attempt %d\n", retry+1);
        
        // 发送CMD0之前确保CS为低电平
        SD_CS_LOW();
        msleep(200); // 增加延时确保CS稳定
        
        // 先发送一些时钟脉冲
        sd_send_dummy_bytes(50);
        
        // 打印当前SPI状态
        printf("Current SPI state before CMD0:\n");
        printf("  CS pin state: %d\n", gpiohs_get_pin(SD_CS_GPIO_NUM));
        printf("  MOSI pin: %d\n", PIN_SPI_MOSI);
        printf("  MISO pin: %d\n", PIN_SPI_MISO);
        printf("  SCLK pin: %d\n", PIN_SPI_SCLK);
        printf("  SPI clock rate: 100Hz\n");
        
        // 检查MISO引脚状态
        uint8_t miso_state = 0;
        for(i = 0; i < 50; i++) {
            sd_read_data(&miso_state, 1);
            printf("  MISO state: 0x%02X\n", miso_state);
        }
        
        // 尝试不同的重置方式：先拉高CS，发送更多时钟，再拉低CS
        SD_CS_HIGH();
        msleep(100);
        sd_send_dummy_bytes(50);
        SD_CS_LOW();
        msleep(100);
        
        r1 = sd_send_cmd(SD_CMD0, 0, 0x95);
        printf("CMD0 response: 0x%02X\n", r1);
        
        // 完成命令后释放片选
        SD_CS_HIGH();
        sd_send_dummy_bytes(50);
        
        // 延时让SD卡处理命令
        msleep(500);
        
        if(retry++ > 1000) {  // 增加重试次数
            printf("SD card init failed: CMD0 timeout (r1=FF)\n");
            printf("Please check physical connections:\n");
            printf("1. Is the SD card properly inserted?\n");
            printf("2. Are the pin connections correct? (MOSI, MISO, SCLK, CS)\n");
            printf("3. Does the SD card have power?\n");
            printf("4. Check voltage levels (should be 3.3V)\n");
            printf("5. Check SPI clock frequency (currently 100Hz)\n");
            printf("6. Verify CS pin is properly toggling\n");
            printf("7. Check MISO pin pull-up resistor\n");
            printf("8. Try a different SD card\n");
            printf("9. Check power supply stability\n");
            printf("10. Verify SD card supports SPI mode\n");
            printf("11. Check for proper ground connections\n");
            printf("12. Verify SD card is not write-protected\n");
            return 1;   // 超时，初始化失败
        }
    } while(r1 != 0x01);
    
    printf("SD card entered SPI mode successfully (r1=0x01)\n");
    
    // 延时一下再继续
    msleep(50);
    
    // CMD8: 检查SD卡版本
    SD_CS_LOW();
    msleep(10);
    
    // 0x000001AA - 供电电压2.7-3.6V + 检查模式
    r1 = sd_send_cmd(SD_CMD8, 0x000001AA, 0x87);
    printf("SD_CMD8 response: 0x%02X\n", r1);
    
    // 获取R7响应(共4字节)
    sd_read_data(buff, 4);
    printf("SD_CMD8 R7 data: %02X %02X %02X %02X\n", buff[0], buff[1], buff[2], buff[3]);
    
    // 完成命令后释放片选
    SD_CS_HIGH();
    sd_send_dummy_bytes(5);
    
    // 延迟一下
    msleep(50);
    
    if(r1 == 0x01) { // SD v2.0 或更高版本
        // 检查电压范围和回写的检查模式值
        if(buff[2] == 0x01 && buff[3] == 0xAA) {
            printf("SDv2 card detected\n");
            
            // 尝试初始化SD卡
            retry = 0;
            do {
                // CMD55 + ACMD41 初始化SD卡
                SD_CS_LOW();
                msleep(10);
                
                r1 = sd_send_cmd(SD_CMD55, 0, 0);
                SD_CS_HIGH();
                sd_send_dummy_bytes(3);
                msleep(20);  // 延时
                
                SD_CS_LOW(); 
                msleep(10);
                
                r1 = sd_send_cmd(SD_ACMD41, 0x40000000, 0); // HCS位置1，支持高容量
                SD_CS_HIGH();
                sd_send_dummy_bytes(3);
                msleep(20);  // 延时
                
                if(retry++ > 500) {
                    printf("SD card init failed: ACMD41 timeout (r1=%02X)\n", r1);
                    return 2;
                }
            } while(r1 != 0);
            
            // 延时
            msleep(50);
            
            // CMD58: 读取OCR寄存器，检查CCS位确定是SDHC还是SDSC
            SD_CS_LOW();
            msleep(10);
            
            r1 = sd_send_cmd(SD_CMD58, 0, 0);
            if(r1 == 0) {
                sd_read_data(buff, 4);
                
                // 打印OCR寄存器内容
                printf("OCR Register: %02X %02X %02X %02X\n", buff[0], buff[1], buff[2], buff[3]);
                
                // 检查CCS位(OCR[30])
                if(buff[0] & 0x40) {
                    printf("SDHC/SDXC card detected (OCR[30]=1)\n");
                    is_high_capacity = true;
                } else {
                    printf("SDSC card detected (OCR[30]=0)\n");
                }
            }
            SD_CS_HIGH();
            sd_send_dummy_bytes(3);
        } else {
            printf("Unexpected R7 response, voltage range check failed\n");
        }
    } else { // SD v1.x 或 MMC
        // 尝试SD v1.x初始化
        printf("SDv1 or MMC card detected\n");
        
        SD_CS_LOW();
        msleep(10);
        r1 = sd_send_cmd(SD_CMD55, 0, 0);
        SD_CS_HIGH();
        sd_send_dummy_bytes(3);
        msleep(20);
        
        SD_CS_LOW();
        msleep(10);
        r1 = sd_send_cmd(SD_ACMD41, 0, 0);
        SD_CS_HIGH();
        sd_send_dummy_bytes(3);
        
        if(r1 <= 1) { // SD v1.x
            retry = 0;
            do {
                SD_CS_LOW();
                msleep(10);
                r1 = sd_send_cmd(SD_CMD55, 0, 0);
                SD_CS_HIGH();
                sd_send_dummy_bytes(3);
                msleep(20);
                
                SD_CS_LOW();
                msleep(10);
                r1 = sd_send_cmd(SD_ACMD41, 0, 0);
                SD_CS_HIGH();
                sd_send_dummy_bytes(3);
                msleep(20);
                
                if(retry++ > 500) {
                    printf("SD card init failed: SDv1 ACMD41 timeout\n");
                    return 3;
                }
            } while(r1 != 0);
            
            printf("SDv1 card initialized successfully\n");
        } else { // 可能是MMC卡
            // 尝试使用MMC初始化命令
            retry = 0;
            do {
                SD_CS_LOW();
                msleep(10);
                r1 = sd_send_cmd(1, 0, 0); // CMD1用于MMC卡初始化
                SD_CS_HIGH();
                sd_send_dummy_bytes(3);
                msleep(20);
                
                if(retry++ > 500) {
                    printf("SD card init failed: MMC init timeout\n");
                    return 4;
                }
            } while(r1 != 0);
            
            printf("MMC card initialized successfully\n");
        }
    }
    
    // 设置块大小为512字节
    SD_CS_LOW();
    msleep(10);
    r1 = sd_send_cmd(SD_CMD16, 512, 0);
    SD_CS_HIGH();
    sd_send_dummy_bytes(3);
    
    if(r1 != 0) {
        printf("SD card init failed: Failed to set block size (r1=%02X)\n", r1);
        return 5;
    }
    
    // 启用高速模式 (在初始化完成后)
    msleep(50);
    SD_HIGH_SPEED_ENABLE();
    
    // 获取卡信息
    msleep(20);
    r1 = sd_get_cardinfo(&cardinfo);
    if(r1 != 0) {
        printf("SD card init failed: Failed to get card info (r1=%02X)\n", r1);
        return 6;
    }
    
    // 打印卡信息
    printf("SD card capacity: %lu bytes\n", (unsigned long)cardinfo.CardCapacity);
    printf("SD card block size: %u bytes\n", (unsigned int)cardinfo.CardBlockSize);
    
    // 保存卡片类型信息，可以在全局变量中使用
    if(is_high_capacity) {
        printf("Using high capacity card mode (SDHC/SDXC)\n");
        // 在高容量卡模式下，地址单位是块而不是字节
    } else {
        printf("Using standard capacity card mode (SDSC)\n");
        // 在标准容量卡模式下，地址单位是字节
    }
    
    printf("SD card initialization complete\n");
    return 0;
}

/*
 * @brief  Reads a block of data from the SD.
 * @param  data_buff: pointer to the buffer that receives the data read from the
 *                  SD.
 * @param  sector: SD's internal address to read from.
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
uint8_t sd_read_sector(uint8_t *data_buff, uint32_t sector, uint32_t count)
{
    uint8_t frame[2], flag;
    uint8_t response;
    uint8_t token;
    
    printf("Reading sector %lu, count=%lu\n", (unsigned long)sector, (unsigned long)count);
    
    // 确保之前的操作已完成
    SD_CS_HIGH();
    sd_send_dummy_bytes(10);
    msleep(10);
    
    // 拉低CS信号
    SD_CS_LOW();
    msleep(10);

    // 发送读取命令
    if (count == 1) {
        flag = 0;
        response = sd_send_cmd(SD_CMD17, sector, 0);
    } else {
        flag = 1;
        response = sd_send_cmd(SD_CMD18, sector, 0);
    }
    
    // 检查响应
    if (response != 0x00) {
        printf("Read command failed, response=0x%02X\n", response);
        SD_CS_HIGH();
        sd_send_dummy_bytes(10);
        return 0xFF;
    }
    
    // 读取数据
    uint32_t i;
    for (i = 0; i < count; i++) {
        // 等待数据起始标记
        uint16_t timeout = 0xFFF;
        do {
            sd_read_data(&token, 1);
            timeout--;
            if (timeout == 0) {
                printf("Data token timeout\n");
                SD_CS_HIGH();
                sd_send_dummy_bytes(10);
                return 0xFF;
            }
        } while (token != SD_START_DATA_SINGLE_BLOCK_READ);
        
        // 读取512字节数据
        sd_read_data(data_buff + i * 512, 512);
        
        // 读取CRC (2字节)
        sd_read_data(frame, 2);
        
        // 多块读取需要继续读取下一块
        if (i < count - 1) {
            sd_send_dummy_bytes(5); // 发送额外的时钟脉冲
        }
    }
    
    // 如果是多块读取，发送停止命令
    if (flag) {
        response = sd_send_cmd(SD_CMD12, 0, 0);
        // 读取停止命令的响应字节
        sd_read_data(&token, 1);
    }
    
    // 释放CS信号
    SD_CS_HIGH();
    sd_send_dummy_bytes(10);
    
    printf("Sector read complete\n");
    return 0;
}

/*
 * @brief  Writes a block on the SD
 * @param  data_buff: pointer to the buffer containing the data to be written on
 *                  the SD.
 * @param  sector: address to write on.
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
uint8_t sd_write_sector(uint8_t *data_buff, uint32_t sector, uint32_t count)
{
    uint8_t response;
    uint8_t token;
    uint32_t i;
    
    printf("Writing sector %lu, count=%lu\n", (unsigned long)sector, (unsigned long)count);
    
    // 确保之前的操作已完成
    SD_CS_HIGH();
    sd_send_dummy_bytes(10);
    msleep(10);
    
    // 写入单块或多块
    if (count == 1) {
        // 单块写入
        SD_CS_LOW();
        msleep(10);
        
        // 发送写命令
        response = sd_send_cmd(SD_CMD24, sector, 0);
        if (response != 0x00) {
            printf("Write command failed, response=0x%02X\n", response);
            SD_CS_HIGH();
            sd_send_dummy_bytes(10);
            return 0xFF;
        }
        
        // 发送数据起始令牌
        token = SD_START_DATA_SINGLE_BLOCK_WRITE;
        sd_write_data(&token, 1);
        
        // 发送数据
        sd_write_data(data_buff, 512);
        
        // 发送CRC (不需要真正的CRC)
        uint8_t crc[2] = {0xFF, 0xFF};
        sd_write_data(crc, 2);
        
        // 获取数据响应
        sd_read_data(&response, 1);
        response &= 0x1F;
        
        // 检查数据是否被接受
        if ((response & 0x1F) != 0x05) {
            printf("Data rejected, response=0x%02X\n", response);
            SD_CS_HIGH();
            sd_send_dummy_bytes(10);
            return 0xFF;
        }
        
        // 等待写入完成
        uint16_t timeout = 0xFFF;
        do {
            sd_read_data(&response, 1);
            timeout--;
            if (timeout == 0) {
                printf("Write operation timeout\n");
                SD_CS_HIGH();
                sd_send_dummy_bytes(10);
                return 0xFF;
            }
        } while (response == 0x00);
    } else {
        // 多块写入（每次单独发送CMD24以提高兼容性）
        for (i = 0; i < count; i++) {
            // 写入第i个扇区
            response = sd_write_sector(data_buff + i * 512, sector + i, 1);
            if (response != 0) {
                return response; // 如果写入失败，返回错误
            }
            msleep(10); // 块之间稍作延时
        }
    }
    
    // 释放CS信号
    SD_CS_HIGH();
    sd_send_dummy_bytes(10);
    
    printf("Sector write complete\n");
    return 0;
}

uint8_t sd_read_sector_dma(uint8_t *data_buff, uint32_t sector, uint32_t count)
{
	uint8_t frame[2], flag;
	uint8_t response;

	/*!< Send CMD17 (SD_CMD17) to read one block */
	if (count == 1) {
		flag = 0;
		response = sd_send_cmd(SD_CMD17, sector, 0);
	} else {
		flag = 1;
		response = sd_send_cmd(SD_CMD18, sector, 0);
	}
	/*!< Check if the SD acknowledged the read block command: R1 response (0x00: no errors) */
	if (response != 0x00) {
		sd_end_cmd();
		return 0xFF;
	}
	while (count) {
		if (sd_get_response() != SD_START_DATA_SINGLE_BLOCK_READ)
			break;
		/*!< Read the SD block data : read NumByteToRead data */
		sd_read_data_dma(data_buff);
		/*!< Get CRC bytes (not really needed by us, but required by SD) */
		sd_read_data(frame, 2);
		data_buff += 512;
		count--;
	}
	sd_end_cmd();
	if (flag) {
		response = sd_send_cmd(SD_CMD12, 0, 0);
		sd_end_cmd();
	}
	/*!< Returns the reponse */
	return count > 0 ? 0xFF : 0;
}

uint8_t sd_write_sector_dma(uint8_t *data_buff, uint32_t sector, uint32_t count)
{
	uint8_t frame[2] = {0xFF};
    frame[1] = SD_START_DATA_SINGLE_BLOCK_WRITE;
    uint32_t i = 0;
    uint8_t response;
    
	while (count--) {
        response = sd_send_cmd(SD_CMD24, sector + i, 0);
        /*!< Check if the SD acknowledged the write block command: R1 response (0x00: no errors) */
        if (response != 0x00) {
            sd_end_cmd();
            return 0xFF;
        }

		/*!< Send the data token to signify the start of the data */
		sd_write_data(frame, 2);
		/*!< Write the block data to SD : write count data by block */
		sd_write_data_dma(data_buff);
		/*!< Put CRC bytes (not really needed by us, but required by SD) */
		sd_write_data(frame, 2);
		data_buff += 512;
		/*!< Read data response */
		if (sd_get_dataresponse() != 0x00) {
			sd_end_cmd();
			return 0xFF;
		}
		i++;
	}
	sd_end_cmd();
	sd_end_cmd();
	/*!< Returns the reponse */
	return 0;
}

