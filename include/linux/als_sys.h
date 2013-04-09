/*
 * als.h
 *
 * Copyright (C) 2009 Intel Corp
 * Copyright (C) 2009 Zhang Rui <rui.zhang@xxxxxxxxx>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

#ifndef __ALS_SYS_H__
#define __ALS_SYS_H__

#include <linux/device.h>

struct als_device;

struct als_device_ops {
 int (*get_illuminance) (struct als_device *, int *);
 int (*update_mappings) (struct als_device *);
};

struct als_mapping {
 int illuminance;
 int adjustment;
};

struct als_device {
 struct device device;
 struct als_device_ops *ops;
 int count;
 struct als_mapping *mappings;
};

struct als_device *als_device_register(struct als_device_ops *, char *, void *);
void als_device_unregister(struct als_device *);

#endif /* __ALS_SYS_H__ */
