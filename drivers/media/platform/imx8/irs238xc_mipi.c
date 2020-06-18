/*
 * Copyright (C) 2012-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2018 NXP
 */
/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/v4l2-mediabus.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

#define MIN_FPS 15
#define MAX_FPS 30
#define DEFAULT_FPS 30

#define IRS238XC_XCLK 24000000

#define IRS238XC_SENS_PAD_SOURCE	0
#define IRS238XC_SENS_PADS_NUM	1

#define IRS238XC_DEF_WIDTH		224
#define IRS238XC_DEF_HEIGHT		172

enum irs238xc_mode {
	IRS238XC_MODE_DEF = 0,
};

enum irs238xc_frame_rate {
	irs238xc_15_fps,
	irs238xc_30_fps
};

struct irs238xc_datafmt {
	u32	code;
	enum v4l2_colorspace		colorspace;
};

struct reg_value {
	u16 RegAddr;
	u16 Val;
	u16 Mask;
	u32 Delay_ms;
};

struct irs238xc_mode_info {
	enum irs238xc_mode mode;
	u32 width;
	u32 height;
	struct reg_value *init_data_ptr;
	u32 init_data_size;
};

struct irs238xc {
	struct v4l2_subdev		subdev;
	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	const struct irs238xc_datafmt	*fmt;
	struct v4l2_captureparm streamcap;
	struct media_pad pads[IRS238XC_SENS_PADS_NUM];
	bool on;

	/* control settings */
	int brightness;
	int hue;
	int contrast;
	int saturation;
	int red;
	int green;
	int blue;
	int ae_mode;

	u32 mclk;
	u8 mclk_source;
	struct clk *sensor_clk;
	int csi;

	void (*io_init)(struct irs238xc *);
	int pwn_gpio, rst_gpio;
};

