/* drivers/hwmon/mt6516/amit/APDS9930.c - APDS9930 ALS/PS driver
 *
 * Author: MingHsien Hsieh <minghsien.hsieh@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * By LiuQiang submit for PSENSOR REPORT DATA AND SYSTEM REBOOT ISSUE 2016/09/23
 */

#include <alsps.h>
#include <hwmsensor.h>
#include <cust_alsps.h>
#include "APDS9930.h"
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
/*by qliu4244 add sensors function start*/
#include <cust_gpio_usage.h>
#include <mt-plat/mt_gpio.h>
/*by qliu4244 add sensors function end*/
/******************************************************************************
 * configuration
*******************************************************************************/
/*----------------------------------------------------------------------------*/
#define IRQ_REQUEST_OTHER_GPIO 0
#define ADPS9933_NEW_ARCH
#define APDS9930_DEV_NAME     "APDS9930"
/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS]"
#define APS_FUN(f)               pr_debug(APS_TAG"%s\n", __func__)
#define APS_ERR(fmt, args...)    pr_err(APS_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    pr_debug(APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    pr_debug(APS_TAG fmt, ##args)

#define I2C_FLAG_WRITE	0
#define I2C_FLAG_READ	1

/*fix nx541j psensor thres  value too small start*/
int APDS9930_CMM_PPCOUNT_VALUE = 0x07;
int APDS9930_CMM_CONTROL_VALUE = 0x68;
/*fix nx541j psensor thres  value too small end*/

int ZOOM_TIME = 4;
/*by qliu4244 add sensors function start*/
/*unsigned int alsps_int_gpio_number = 0;*/
/*static int of_get_APDS9930_platform_data(struct device *dev);*/
/*by qliu4244 add sensors function end*/
struct alsps_hw alsps_cust;
static struct alsps_hw *hw = &alsps_cust;
struct platform_device *alspsPltFmDev;

/* For alsp driver get cust info */
struct alsps_hw *get_cust_alsps(void)
{
	return &alsps_cust;
}



/*----------------------------------------------------------------------------*/
static struct i2c_client *APDS9930_i2c_client;
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id APDS9930_i2c_id[] = { {APDS9930_DEV_NAME, 0}, {} };

/*----------------------------------------------------------------------------*/
static int APDS9930_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int APDS9930_i2c_remove(struct i2c_client *client);
static int APDS9930_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
/*----------------------------------------------------------------------------*/
static int APDS9930_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int APDS9930_i2c_resume(struct i2c_client *client);
static int APDS9930_remove(void);
static int APDS9930_local_init(void);
/*by qliu4244 add sensors function start*/
extern int mt_gpio_set_debounce(unsigned int gpio, unsigned int debounce);

#ifdef FACTORY_MACRO_PS
static int APDS9930_prox_offset_cal_process(void);
static int APDS9930_read_cal_file(char *file_path);
static int APDS9930_write_cal_file(char *file_path,unsigned int value);
static int APDS9930_load_calibration_data(struct i2c_client *client);
#endif
/*by qliu4244 add sensors function end*/

static int APDS9930_init_flag = -1; /* 0<==>OK -1 <==> fail*/
static struct alsps_init_info APDS9930_init_info = {
		.name = "APDS9930",
		.init = APDS9930_local_init,
		.uninit = APDS9930_remove,
};


static DEFINE_MUTEX(APDS9930_mutex);


static struct APDS9930_priv *g_APDS9930_ptr;
static unsigned int alsps_irq;

struct PS_CALI_DATA_STRUCT {
	int close;
	int far_away;
	int valid;
};

static struct PS_CALI_DATA_STRUCT ps_cali = { 0, 0, 0 };

static int intr_flag_value;
static unsigned long long int_top_time;

enum {
	CMC_BIT_ALS = 1,
	CMC_BIT_PS = 2,
};


/*----------------------------------------------------------------------------*/
struct APDS9930_i2c_addr {	/*define a series of i2c slave address */
	u8 write_addr;
	u8 ps_thd;		/*PS INT threshold */
};
/*----------------------------------------------------------------------------*/
struct APDS9930_priv {
	struct alsps_hw *hw;
	struct i2c_client *client;
	struct work_struct irq_work;
/*by qliu4244 add sensors function start*/
#ifdef FACTORY_MACRO_PS
        struct delayed_work ps_cal_poll_work;
#endif
#ifdef CONFIG_OF
	struct device_node *irq_node;
#endif
/*by qliu4244 add sensors function end*/
	/*i2c address group */
	struct APDS9930_i2c_addr addr;

	/*misc */
	u16 als_modulus;
	atomic_t i2c_retry;
	atomic_t als_suspend;
	atomic_t als_debounce;	/*debounce time after enabling als */
	atomic_t als_deb_on;	/*indicates if the debounce is on */
	atomic_t als_deb_end;	/*the jiffies representing the end of debounce */
	atomic_t ps_mask;	/*mask ps: always return far away */
	atomic_t ps_debounce;	/*debounce time after enabling ps */
	atomic_t ps_deb_on;	/*indicates if the debounce is on */
	atomic_t ps_deb_end;	/*the jiffies representing the end of debounce */
	atomic_t ps_suspend;


	/*data */
	u16 als;
	u16 ps;
	u8 _align;
	u16 als_level_num;
	u16 als_value_num;
	u32 als_level[C_CUST_ALS_LEVEL - 1];
	u32 als_value[C_CUST_ALS_LEVEL];
	int ps_cali;
#ifdef FACTORY_MACRO_PS
	bool		debug;
	bool		prox_cal_valid;
	u32		prox_threshold_hi;
	u32		prox_threshold_lo;
	u32		prox_thres_hi_max;
	u32		prox_thres_hi_min;
	u32		prox_thres_lo_max;
	u32		prox_thres_lo_min;
	u32		prox_uncover_data;
	u32		prox_offset;
	u32		prox_manual_calibrate_threshold;
#endif
	atomic_t als_cmd_val;	/*the cmd value can't be read, stored in ram */
	atomic_t ps_cmd_val;	/*the cmd value can't be read, stored in ram */
	atomic_t ps_thd_val_high;	/*the cmd value can't be read, stored in ram */
	atomic_t ps_thd_val_low;	/*the cmd value can't be read, stored in ram */
	ulong enable;		/*enable mask */
	ulong pending_intr;	/*pending interrupt */

	/*early suspend */
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_drv;
#endif
};
/*by qliu4244 add sensors function start*/
#ifdef CONFIG_OF
static const struct of_device_id alsps_of_match[] = {
	{.compatible = "mediatek,als_ps"},
	{},
};
#endif
/*by qliu4244 add sensors function end*/
/*----------------------------------------------------------------------------*/
static struct i2c_driver APDS9930_i2c_driver = {
	.probe = APDS9930_i2c_probe,
	.remove = APDS9930_i2c_remove,
	.detect = APDS9930_i2c_detect,
	.suspend = APDS9930_i2c_suspend,
	.resume = APDS9930_i2c_resume,
	.id_table = APDS9930_i2c_id,
	.driver = {
		.name = APDS9930_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = alsps_of_match,
#endif
	},
};

static struct APDS9930_priv *APDS9930_obj;
/*------------------------i2c function for 89-------------------------------------*/
int APDS9930_i2c_master_operate(struct i2c_client *client, char *buf, int count, int i2c_flag)
{
	int res = 0;

	mutex_lock(&APDS9930_mutex);
	switch (i2c_flag) {
	case I2C_FLAG_WRITE:
		/* client->addr &= I2C_MASK_FLAG; */
		res = i2c_master_send(client, buf, count);
		/* client->addr &= I2C_MASK_FLAG; */
		break;

	case I2C_FLAG_READ:
		/*
		   client->addr &= I2C_MASK_FLAG;
		   client->addr |= I2C_WR_FLAG;
		   client->addr |= I2C_RS_FLAG;
		 */
		res = i2c_master_send(client, buf, count & 0xFF);
		/* client->addr &= I2C_MASK_FLAG; */
		res = i2c_master_recv(client, buf, count >> 0x08);
		break;
	default:
		APS_LOG("APDS9930_i2c_master_operate i2c_flag command not support!\n");
		break;
	}
	if (res <= 0)
		goto EXIT_ERR;

	mutex_unlock(&APDS9930_mutex);
	return res;
 EXIT_ERR:
	mutex_unlock(&APDS9930_mutex);
	APS_ERR("APDS9930_i2c_transfer fail\n");
	return res;
}

/*----------------------------------------------------------------------------*/
int APDS9930_get_addr(struct alsps_hw *hw, struct APDS9930_i2c_addr *addr)
{
	if (!hw || !addr)
		return -EFAULT;

	addr->write_addr = hw->i2c_addr[0];
	return 0;
}

/*----------------------------------------------------------------------------*/
static void APDS9930_power(struct alsps_hw *hw, unsigned int on)
{
}

/*----------------------------------------------------------------------------*/
static long APDS9930_enable_als(struct i2c_client *client, int enable)
{
	struct APDS9930_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];
	long res = 0;

	databuf[0] = APDS9930_CMM_ENABLE;
	res = APDS9930_i2c_master_operate(client, databuf, 0x101, I2C_FLAG_READ);
	if (res <= 0)
		goto EXIT_ERR;

	/* APS_LOG("APDS9930_CMM_ENABLE als value = %x\n",databuf[0]); */

	if (enable) {
		databuf[1] = databuf[0] | 0x03;
		databuf[0] = APDS9930_CMM_ENABLE;
		/* APS_LOG("APDS9930_CMM_ENABLE enable als value = %x\n",databuf[1]); */
		res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
		if (res <= 0)
			goto EXIT_ERR;

		atomic_set(&obj->als_deb_on, 1);
		atomic_set(&obj->als_deb_end,
			   jiffies + atomic_read(&obj->als_debounce) / (1000 / HZ));
	} else {
		if (test_bit(CMC_BIT_PS, &obj->enable))
			databuf[1] = databuf[0] & 0xFD;
		else
			databuf[1] = databuf[0] & 0xF8;

		databuf[0] = APDS9930_CMM_ENABLE;
		/* APS_LOG("APDS9930_CMM_ENABLE disable als value = %x\n",databuf[1]); */
		res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
		if (res <= 0)
			goto EXIT_ERR;

	}
	return 0;

 EXIT_ERR:
	APS_ERR("APDS9930_enable_als fail\n");
	return res;
}

