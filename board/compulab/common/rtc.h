#ifndef _RTC_H_
#define _RTC_H_


#ifndef CONFIG_SYS_I2C_RTC_BUS
#define CONFIG_SYS_I2C_RTC_BUS	1
#endif

#ifndef CONFIG_SYS_I2C_RTC_ADDR
#define CONFIG_SYS_I2C_RTC_ADDR	0x69
#endif

#define ABX8XX_REG_CFG_KEY	0x1f
#define ABX8XX_CFG_KEY_MISC	0x9d

#define ABX8XX_REG_BATMODE	0x27
#define ABX8XX_BATMODE_IOBM_NOT	0

#endif //_RTC_H_