static struct reg_value irs238xc_init_setting[] = {
	{0x9000, 0x1E1E, 0, 0},
	{0x9001, 0x0000, 0, 0},
	{0x9002, 0x4DBA, 0, 0},
	{0x9003, 0x0000, 0, 0},
	{0x9004, 0x4DBA, 0, 0},
	{0x9005, 0x0000, 0, 0},
	{0x9006, 0x4DBA, 0, 0},
	{0x9007, 0x0000, 0, 0},
	{0x9008, 0x4DBA, 0, 0},
	{0x9009, 0x0000, 0, 0},
	{0x900A, 0x4A4B, 0, 0},
	{0x900B, 0x0000, 0, 0},
	{0x900C, 0x4A4B, 0, 0},
	{0x900D, 0x0000, 0, 0},
	{0x900E, 0x4A4B, 0, 0},
	{0x900F, 0x0000, 0, 0},
	{0x9010, 0x4A4B, 0, 0},
	{0x9011, 0x0000, 0, 0},
	{0x9080, 0x1E1E, 0, 0},
	{0x9081, 0x0000, 0, 0},
	{0x9082, 0x10A0, 0, 0},
	{0x9083, 0x00A0, 0, 0},
	{0x9084, 0x8000, 0, 0},
	{0x9085, 0x4DBA, 0, 0},
	{0x9086, 0x0000, 0, 0},
	{0x9087, 0x0000, 0, 0},
	{0x9088, 0x0000, 0, 0},
	{0x9089, 0x0000, 0, 0},
	{0x908A, 0x4DBA, 0, 0},
	{0x908B, 0x0000, 0, 0},
	{0x908C, 0x0020, 0, 0},
	{0x908D, 0x0000, 0, 0},
	{0x908E, 0x0000, 0, 0},
	{0x908F, 0x4DBA, 0, 0},
	{0x9090, 0x0000, 0, 0},
	{0x9091, 0x0040, 0, 0},
	{0x9092, 0x0000, 0, 0},
	{0x9093, 0x0000, 0, 0},
	{0x9094, 0x4DBA, 0, 0},
	{0x9095, 0x0000, 0, 0},
	{0x9096, 0x0060, 0, 0},
	{0x9097, 0x0000, 0, 0},
	{0x9098, 0x0000, 0, 0},
	{0x9099, 0x4A4B, 0, 0},
	{0x909A, 0x0000, 0, 0},
	{0x909B, 0x1000, 0, 0},
	{0x909C, 0x0000, 0, 0},
	{0x909D, 0x0000, 0, 0},
	{0x909E, 0x4A4B, 0, 0},
	{0x909F, 0x0000, 0, 0},
	{0x90A0, 0x1020, 0, 0},
	{0x90A1, 0x0000, 0, 0},
	{0x90A2, 0x0000, 0, 0},
	{0x90A3, 0x4A4B, 0, 0},
	{0x90A4, 0x0000, 0, 0},
	{0x90A5, 0x1040, 0, 0},
	{0x90A6, 0x0000, 0, 0},
	{0x90A7, 0x0000, 0, 0},
	{0x90A8, 0x4A4B, 0, 0},
	{0x90A9, 0x0000, 0, 0},
	{0x90AA, 0x1060, 0, 0},
	{0x90AB, 0x0000, 0, 0},
	{0x90AC, 0x8000, 0, 0},
	{0x91C0, 0x0592, 0, 0},
	{0x91C1, 0xDF00, 0, 0},
	{0x91C2, 0xAB00, 0, 0},
	{0x91C3, 0x0508, 0, 0},
	{0x91C4, 0x0008, 0, 0},
	{0x91C5, 0x0020, 0, 0},
	{0x91C6, 0x8008, 0, 0},
	{0x91CF, 0x0009, 0, 0},
	{0x91D3, 0x061A, 0, 0},
	{0x91DB, 0x0008, 0, 0},
	{0x91EA, 0x1EA1, 0, 0},
	{0x91EB, 0xBF26, 0, 0},
	{0x91EC, 0x0008, 0, 0},
	{0x91ED, 0x0D01, 0, 0},
	{0x91EE, 0x5555, 0, 0},
	{0x91EF, 0x02F5, 0, 0},
	{0x91F0, 0x0009, 0, 0},
	{0x91F1, 0x0031, 0, 0},
	{0x91F2, 0x16A1, 0, 0},
	{0x91F3, 0x1EB8, 0, 0},
	{0x91F4, 0x0005, 0, 0},
	{0x91F5, 0x0C01, 0, 0},
	{0x91F6, 0xFCBF, 0, 0},
	{0x91F7, 0x04A7, 0, 0},
	{0x91F8, 0x000D, 0, 0},
	{0x91F9, 0x0022, 0, 0},
	{0x9220, 0x0003, 0, 0},
	{0x9221, 0x000C, 0, 0},
	{0x9244, 0x0003, 0, 0},
	{0x9245, 0x0008, 0, 0},
	{0x9268, 0x0001, 0, 0},
	{0x9269, 0x0001, 0, 0},
	{0x9278, 0x0B01, 0, 0},
	{0x9279, 0x0001, 0, 0},
	{0x9288, 0x0236, 0, 0},
	{0x9289, 0x0236, 0, 0},
	{0x928A, 0x0238, 0, 0},
	{0x928B, 0x0236, 0, 0},
	{0x928C, 0x0236, 0, 0},
	{0x928D, 0x0238, 0, 0},
	{0x928E, 0x0236, 0, 0},
	{0x928F, 0x0236, 0, 0},
	{0x9290, 0x0238, 0, 0},
	{0x9291, 0x0236, 0, 0},
	{0x9292, 0x0236, 0, 0},
	{0x9293, 0x0238, 0, 0},
	{0x9294, 0x011C, 0, 0},
	{0x9295, 0x011C, 0, 0},
	{0x9296, 0x011C, 0, 0},
	{0x9297, 0x011C, 0, 0},
	{0x9298, 0x011C, 0, 0},
	{0x9299, 0x011C, 0, 0},
	{0x929A, 0x011C, 0, 0},
	{0x929B, 0x011C, 0, 0},
	{0x929C, 0x0080, 0, 0},
	{0x929D, 0x0096, 0, 0},
	{0x929E, 0x008E, 0, 0},
	{0x929F, 0x0088, 0, 0},
	{0x92A0, 0x0096, 0, 0},
	{0x92A1, 0x0080, 0, 0},
	{0x92A2, 0x0096, 0, 0},
	{0x92A3, 0x0080, 0, 0},
	{0x92A4, 0x0040, 0, 0},
	{0x92A5, 0x0040, 0, 0},
	{0x92A6, 0x0040, 0, 0},
	{0x92A7, 0x0040, 0, 0},
	{0x92A8, 0x0040, 0, 0},
	{0x92A9, 0x0040, 0, 0},
	{0x92AA, 0x0040, 0, 0},
	{0x92AB, 0x0040, 0, 0},
	{0x92AC, 0x0040, 0, 0},
	{0x92AD, 0x0040, 0, 0},
	{0x92AE, 0x0040, 0, 0},
	{0x92AF, 0x0040, 0, 0},
	{0x92B0, 0x0040, 0, 0},
	{0x92B1, 0x0040, 0, 0},
	{0x92B2, 0x0040, 0, 0},
	{0x92B3, 0x0040, 0, 0},
	{0x92B4, 0x0040, 0, 0},
	{0x92B5, 0x0040, 0, 0},
	{0x92B6, 0x0040, 0, 0},
	{0x92B7, 0x0040, 0, 0},
	{0x92B8, 0x0040, 0, 0},
	{0x92B9, 0x0040, 0, 0},
	{0x92BA, 0x0040, 0, 0},
	{0x92BB, 0x0040, 0, 0},
	{0x92BC, 0x0040, 0, 0},
	{0x92BD, 0x0040, 0, 0},
	{0x92BE, 0x0040, 0, 0},
	{0x92BF, 0x0040, 0, 0},
	{0x92C0, 0x0040, 0, 0},
	{0x92C1, 0x0040, 0, 0},
	{0x92C2, 0x0040, 0, 0},
	{0x92C3, 0x0040, 0, 0},
	{0x92C4, 0x643B, 0, 0},
	{0x92C5, 0xA889, 0, 0},
	{0x92C6, 0xE5C7, 0, 0},
	{0x92C7, 0xFFFF, 0, 0},
	{0x92C8, 0x513C, 0, 0},
	{0x92C9, 0x7865, 0, 0},
	{0x92CA, 0x9B8A, 0, 0},
	{0x92CB, 0x00AC, 0, 0},
	{0x92CC, 0x3124, 0, 0},
	{0x92CD, 0x493E, 0, 0},
	{0x92CE, 0x5F54, 0, 0},
	{0x92CF, 0x0069, 0, 0},
	{0x92D0, 0x403C, 0, 0},
	{0x92D1, 0x4844, 0, 0},
	{0x92D2, 0x504C, 0, 0},
	{0x92D3, 0x0054, 0, 0},
	{0x92D4, 0x2E26, 0, 0},
	{0x92D5, 0x4D3E, 0, 0},
	{0x92D6, 0x6455, 0, 0},
	{0x92D7, 0x006C, 0, 0},
	{0x92D8, 0x4932, 0, 0},
	{0x92D9, 0x745F, 0, 0},
	{0x92DA, 0x9F8A, 0, 0},
	{0x92DB, 0x00B4, 0, 0},
	{0x92DC, 0x0F0A, 0, 0},
	{0x92DD, 0x1914, 0, 0},
	{0x92DE, 0x221E, 0, 0},
	{0x92DF, 0x2C27, 0, 0},
	{0x92E0, 0x3631, 0, 0},
	{0x92E1, 0x003B, 0, 0},
	{0x92E2, 0x0540, 0, 0},
	{0x92E3, 0x08D4, 0, 0},
	{0x92E4, 0x0875, 0, 0},
	{0x92E5, 0x0848, 0, 0},
	{0x92E6, 0x0837, 0, 0},
	{0x92E7, 0x0001, 0, 0},
	{0x92E8, 0x0068, 0, 0},
	{0x92E9, 0x0300, 0, 0},
	{0x92EA, 0x0000, 0, 0},
	{0x92EB, 0x0000, 0, 0},
	{0x92EC, 0x0000, 0, 0},
	{0x92ED, 0x0000, 0, 0},
	{0x92EE, 0x0000, 0, 0},
	{0x92EF, 0x0000, 0, 0},
	{0x92F0, 0x0016, 0, 0},
	{0x92F1, 0xEF57, 0, 0},
	{0x9401, 0x0002, 0, 0},
	{0xA001, 0x0007, 0, 0},
	{0xA008, 0x1513, 0, 0},
	{0xA00C, 0x0135, 0, 0},
	{0xA039, 0x1AA1, 0, 0},
	{0xA03A, 0xAAAB, 0, 0},
	{0xA03B, 0x000A, 0, 0},
	{0xA03C, 0x0000, 0, 0},
	{0xA03D, 0x03C0, 0, 0},
	{0xA03E, 0x0000, 0, 0},
	{0xA03F, 0x0017, 0, 0},
	{0x9400, 0x0001, 0, 0},
};