/*----------------------------------------------------------------------------*/
static long APDS9930_enable_ps(struct i2c_client *client, int enable)
{
	struct APDS9930_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];
	long res = 0;

	databuf[0] = APDS9930_CMM_ENABLE;
	res = APDS9930_i2c_master_operate(client, databuf, 0x101, I2C_FLAG_READ);
	if (res <= 0)
		goto EXIT_ERR;

	/* APS_LOG("APDS9930_CMM_ENABLE ps value = %x\n",databuf[0]); */

	if (enable) {
		databuf[1] = databuf[0] | 0x05;
		databuf[0] = APDS9930_CMM_ENABLE;
		/* APS_LOG("APDS9930_CMM_ENABLE enable ps value = %x\n",databuf[1]); */
		res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
		if (res <= 0)
			goto EXIT_ERR;

		atomic_set(&obj->ps_deb_on, 1);
		atomic_set(&obj->ps_deb_end,
			   jiffies + atomic_read(&obj->ps_debounce) / (1000 / HZ));
#ifdef FACTORY_MACRO_PS
		if(obj->prox_cal_valid == false)
		{
	            res = APDS9930_load_calibration_data(client);
                    if(res >= 0)
                    {
                        obj->prox_cal_valid = true;
                    }
                }
#endif
	} else {
		if (test_bit(CMC_BIT_ALS, &obj->enable))
			databuf[1] = databuf[0] & 0xFB;
		else
			databuf[1] = databuf[0] & 0xF8;

		databuf[0] = APDS9930_CMM_ENABLE;
		/* APS_LOG("APDS9930_CMM_ENABLE disable ps value = %x\n",databuf[1]); */
		res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
		if (res <= 0)
			goto EXIT_ERR;

		/*fix bug */
		databuf[0] = APDS9930_CMM_INT_LOW_THD_LOW;
		databuf[1] = (u8)(750 & 0x00FF);
		res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
		if (res <= 0)
			goto EXIT_ERR;

		databuf[0] = APDS9930_CMM_INT_LOW_THD_HIGH;
			databuf[1] = (u8)((750 & 0xFF00) >> 8);
		res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
		if (res <= 0)
			goto EXIT_ERR;

		databuf[0] = APDS9930_CMM_INT_HIGH_THD_LOW;
		databuf[1] = (u8)(900 & 0x00FF);
		res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
		if (res <= 0)
			goto EXIT_ERR;

		databuf[0] = APDS9930_CMM_INT_HIGH_THD_HIGH;
		databuf[1] = (u8)((900 & 0xFF00) >> 8);
		res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
		if (res <= 0)
			goto EXIT_ERR;

		/*fix bug */
	}
	return 0;

 EXIT_ERR:
	APS_ERR("APDS9930_enable_ps fail\n");
	return res;
}

#ifdef FACTORY_MACRO_PS
/*----------------------------------------------------------------------------*/
static int APDS9930_read_cal_file(char *file_path)
{
    struct file *file_p;
    int vfs_read_retval = 0;
    mm_segment_t old_fs;
    char read_buf[32];
    unsigned short read_value;

    if (NULL==file_path)
    {
	APS_ERR("file_path is NULL\n");
	goto error;
    }

    memset(read_buf, 0, 32);

    file_p = filp_open(file_path, O_RDONLY , 0);
    if (IS_ERR(file_p))
    {
	APS_ERR("[open file <%s>failed]\n",file_path);
	goto error;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    vfs_read_retval = vfs_read(file_p, (char*)read_buf, 16, &file_p->f_pos);
    if (vfs_read_retval < 0)
    {
	APS_ERR("[read file <%s>failed]\n",file_path);
	goto error;
    }

    set_fs(old_fs);
    filp_close(file_p, NULL);

    if (kstrtou16(read_buf, 10, &read_value) < 0)
    {
	APS_ERR("[kstrtou16 %s failed]\n",read_buf);
	goto error;
    }

    APS_ERR("[the content of %s is %s]\n", file_path, read_buf);

    return read_value;

error:
    return -1;
}

static int APDS9930_write_cal_file(char *file_path,unsigned int value)
{
    struct file *file_p;
    char write_buf[10];
	 mm_segment_t old_fs;
    int vfs_write_retval=0;
    if (NULL==file_path)
    {
	APS_ERR("file_path is NULL\n");

    }
       memset(write_buf, 0, sizeof(write_buf));
      sprintf(write_buf, "%d\n", value);
    file_p = filp_open(file_path, O_CREAT|O_RDWR , 0665);
    if (IS_ERR(file_p))
    {
	APS_ERR("[open file <%s>failed]\n",file_path);
	goto error;
    }
    old_fs = get_fs();
    set_fs(KERNEL_DS);

    vfs_write_retval = vfs_write(file_p, (char*)write_buf, sizeof(write_buf), &file_p->f_pos);
    if (vfs_write_retval < 0)
    {
	APS_ERR("[write file <%s>failed]\n",file_path);
	goto error;
    }

    set_fs(old_fs);
    filp_close(file_p, NULL);

    return 1;

error:
    return -1;
}

static int APDS9930_load_calibration_data(struct i2c_client *client)
{
	struct APDS9930_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];
	int res = 0;

	APS_DBG("%s\n", __FUNCTION__);
	obj->prox_uncover_data = APDS9930_read_cal_file(PATH_PROX_UNCOVER_DATA);
	if (obj->prox_uncover_data < 0)
	{
		obj->prox_uncover_data = 0x00;
		APS_ERR("APDS9930_load_calibration_data read PATH_PROX_UNCOVER_DATA error\n");
	}
	if (obj->prox_uncover_data > 0 && obj->prox_uncover_data < APDS9930_DATA_SAFE_RANGE_MAX)
	{
		obj->prox_thres_hi_min = obj->prox_uncover_data + APDS9930_THRESHOLD_SAFE_DISTANCE;
		obj->prox_thres_hi_min = (obj->prox_thres_hi_min > APDS9930_THRESHOLD_HIGH_MIN) ? obj->prox_thres_hi_min : APDS9930_THRESHOLD_HIGH_MIN;

		obj->prox_thres_hi_max = obj->prox_thres_hi_min + APDS9930_THRESHOLD_DISTANCE / 2;
		obj->prox_thres_hi_max = (obj->prox_thres_hi_max > APDS9930_THRESHOLD_HIGH_MAX) ? APDS9930_THRESHOLD_HIGH_MAX : obj->prox_thres_hi_max;

		obj->prox_thres_lo_min = obj->prox_uncover_data + APDS9930_THRESHOLD_DISTANCE;
		obj->prox_thres_lo_min = (obj->prox_thres_lo_min > APDS9930_THRESHOLD_LOW_MIN) ? obj->prox_thres_lo_min : APDS9930_THRESHOLD_LOW_MIN;

		obj->prox_thres_lo_max = obj->prox_thres_lo_min + APDS9930_THRESHOLD_DISTANCE;
		if (obj->prox_thres_lo_max > (obj->prox_thres_hi_min - 100))
		{
			obj->prox_thres_lo_max = (obj->prox_thres_hi_min - 100);
		}
		APS_LOG("get uncover data success\n");
	}
	else
	{
		obj->prox_thres_hi_min = APDS9930_DEFAULT_THRESHOLD_HIGH;
		obj->prox_thres_hi_max = APDS9930_DEFAULT_THRESHOLD_HIGH;
		obj->prox_thres_lo_min = APDS9930_DEFAULT_THRESHOLD_LOW;
		obj->prox_thres_lo_max = APDS9930_DEFAULT_THRESHOLD_LOW;
		APS_ERR("get uncover data failed for data too high\n");
	}
	APS_ERR("prox_thres_hi range is [%d--%d]\n", obj->prox_thres_hi_min, obj->prox_thres_hi_max);
	APS_ERR("prox_thres_lo range is [%d--%d]\n", obj->prox_thres_lo_min, obj->prox_thres_lo_max);

	res = APDS9930_read_cal_file(CAL_THRESHOLD);
	if (res <= 0)
	{
		APS_ERR("APDS9930_load_calibration_data read CAL_THRESHOLD error\n");
		res = APDS9930_write_cal_file(CAL_THRESHOLD, 0);
		if (res < 0)
		{
			APS_ERR("APDS9930_load_calibration_data create CAL_THRESHOLD error\n");
		}
		obj->prox_threshold_hi = APDS9930_DEFAULT_THRESHOLD_HIGH;
		obj->prox_threshold_lo = APDS9930_DEFAULT_THRESHOLD_LOW;
		goto EXIT_ERR;
	}
	else
	{
		APS_LOG("APDS9930_load_calibration_data CAL_THRESHOLD= %d\n", res);
		obj->prox_manual_calibrate_threshold = res;
		obj->prox_threshold_hi = res;

		obj->prox_threshold_hi = (obj->prox_threshold_hi < obj->prox_thres_hi_max) ? obj->prox_threshold_hi : obj->prox_thres_hi_max;
		obj->prox_threshold_hi = (obj->prox_threshold_hi > obj->prox_thres_hi_min) ? obj->prox_threshold_hi : obj->prox_thres_hi_min;
		obj->prox_threshold_lo = obj->prox_threshold_hi - APDS9930_THRESHOLD_DISTANCE;
		obj->prox_threshold_lo = (obj->prox_threshold_lo < obj->prox_thres_lo_max) ? obj->prox_threshold_lo : obj->prox_thres_lo_max;
		obj->prox_threshold_lo = (obj->prox_threshold_lo > obj->prox_thres_lo_min) ? obj->prox_threshold_lo : obj->prox_thres_lo_min;
	}
	APS_ERR("prox_threshold_lo %d-- prox_threshold_hi %d\n", obj->prox_threshold_lo, obj->prox_threshold_hi);
	databuf[0] = APDS9930_CMM_INT_LOW_THD_LOW;
	databuf[1] = (u8)(obj->prox_threshold_lo & 0x00FF);
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	databuf[0] = APDS9930_CMM_INT_LOW_THD_HIGH;
	databuf[1] = (u8)((obj->prox_threshold_lo & 0xFF00) >> 8);
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	databuf[0] = APDS9930_CMM_INT_HIGH_THD_LOW;
	databuf[1] = (u8)(obj->prox_threshold_hi & 0x00FF);
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	databuf[0] = APDS9930_CMM_INT_HIGH_THD_HIGH;
	databuf[1] = (u8)((obj->prox_threshold_hi & 0xFF00) >> 8);;
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}

	res = APDS9930_read_cal_file(PATH_PROX_OFFSET);
	if(res < 0)
	{
		APS_ERR("APDS9930_load_calibration_data read PATH_PROX_OFFSET error\n");
		goto EXIT_ERR;
	}
	else
	{
		obj->prox_offset = res;
	}

	databuf[0] = 0x9E;
	databuf[1] = obj->prox_offset;
	APS_ERR("APDS9930_load_calibration_data offset = %d\n", databuf[1]);
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	APS_LOG("APDS9930_load_calibration_data done\n");
	return 0;

EXIT_ERR:
	APS_ERR("APDS9930_load_calibration_data fail\n");
	return -1;
}


/******************************************************************************
 * Sysfs attributes
*******************************************************************************/

static ssize_t APDS9930_show_chip_name(struct device_driver *ddri, char *buf)
{
       ssize_t res;

       APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "%s\n", APDS9930_CHIP_NAME);
	return res;
}

static ssize_t APDS9930_show_prox_thres_min(struct device_driver *ddri, char *buf)
{
       ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_THRES_MIN);
	return res;
}

static ssize_t APDS9930_show_prox_thres_max(struct device_driver *ddri, char *buf)
{
       ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_THRES_MAX);
	return res;
}

static ssize_t APDS9930_show_prox_thres(struct device_driver *ddri, char *buf)
{
       ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_obj->prox_threshold_hi);
	return res;
}

static ssize_t APDS9930_store_prox_thres(struct device_driver *ddri,const char *buf, size_t count)
{
	ssize_t res;
	 int value = 0;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = sscanf(buf, "%d", &value);

	if (value == 1)
	{
		APDS9930_load_calibration_data(APDS9930_obj->client);
	}

	return count;
}

