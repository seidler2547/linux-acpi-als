linux-acpi-als
==============

Linux kernel module for ACPI ALS (ambient light sensor) aka hid ACPI0008.

Sources:
http://lkml.indiana.edu/hypermail/linux/kernel/0909.0/00036.html
http://lkml.indiana.edu/hypermail/linux/kernel/0909.0/00037.html

ALS sysfs class provides a standard sysfs interface for Ambient Light Sensor devices.

ACPI spec defines ACPI Ambient Light Sensor device (hid ACPI0008),
which provides a standard interface by which the OS may query properties
of the ambient light environment the system is currently operating in,
as well as the ability to detect meaningful changes in these values when
the environment changes.