static struct regulator *io_regulator;
static struct regulator *core_regulator;
static struct regulator *analog_regulator;

static int irs238xc_probe(struct i2c_client *adapter,
				const struct i2c_device_id *device_id);
static int irs238xc_remove(struct i2c_client *client);

static s32 irs238xc_read_reg(struct irs238xc *sensor, u16 reg, u16 *val);
static s32 irs238xc_write_reg(struct irs238xc *sensor, u16 reg, u16 val);

static const struct i2c_device_id irs238xc_id[] = {
	{"irs238xc_mipi", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, irs238xc_id);

static struct i2c_driver irs238xc_i2c_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = "irs238xc_mipi",
		  },
	.probe  = irs238xc_probe,
	.remove = irs238xc_remove,
	.id_table = irs238xc_id,
};

static const struct irs238xc_datafmt irs238xc_colour_fmts[] = {
	{MEDIA_BUS_FMT_YVYU8_2X8, V4L2_COLORSPACE_JPEG},
	{MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
};

static struct irs238xc *to_irs238xc(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct irs238xc, subdev);
}

static enum irs238xc_frame_rate to_irs238xc_frame_rate(struct v4l2_fract *timeperframe)
{
	enum irs238xc_frame_rate rate;
	u32 tgt_fps;	/* target frames per secound */