static ssize_t APDS9930_show_prox_data_max(struct device_driver *ddri, char *buf)
{
       ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_DATA_MAX);
	return res;
}

static ssize_t APDS9930_show_prox_calibrate_start(struct device_driver *ddri, char *buf)
{
       ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_DATA_MAX);
	return res;
}

static ssize_t APDS9930_store_prox_calibrate_start(struct device_driver *ddri, const char *buf, size_t count)
{
       ssize_t res;
	 int value = 0;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = sscanf(buf, "%d", &value);
	if (value == 1)
	{
		APDS9930_obj->debug = true;
		schedule_delayed_work(&APDS9930_obj->ps_cal_poll_work, msecs_to_jiffies(100));
		APS_LOG("ps_cal_poll_work scheduled\n");
	}
	else
	{
		APDS9930_obj->debug = false;
		cancel_delayed_work(&APDS9930_obj->ps_cal_poll_work);
		APS_LOG("ps_cal_poll_work cancelled\n");
	}
	return count;
}

static ssize_t APDS9930_show_prox_calibrate_verify(struct device_driver *ddri, char *buf)
{
       ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_DATA_MAX);
	return res;
}

static ssize_t APDS9930_store_prox_calibrate_verify(struct device_driver *ddri, const char *buf, size_t count)
{
       //ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	//res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_DATA_MAX);
	return count;
}

static ssize_t APDS9930_show_prox_data_safe_range_min(struct device_driver *ddri, char *buf)
{
       ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_DATA_SAFE_RANGE_MIN);
	return res;
}

static ssize_t APDS9930_show_prox_data_safe_range_max(struct device_driver *ddri, char *buf)
{
       ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_DATA_SAFE_RANGE_MAX);
	return res;
}
static ssize_t APDS9930_show_prox_offset_cal_start(struct device_driver *ddri, char *buf)
{
       ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_DATA_MAX);
	return res;
}

static ssize_t APDS9930_store_prox_offset_cal_start(struct device_driver *ddri, const char *buf, size_t count)
{
       ssize_t res;
	int value = 0;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}
	res = sscanf(buf, "%d", &value);
	if (value == 1)
	{
		APDS9930_obj->debug = true;
		schedule_delayed_work(&APDS9930_obj->ps_cal_poll_work, msecs_to_jiffies(100));
		APS_LOG("ps_cal_poll_work scheduled\n");
	}
	else
	{
		APDS9930_obj->debug = false;
		cancel_delayed_work(&APDS9930_obj->ps_cal_poll_work);
		APS_LOG("ps_cal_poll_work cancelled\n");
	}
	return count;
}
static ssize_t APDS9930_show_prox_offset_cal(struct device_driver *ddri, char *buf)
{
       ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_obj->prox_offset);
	return res;
}

static ssize_t APDS9930_store_prox_offset_cal(struct device_driver *ddri, const char *buf, size_t count)
{
       ssize_t res;
	 int value = 0;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = sscanf(buf, "%d", &value);

	if (value == 1)
	{
		APDS9930_prox_offset_cal_process();
	}

	return count;
}
static ssize_t APDS9930_show_prox_offset_cal_verify(struct device_driver *ddri, char *buf)
{
       ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_DATA_MAX);
	return res;
}

static ssize_t APDS9930_store_prox_offset_cal_verify(struct device_driver *ddri, const char *buf, size_t count)
{
       //ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}

	//res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_DATA_MAX);
	return count;
}
static ssize_t APDS9930_show_prox_manual_calibrate_threshold(struct device_driver *ddri, char *buf)
{
       ssize_t res;

	APS_DBG("%s\n", __FUNCTION__);

	if(!APDS9930_obj)
	{
		APS_ERR("APDS9930_obj is null!!\n");
		return 0;
	}
	APDS9930_load_calibration_data(APDS9930_obj->client);

	res = snprintf(buf, PAGE_SIZE, "%d\n", APDS9930_obj->prox_manual_calibrate_threshold);
	return res;
}

static DRIVER_ATTR(chip_name,                           S_IRUGO, APDS9930_show_chip_name, NULL);
static DRIVER_ATTR(prox_thres_min,                      S_IRUGO, APDS9930_show_prox_thres_min, NULL);
static DRIVER_ATTR(prox_manual_calibrate_threshold,     S_IRUGO, APDS9930_show_prox_manual_calibrate_threshold, NULL);
static DRIVER_ATTR(prox_thres_max,                      S_IRUGO, APDS9930_show_prox_thres_max, NULL);
static DRIVER_ATTR(prox_data_max,                       S_IRUGO, APDS9930_show_prox_data_max, NULL);
static DRIVER_ATTR(prox_data_safe_range_min,            S_IRUGO, APDS9930_show_prox_data_safe_range_min, NULL);
static DRIVER_ATTR(prox_data_safe_range_max,            S_IRUGO, APDS9930_show_prox_data_safe_range_max, NULL);

static DRIVER_ATTR(prox_thres,                          S_IWUSR | S_IRUGO, APDS9930_show_prox_thres, APDS9930_store_prox_thres);
static DRIVER_ATTR(prox_calibrate_start,                S_IWUSR | S_IRUGO, APDS9930_show_prox_calibrate_start, APDS9930_store_prox_calibrate_start);
static DRIVER_ATTR(prox_calibrate_verify,               S_IWUSR | S_IRUGO, APDS9930_show_prox_calibrate_verify, APDS9930_store_prox_calibrate_verify);
static DRIVER_ATTR(prox_offset_cal_start,               S_IWUSR | S_IRUGO, APDS9930_show_prox_offset_cal_start, APDS9930_store_prox_offset_cal_start);
static DRIVER_ATTR(prox_offset_cal,                     S_IWUSR | S_IRUGO, APDS9930_show_prox_offset_cal, APDS9930_store_prox_offset_cal);
static DRIVER_ATTR(prox_offset_cal_verify,              S_IWUSR | S_IRUGO, APDS9930_show_prox_offset_cal_verify, APDS9930_store_prox_offset_cal_verify);









/*----------------------------------------------------------------------------*/
static struct driver_attribute *APDS9930_attr_list[] = {
	&driver_attr_chip_name,
	&driver_attr_prox_thres_min,
	&driver_attr_prox_thres_max,
	&driver_attr_prox_thres,
	&driver_attr_prox_data_max,
	&driver_attr_prox_calibrate_start,
	&driver_attr_prox_calibrate_verify,
	&driver_attr_prox_data_safe_range_min,
	&driver_attr_prox_data_safe_range_max,
	&driver_attr_prox_offset_cal_start,
	&driver_attr_prox_offset_cal,
	&driver_attr_prox_offset_cal_verify,
	&driver_attr_prox_manual_calibrate_threshold,
};

/*----------------------------------------------------------------------------*/
static int APDS9930_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(APDS9930_attr_list)/sizeof(APDS9930_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, APDS9930_attr_list[idx])))
		{
			APS_ERR("driver_create_file (%s) = %d\n", APDS9930_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}
/*----------------------------------------------------------------------------*/
	static int APDS9930_delete_attr(struct device_driver *driver)
	{
	int idx ,err = 0;
	int num = (int)(sizeof(APDS9930_attr_list)/sizeof(APDS9930_attr_list[0]));

	if (!driver)
	return -EINVAL;

	for (idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, APDS9930_attr_list[idx]);
	}

	return err;
}
/*----------------------------------------------------------------------------*/
#endif  /*FACTORY_MACRO_PS*/


/*for interrupt work mode support*/
static int APDS9930_check_and_clear_intr(struct i2c_client *client)
{
	int res, intp, intl;
	u8 buffer[2];

	if (mt_get_gpio_in(GPIO_ALS_EINT_PIN) == 1)	/*skip if no interrupt */
		return 0;

	buffer[0] = APDS9930_CMM_STATUS;
	res = APDS9930_i2c_master_operate(client, buffer, 0x101, I2C_FLAG_READ);
	if (res <= 0)
		goto EXIT_ERR;


	res = 0;
	intp = 0;
	intl = 0;
	if (0 != (buffer[0] & 0x20)) {
		res = 1;
		intp = 1;
	}
	if (0 != (buffer[0] & 0x10)) {
		res = 1;
		intl = 1;
	}

	if (1 == res) {
		if ((1 == intp) && (0 == intl))
			buffer[0] = (TAOS_TRITON_CMD_REG | TAOS_TRITON_CMD_SPL_FN | 0x05);
		else if ((0 == intp) && (1 == intl))
			buffer[0] = (TAOS_TRITON_CMD_REG | TAOS_TRITON_CMD_SPL_FN | 0x06);
		else
			buffer[0] = (TAOS_TRITON_CMD_REG | TAOS_TRITON_CMD_SPL_FN | 0x07);


		res = APDS9930_i2c_master_operate(client, buffer, 0x1, I2C_FLAG_WRITE);
		if (res <= 0)
			goto EXIT_ERR;
		else
			res = 0;

	}

	return res;

 EXIT_ERR:
	APS_ERR("APDS9930_check_and_clear_intr fail\n");
	return 1;
}

/*----------------------------------------------------------------------------*/

/*yucong add for interrupt mode support MTK inc 2012.3.7*/
static int APDS9930_check_intr(struct i2c_client *client)
{
	int res, intp, intl;
	u8 buffer[2];

	if (mt_get_gpio_in(GPIO_ALS_EINT_PIN) == 1)	/*skip if no interrupt */
		return 0;

	buffer[0] = APDS9930_CMM_STATUS;
	res = APDS9930_i2c_master_operate(client, buffer, 0x101, I2C_FLAG_READ);
	if (res <= 0)
		goto EXIT_ERR;

	res = 0;
	intp = 0;
	intl = 0;
	if (0 != (buffer[0] & 0x20)) {
		res = 0;
		intp = 1;
	}
	if (0 != (buffer[0] & 0x10)) {
		res = 0;
		intl = 1;
	}

	return res;

 EXIT_ERR:
	APS_ERR("APDS9930_check_intr fail\n");
	return 1;
}

static int APDS9930_clear_intr(struct i2c_client *client)
{
	int res;
	u8 buffer[2];

	buffer[0] = (TAOS_TRITON_CMD_REG | TAOS_TRITON_CMD_SPL_FN | 0x07);
	res = APDS9930_i2c_master_operate(client, buffer, 0x1, I2C_FLAG_WRITE);
	if (res <= 0)
		goto EXIT_ERR;
	else
		res = 0;

	return res;

 EXIT_ERR:
	APS_ERR("APDS9930_check_and_clear_intr fail\n");
	return 1;
}



static irqreturn_t alsps_interrupt_handler(int irq, void *dev_id)
{
	struct APDS9930_priv *obj = g_APDS9930_ptr;

	if (!obj)
		return IRQ_HANDLED;

	disable_irq_nosync(alsps_irq);
	int_top_time = sched_clock();
	schedule_work(&obj->irq_work);
	return IRQ_HANDLED;
}

