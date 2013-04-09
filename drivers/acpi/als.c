/*
 *  als.c - ACPI Ambient Light Sensor Driver
 *
 *  Copyright (C) 2009 Intel Corp
 *  Copyright (C) 2009 Zhang Rui <rui.zhang@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>
#include "../../include/linux/als_sys.h"

#define PREFIX "ACPI: "

#define ACPI_ALS_CLASS			"als"
#define ACPI_ALS_DEVICE_NAME		"Ambient Light Sensor"
#define ACPI_ALS_NOTIFY_ILLUMINANCE	0x80
#define ACPI_ALS_NOTIFY_COLOR_TEMP	0x81
#define ACPI_ALS_NOTIFY_RESPONSE	0x82

#define _COMPONENT		ACPI_ALS_COMPONENT
ACPI_MODULE_NAME("als");

MODULE_AUTHOR("Zhang Rui");
MODULE_DESCRIPTION("ACPI Ambient Light Sensor Driver");
MODULE_LICENSE("GPL");

static int acpi_als_add(struct acpi_device *device);
static int acpi_als_remove(struct acpi_device *device, int type);
static void acpi_als_notify(struct acpi_device *device, u32 event);

static const struct acpi_device_id als_device_ids[] = {
	{"ACPI0008", 0},
	{"", 0},
};

MODULE_DEVICE_TABLE(acpi, als_device_ids);

static struct acpi_driver acpi_als_driver = {
	.name = "als",
	.class = ACPI_ALS_CLASS,
	.ids = als_device_ids,
	.ops = {
		.add = acpi_als_add,
		.remove = acpi_als_remove,
		.notify = acpi_als_notify,
		},
};

struct acpi_als {
	struct acpi_device *device;
	struct als_device *als_sys;
	int illuminance;
	int chromaticity;
	int temperature;
	int polling;
	int count;
	struct als_mapping *mappings;
};

#define ALS_INVALID_VALUE_LOW		0
#define ALS_INVALID_VALUE_HIGH		-1

/* --------------------------------------------------------------------------
		Ambient Light Sensor device Management
   -------------------------------------------------------------------------- */

/*
 * acpi_als_get_illuminance - get the current ambient light illuminance
 */
static int acpi_als_get_illuminance(struct acpi_als *als)
{
	acpi_status status;
	unsigned long long illuminance;

	status =
	    acpi_evaluate_integer(als->device->handle, "_ALI", NULL,
				  &illuminance);
	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status,
				"Error reading ALS illuminance"));
		als->illuminance = ALS_INVALID_VALUE_LOW;
		return -ENODEV;
	}
	als->illuminance = illuminance;
	return 0;
}

/*
 * acpi_als_get_color_chromaticity - get the ambient light color chromaticity
 */
static int acpi_als_get_color_chromaticity(struct acpi_als *als)
{
	acpi_status status;
	unsigned long long chromaticity;

	status =
	    acpi_evaluate_integer(als->device->handle, "_ALC", NULL,
				  &chromaticity);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "_ALC not available\n"));
		return -ENODEV;
	}
	als->chromaticity = chromaticity;
	return 0;
}

/*
 * acpi_als_get_color_temperature - get the ambient light color temperature
 */
static int acpi_als_get_color_temperature(struct acpi_als *als)
{
	acpi_status status;
	unsigned long long temperature;

	status =
	    acpi_evaluate_integer(als->device->handle, "_ALT", NULL,
				  &temperature);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "_ALT not available\n"));
		return -ENODEV;
	}
	als->temperature = temperature;
	return 0;
}

/*
 * acpi_als_get_mappings - get the ALS illuminance mappings
 *
 * Return a package of ALS illuminance to display adjustment mappings
 * that can be used by OS to calibrate its ambient light policy
 * for a given sensor configuration.
 */
static int acpi_als_get_mappings(struct acpi_als *als)
{
	int result = 0;
	acpi_status status;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *alr;
	int i, j;

	/* Free the old mappings */
	kfree(als->mappings);
	als->mappings = NULL;

	status =
	    acpi_evaluate_object(als->device->handle, "_ALR", NULL, &buffer);
	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "Error reading ALS mappings"));
		return -ENODEV;
	}

	alr = buffer.pointer;
	if (!alr || (alr->type != ACPI_TYPE_PACKAGE)) {
		printk(KERN_ERR PREFIX "Invalid _ALR data\n");
		result = -EFAULT;
		goto end;
	}

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Found %d illuminance mappings\n",
			  alr->package.count));

	als->count = alr->package.count;

	if (!als->count)
		return 0;

	als->mappings =
	    kmalloc(sizeof(struct als_mapping) * als->count, GFP_KERNEL);
	if (!als->mappings) {
		result = -ENOMEM;
		goto end;
	}

	for (i = 0, j = 0; i < als->count; i++) {
		struct als_mapping *mapping = &(als->mappings[j]);
		union acpi_object *element = &(alr->package.elements[i]);

		if (element->type != ACPI_TYPE_PACKAGE)
			continue;

		if (element->package.count != 2)
			continue;

		if (element->package.elements[0].type != ACPI_TYPE_INTEGER ||
		    element->package.elements[1].type != ACPI_TYPE_INTEGER)
			continue;

		mapping->adjustment =
		    element->package.elements[0].integer.value;
		mapping->illuminance =
		    element->package.elements[1].integer.value;
		j++;

		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Mapping [%d]: "
				  "adjuestment [%d] illuminance[%d]\n",
				  i, mapping->adjustment,
				  mapping->illuminance));
	}