	tgt_fps = timeperframe->denominator / timeperframe->numerator;

	if (tgt_fps == 30)
		rate = irs238xc_30_fps;
	else if (tgt_fps == 15)
		rate = irs238xc_15_fps;
	else
		rate = -EINVAL;

	return rate;
}

static inline void irs238xc_power_down(struct irs238xc *sensor, int enable)
{
	// We don't have power down mode yet.
}

static inline void irs238xc_reset(struct irs238xc *sensor)
{
	gpio_set_value_cansleep(sensor->rst_gpio, 0);
	udelay(5000);

	gpio_set_value_cansleep(sensor->rst_gpio, 1);
	msleep(20);
}

static int irs238xc_regulator_enable(struct device *dev)
{
	int ret = 0;
	// We don't have regulators.

	return ret;
}

static s32 irs238xc_write_reg(struct irs238xc *sensor, u16 reg, u16 val)
{
	struct device *dev = &sensor->i2c_client->dev;
	u8 au8Buf[4] = {0};

	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xFF;
	au8Buf[2] = val >> 8;
	au8Buf[3] = val & 0xFF;

	if (i2c_master_send(sensor->i2c_client, au8Buf, 4) < 0) {
		dev_err(dev, "Write reg error: reg=%x, val=%x\n", reg, val);
		return -1;
	}

	return 0;
}

static s32 irs238xc_read_reg(struct irs238xc *sensor, u16 reg, u16 *val)
{
	struct device *dev = &sensor->i2c_client->dev;
	u8 au8RegBuf[2] = {0};
	u8 u8RdBuf[2] = {0};

	au8RegBuf[0] = reg >> 8;
	au8RegBuf[1] = reg & 0xff;

	if (i2c_master_send(sensor->i2c_client, au8RegBuf, 2) != 2) {
		dev_err(dev, "Read reg error: reg=%x\n", reg);
		return -1;
	}

	if (i2c_master_recv(sensor->i2c_client, u8RdBuf, 2) != 2) {
		dev_err(dev, "Read reg error: reg=%x, val0=%x val1=%x\n", reg, u8RdBuf[0], u8RdBuf[1]);
		return -1;
	}
	//printk("b0=%x b1=%x\n", u8RdBuf[0], u8RdBuf[1]);

	*val = (((u16)u8RdBuf[0]) << 8) | u8RdBuf[1];

	return *val;
}

static int irs238xc_set_clk_rate(struct irs238xc *sensor)
{
	int ret;

	/* mclk */
	return 0;
}

/* download irs238xc settings to sensor through i2c */
static int irs238xc_download_firmware(struct irs238xc *sensor,
				struct reg_value *pModeSetting, s32 ArySize)
{
	register u32 Delay_ms = 0;
	register u16 RegAddr = 0;
	register u16 Mask = 0;
	register u16 Val = 0;
	u16 RegVal = 0;
	int i, retval = 0;