/*by qliu4244 add sensors function start*/
int APDS9930_irq_registration(struct i2c_client *client)
{
	int ints[2] = {0};
	int err = 0;

	struct APDS9930_priv *obj = i2c_get_clientdata(client);
	/*struct task_struct *thread = NULL;*/
#if IRQ_REQUEST_OTHER_GPIO
	int ret = -1;

	gpio_direction_input(alsps_int_gpio_number);

	ret = request_irq(alsps_irq, alsps_interrupt_handler, IRQF_TRIGGER_FALLING, "als_ps", NULL);

	if (ret > 0) {
		APS_ERR("alsps request_irq IRQ LINE NOT AVAILABLE!.");
		return ret;
	}

 /*   disable_irq_nosync(alsps_irq);*/
 /*   enable_irq(alsps_irq);*/
	return ret;
#else
	g_APDS9930_ptr = obj;
#ifdef CONFIG_OF

	if(obj == NULL){
		APS_ERR("apds9930_obj is null!\n");
		return -EINVAL;
	}
	obj->irq_node = of_find_compatible_node(NULL, NULL, "mediatek, ALS-eint");
#endif

	APS_LOG("apds9930_setup_eint, GPIO_ALS_EINT_PIN = 0x%x.\n",GPIO_ALS_EINT_PIN);//add for test

	/*configure to GPIO function, external interrupt*/

	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, 1);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

#ifdef CONFIG_OF
	if(obj->irq_node != NULL){
		of_property_read_u32_array(obj->irq_node, "debounce", ints, ARRAY_SIZE(ints));
		APS_LOG("ins[0] = %d, ints[1] = %d\n", ints[0], ints[1]);
		mt_gpio_set_debounce(ints[0], ints[1]);

		alsps_irq = irq_of_parse_and_map(obj->irq_node, 0);
		if(alsps_irq != 0){
			err = request_irq(alsps_irq, alsps_interrupt_handler, IRQF_TRIGGER_NONE, "ALS-eint", NULL);
			if(err < 0){
				APS_ERR("request_irq failed!\n");
				return -EFAULT;
			}else{
				enable_irq(alsps_irq);
			}
		}else{
			APS_ERR("irq_of_parse_and_map failed!\n");
			return -EFAULT;
		}
	}else{
		APS_ERR("apds9930_obj->irq_node is null!\n");
		return -EFAULT;
	}

#elif defined(CUST_EINT_ALS_TYPE)

	mt_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_TYPE, APDS9930_eint_func, 0);

	mt_eint_unmask(CUST_EINT_ALS_NUM);
#endif
	return 0;
#endif

}
/*by qliu4244 add sensors function end*/

/*----------------------------------------------------------------------------*/

static int APDS9930_init_client(struct i2c_client *client)
{
	struct APDS9930_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];
	int res = 0;

	databuf[0] = (TAOS_TRITON_CMD_REG | TAOS_TRITON_CMD_SPL_FN | 0x00);
	res = APDS9930_i2c_master_operate(client, databuf, 0x1, I2C_FLAG_WRITE);
	if (res <= 0)
		goto EXIT_ERR;

	databuf[0] = APDS9930_CMM_ENABLE;
	if (obj->hw->polling_mode_ps == 1)
		databuf[1] = 0x08;
	if (obj->hw->polling_mode_ps == 0)
		databuf[1] = 0x28;

	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res <= 0)
		goto EXIT_ERR;

	databuf[0] = APDS9930_CMM_ATIME;
	databuf[1] = 0xF6;
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res <= 0)
		goto EXIT_ERR;

	databuf[0] = APDS9930_CMM_PTIME;
	databuf[1] = 0xFF;
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res <= 0)
		goto EXIT_ERR;

	databuf[0] = APDS9930_CMM_WTIME;
	databuf[1] = 0xFC;
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res <= 0)
		goto EXIT_ERR;

	/*for interrupt work mode support*/
	if (0 == obj->hw->polling_mode_ps) {
		if (1 == ps_cali.valid) {
			databuf[0] = APDS9930_CMM_INT_LOW_THD_LOW;
			databuf[1] = (u8) (ps_cali.far_away & 0x00FF);
			res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;

			databuf[0] = APDS9930_CMM_INT_LOW_THD_HIGH;
			databuf[1] = (u8) ((ps_cali.far_away & 0xFF00) >> 8);
			res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;

			databuf[0] = APDS9930_CMM_INT_HIGH_THD_LOW;
			databuf[1] = (u8) (ps_cali.close & 0x00FF);
			res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;

			databuf[0] = APDS9930_CMM_INT_HIGH_THD_HIGH;
			databuf[1] = (u8) ((ps_cali.close & 0xFF00) >> 8);
			res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;

		} else {
			databuf[0] = APDS9930_CMM_INT_LOW_THD_LOW;
			databuf[1] = (u8)(750 & 0x00FF);
			res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;

			databuf[0] = APDS9930_CMM_INT_LOW_THD_HIGH;
			databuf[1] = (u8)((750 & 0xFF00) >> 8);
			res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;

			databuf[0] = APDS9930_CMM_INT_HIGH_THD_LOW;
			databuf[1] = (u8)(900 & 0x00FF);
			res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;

			databuf[0] = APDS9930_CMM_INT_HIGH_THD_HIGH;
			databuf[1] = (u8)((900 & 0xFF00) >> 8);
			res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;


		}

		databuf[0] = APDS9930_CMM_Persistence;
		databuf[1] = 0x20;
		res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
		if (res <= 0)
			goto EXIT_ERR;


	}

	databuf[0] = APDS9930_CMM_CONFIG;
	databuf[1] = 0x00;
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res <= 0)
		goto EXIT_ERR;


	/*modified pulse 2  to 4 */
	databuf[0] = APDS9930_CMM_PPCOUNT;
	databuf[1] = APDS9930_CMM_PPCOUNT_VALUE;
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res <= 0)
		goto EXIT_ERR;


	/*modified gain 16  to 1 */
	databuf[0] = APDS9930_CMM_CONTROL;
	databuf[1] = APDS9930_CMM_CONTROL_VALUE;
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res <= 0)
		goto EXIT_ERR;

	res = APDS9930_irq_registration(client);
	if (res != 0) {
		APS_ERR("registration failed: %d\n", res);
		return res;
	}

	res = APDS9930_check_and_clear_intr(client);
	if (res) {
		APS_ERR("check/clear intr: %d\n", res);
		return res;
	}

	return APDS9930_SUCCESS;

 EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return res;
}

