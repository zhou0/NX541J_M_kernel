/*
 *  Header for Richtek RT9466 Charger
 *
 *  Copyright (C) 2015 Richtek Technology Corp.
 *  ShuFanLee <shufan_lee@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef __RT9466_CHARGER_H
#define __RT9466_CHARGER_H

#define RT9466_SLAVE_ADDR    0x53
#define RT9466_DEVICE_ID_E2  0x81
#define RT9466_DEVICE_ID_E3  0x82

enum rt9466_reg_addr {
	RT9466_REG_CORE_CTRL0,
	RT9466_REG_CHG_CTRL1,
	RT9466_REG_CHG_CTRL2,
	RT9466_REG_CHG_CTRL3,
	RT9466_REG_CHG_CTRL4,
	RT9466_REG_CHG_CTRL5,
	RT9466_REG_CHG_CTRL6,
	RT9466_REG_CHG_CTRL7,
	RT9466_REG_CHG_CTRL8,
	RT9466_REG_CHG_CTRL9,
	RT9466_REG_CHG_CTRL10,
	RT9466_REG_CHG_CTRL11,
	RT9466_REG_CHG_CTRL12,
	RT9466_REG_CHG_CTRL13,
	RT9466_REG_CHG_CTRL14,
	RT9466_REG_CHG_CTRL15,
	RT9466_REG_CHG_CTRL16,
	RT9466_REG_CHG_ADC,
	RT9466_REG_CHG_CTRL17 = 0x19,
	RT9466_REG_CHG_CTRL18,
	RT9466_REG_DEVICE_ID = 0x40,
	RT9466_REG_CHG_STAT = 0x42,
	RT9466_REG_CHG_NTC,
	RT9466_REG_ADC_DATA_H,
	RT9466_REG_ADC_DATA_L,
	RT9466_REG_CHG_STATC = 0x50,
	RT9466_REG_CHG_FAULT,
	RT9466_REG_TS_STATC,
	RT9466_REG_CHG_IRQ1,
	RT9466_REG_CHG_IRQ2,
	RT9466_REG_CHG_IRQ3,
	RT9466_REG_CHG_STATC_CTRL = 0x60,
	RT9466_REG_CHG_FAULT_CTRL,
	RT9466_REG_TS_STATC_CTRL,
	RT9466_REG_CHG_IRQ1_CTRL,
	RT9466_REG_CHG_IRQ2_CTRL,
	RT9466_REG_CHG_IRQ3_CTRL,
	RT9466_REG_MAX,
};

/* This enumeration defines the index of register
* You can get register address by the following two ways
* 1. RT9466_REG_XXXX
* 2. rt9466_reg_addr[RT9466_REG_IDX_XXXX]
*/
enum rt9466_reg_addr_idx {
	RT9466_REG_IDX_CORE_CTRL0 = 0,
	RT9466_REG_IDX_CHG_CTRL1,
	RT9466_REG_IDX_CHG_CTRL2,
	RT9466_REG_IDX_CHG_CTRL3,
	RT9466_REG_IDX_CHG_CTRL4,
	RT9466_REG_IDX_CHG_CTRL5,
	RT9466_REG_IDX_CHG_CTRL6,
	RT9466_REG_IDX_CHG_CTRL7,
	RT9466_REG_IDX_CHG_CTRL8,
	RT9466_REG_IDX_CHG_CTRL9,
	RT9466_REG_IDX_CHG_CTRL10,
	RT9466_REG_IDX_CHG_CTRL11,
	RT9466_REG_IDX_CHG_CTRL12,
	RT9466_REG_IDX_CHG_CTRL13,
	RT9466_REG_IDX_CHG_CTRL14,
	RT9466_REG_IDX_CHG_CTRL15,
	RT9466_REG_IDX_CHG_CTRL16,
	RT9466_REG_IDX_CHG_ADC,
	RT9466_REG_IDX_CHG_CTRL17,
	RT9466_REG_IDX_CHG_CTRL18,
	RT9466_REG_IDX_DEVICE_ID,
	RT9466_REG_IDX_CHG_STAT,
	RT9466_REG_IDX_CHG_NTC,
	RT9466_REG_IDX_ADC_DATA_H,
	RT9466_REG_IDX_ADC_DATA_L,
	RT9466_REG_IDX_CHG_STATC,
	RT9466_REG_IDX_CHG_FAULT,
	RT9466_REG_IDX_TS_STATC,
	RT9466_REG_IDX_CHG_IRQ1,
	RT9466_REG_IDX_CHG_IRQ2,
	RT9466_REG_IDX_CHG_IRQ3,
	RT9466_REG_IDX_CHG_STATC_CTRL,
	RT9466_REG_IDX_CHG_FAULT_CTRL,
	RT9466_REG_IDX_TS_STATC_CTRL,
	RT9466_REG_IDX_CHG_IRQ1_CTRL,
	RT9466_REG_IDX_CHG_IRQ2_CTRL,
	RT9466_REG_IDX_CHG_IRQ3_CTRL,
	RT9466_REG_IDX_MAX,
};

/* =========================== */
/* RT9466 Parameter            */
/* =========================== */