	for (i = 0; i < ArySize; ++i, ++pModeSetting) {
		Delay_ms = pModeSetting->Delay_ms;
		RegAddr = pModeSetting->RegAddr;
		Val = pModeSetting->Val;
		Mask = pModeSetting->Mask;

		if (Mask) {
			retval = irs238xc_read_reg(sensor, RegAddr, &RegVal);
			if (retval < 0)
				goto err;

			RegVal &= ~(u16)Mask;
			Val &= Mask;
			Val |= RegVal;
		}

		retval = irs238xc_write_reg(sensor, RegAddr, Val);
		if (retval < 0)
			goto err;

		if (Delay_ms)
			msleep(Delay_ms);
	}
err:
	return retval;
}

static void irs238xc_soft_reset(struct irs238xc *sensor)
{
	// Do something to soft reset?
}

static int irs238xc_config_init(struct irs238xc *sensor)
{
	struct reg_value *pModeSetting = NULL;
	int ArySize = 0, retval = 0;

	/* Configure irs238xc initial parm */
	pModeSetting = irs238xc_init_setting;
	ArySize = ARRAY_SIZE(irs238xc_init_setting);

	retval = irs238xc_download_firmware(sensor, pModeSetting, ArySize);
	if (retval < 0)
		return retval;

	return 0;
}

static void irs238xc_start(struct irs238xc *sensor)
{
	// Do something for start?
}

static int init_device(struct irs238xc *sensor)
{
	int retval;

	irs238xc_soft_reset(sensor);
	retval = irs238xc_config_init(sensor);
	if (retval < 0)
		return retval;

	irs238xc_start(sensor);

	return 0;
}

static void irs238xc_stop(struct irs238xc *sensor)
{
	// Do something for stop?
}

/*!
 * irs238xc_s_power - V4L2 sensor interface handler for VIDIOC_S_POWER ioctl
 * @s: pointer to standard V4L2 device structure
 * @on: indicates power mode (on or off)
 *
 * Turns the power on or off, depending on the value of on and returns the
 * appropriate error code.
 */
static int irs238xc_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct irs238xc *sensor = to_irs238xc(client);

	if (on)
		clk_prepare_enable(sensor->sensor_clk);
	else
		clk_disable_unprepare(sensor->sensor_clk);

	sensor->on = on;
	return 0;
}

/*!
 * irs238xc_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 sub device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int irs238xc_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct irs238xc *sensor = to_irs238xc(client);
	struct v4l2_captureparm *cparm = &a->parm.capture;
	int ret = 0;

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->streamcap.capability;
		cparm->timeperframe = sensor->streamcap.timeperframe;
		cparm->capturemode = sensor->streamcap.capturemode;
		ret = 0;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*!
 * ov5460_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 sub device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int irs238xc_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct irs238xc *sensor = to_irs238xc(client);
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	u32 tgt_fps;	/* target frames per secound */
	enum irs238xc_mode mode = a->parm.capture.capturemode;
	int ret = 0;


	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		/* Check that the new frame rate is allowed. */
		if ((timeperframe->numerator == 0) ||
		    (timeperframe->denominator == 0)) {
			timeperframe->denominator = DEFAULT_FPS;
			timeperframe->numerator = 1;
		}

		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps > MAX_FPS) {
			timeperframe->denominator = MAX_FPS;
			timeperframe->numerator = 1;
		} else if (tgt_fps < MIN_FPS) {
			timeperframe->denominator = MIN_FPS;
			timeperframe->numerator = 1;
		}

		sensor->streamcap.capturemode = mode;
		sensor->streamcap.timeperframe = *timeperframe;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		pr_debug("   type is not V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
					a->type);
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int irs238xc_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct irs238xc *sensor = to_irs238xc(client);

	if (enable)
		irs238xc_start(sensor);
	else
		irs238xc_stop(sensor);

	sensor->on = enable;
	return 0;
}

static struct irs238xc_mode_info *get_max_resolution(enum irs238xc_frame_rate rate)
{
	u32 max_width;
	enum irs238xc_mode mode;
	int i;

	mode = 0;
	
	return NULL;
}

static struct irs238xc_mode_info *match(struct v4l2_mbus_framefmt *fmt,
			enum irs238xc_frame_rate rate)
{
	struct irs238xc_mode_info *info = NULL;
	int i;

	return info;
}

static void try_to_find_resolution(struct irs238xc *sensor,
			struct v4l2_mbus_framefmt *mf)
{
	//?
}