/******************************************************************************
 * Function Configuration
******************************************************************************/
int APDS9930_read_als(struct i2c_client *client, u16 *data)
{
	struct APDS9930_priv *obj = i2c_get_clientdata(client);
	u16 c0_value, c1_value;
	u32 c0_nf, c1_nf;
	u8 buffer[2];
	u16 atio;
	int res = 0;

	if (client == NULL) {
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

	buffer[0] = APDS9930_CMM_C0DATA_L;
	res = APDS9930_i2c_master_operate(client, buffer, 0x201, I2C_FLAG_READ);
	if (res <= 0)
		goto EXIT_ERR;


	c0_value = buffer[0] | (buffer[1] << 8);
	c0_nf = obj->als_modulus * c0_value;
	APS_LOG("c0_value=%d, c0_nf=%d, als_modulus=%d\n", c0_value, c0_nf, obj->als_modulus);

	buffer[0] = APDS9930_CMM_C1DATA_L;
	res = APDS9930_i2c_master_operate(client, buffer, 0x201, I2C_FLAG_READ);
	if (res <= 0)
		goto EXIT_ERR;


	c1_value = buffer[0] | (buffer[1] << 8);
	c1_nf = obj->als_modulus * c1_value;
	APS_LOG("c1_value=%d, c1_nf=%d, als_modulus=%d\n", c1_value, c1_nf, obj->als_modulus);

	if ((c0_value > c1_value) && (c0_value < 50000)) {
		atio = (c1_nf * 100) / c0_nf;

		APS_LOG("atio = %d\n", atio);
		if (atio < 30) {
			*data = (13 * c0_nf - 24 * c1_nf) / 10000;
		} else if (atio >= 30 && atio < 38) {
			*data = (16 * c0_nf - 35 * c1_nf) / 10000;
		} else if (atio >= 38 && atio < 45) {
			*data = (9 * c0_nf - 17 * c1_nf) / 10000;
		} else if (atio >= 45 && atio < 54) {
			*data = (6 * c0_nf - 10 * c1_nf) / 10000;
		} else
			*data = 0;

	} else if (c0_value > 50000) {
		*data = 65535;
	} else if (c0_value == 0) {
		*data = 0;
	} else {
		APS_DBG("APDS9930_read_als als_value is invalid!!\n");
		return -1;
	}

	APS_LOG("APDS9930_read_als als_value_lux = %d\n", *data);
	return 0;



 EXIT_ERR:
	APS_ERR("APDS9930_read_ps fail\n");
	return res;
}

int APDS9930_read_als_ch0(struct i2c_client *client, u16 *data)
{
	/* struct APDS9930_priv *obj = i2c_get_clientdata(client); */
	u16 c0_value;
	u8 buffer[2];
	int res = 0;

	if (client == NULL) {
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}
/* get adc channel 0 value */
	buffer[0] = APDS9930_CMM_C0DATA_L;
	res = APDS9930_i2c_master_operate(client, buffer, 0x201, I2C_FLAG_READ);
	if (res <= 0)
		goto EXIT_ERR;

	c0_value = buffer[0] | (buffer[1] << 8);
	*data = c0_value;
	/* APS_LOG("c0_value=%d\n", c0_value); */
	return 0;



 EXIT_ERR:
	APS_ERR("APDS9930_read_ps fail\n");
	return res;
}

/*----------------------------------------------------------------------------*/

static int APDS9930_get_als_value(struct APDS9930_priv *obj, u16 als)
{
	int idx;
	int invalid = 0;

	for (idx = 0; idx < obj->als_level_num; idx++) {
		if (als < obj->hw->als_level[idx])
			break;
	}

	if (idx >= obj->als_value_num) {
		APS_ERR("APDS9930_get_als_value exceed range\n");
		idx = obj->als_value_num - 1;
	}

	if (1 == atomic_read(&obj->als_deb_on)) {
		unsigned long endt = atomic_read(&obj->als_deb_end);

		if (time_after(jiffies, endt))
			atomic_set(&obj->als_deb_on, 0);


		if (1 == atomic_read(&obj->als_deb_on))
			invalid = 1;

	}

	if (!invalid) {
#if defined(CONFIG_NUBIA_APDS9930_NEW_ALS_CONT_VALUE)
		int level_high = obj->hw->als_level[idx];
		int level_low = (idx > 0) ? obj->hw->als_level[idx - 1] : 0;
		int level_diff = level_high - level_low;
		int value_high = obj->hw->als_value[idx];
		int value_low = (idx > 0) ? obj->hw->als_value[idx - 1] : 0;
		int value_diff = value_high - value_low;
		int value = 0;

		if ((level_low >= level_high) || (value_low >= value_high))
			value = value_low;
		else
			value =
				(level_diff * value_low + (als - level_low) * value_diff +
				 ((level_diff + 1) >> 1)) / level_diff;

		APS_ERR("Malik ALS: %d [%d, %d] => %d [%d, %d]\n", als, level_low, level_high, value,
			value_low, value_high);
		return (value > 2) ? value : 0;
#endif
		/*APS_ERR("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);*/
		return obj->hw->als_value[idx];
	}
	/*APS_ERR("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]); */
	return -1;
}

/*----------------------------------------------------------------------------*/
long APDS9930_read_ps(struct i2c_client *client, u16 *data)
{
	struct APDS9930_priv *obj = i2c_get_clientdata(client);
	u8 buffer[2];
	u16 temp_data;
	long res = 0;

	if (client == NULL) {
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

	buffer[0] = APDS9930_CMM_PDATA_L;
	res = APDS9930_i2c_master_operate(client, buffer, 0x201, I2C_FLAG_READ);
	if (res <= 0)
		goto EXIT_ERR;


	temp_data = buffer[0] | (buffer[1] << 8);
	/* APS_LOG("yucong APDS9930_read_ps ps_data=%d, low:%d  high:%d", *data, buffer[0], buffer[1]); */
	if (temp_data < obj->ps_cali)
		*data = 0;
	else
		*data = temp_data - obj->ps_cali;
	return 0;
	return 0;

 EXIT_ERR:
	APS_ERR("APDS9930_read_ps fail\n");
	return res;
}

/*----------------------------------------------------------------------------*/
static int APDS9930_get_ps_value(struct APDS9930_priv *obj, u16 ps)
{
	int val;		/* mask = atomic_read(&obj->ps_mask); */
	int invalid = 0;
	static int val_temp = 1;

	if (ps_cali.valid == 1) {
		if (ps > ps_cali.close) {
			val = 0;	/*close */
			val_temp = 0;
			intr_flag_value = 1;
		}

		else if (ps < ps_cali.far_away) {
			val = 1;	/*far away */
			val_temp = 1;
			intr_flag_value = 0;
		} else
			val = val_temp;

		APS_LOG("APDS9930_get_ps_value val  = %d", val);
	} else {
		if (ps > atomic_read(&obj->ps_thd_val_high)) {
			val = 0;	/*close */
			val_temp = 0;
			intr_flag_value = 1;
		} else if (ps < atomic_read(&obj->ps_thd_val_low)) {
			val = 1;	/*far away */
			val_temp = 1;
			intr_flag_value = 0;
		} else
			val = val_temp;

	}

	if (atomic_read(&obj->ps_suspend)) {
		invalid = 1;
	} else if (1 == atomic_read(&obj->ps_deb_on)) {
		unsigned long endt = atomic_read(&obj->ps_deb_end);

		if (time_after(jiffies, endt))
			atomic_set(&obj->ps_deb_on, 0);


		if (1 == atomic_read(&obj->ps_deb_on))
			invalid = 1;

	} else if (obj->als > 45000) {
		/* invalid = 1; */
		APS_DBG("ligh too high will result to failt proximiy\n");
		return 1;	/*far away */
	}

	if (!invalid) {
		/* APS_DBG("PS:  %05d => %05d\n", ps, val); */
#ifdef FACTORY_MACRO_PS
		if (obj->debug == true)
		{
			return (int)ps;
		}
		else
		{
			return val;
		}
#else
		return val;
#endif
	} else {
		return -1;
	}
}

#ifdef FACTORY_MACRO_PS
/*----------------------------------------------------------------------------*/
static void APDS9930_cal_poll_work(struct work_struct *work)
{
	struct APDS9930_priv *obj = APDS9930_obj;
	int err;
	struct hwm_sensor_data sensor_data;
	//u8 databuf[3];
		//get raw data
		APDS9930_read_ps(obj->client, &obj->ps);
		APDS9930_read_als_ch0(obj->client, &obj->als);
		APS_LOG("APDS9930_cal_poll_work rawdata ps=%d als_ch0=%d!\n",obj->ps,obj->als);

		if(obj->als > 40000)
			{
			APS_LOG("APDS9930_cal_poll_work ALS too large may under lighting als_ch0=%d!\n",obj->als);
			return;
			}
		sensor_data.values[0] = APDS9930_get_ps_value(obj, obj->ps);
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
#ifdef ADPS9933_NEW_ARCH
		err = ps_report_interrupt_data(sensor_data.values[0]);
		if (err)
			APS_ERR("call ps_report_interrupt_data fail = %d\n", err);
#else
		err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data);
		if (err)
			APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", err);
#endif
	schedule_delayed_work(&APDS9930_obj->ps_cal_poll_work, msecs_to_jiffies(100));
}

static int APDS9930_prox_offset_calculate(int data, int target)
{
    int offset;

    if (data > target)
    {
	offset = (data - target) * 10 / 60; //offset = (data - APDS9930_DATA_TARGET) * 10 / taos_datap->prox_offset_cal_per_bit;
	if (offset > 0x7F)
	{
		offset = 0x7F;
	}
    }
    else
    {
	offset = (target - data) * 16 / 60 + 128; //offset = (APDS9930_DATA_TARGET - data) * 16 / taos_datap->prox_offset_cal_per_bit + 128;
    }

    APS_LOG("APDS9930_prox_offset_calculate offset=%d!\n", offset);

    return offset;
}


static int APDS9930_prox_offset_cal_process(void)
{
	struct APDS9930_priv *obj = APDS9930_obj;
	int i;
	unsigned int ps_mean = 0, ps_sum = 0, ps_num = 0;
	int err, res;
	unsigned char databuf[2], offset = 0;

	cancel_delayed_work(&obj->ps_cal_poll_work);

	databuf[0] = 0x9E;
	databuf[1] = 0x00;
	APS_LOG("APDS9930_prox_offset_cal_process offset set to 0!\n");
	res = APDS9930_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res <= 0)
	{
		goto prox_offset_cal_process_error;
	}

	for(i = 0; i < 10; i++)
	{
		mdelay(30);
		err = (int)APDS9930_read_ps(obj->client, &obj->ps);
		if(err)
		{
			APS_LOG("APDS9930_prox_offset_cal_process read rawdata num:%d failed!\n", i);
			continue;
		}
		else
		{
			ps_sum += obj->ps;
			ps_num++;
			APS_LOG("APDS9930_prox_offset_cal_process rawdata num:%d ps=%d!\n", ps_num, obj->ps);
		}
	}
	if(ps_num != 10)
	{
		APS_ERR("APDS9930_prox_offset_cal_process read rawdata failed!\n");
		goto prox_offset_cal_process_error;
	}
	ps_mean = ps_sum / ps_num;
	APS_LOG("APDS9930_prox_offset_cal_process ps_mean=%d ps_sum=%d ps_num=%d\n", ps_mean, ps_sum, ps_num);

	offset = APDS9930_prox_offset_calculate(ps_mean, APDS9930_DATA_TARGET);
	APS_LOG("APDS9930_prox_offset_cal_process offset=%d!\n", offset);

	databuf[0] = 0x9E;
	databuf[1] = offset;
	res = APDS9930_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res <= 0)
	{
		goto prox_offset_cal_process_error;
	}

	res = APDS9930_write_cal_file(PATH_PROX_OFFSET, offset);
	if(res < 0)
	{
		APS_ERR("APDS9930_prox_offset_cal_process write PATH_PROX_OFFSET error!\n");
		goto prox_offset_cal_process_error;
	}

	APS_LOG("APDS9930_prox_offset_cal_process write PATH_PROX_OFFSET succeeded!\n");

	for(i = 0, ps_sum =0, ps_num = 0; i < 5; i++)
	{
		mdelay(30);
		err = (int)APDS9930_read_ps(obj->client, &obj->ps);
		if(err)
		{
			APS_LOG("APDS9930_prox_offset_cal_process after cal read rawdata num:%d failed!\n", i);
			continue;
		}
		else
		{
			ps_sum += obj->ps;
			ps_num++;
			APS_LOG("APDS9930_prox_offset_cal_process after cal rawdata num:%d ps=%d!\n", ps_num, obj->ps);
		}
	}
	if(ps_num != 5)
	{
		APS_ERR("APDS9930_prox_offset_cal_process after cal read rawdata failed!\n");
		goto prox_offset_cal_process_error;
	}
	ps_mean = ps_sum / ps_num;

	res = APDS9930_write_cal_file(PATH_PROX_UNCOVER_DATA, ps_mean);
	if(res < 0)
	{
		APS_ERR("APDS9930_prox_offset_cal_process write PATH_PROX_UNCOVER_DATA error!\n");
		goto prox_offset_cal_process_error;
	}
	APS_LOG("APDS9930_prox_offset_cal_process write PATH_PROX_UNCOVER_DATA succeeded!\n");

	APS_LOG("APDS9930_prox_offset_cal_process succeeded!\n");
	schedule_delayed_work(&obj->ps_cal_poll_work, msecs_to_jiffies(100));
	return 1;


prox_offset_cal_process_error:
	APS_ERR("APDS9930_prox_offset_cal_process failed!\n");
	schedule_delayed_work(&obj->ps_cal_poll_work, msecs_to_jiffies(100));
	return -1;

}
#endif /*FACTORY_MACRO_PS*/