/* mA */
#define RT9466_ICHG_NUM		64
#define RT9466_ICHG_MIN		100
#define RT9466_ICHG_MAX		5000
#define RT9466_ICHG_STEP	100

/* mA */
#define RT9466_IEOC_NUM		16
#define RT9466_IEOC_MIN		100
#define RT9466_IEOC_MAX		850
#define RT9466_IEOC_STEP	50

/* mV */
#define RT9466_MIVR_NUM		128
#define RT9466_MIVR_MIN		3900
#define RT9466_MIVR_MAX		13400
#define RT9466_MIVR_STEP	100

/* mA */
#define RT9466_AICR_NUM		64
#define RT9466_AICR_MIN		100
#define RT9466_AICR_MAX		3250
#define RT9466_AICR_STEP	50

/* mV */
#define RT9466_BAT_VOREG_NUM	128
#define RT9466_BAT_VOREG_MIN	3900
#define RT9466_BAT_VOREG_MAX	4710
#define RT9466_BAT_VOREG_STEP	10

/* mV */
#define RT9466_BOOST_VOREG_NUM	64
#define RT9466_BOOST_VOREG_MIN	4425
#define RT9466_BOOST_VOREG_MAX	5825
#define RT9466_BOOST_VOREG_STEP	25

/* mV */
#define RT9466_VPREC_NUM	16
#define RT9466_VPREC_MIN	2000
#define RT9466_VPREC_MAX	3500
#define RT9466_VPREC_STEP	100

/* mA */
#define RT9466_IPREC_NUM	16
#define RT9466_IPREC_MIN	100
#define RT9466_IPREC_MAX	850
#define RT9466_IPREC_STEP	50

/* Watchdog fast-charge timer */
/* hour */
#define RT9466_WT_FC_NUM	8
#define RT9466_WT_FC_MIN	4
#define RT9466_WT_FC_MAX	20
#define RT9466_WT_FC_STEP	2

/* IR compensation */
/* ohm */
#define RT9466_IRCMP_RES_NUM	8
#define RT9466_IRCMP_RES_MIN	0
#define RT9466_IRCMP_RES_MAX	140
#define RT9466_IRCMP_RES_STEP	20

/* IR compensation maximum voltage clamp */
/* mV */
#define RT9466_IRCMP_VCLAMP_NUM		8
#define RT9466_IRCMP_VCLAMP_MIN		0
#define RT9466_IRCMP_VCLAMP_MAX		224
#define RT9466_IRCMP_VCLAMP_STEP	32

/* PE+20 voltage */
/* mV */
#define RT9466_PEP20_VOLT_NUM	19
#define RT9466_PEP20_VOLT_MIN	5500
#define RT9466_PEP20_VOLT_MAX	14500
#define RT9466_PEP20_VOLT_STEP	500

/* There's no pattern in Boost OC */
#define RT9466_BOOST_OC_THRESHOLD_NUM 8
extern const u32 rt9466_boost_oc_threshold[RT9466_BOOST_OC_THRESHOLD_NUM];

/* ADC unit/offset */
#define RT9466_ADC_UNIT_VBUS_DIV5	25 /* mV */
#define RT9466_ADC_UNIT_VBUS_DIV2	10 /* mV */
#define RT9466_ADC_UNIT_VBAT		5  /* mV */
#define RT9466_ADC_UNIT_VSYS		5  /* mV */
#define RT9466_ADC_UNIT_REGN		5  /* mV */
#define RT9466_ADC_UNIT_TS_BAT		25 /* 0.01% */
#define RT9466_ADC_UNIT_IBAT		50 /* mA */
#define RT9466_ADC_UNIT_TEMP_JC		2  /* degree */

#define RT9466_ADC_OFFSET_VBUS_DIV5	0 /* mV */
#define RT9466_ADC_OFFSET_VBUS_DIV2	0 /* mV */
#define RT9466_ADC_OFFSET_VBAT		0 /* mV */
#define RT9466_ADC_OFFSET_VSYS		0 /* mV */
#define RT9466_ADC_OFFSET_REGN		0 /* mV */
#define RT9466_ADC_OFFSET_TS_BAT	0 /* % */
#define RT9466_ADC_OFFSET_IBAT		0 /* mA */
#define RT9466_ADC_OFFSET_TEMP_JC	(-40)  /* degree */


/* ========== CORE_CTRL0 0x00 ============ */
#define RT9466_SHIFT_RST 7
#define RT9466_MASK_RST  (1 << RT9466_SHIFT_RST)

/* ========== CHG_CTRL1 0x01 ============ */
#define RT9466_SHIFT_OPA_MODE   0
#define RT9466_SHIFT_HZ_EN      2

#define RT9466_MASK_OPA_MODE   (1 << RT9466_SHIFT_OPA_MODE)
#define RT9466_MASK_HZ_EN      (1 << RT9466_SHIFT_HZ_EN)

/* ========== CHG_CTRL2 0x02 ============ */
#define RT9466_SHIFT_CHG_EN	0
#define RT9466_SHIFT_CFO_EN	1
#define RT9466_SHIFT_IINLMTSEL	2
#define RT9466_SHIFT_TE_EN	4