static int irs238xc_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;

	return 0;
}

static int irs238xc_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (format->pad)
		return -EINVAL;

	memset(mf, 0, sizeof(struct v4l2_mbus_framefmt));

	mf->code = MEDIA_BUS_FMT_SBGGR12_1X12;
	mf->colorspace = V4L2_COLORSPACE_RAW;
	mf->width = IRS238XC_DEF_WIDTH;
	mf->height = IRS238XC_DEF_HEIGHT * 9;
	mf->field = V4L2_FIELD_NONE;
	//mf->reserved[1] = find_hs_configure(sensor);

	dev_dbg(&client->dev, "%s code=0x%x, w/h=(%d,%d), colorspace=%d, field=%d\n",
		__func__, mf->code, mf->width, mf->height, mf->colorspace, mf->field);

	return 0;
}

static int irs238xc_enum_code(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= 1)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SBGGR12_1X12;

	return 0;
}

/*!
 * irs238xc_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int irs238xc_enum_framesizes(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
{

	if (fse->index > 0)
		return -EINVAL;

	fse->max_width = IRS238XC_DEF_WIDTH;
			
	fse->min_width = IRS238XC_DEF_WIDTH;

	fse->max_height = IRS238XC_DEF_HEIGHT * 9;
			
	fse->min_height = IRS238XC_DEF_HEIGHT * 9;

	return 0;
}

/*!
 * irs238xc_enum_frameintervals - V4L2 sensor interface handler for
 *			       VIDIOC_ENUM_FRAMEINTERVALS ioctl
 * @s: pointer to standard V4L2 device structure
 * @fival: standard V4L2 VIDIOC_ENUM_FRAMEINTERVALS ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int irs238xc_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_frame_interval_enum *fie)
{
	int i, j, count;

	return -EINVAL;
}

static int irs238xc_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote, u32 flags)
{
	return 0;
}

static struct v4l2_subdev_video_ops irs238xc_subdev_video_ops = {
	.g_parm = irs238xc_g_parm,
	.s_parm = irs238xc_s_parm,
	.s_stream = irs238xc_s_stream,
};

static const struct v4l2_subdev_pad_ops irs238xc_subdev_pad_ops = {
	.enum_frame_size       = irs238xc_enum_framesizes,
	.enum_frame_interval   = irs238xc_enum_frameintervals,
	.enum_mbus_code        = irs238xc_enum_code,
	.set_fmt               = irs238xc_set_fmt,
	.get_fmt               = irs238xc_get_fmt,
};

static struct v4l2_subdev_core_ops irs238xc_subdev_core_ops = {
	.s_power	= irs238xc_s_power,
};

static struct v4l2_subdev_ops irs238xc_subdev_ops = {
	.core	= &irs238xc_subdev_core_ops,
	.video	= &irs238xc_subdev_video_ops,
	.pad	= &irs238xc_subdev_pad_ops,
};

static const struct media_entity_operations irs238xc_sd_media_ops = {
	.link_setup = irs238xc_link_setup,
};

/*!
 * irs238xc I2C probe function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
static int irs238xc_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct pinctrl *pinctrl;
	struct device *dev = &client->dev;
	struct v4l2_subdev *sd;
	int retval, i;
	u16 val16;
	struct irs238xc *sensor;

	sensor = devm_kmalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;
	/* Set initial values for the sensor struct. */
	memset(sensor, 0, sizeof(*sensor));

	/* irs238xc pinctrl */
	pinctrl = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(pinctrl)) {
		dev_err(dev, "setup pinctrl failed\n");
		return PTR_ERR(pinctrl);
	}

	/* request power down pin */
	sensor->pwn_gpio = of_get_named_gpio(dev->of_node, "pwn-gpios", 0);
	if (!gpio_is_valid(sensor->pwn_gpio))
		dev_warn(dev, "No sensor pwdn pin available");
	else {
		retval = devm_gpio_request_one(dev, sensor->pwn_gpio,
				GPIOF_OUT_INIT_HIGH, "irs238xc_mipi_pwdn");
		if (retval < 0) {
			dev_warn(dev, "Failed to set power pin\n");
			dev_warn(dev, "retval=%d\n", retval);
			return retval;
		}
	}

	/* request reset pin */
	sensor->rst_gpio = of_get_named_gpio(dev->of_node, "rst-gpios", 0);
	if (!gpio_is_valid(sensor->rst_gpio))
		dev_warn(dev, "No sensor reset pin available");
	else {
		retval = devm_gpio_request_one(dev, sensor->rst_gpio,
				GPIOF_OUT_INIT_HIGH, "irs238xc_mipi_reset");
		if (retval < 0) {
			dev_warn(dev, "Failed to set reset pin\n");
			return retval;
		}
	}