/*----------------------------------------------------------------------------*/
/*for interrupt work mode support*/
/* #define DEBUG_APDS9930 */
static void APDS9930_irq_work(struct work_struct *work)
{
	struct APDS9930_priv *obj =
	    (struct APDS9930_priv *)container_of(work, struct APDS9930_priv, irq_work);
	int err;
	struct hwm_sensor_data sensor_data;
	u8 databuf[3];
	int res = 0;

	err = APDS9930_check_intr(obj->client);
	if (err) {
		APS_ERR("APDS9930_irq_work check intrs: %d\n", err);
	} else {
		/* get raw data */
		APDS9930_read_ps(obj->client, &obj->ps);
		APDS9930_read_als_ch0(obj->client, &obj->als);
		APS_LOG("APDS9930_irq_work rawdata ps=%d als_ch0=%d!\n", obj->ps, obj->als);
		APS_LOG("APDS9930 int top half time = %lld\n", int_top_time);

		if (obj->als > 40000) {
			APS_LOG("APDS9930_irq_work ALS too large may under lighting als_ch0=%d!\n",
				obj->als);
			return;
			}
		sensor_data.values[0] = APDS9930_get_ps_value(obj, obj->ps);
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;

#ifdef DEBUG_APDS9930
		databuf[0] = APDS9930_CMM_ENABLE;
		res = APDS9930_i2c_master_operate(obj->client, databuf, 0x101, I2C_FLAG_READ);
		if (res <= 0)
			goto EXIT_ERR;

		APS_LOG("APDS9930_irq_work APDS9930_CMM_ENABLE ps value = %x\n", databuf[0]);

		databuf[0] = APDS9930_CMM_INT_LOW_THD_LOW;
		res = APDS9930_i2c_master_operate(obj->client, databuf, 0x201, I2C_FLAG_READ);
		if (res <= 0)
			goto EXIT_ERR;

		APS_LOG
		    ("APDS9930_irq_work APDS9930_CMM_INT_LOW_THD_LOW before databuf[0]=%d databuf[1]=%d!\n",
		     databuf[0], databuf[1]);

		databuf[0] = APDS9930_CMM_INT_HIGH_THD_LOW;
		res = APDS9930_i2c_master_operate(obj->client, databuf, 0x201, I2C_FLAG_READ);
		if (res <= 0)
			goto EXIT_ERR;

		APS_LOG
		    ("APDS9930_irq_work APDS9930_CMM_INT_HIGH_THD_LOW before databuf[0]=%d databuf[1]=%d!\n",
		     databuf[0], databuf[1]);
#endif
/*signal interrupt function add*/
		if (intr_flag_value) {
			databuf[0] = APDS9930_CMM_INT_LOW_THD_LOW;
			databuf[1] = (u8) ((atomic_read(&obj->ps_thd_val_low)) & 0x00FF);
			res =
			    APDS9930_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;


			databuf[0] = APDS9930_CMM_INT_LOW_THD_HIGH;
			databuf[1] = (u8) (((atomic_read(&obj->ps_thd_val_low)) & 0xFF00) >> 8);
			res =
			    APDS9930_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;

			databuf[0] = APDS9930_CMM_INT_HIGH_THD_LOW;
			databuf[1] = (u8) (0x00FF);
			res =
			    APDS9930_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;


			databuf[0] = APDS9930_CMM_INT_HIGH_THD_HIGH;
			databuf[1] = (u8) ((0xFF00) >> 8);
			res =
			    APDS9930_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;


		} else {
			databuf[0] = APDS9930_CMM_INT_LOW_THD_LOW;
			databuf[1] = (u8) (0 & 0x00FF);
			res =
			    APDS9930_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;


			databuf[0] = APDS9930_CMM_INT_LOW_THD_HIGH;
			databuf[1] = (u8) ((0 & 0xFF00) >> 8);
			res =
			    APDS9930_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;


			databuf[0] = APDS9930_CMM_INT_HIGH_THD_LOW;
			databuf[1] = (u8) ((atomic_read(&obj->ps_thd_val_high)) & 0x00FF);
			res =
			    APDS9930_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
			if (res <= 0)
				goto EXIT_ERR;


			databuf[0] = APDS9930_CMM_INT_HIGH_THD_HIGH;
			databuf[1] = (u8) (((atomic_read(&obj->ps_thd_val_high)) & 0xFF00) >> 8);
			res =
			    APDS9930_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
			res = i2c_master_send(obj->client, databuf, 0x2);
			if (res <= 0)
				goto EXIT_ERR;

		}

		/* let up layer to know */
#ifdef DEBUG_APDS9930
		databuf[0] = APDS9930_CMM_INT_LOW_THD_LOW;
		res = APDS9930_i2c_master_operate(obj->client, databuf, 0x201, I2C_FLAG_READ);
		if (res <= 0)
			goto EXIT_ERR;

		APS_LOG
		    ("APDS9930_irq_work APDS9930_CMM_INT_LOW_THD_LOW after databuf[0]=%d databuf[1]=%d!\n",
		     databuf[0], databuf[1]);

		databuf[0] = APDS9930_CMM_INT_HIGH_THD_LOW;
		res = APDS9930_i2c_master_operate(obj->client, databuf, 0x201, I2C_FLAG_READ);
		if (res <= 0)
			goto EXIT_ERR;

		APS_LOG
		    ("APDS9930_irq_work APDS9930_CMM_INT_HIGH_THD_LOW after databuf[0]=%d databuf[1]=%d!\n",
		     databuf[0], databuf[1]);
#endif

#ifdef ADPS9933_NEW_ARCH
		res = ps_report_interrupt_data(sensor_data.values[0]);
		if (res)
			APS_ERR("call ps_report_interrupt_data fail = %d\n", res);
#else
		err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data);
		if (err)
			APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", err);
#endif

	}

	APDS9930_clear_intr(obj->client);
 /*   disable_irq_nosync(alsps_irq);*/
	enable_irq(alsps_irq);
	return;
 EXIT_ERR:
	APDS9930_clear_intr(obj->client);
 /*   disable_irq_nosync(alsps_irq);*/
	enable_irq(alsps_irq);
	APS_ERR("i2c_transfer error = %d\n", res);
}


/******************************************************************************
 * Function Configuration
******************************************************************************/
static int APDS9930_open(struct inode *inode, struct file *file)
{
	file->private_data = APDS9930_i2c_client;

	if (!file->private_data) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int APDS9930_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/
static int set_psensor_threshold(struct i2c_client *client)
{
	struct APDS9930_priv *obj = i2c_get_clientdata(client);
	u8 databuf[3];
	int res = 0;

	APS_ERR("set_psensor_threshold function high: 0x%x, low:0x%x\n",
		atomic_read(&obj->ps_thd_val_high), atomic_read(&obj->ps_thd_val_low));

	databuf[0] = APDS9930_CMM_INT_LOW_THD_LOW;
	databuf[1] = (u8) (atomic_read(&obj->ps_thd_val_low) & 0x00FF);
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res <= 0)
		return -1;

	databuf[0] = APDS9930_CMM_INT_LOW_THD_HIGH;
	databuf[1] = (u8) ((atomic_read(&obj->ps_thd_val_low) & 0xFF00) >> 8);
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res <= 0)
		return -1;

	databuf[0] = APDS9930_CMM_INT_HIGH_THD_LOW;
	databuf[1] = (u8) (atomic_read(&obj->ps_thd_val_high) & 0x00FF);
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res <= 0)
		return -1;

	databuf[0] = APDS9930_CMM_INT_HIGH_THD_HIGH;
	databuf[1] = (u8) ((atomic_read(&obj->ps_thd_val_high) & 0xFF00) >> 8);
	res = APDS9930_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res <= 0)
		return -1;


	return 0;
}

