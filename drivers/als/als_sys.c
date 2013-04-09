/*
 * als_sys.c - Ambient Light Sensor Sysfs support.
 *
 * Copyright (C) 2009 Intel Corp
 * Copyright (C) 2009 Zhang Rui <rui.zhang@xxxxxxxxx>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include "../../include/linux/als_sys.h"

MODULE_AUTHOR("Zhang Rui");
MODULE_DESCRIPTION("Ambient Light Sensor sysfs support");
MODULE_LICENSE("GPL");

static int als_get_adjustment(struct als_device *, int, int *);

/* sys I/F for Ambient Light Sensor */

#define to_als_device(dev) container_of(dev, struct als_device, device)

static ssize_t
illuminance_show(struct device *dev, struct device_attribute *attr, char *buf)
{
 struct als_device *als = to_als_device(dev);
 int illuminance;
 int result;

 result = als->ops->get_illuminance(als, &illuminance);
 if (result)
 return result;

 if (!illuminance)
 return sprintf(buf, "0\n");
 else if (illuminance == -1)
 return sprintf(buf, "-1\n");
 else if (illuminance < -1)
 return -ERANGE;
 else
 return sprintf(buf, "%d\n", illuminance);
}

static ssize_t
adjustment_show(struct device *dev, struct device_attribute *attr, char *buf)
{
 struct als_device *als = to_als_device(dev);
 int illuminance, adjustment;
 int result;

 result = als->ops->get_illuminance(als, &illuminance);
 if (result)
 return result;

 if (illuminance < 0 && illuminance != -1)
 return sprintf(buf, "Current illuminance invalid\n");

 result = als_get_adjustment(als, illuminance, &adjustment);
 if (result)
 return result;

 return sprintf(buf, "%d%%\n", adjustment);
}

static struct device_attribute als_attrs[] = {
 __ATTR(illuminance, 0444, illuminance_show, NULL),
 __ATTR(display_adjustment, 0444, adjustment_show, NULL),
 __ATTR_NULL,
};

static void als_release(struct device *dev)
{
 struct als_device *als = to_als_device(dev);

 kfree(als);
}

static struct class als_class = {
 .name = "als",
 .dev_release = als_release,
 .dev_attrs = als_attrs,
};

static int als_get_adjustment(struct als_device *als, int illuminance,
 int *adjustment)
{
 int lux_high, lux_low, adj_high, adj_low;
 int i;

 if (!als->mappings)
 return -EINVAL;

 if (illuminance == -1
 || illuminance > als->mappings[als->count - 1].illuminance)
 illuminance = als->mappings[als->count - 1].illuminance;
 else if (illuminance < als->mappings[0].illuminance)
 illuminance = als->mappings[0].illuminance;

 for (i = 0; i < als->count; i++) {
 if (illuminance == als->mappings[i].illuminance) {
 *adjustment = als->mappings[i].adjustment;
 return 0;
 }

 if (illuminance > als->mappings[i].illuminance)
 continue;

 lux_high = als->mappings[i].illuminance;
 lux_low = als->mappings[i - 1].illuminance;
 adj_high = als->mappings[i].adjustment;
 adj_low = als->mappings[i - 1].adjustment;

 *adjustment =
 ((adj_high - adj_low) * (illuminance - lux_low)) /
 (lux_high - lux_low) + adj_low;
 return 0;
 }
 return -EINVAL;
}

/**
 * als_device_register - register a new Ambient Light Sensor class device
 * @ops: standard ALS devices callbacks.
 * @devdata: device private data.
 */
struct als_device *als_device_register(struct als_device_ops *ops,
 char *name, void *devdata)
{
 struct als_device *als;
 int result = -ENOMEM;

 if (!ops || !ops->get_illuminance || !name)
 return ERR_PTR(-EINVAL);

 als = kzalloc(sizeof(struct als_device), GFP_KERNEL);
 if (!als)
 return ERR_PTR(-ENOMEM);

 als->ops = ops;
 als->device.class = &als_class;
 dev_set_name(&als->device, name);
 dev_set_drvdata(&als->device, devdata);
 result = device_register(&als->device);
 if (result)
 goto err;

 if (ops->update_mappings) {
 result = ops->update_mappings(als);
 if (result) {
 device_unregister(&als->device);
 goto err;
 }
 }
 return als;

err:
 kfree(als);
 return ERR_PTR(result);
}
EXPORT_SYMBOL(als_device_register);

/**
 * als_device_unregister - removes the registered ALS device
 * @als: the ALS device to remove.
 */
void als_device_unregister(struct als_device *als)
{
 device_unregister(&als->device);
}
EXPORT_SYMBOL(als_device_unregister);

static int __init als_init(void)
{
 return class_register(&als_class);
}

static void __exit als_exit(void)
{
 class_unregister(&als_class);
}

subsys_initcall(als_init);
module_exit(als_exit);