end:
	kfree(buffer.pointer);
	return result;
}

/*
 * acpi_als_get_polling - get a recommended polling frequency
 * 			  for the Ambient Light Sensor device
 */
static int acpi_als_get_polling(struct acpi_als *als)
{
	acpi_status status;
	unsigned long long polling;

	status =
	    acpi_evaluate_integer(als->device->handle, "_ALP", NULL, &polling);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "_ALP not available\n"));
		return -ENODEV;
	}

	als->polling = polling;
	return 0;
}

/*------------------------------------------------------------------------
 *                                ALS sysfs I/F
 *------------------------------------------------------------------------ */

static int get_illuminance(struct als_device *als_sys, int *illuminance)
{
	struct acpi_als *als = dev_get_drvdata(&als_sys->device);
	int result;

	result = acpi_als_get_illuminance(als);
	if (!result)
		*illuminance = als->illuminance;
	return result;
}

static int update_mappings(struct als_device *als_sys)
{
	struct acpi_als *als = dev_get_drvdata(&als_sys->device);
	int result;

	result = acpi_als_get_mappings(als);
	if (result)
		return result;

	als_sys->mappings = als->mappings;
	als_sys->count = als->count;

	return 0;
}

struct als_device_ops acpi_als_ops = {
	.get_illuminance = get_illuminance,
	.update_mappings = update_mappings,
};

/* --------------------------------------------------------------------------
				 Driver Model
   -------------------------------------------------------------------------- */

static void acpi_als_notify(struct acpi_device *device, u32 event)
{
	struct acpi_als *als = acpi_driver_data(device);

	if (!als)
		return;

	switch (event) {
	case ACPI_ALS_NOTIFY_ILLUMINANCE:
		acpi_als_get_illuminance(als);
		break;
	case ACPI_ALS_NOTIFY_COLOR_TEMP:
		acpi_als_get_color_temperature(als);
		acpi_als_get_color_chromaticity(als);
		break;
	case ACPI_ALS_NOTIFY_RESPONSE:
		update_mappings(als->als_sys);
		break;
	default:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "Unsupported event [0x%x]\n", event));
	}
	acpi_bus_generate_proc_event(device, event, (u32) als->illuminance);
	acpi_bus_generate_netlink_event(device->pnp.device_class,
					dev_name(&device->dev), event,
					(u32) als->illuminance);
}

static int acpi_als_add(struct acpi_device *device)
{
	int result;
	static int als_id;
	char name[10];
	struct acpi_als *als;

	if (unlikely(als_id >= 10)) {
		printk(KERN_WARNING PREFIX "Too many ALS device found\n");
		return -ENODEV;
	}

	als = kzalloc(sizeof(struct acpi_als), GFP_KERNEL);
	if (!als)
		return -ENOMEM;

	als->device = device;
	strcpy(acpi_device_name(device), ACPI_ALS_DEVICE_NAME);
	strcpy(acpi_device_class(device), ACPI_ALS_CLASS);
	device->driver_data = als;

	result = acpi_als_get_illuminance(als);
	if (result)
		goto err;

	result = acpi_als_get_mappings(als);
	if (result)
		goto err;

	acpi_als_get_color_temperature(als);
	acpi_als_get_color_chromaticity(als);
	acpi_als_get_polling(als);

	sprintf(name, "acpi_als%d", als_id++);
	als->als_sys = als_device_register(&acpi_als_ops, name, als);
	if (IS_ERR(als->als_sys)) {
		result = PTR_ERR(als->als_sys);
		als->als_sys = NULL;
		goto err;
	}

	result = sysfs_create_link(&als->als_sys->device.kobj,
				&device->dev.kobj, "device");
	if (result) {
		printk(KERN_ERR PREFIX "Create sysfs link\n");
		goto err;
	}

	return 0;

err:
	if (als->als_sys)
		als_device_unregister(als->als_sys);
	kfree(als->mappings);
	kfree(als);
	return result;
}

static int acpi_als_remove(struct acpi_device *device, int type)
{
	struct acpi_als *als = acpi_driver_data(device);

	sysfs_remove_link(&als->als_sys->device.kobj, "device");
	als_device_unregister(als->als_sys);
	kfree(als->mappings);
	kfree(als);
	return 0;
}

static int __init acpi_als_init(void)
{
	return acpi_bus_register_driver(&acpi_als_driver);
}

static void __exit acpi_als_exit(void)
{
	acpi_bus_unregister_driver(&acpi_als_driver);
}

module_init(acpi_als_init);
module_exit(acpi_als_exit);