#if 0
	/* Set initial values for the sensor struct. */
	sensor->sensor_clk = devm_clk_get(dev, "csi_mclk");
	if (IS_ERR(sensor->sensor_clk)) {
		/* assuming clock enabled by default */
		sensor->sensor_clk = NULL;
		dev_err(dev, "clock-frequency missing or invalid\n");
		return PTR_ERR(sensor->sensor_clk);
	}

	retval = of_property_read_u32(dev->of_node, "mclk",
					&(sensor->mclk));
	if (retval) {
		dev_err(dev, "mclk missing or invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "mclk_source",
					(u32 *) &(sensor->mclk_source));
	if (retval) {
		dev_err(dev, "mclk_source missing or invalid\n");
		return retval;
	}
#endif
	retval = of_property_read_u32(dev->of_node, "csi_id",
					&(sensor->csi));
	if (retval) {
		dev_err(dev, "csi id missing or invalid\n");
		return retval;
	}

	/* Set mclk rate before clk on */
	irs238xc_set_clk_rate(sensor);

#if 0
	retval = clk_prepare_enable(sensor->sensor_clk);
	if (retval < 0) {
		dev_err(dev, "%s: enable sensor clk fail\n", __func__);
		return -EINVAL;
	}
#endif

	sensor->io_init = irs238xc_reset;
	sensor->i2c_client = client;

	sensor->pix.pixelformat = V4L2_PIX_FMT_SBGGR12P;
	sensor->pix.width = IRS238XC_DEF_WIDTH;
	sensor->pix.height =  IRS238XC_DEF_HEIGHT;
	sensor->streamcap.capability = V4L2_MODE_HIGHQUALITY |
					   V4L2_CAP_TIMEPERFRAME;
	sensor->streamcap.capturemode = 0;
	sensor->streamcap.timeperframe.denominator = DEFAULT_FPS;
	sensor->streamcap.timeperframe.numerator = 1;

	irs238xc_regulator_enable(&client->dev);

	irs238xc_reset(sensor);

	/*
	for (i=0x9000; i<0xA03F; i++) {
		retval = irs238xc_read_reg(sensor, i,
					&val16);
		printk("reg=%x, val=%x\n", i, val16);
	}*/
	retval = init_device(sensor);
	if (retval < 0) {
		clk_disable_unprepare(sensor->sensor_clk);
		pr_warn("camera irs238xc init fail\n");
		return -ENODEV;
	}

	sd = &sensor->subdev;
	v4l2_i2c_subdev_init(sd, client, &irs238xc_subdev_ops);

	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	sensor->pads[IRS238XC_SENS_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;

	retval = media_entity_pads_init(&sd->entity, IRS238XC_SENS_PADS_NUM,
							sensor->pads);
	sd->entity.ops = &irs238xc_sd_media_ops;
	if (retval < 0)
		return retval;

	retval = v4l2_async_register_subdev(sd);
	if (retval < 0) {
		dev_err(&client->dev,
				"%s--Async register failed, ret=%d\n", __func__, retval);
		media_entity_cleanup(&sd->entity);
	}

	clk_disable_unprepare(sensor->sensor_clk);

	pr_info("%s camera mipi irs238xc, is found\n", __func__);
	return retval;
}

/*!
 * irs238xc I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int irs238xc_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct irs238xc *sensor = to_irs238xc(client);

	v4l2_async_unregister_subdev(sd);

	clk_unprepare(sensor->sensor_clk);

	irs238xc_power_down(sensor, 1);

	if (analog_regulator)
		regulator_disable(analog_regulator);

	if (core_regulator)
		regulator_disable(core_regulator);

	if (io_regulator)
		regulator_disable(io_regulator);

	return 0;
}

module_i2c_driver(irs238xc_i2c_driver);

MODULE_AUTHOR("I4VINE, Inc.");
MODULE_DESCRIPTION("irs238xc depth camera Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("3.0");
MODULE_ALIAS("MIPI CSI");