/*----------------------------------------------------------------------------*/
static long APDS9930_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct APDS9930_priv *obj = i2c_get_clientdata(client);
	long err = 0;
	void __user *ptr = (void __user *)arg;
	int dat;
	uint32_t enable;
	int ps_result;
	int ps_cali;
	int threshold[2];

	switch (cmd) {
	case ALSPS_SET_PS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		if (enable) {
			err = APDS9930_enable_ps(obj->client, 1);
			if (err) {
				APS_ERR("enable ps fail: %ld\n", err);
				goto err_out;
			}

			set_bit(CMC_BIT_PS, &obj->enable);
		} else {
			err = APDS9930_enable_ps(obj->client, 0);
			if (err) {
				APS_ERR("disable ps fail: %ld\n", err);
				goto err_out;
			}

			clear_bit(CMC_BIT_PS, &obj->enable);
		}
		break;

	case ALSPS_GET_PS_MODE:
		enable = test_bit(CMC_BIT_PS, &obj->enable) ? (1) : (0);
		if (copy_to_user(ptr, &enable, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_PS_DATA:
		err = APDS9930_read_ps(obj->client, &obj->ps);
		if (err)
			goto err_out;


		dat = APDS9930_get_ps_value(obj, obj->ps);
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_PS_RAW_DATA:
		err = APDS9930_read_ps(obj->client, &obj->ps);
		if (err)
			goto err_out;


		dat = obj->ps;
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_SET_ALS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		if (enable) {
			err = APDS9930_enable_als(obj->client, 1);
			if (err) {
				APS_ERR("enable als fail: %ld\n", err);
				goto err_out;
			}
			set_bit(CMC_BIT_ALS, &obj->enable);
		} else {
			err = APDS9930_enable_als(obj->client, 0);
			if (err) {
				APS_ERR("disable als fail: %ld\n", err);
				goto err_out;
			}
			clear_bit(CMC_BIT_ALS, &obj->enable);
		}
		break;

	case ALSPS_GET_ALS_MODE:
		enable = test_bit(CMC_BIT_ALS, &obj->enable) ? (1) : (0);
		if (copy_to_user(ptr, &enable, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_ALS_DATA:
		err = APDS9930_read_als(obj->client, &obj->als);
		if (err)
			goto err_out;


		dat = APDS9930_get_als_value(obj, obj->als);
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_ALS_RAW_DATA:
		err = APDS9930_read_als(obj->client, &obj->als);
		if (err)
			goto err_out;


		dat = obj->als;
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;
		/*----------------------------------for factory mode test---------------------------------------*/
	case ALSPS_GET_PS_TEST_RESULT:
		err = APDS9930_read_ps(obj->client, &obj->ps);
		if (err)
			goto err_out;

		if (obj->ps > atomic_read(&obj->ps_thd_val_high))
			ps_result = 0;
		else
			ps_result = 1;

		if (copy_to_user(ptr, &ps_result, sizeof(ps_result))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_IOCTL_CLR_CALI:
		if (copy_from_user(&dat, ptr, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		if (dat == 0)
			obj->ps_cali = 0;
		break;

	case ALSPS_IOCTL_GET_CALI:
		ps_cali = obj->ps_cali;
		if (copy_to_user(ptr, &ps_cali, sizeof(ps_cali))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_IOCTL_SET_CALI:
		if (copy_from_user(&ps_cali, ptr, sizeof(ps_cali))) {
			err = -EFAULT;
			goto err_out;
		}

		obj->ps_cali = ps_cali;
		break;

	case ALSPS_SET_PS_THRESHOLD:
		if (copy_from_user(threshold, ptr, sizeof(threshold))) {
			err = -EFAULT;
			goto err_out;
		}
		APS_ERR("%s set threshold high: 0x%x, low: 0x%x\n", __func__, threshold[0],
			threshold[1]);
		atomic_set(&obj->ps_thd_val_high, (threshold[0] + obj->ps_cali));
		atomic_set(&obj->ps_thd_val_low, (threshold[1] + obj->ps_cali));	/* need to confirm */

		set_psensor_threshold(obj->client);

		break;

	case ALSPS_GET_PS_THRESHOLD_HIGH:
		threshold[0] = atomic_read(&obj->ps_thd_val_high) - obj->ps_cali;
		APS_ERR("%s get threshold high: 0x%x\n", __func__, threshold[0]);
		if (copy_to_user(ptr, &threshold[0], sizeof(threshold[0]))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_PS_THRESHOLD_LOW:
		threshold[0] = atomic_read(&obj->ps_thd_val_low) - obj->ps_cali;
		APS_ERR("%s get threshold low: 0x%x\n", __func__, threshold[0]);
		if (copy_to_user(ptr, &threshold[0], sizeof(threshold[0]))) {
			err = -EFAULT;
			goto err_out;
		}
		break;
			/*------------------------------------------------------------------------------------------*/
	default:
		APS_ERR("%s not supported = 0x%04x", __func__, cmd);
		err = -ENOIOCTLCMD;
		break;
	}

 err_out:
	return err;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_COMPAT
static long APDS9930_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long err = 0;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_ALSPS_SET_PS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_SET_PS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_RAW_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_RAW_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_SET_ALS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_SET_ALS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_ALS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_ALS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_ALS_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_ALS_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_ALS_RAW_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_ALS_RAW_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_TEST_RESULT:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_TEST_RESULT, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_IOCTL_CLR_CALI:
		err = file->f_op->unlocked_ioctl(file, ALSPS_IOCTL_CLR_CALI, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_IOCTL_GET_CALI:
		err = file->f_op->unlocked_ioctl(file, ALSPS_IOCTL_GET_CALI, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_IOCTL_SET_CALI:
		err = file->f_op->unlocked_ioctl(file, ALSPS_IOCTL_SET_CALI, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_SET_PS_THRESHOLD:
		err = file->f_op->unlocked_ioctl(file, ALSPS_SET_PS_THRESHOLD, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_THRESHOLD_HIGH:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_THRESHOLD_HIGH, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_THRESHOLD_LOW:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_THRESHOLD_LOW, (unsigned long)arg32);
		break;
	default:
		APS_ERR("%s not supported = 0x%04x", __func__, cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}
#endif
/*----------------------------------------------------------------------------*/
static const struct file_operations APDS9930_fops = {
	.owner = THIS_MODULE,
	.open = APDS9930_open,
	.release = APDS9930_release,
	.unlocked_ioctl = APDS9930_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = APDS9930_compat_ioctl,
#endif
};

/*----------------------------------------------------------------------------*/
static struct miscdevice APDS9930_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &APDS9930_fops,
};

/*----------------------------------------------------------------------------*/
static int APDS9930_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
/* struct APDS9930_priv *obj = i2c_get_clientdata(client); */
/* int err; */
	APS_FUN();
#if 0
	if (msg.event == PM_EVENT_SUSPEND) {
		if (!obj) {
			APS_ERR("null pointer!!\n");
			return -EINVAL;
		}

		atomic_set(&obj->als_suspend, 1);
		err = APDS9930_enable_als(client, 0);
		if (err) {
			APS_ERR("disable als: %d\n", err);
			return err;
		}

		atomic_set(&obj->ps_suspend, 1);
		err = APDS9930_enable_ps(client, 0);
		if (err) {
			APS_ERR("disable ps:  %d\n", err);
			return err;
		}

		APDS9930_power(obj->hw, 0);
	}
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
static int APDS9930_i2c_resume(struct i2c_client *client)
{
/* struct APDS9930_priv *obj = i2c_get_clientdata(client); */
/* int err; */
	APS_FUN();
#if 0
	if (!obj) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	APDS9930_power(obj->hw, 1);
	err = APDS9930_init_client(client);
	if (err) {
		APS_ERR("initialize client fail!!\n");
		return err;
	}
	atomic_set(&obj->als_suspend, 0);
	if (test_bit(CMC_BIT_ALS, &obj->enable)) {
		err = APDS9930_enable_als(client, 1);
		if (err)
			APS_ERR("enable als fail: %d\n", err);

	}
	atomic_set(&obj->ps_suspend, 0);
	if (test_bit(CMC_BIT_PS, &obj->enable)) {
		err = APDS9930_enable_ps(client, 1);
		if (err)
			APS_ERR("enable ps fail: %d\n", err);

	}
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
static void APDS9930_early_suspend(struct early_suspend *h)
{				/*early_suspend is only applied for ALS */
	struct APDS9930_priv *obj = container_of(h, struct APDS9930_priv, early_drv);
	int err;

	APS_FUN();

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return;
	}
#if 1
	atomic_set(&obj->als_suspend, 1);
	if (test_bit(CMC_BIT_ALS, &obj->enable)) {
		err = APDS9930_enable_als(obj->client, 0);
		if (err)
			APS_ERR("disable als fail: %d\n", err);

	}
#endif
}

/*----------------------------------------------------------------------------*/
static void APDS9930_late_resume(struct early_suspend *h)
{				/*early_suspend is only applied for ALS */
	struct APDS9930_priv *obj = container_of(h, struct APDS9930_priv, early_drv);
	int err;

	APS_FUN();

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return;
	}
#if 1
	atomic_set(&obj->als_suspend, 0);
	if (test_bit(CMC_BIT_ALS, &obj->enable)) {
		err = APDS9930_enable_als(obj->client, 1);
		if (err)
			APS_ERR("enable als fail: %d\n", err);

	}
#endif
}
#endif

/*----------------------------------------------------------------------------*/

#ifndef ADPS9933_NEW_ARCH
static int temp_als;
static int ALS_FLAG;

int APDS9930_ps_operate(void *self, uint32_t command, void *buff_in, int size_in,
			void *buff_out, int size_out, int *actualout)
{
	int value;
	int err = 0;

	struct hwm_sensor_data *sensor_data;
	struct APDS9930_priv *obj = (struct APDS9930_priv *)self;

	/* APS_FUN(f); */
	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			APS_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		}
		/* Do nothing */
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			APS_ERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (value) {
				err = APDS9930_enable_ps(obj->client, 1);
				if (err) {
					APS_ERR("enable ps fail: %d\n", err);
					return -1;
				}
				set_bit(CMC_BIT_PS, &obj->enable);
#if 1
				if (!test_bit(CMC_BIT_ALS, &obj->enable)) {
					ALS_FLAG = 1;
					err = APDS9930_enable_als(obj->client, 1);
					if (err) {
						APS_ERR("enable als fail: %d\n", err);
						return -1;
					}
				}
#endif
			} else {
				err = APDS9930_enable_ps(obj->client, 0);
				if (err) {
					APS_ERR("disable ps fail: %d\n", err);
					return -1;
				}
				clear_bit(CMC_BIT_PS, &obj->enable);
#if 1
				if (ALS_FLAG == 1) {
					err = APDS9930_enable_als(obj->client, 0);
					if (err) {
						APS_ERR("disable als fail: %d\n", err);
						return -1;
					}
					ALS_FLAG = 0;
				}
#endif
			}
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data))) {
			APS_ERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			sensor_data = (struct hwm_sensor_data *) buff_out;
			APDS9930_read_ps(obj->client, &obj->ps);
			APDS9930_read_als_ch0(obj->client, &obj->als);
			APS_ERR("APDS9930_ps_operate als data=%d!\n", obj->als);
			sensor_data->values[0] = APDS9930_get_ps_value(obj, obj->ps);
			sensor_data->value_divide = 1;
			sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
		}
		break;
	default:
		APS_ERR("proxmy sensor operate function no this parameter %d!\n", command);
		err = -1;
		break;
	}

	return err;
}


int APDS9930_als_operate(void *self, uint32_t command, void *buff_in, int size_in,
			 void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *sensor_data;
	struct APDS9930_priv *obj = (struct APDS9930_priv *)self;

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			APS_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		}
		/* Do nothing */
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			APS_ERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (value) {
				err = APDS9930_enable_als(obj->client, 1);
				if (err) {
					APS_ERR("enable als fail: %d\n", err);
					return -1;
				}
				set_bit(CMC_BIT_ALS, &obj->enable);
			} else {
				err = APDS9930_enable_als(obj->client, 0);
				if (err) {
					APS_ERR("disable als fail: %d\n", err);
					return -1;
				}
				clear_bit(CMC_BIT_ALS, &obj->enable);
			}

		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data))) {
			APS_ERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			sensor_data = (struct hwm_sensor_data *) buff_out;
			/*yucong MTK add for fixing known issue */
			APDS9930_read_als(obj->client, &obj->als);
			if (obj->als == 0) {
				sensor_data->values[0] = temp_als;
			} else {
				u16 b[2];
				int i;

				for (i = 0; i < 2; i++) {
					APDS9930_read_als(obj->client, &obj->als);
					b[i] = obj->als;
				}
				(b[1] > b[0]) ? (obj->als = b[0]) : (obj->als = b[1]);
				sensor_data->values[0] = APDS9930_get_als_value(obj, obj->als);
				temp_als = sensor_data->values[0];
			}
			sensor_data->value_divide = 1;
			sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
		}
		break;
	default:
		APS_ERR("light sensor operate function no this parameter %d!\n", command);
		err = -1;
		break;
	}

	return err;
}
#endif


/*----------------------------------------------------------------------------*/
static int APDS9930_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, APDS9930_DEV_NAME);
	return 0;
}
/* if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL */
static int als_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */

static int als_enable_nodata(int en)
{
	int res = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

	APS_LOG("APDS9930_obj als enable value = %d\n", en);

#ifdef CUSTOM_KERNEL_SENSORHUB
	if (atomic_read(&APDS9930_obj->init_done)) {
		req.activate_req.sensorType = ID_LIGHT;
		req.activate_req.action = SENSOR_HUB_ACTIVATE;
		req.activate_req.enable = en;
		len = sizeof(req.activate_req);
		res = SCP_sensorHub_req_send(&req, &len, 1);
	} else
		APS_ERR("sensor hub has not been ready!!\n");

	mutex_lock(&APDS9930_mutex);
	if (en)
		set_bit(CMC_BIT_ALS, &APDS9930_obj->enable);
	else
		clear_bit(CMC_BIT_ALS, &APDS9930_obj->enable);
	mutex_unlock(&APDS9930_mutex);
#else /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	mutex_lock(&APDS9930_mutex);
	if (en)
		set_bit(CMC_BIT_ALS, &APDS9930_obj->enable);
	else
		clear_bit(CMC_BIT_ALS, &APDS9930_obj->enable);
	mutex_unlock(&APDS9930_mutex);
	if (!APDS9930_obj) {
		APS_ERR("APDS9930_obj is null!!\n");
		return -1;
	}
	res = APDS9930_enable_als(APDS9930_obj->client, en);
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	if (res) {
		APS_ERR("als_enable_nodata is failed!!\n");
		return -1;
	}
	return 0;
}

static int als_set_delay(u64 ns)
{
	return 0;
}

