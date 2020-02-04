fmcw radar a simple demo
========================

Authors
--------
Robert Olsson roolss@kth.se and robert@radio-sensors.com

Example
-------
	fmcw_radar /dev/ttyUSB0 
	D: 214   202_10 315_44 592_10 706_10 
	D: 214   202_12 315_44 592_10 706_10 
	D: 214   202_13 315_44 592_10 706_10 
	D: 214   202_14 315_44 592_10 706_10 

	Object with strongest refelection @ 214cm
	2:nd object @ 202cm with reflection index 10-14
	3:rd obejct @ 315cm with reflection index 44
	etc

HW support
----------
DF Robot 24GHz RADAR.

https://www.dfrobot.com/product-1882.html


Usage
-----
	fmcw_radar 

	fmcw_radar version 1.0 2020-02-03

	fmcw_radar parses fmcw radar dev on serial port
	fmcw_radar [-BAUDRATE] [-d] [-thresh level] device command
	 Valid baudrates 4800, 9600 (Default), 19200, 38400, 57600, 115200 bps
	  -thresh level is noise filer 1-44
	   fmcw_radar can handle devtag

	   Example 1: Simple
	     fmcw_radar  /dev/ttyUSB0

	     Example 2: Debug
	       fmcw_radar -d /dev/ttyUSB0

	     Example 3: Sensitive
	         fmcw_radar -thresh 4 /dev/ttyUSB0


Radar sentence in hex
---------------------
FF FF FF 00 D3 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 0C 01 01 01 01 01 01 01 01 2C 01 01 01 01 01 01 0A 01 01 01 01 01 01 01 01 01 01 01 01 01 0A 01 01 01 01 01 01 01 01 01 0D 01 01 01 01 01 01 01 01 01 01 01 01 01 03 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 00 00 00

Radar config
------------
Pin 6 connected to GND, the output is distance + spectral data.

BAUD rate 57600 bps

