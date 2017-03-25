/*****************************************************************************
 *
 * Filename:
 * ---------
 *   catc24c16.h
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   Header file of CAM_CAL driver
 *
 *
 * Author:
 * -------
 *   LukeHu
 *
 *============================================================================*/
#ifndef __OV8858_EEPROM_H
#define __OV8858_EEPROM_H

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 *a_pRecvData,
u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
#endif /* __CAM_CAL_H */