static int als_get_data(int *value, int *status)
{
	int err = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#else
	struct APDS9930_priv *obj = NULL;
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

#ifdef CUSTOM_KERNEL_SENSORHUB
	if (atomic_read(&APDS9930_obj->init_done)) {
		req.get_data_req.sensorType = ID_LIGHT;
		req.get_data_req.action = SENSOR_HUB_GET_DATA;
		len = sizeof(req.get_data_req);
		err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err)
		APS_ERR("SCP_sensorHub_req_send fail!\n");
	else {
		*value = req.get_data_rsp.int16_Data[0];
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	if (atomic_read(&APDS9930_obj->trace) & CMC_TRC_PS_DATA)
		APS_LOG("value = %d\n", *value);
	else {
		APS_ERR("sensor hub hat not been ready!!\n");
		err = -1;
	}
#else /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	if (!APDS9930_obj) {
		APS_ERR("APDS9930_obj is null!!\n");
		return -1;
	}
	obj = APDS9930_obj;
	err = APDS9930_read_als(obj->client, &obj->als);
	if (err)
		err = -1;
	else {
		*value = APDS9930_get_als_value(obj, obj->als);
		if (*value < 0)
			err = -1;
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

	return err;
}

/* if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL */
static int ps_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */

static int ps_enable_nodata(int en)
{
	int res = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

	APS_LOG("APDS9930_obj als enable value = %d\n", en);

#ifdef CUSTOM_KERNEL_SENSORHUB
	if (atomic_read(&APDS9930_obj->init_done)) {
		req.activate_req.sensorType = ID_PROXIMITY;
		req.activate_req.action = SENSOR_HUB_ACTIVATE;
		req.activate_req.enable = en;
		len = sizeof(req.activate_req);
		res = SCP_sensorHub_req_send(&req, &len, 1);
	} else
		APS_ERR("sensor hub has not been ready!!\n");

	mutex_lock(&APDS9930_mutex);
	if (en)
		set_bit(CMC_BIT_PS, &APDS9930_obj->enable);
	else
		clear_bit(CMC_BIT_PS, &APDS9930_obj->enable);
	mutex_unlock(&APDS9930_mutex);
#else /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	mutex_lock(&APDS9930_mutex);
	if (en)
		set_bit(CMC_BIT_PS, &APDS9930_obj->enable);

	else
		clear_bit(CMC_BIT_PS, &APDS9930_obj->enable);

	mutex_unlock(&APDS9930_mutex);
	if (!APDS9930_obj) {
		APS_ERR("APDS9930_obj is null!!\n");
		return -1;
	}
	res = APDS9930_enable_ps(APDS9930_obj->client, en);
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

	if (res) {
		APS_ERR("als_enable_nodata is failed!!\n");
		return -1;
	}
	return 0;

}

static int ps_set_delay(u64 ns)
{
	return 0;
}

static int ps_get_data(int *value, int *status)
{
	int err = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

#ifdef CUSTOM_KERNEL_SENSORHUB
	if (atomic_read(&APDS9930_obj->init_done)) {
		req.get_data_req.sensorType = ID_PROXIMITY;
		req.get_data_req.action = SENSOR_HUB_GET_DATA;
		len = sizeof(req.get_data_req);
		err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err) {
		APS_ERR("SCP_sensorHub_req_send fail!\n");
		*value = -1;
		err = -1;
	} else {
		*value = req.get_data_rsp.int16_Data[0];
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	if (atomic_read(&APDS9930_obj->trace) & CMC_TRC_PS_DATA)
		APS_LOG("value = %d\n", *value)
	else {
		APS_ERR("sensor hub has not been ready!!\n");
		err = -1;
	}
#else /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	if (!APDS9930_obj) {
		APS_ERR("APDS9930_obj is null!!\n");
		return -1;
	}

	err = APDS9930_read_ps(APDS9930_obj->client, &APDS9930_obj->ps);
	if (err)
		err = -1;
	else {
		*value = APDS9930_get_ps_value(APDS9930_obj, APDS9930_obj->ps);
		if (*value < 0)
			err = -1;
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

	return err;
}
/*----------------------------------------------------------------------------*/
static int APDS9930_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct APDS9930_priv *obj;
#ifndef ADPS9933_NEW_ARCH
	struct hwmsen_object obj_ps, obj_als;
#endif
	struct als_control_path als_ctl = {0};
	struct als_data_path als_data = {0};
	struct ps_control_path ps_ctl = {0};
	struct ps_data_path ps_data = {0};
	int err = 0;

	APS_FUN();

/*by qliu4244 add sensors function start*/
	/*of_get_APDS9930_platform_data(&client->dev);*/
	/* configure the gpio pins */
	/*err = gpio_request_one(alsps_int_gpio_number, GPIOF_IN,
				 "alsps_int");
	if (err < 0) {
		APS_ERR("Unable to request gpio int_pin\n");
		return -1;
	}*/
/*by qliu4244 add sensors function end*/
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!(obj)) {
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(*obj));
	APDS9930_obj = obj;
	obj->hw = hw;
	APDS9930_get_addr(obj->hw, &obj->addr);

	/*for interrupt work mode support*/
	INIT_WORK(&obj->irq_work, APDS9930_irq_work);
#ifdef FACTORY_MACRO_PS
	INIT_DELAYED_WORK(&obj->ps_cal_poll_work,APDS9930_cal_poll_work);
#endif
	obj->client = client;
	i2c_set_clientdata(client, obj);
	atomic_set(&obj->als_debounce, 50);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 10);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->als_cmd_val, 0xDF);
	atomic_set(&obj->ps_cmd_val, 0xC1);
	atomic_set(&obj->ps_thd_val_high, obj->hw->ps_threshold_high);
	atomic_set(&obj->ps_thd_val_low, obj->hw->ps_threshold_low);
#ifdef FACTORY_MACRO_PS
	obj->prox_uncover_data = 0;
	obj->prox_manual_calibrate_threshold = 0;
	obj->prox_offset = 0;
	obj->prox_threshold_hi = APDS9930_DEFAULT_THRESHOLD_HIGH;
	obj->prox_threshold_lo = APDS9930_DEFAULT_THRESHOLD_LOW;
	obj->prox_thres_hi_max = APDS9930_DEFAULT_THRESHOLD_HIGH;
	obj->prox_thres_hi_min = APDS9930_DEFAULT_THRESHOLD_HIGH;
	obj->prox_thres_lo_max = APDS9930_DEFAULT_THRESHOLD_LOW;
	obj->prox_thres_lo_max = APDS9930_DEFAULT_THRESHOLD_LOW;
	obj->prox_cal_valid = false;
	obj->debug = false;
#endif
	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_level_num = sizeof(obj->hw->als_level) / sizeof(obj->hw->als_level[0]);
	obj->als_value_num = sizeof(obj->hw->als_value) / sizeof(obj->hw->als_value[0]);
	/*modified gain 16 to 1/5 according to actual thing */
	/* (1/Gain)*(400/Tine), this value is fix after init ATIME and CONTROL register value */
	obj->als_modulus = (400 * 100 * ZOOM_TIME) / (1 * 150);
	/* (400)/16*2.72 here is amplify *100 / *16 */
	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	set_bit(CMC_BIT_ALS, &obj->enable);
	set_bit(CMC_BIT_PS, &obj->enable);

	obj->ps_cali = 0;

	APDS9930_i2c_client = client;

#ifndef ADPS9933_NEW_ARCH
	if (1 == obj->hw->polling_mode_ps)
	{
		obj_ps.polling = 1;
	} else {
		obj_ps.polling = 0;
	}
#endif
	err = APDS9930_init_client(client);
	if (err)
		goto exit_init_failed;
	APS_LOG("APDS9930_init_client() OK!\n");

	err = misc_register(&APDS9930_device);
	if (err) {
		APS_ERR("APDS9930_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	als_ctl.is_use_common_factory = false;
	ps_ctl.is_use_common_factory = false;

#ifdef FACTORY_MACRO_PS
	if((err = APDS9930_create_attr(&(APDS9930_init_info.platform_diver_addr->driver))))
	{
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
#endif

#ifndef ADPS9933_NEW_ARCH

	obj_ps.self = APDS9930_obj;

	obj_ps.sensor_operate = APDS9930_ps_operate;
	err = hwmsen_attach(ID_PROXIMITY, &obj_ps);
	if (err) {
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}

	obj_als.self = APDS9930_obj;
	obj_als.polling = 1;
	obj_als.sensor_operate = APDS9930_als_operate;
	err = hwmsen_attach(ID_LIGHT, &obj_als);
	if (err) {
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
		obj->early_drv.suspend = APDS9930_early_suspend,
		obj->early_drv.resume = APDS9930_late_resume, register_early_suspend(&obj->early_drv);
#endif
	als_ctl.open_report_data = als_open_report_data;
	als_ctl.enable_nodata = als_enable_nodata;
	als_ctl.set_delay  = als_set_delay;
	als_ctl.is_report_input_direct = false;
#ifdef CUSTOM_KERNEL_SENSORHUB
	als_ctl.is_support_batch = obj->hw->is_batch_supported_als;
#else
	als_ctl.is_support_batch = false;
#endif

	err = als_register_control_path(&als_ctl);
	if (err) {
		APS_ERR("register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	als_data.get_data = als_get_data;
	als_data.vender_div = 100;
	err = als_register_data_path(&als_data);
	if (err) {
		APS_ERR("tregister fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	ps_ctl.open_report_data = ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.set_delay  = ps_set_delay;
	ps_ctl.is_report_input_direct = false;
#ifdef CUSTOM_KERNEL_SENSORHUB
	ps_ctl.is_support_batch = obj->hw->is_batch_supported_ps;
#else
	ps_ctl.is_support_batch = false;
#endif
	err = ps_register_control_path(&ps_ctl);
	if (err) {
		APS_ERR("register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	ps_data.get_data = ps_get_data;
	ps_data.vender_div = 100;
	err = ps_register_data_path(&ps_data);
	if (err) {
		APS_ERR("tregister fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	err = batch_register_support_info(ID_LIGHT, als_ctl.is_support_batch, 1, 0);
	if (err)
		APS_ERR("register light batch support err = %d\n", err);

	err = batch_register_support_info(ID_PROXIMITY, ps_ctl.is_support_batch, 1, 0);
	if (err)
		APS_ERR("register proximity batch support err = %d\n", err);

	APDS9930_init_flag = 0;
	APS_LOG("%s: OK\n", __func__);
	return 0;

#ifdef FACTORY_MACRO_PS
exit_create_attr_failed:
#endif
exit_sensor_obj_attach_fail:
	misc_deregister(&APDS9930_device);
exit_misc_device_register_failed:
exit_init_failed:
	/* i2c_detach_client(client); */
	/* exit_kfree: */
	kfree(obj);
exit:
	/*gpio_free(alsps_int_gpio_number);*/
	APDS9930_i2c_client = NULL;
	APS_ERR("%s: err = %d\n", __func__, err);
	APDS9930_init_flag = -1;
	return err;
}
/*----------------------------------------------------------------------------*/
static int APDS9930_i2c_remove(struct i2c_client *client)
{
	int err;
/*
	if(err = APDS9930_delete_attr(&APDS9930_i2c_driver.driver))
	{
		APS_ERR("APDS9930_delete_attr fail: %d\n", err);
	}
*/
#ifdef FACTORY_MACRO_PS
	if((err = APDS9930_delete_attr(&(APDS9930_init_info.platform_diver_addr->driver))))
	{
		APS_ERR("stk3x1x_delete_attr fail: %d\n", err);
	}
#endif
	err = misc_deregister(&APDS9930_device);
	if (err)
		APS_ERR("misc_deregister fail: %d\n", err);


	/*gpio_free(alsps_int_gpio_number);*/
	APDS9930_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}
/*----------------------------------------------------------------------------*/
static int  APDS9930_local_init(void)
{

	APDS9930_power(hw, 1);
	if (i2c_add_driver(&APDS9930_i2c_driver)) {
		APS_ERR("add driver error\n");
		return -1;
	}
	if (-1 == APDS9930_init_flag)
		return -1;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int APDS9930_remove(void)
{
	APS_FUN();
	APDS9930_power(hw, 0);
	i2c_del_driver(&APDS9930_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/

/*by qliu4244 add sensors function start*/
#if 0
static int of_get_APDS9930_platform_data(struct device *dev)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,als_ps");
	if (node) {
		alsps_int_gpio_number = of_get_named_gpio(node, "int-gpio", 0);
		alsps_irq = irq_of_parse_and_map(node, 0);
		if (alsps_irq < 0) {
			APS_ERR("alsps request_irq IRQ LINE NOT AVAILABLE!.");
			return -1;
		}
		APS_ERR("alsps_int_gpio_number %d; alsps_irq : %d\n", alsps_int_gpio_number, alsps_irq);
	}
	return 0;
}
#endif
/*by qliu4244 add sensors function end*/

/*----------------------------------------------------------------------------*/
static int __init APDS9930_init(void)
{
	const char *name = "mediatek,als_ps";

	APS_FUN();

	hw =   get_alsps_dts_func(name, hw);
	if (!hw)
		APS_ERR("get dts info fail\n");
	alsps_driver_add(&APDS9930_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit APDS9930_exit(void)
{
	APS_FUN();
}

/*----------------------------------------------------------------------------*/
module_init(APDS9930_init);
module_exit(APDS9930_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Dexiang Liu");
MODULE_DESCRIPTION("APDS9930 driver");
MODULE_LICENSE("GPL");
