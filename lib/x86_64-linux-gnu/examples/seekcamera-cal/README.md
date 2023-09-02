# seekcamera-cal

The seekcamera-cal application uploads calibration data to Seek cameras using the Seek SDK.

## Building

The Seek SDK must be pre-installed on the system in order to compile and run the application.

### Make

```bash
$ make
```

## Usage

To run the application, call the application from the command-line.

```txt
$ seekcamera-cal -h
Allowed options:
    -m : Discovery mode. Valid options: usb, spi, all (default: usb)
       : Required - No
    -p : Path to top-level directory containing the calibration data
       : Required - No
    -f : Force pairing, even if the device is already paired (default: not specified)
       : Required - No
    -h : Displays this message
       : Required - No
```

### Expected output

Output from a successfull run on a Raspberry Pi 4 is shown below.

```txt
root@raspberrypi4-64:~# seekcamera-cal -m usb
seekcamera-cal starting
settings
        1) mode (-m): usb
        2) path (-p): (null)
camera connect: DE0D2DF11A26
calibration data pairing 100 percent complete: DE0D2DF11A26
calibration data pairing finished successfully: DE0D2DF11A26
```
### Mode (-m)

The discovery mode argument is optional; is it specified via the `-m` flag. It should
either be "usb", "spi", or "all". The default value is "usb". Discovery mode refers to the
protocol interface used to automatically discover the connected Seek devices.

Example usage:

```txt
# For both spi and usb
$ seekcamera-cal -p top-level/ -m all

# For spi only
$ seekcamera-cal -p top-level/ -m spi

# For usb only
$ seekcamera-cal -p top-level/
$ seekcamera-cal -p top-level/ -m usb
```

### Path (-p)

The path argument is optional; it is specified via the `-p` flag.
If specified, it should be an absolute path to the top-level directory containing the camera calibration data.
If unspecified, the camera calibration data will be pulled from sensor flash -- if supported by the device.

The top-level directory can be any directory containing any number of subdirectories whose
names correspond exactly to the unique camera chip identifier (CID). In the illustration
below, `[CID]-i` refers to the `i`th directory whose contents are the calibration data files.

```txt
top-level/
	[CID]-0/
		...
	[CID]-1/
		...
	[CID]-2/
		...
	...
	[CID]-N/
```

Example usage:

```txt
$ seekcamera-cal -p top-level/
```

### Force (-f)

The force pairing argument is optional; it is specified via the `-f` flag.
If specified, pairing will be forced even if the camera is already paired.
If unspecified, the camera will only be paired if it is currently unpaired.

Example usage:

```txt
$ seekcamera-cal -f
```

### Help (-h)

The help argument is optional; it is specified via the `-h` flag.

Example usage:

```txt
$ seekcamera-cal -h
```

## Limitations

The following limitations apply. There are plans to fix both in future SDK releases.

1. Only one physical device can used at a time.
2. Device disconnects are not always detected due to there not being an active capture session.
3. Use of SPI devices is only supported on Linux.
