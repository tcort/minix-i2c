Frame Buffer Driver
===================

Overview
--------

This is the driver for the frame buffer. Currently it only supports the
DM37XX (BeagleBoard-xM).

Limitations
-----------

AM335X - the initial port is targeted at LCD capes. The TDA19988 driver needs
additional improvements to be able to support HDMI output on the BeagleBone
Black. fb will need some slight changes for that as well; for example, with
the TDA19988 it doesn't need to use GPIO3_19 to enable the LCD.

Testing the Code
----------------

Starting up an instance (should be started at boot automatically):

service up /usr/sbin/fb -dev /dev/fb0 -args edid.0=cat24c256.3.50

The arguments take the following form:

	edid.X=L where X is the frame buffer device (usually 0) and L is
	the service label of the service to perform the EDID reading. In
	the example above, it's the EEPROM with slave address 0x50 on
	the 3rd I2C bus. If you want to use the defaults and skip EDID
	reading, you may omit the arguments.

