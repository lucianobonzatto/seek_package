# seekcamera-sdl

The seekcamera-sdl application renders frames from Seek cameras using the Seek SDK.
It supports multiple cameras.

# Building

The Seek SDK must be pre-installed on the system in order to compile and run the application.

## Make
```bash
$ make
```

## Usage

To run the application, call the application from the command-line.

```txt
$ seekcamera-sdl -h
Allowed options:
	-m,--mode                 : Discovery mode. Valid options: usb, spi, all (default: usb)
	                          : Required - No
	-d,--disable-auto-pairing : Disables auto pairing
	                          : Required - No (default: not specified)
	-h,--help                 : Displays this message
	                          : Required - No
```

### Expected output

Output from a successfull run is shown below.

```txt
$ ./seekcamera-sdl
seekcamera-sdl starting
settings:
        1) mode:      usb
        2) auto pair: on
user controls:
	1) mouse click: next color palette
	2) c:           next color palette
	3) a:           next agc mode
	4) s:           next shutter mode
	5) t:           trigger shutter (product dependent)
	6) p:           print frame metadata from the header to the console
	7) r:           restart capture session
	8) h:           display this message
	9) q:           quit
```

### Mode (-m, --mode)

The discovery mode argument is optional; is it specified via the `-m` flag or `--mode` flag.
It should either be "usb", "spi", or "all".
The default value is "all".
Discovery mode refers to the protocol interface used to automatically discover the connected Seek devices.

Example usage:

```txt
# For both spi and usb
$ seekcamera-sdl -m all
$ seekcamera-sdl --mode all

# For spi only
$ seekcamera-sdl -m spi
$ seekcamera-sdl --mode spi

# For usb only
$ seekcamera-sdl
$ seekcamera-sdl -m usb
$ seekcamera-sdl --mode usb
```

### Disable auto pairing (-d,--disable-auto-pair)

The auto pairing argument is optional; it is specified via the `-d` or `--disable-auto-pair` flag.
The default value is not specified.
If auto pairing is enabled, then the sample application will attempt to automatically pair the device.
Pairing refers to the process by which the sensor is associated with the host and the embedded processor.

Example usage:

```txt
# For auto pairing enabled
$ seekcamera-sdl

# For disabling auto pairing
$ seekcamera-sdl -d
$ seekcamera-sdl --disable-auto-pairing
```

### Help (-h, --help)

The help argument is optional; it is specified via the `-h` flag or `--help` flag.

Example usage:

```txt
$ seekcamera-sdl -h
$ seekcamera-sdl --help
```

## User controls

### Color palette
The color palette can be switched by either by pressing the mouse keys down in any area of the graphical user interface
or by pressing the `c` key on the active window.

### AGC mode
The AGC mode can be switched by pressing the `a` key on the active window.

### Shutter mode
The shutter mode can be switched by pressing the `s` key on the active window.
The ability to set the shutter mode is device dependent.

### Trigger shutter
The shutter can be triggered by pressing the `t` key on the active window.
The ability to trigger the shutter is device dependent.

### Help
The user control information can be printed to the console by pressing the `h key on the active window.

### Quit
The application can be exited by either pressing the `q` key on any active window or by pressing `Ctrl+C`
in the console. The application will also exit if all devices are disconnected by the user.