#define RT9466_MASK_CHG_EN	(1 << RT9466_SHIFT_CHG_EN)
#define RT9466_MASK_CFO_EN	(1 << RT9466_SHIFT_CFO_EN)
#define RT9466_MASK_IINLMTSEL	0x0C
#define RT9466_MASK_TE_EN	(1 << RT9466_SHIFT_TE_EN)


/* ========== CHG_CTRL3 0x03 ============ */
#define RT9466_SHIFT_AICR    2
#define RT9466_SHIFT_AICR_EN 1
#define RT9466_SHIFT_ILIM_EN 0

#define RT9466_MASK_AICR     0xFC
#define RT9466_MASK_AICR_EN  (1 << RT9466_SHIFT_AICR_EN)
#define RT9466_MASK_ILIM_EN  (1 << RT9466_SHIFT_ILIM_EN)

/* ========== CHG_CTRL4 0x04 ============ */
#define RT9466_SHIFT_BAT_VOREG  1

#define RT9466_MASK_BAT_VOREG  0xFE

/* ========== CHG_CTRL5 0x05 ============ */
#define RT9466_SHIFT_BOOST_VOREG  2

#define RT9466_MASK_BOOST_VOREG  0xFC

/* ========== CHG_CTRL6 0x06 ============ */
#define RT9466_SHIFT_MIVR    1
#define RT9466_SHIFT_MIVR_EN 0

#define RT9466_MASK_MIVR  0xFE
#define RT9466_MASK_MIVR_EN (1 << RT9466_SHIFT_MIVR_EN)

/* ========== CHG_CTRL7 0x07 ============ */
#define RT9466_SHIFT_ICHG    2

#define RT9466_MASK_ICHG     0xFC

/* ========== CHG_CTRL8 0x08 ============ */
#define RT9466_SHIFT_VPREC 4
#define RT9466_SHIFT_IPREC 0

#define RT9466_MASK_VPREC 0xF0
#define RT9466_MASK_IPREC 0x0F

/* ========== CHG_CTRL9 0x09 ============ */
#define RT9466_SHIFT_IEOC 4

#define RT9466_MASK_IEOC  0xF0

/* ========== CHG_CTRL10 0x0A ============ */
#define RT9466_SHIFT_BOOST_OC 0

#define RT9466_MASK_BOOST_OC 0x07

/* ========== CHG_CTRL12 0x0C ============ */
#define RT9466_SHIFT_TMR_EN 1
#define RT9466_SHIFT_WT_FC  5

#define RT9466_MASK_TMR_EN  (1 << RT9466_SHIFT_TMR_EN)
#define RT9466_MASK_WT_FC 0xE0

/* ========== CHG_CTRL13 0x0D ============ */
#define RT9466_SHIFT_WDT_EN 7

#define RT9466_MASK_WDT_EN (1 << RT9466_SHIFT_WDT_EN)

/* ========== CHG_CTRL17 0x19 ============ */
#define RT9466_SHIFT_PUMPX_EN    7
#define RT9466_SHIFT_PUMPX_20_10 6 /* Version of PE */
#define RT9466_SHIFT_PUMPX_UP_DN 5
#define RT9466_SHIFT_PUMPX_DEC   0


#define RT9466_MASK_PUMPX_EN (1 << RT9466_SHIFT_PUMPX_EN)
#define RT9466_MASK_PUMPX_20_10 (1 << RT9466_SHIFT_PUMPX_20_10)
#define RT9466_MASK_PUMPX_UP_DN (1 << RT9466_SHIFT_PUMPX_UP_DN)
#define RT9466_MASK_PUMPX_DEC 0x1F

/* ========== CHG_CTRL18 0x1A ============ */
#define RT9466_SHIFT_IRCMP_RES 3
#define RT9466_SHIFT_IRCMP_VCLAMP   0

#define RT9466_MASK_IRCMP_RES 0x38
#define RT9466_MASK_IRCMP_VCLAMP   0x07

/* ========== CHG_ADC 0x11 ============ */
#define RT9466_SHIFT_ADC_IN_SEL 4
#define RT9466_SHIFT_ADC_START 0

#define RT9466_MASK_ADC_IN_SEL 0xF0
#define RT9466_MASK_ADC_START (1 << RT9466_SHIFT_ADC_START)

/* ========== CHG_STAT 0x42 ============ */
#define RT9466_SHIFT_ADC_STAT 0
#define RT9466_SHIFT_CHG_STAT 6

#define RT9466_MASK_ADC_STAT  (1 << RT9466_SHIFT_ADC_STAT)
#define RT9466_MASK_CHG_STAT  0xC0

/* ========== CHG_IRQ3 0x55 ============ */
#define RT9466_SHIFT_SSFINISHI 4

#define RT9466_MASK_SSFINISHI (1 << RT9466_SHIFT_SSFINISHI)

/* ========== CHG_IRQ3 0x55 ============ */
#define RT9466_SHIFT_ADC_DONEI 0

#define RT9466_MASK_ADC_DONEI (1 << RT9466_SHIFT_ADC_DONEI)

#endif /* __RT9466_CHARGER_H */
