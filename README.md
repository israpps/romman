# Romman

Romman is a CLI tool for managing PlayStation 2 ROM files, including creating, extracting, and displaying contents of ROM images. It supports various operations on ROM files and provides a command-line interface for ease of use.

## About ROM Files on PlayStation 2

On the PlayStation 2, ROM files are crucial for the system's operation. These include ROM files and IOPRP images:

* **IOPRP Images**: These are I/O Processor ROM images that contain modules and drivers necessary for the PlayStation 2's I/O Processor (IOP) to function. The IOP handles input/output operations, including communication with peripherals and other hardware components.

Romman can be used to manage these ROM files, making it easier to create backups, modify configurations, and explore the contents of these essential system files.

## Features

* Display contents of ROM images in a colored table. The field list can be found below in the Configuration File Format section. This command will also show how much empty space exists inside the image.
* Extract files from ROM images. Modules will be extracted into a subfolder in the same directory where the ROM is placed. All modules that are ROMs themselves will be recursively extracted. File modification dates will be set to the date from ROM metadata. It will also extract a configuration file with all metadata from the ROM (containing info about each MODULE). File size and CRC32 will be present in the output file for each module. All non-empty gaps in the ROM will be extracted (useful for checking leftover data). Overdumped data after the last module until the dump end will also be extracted.
* Create ROM images from configuration files.
* Color-coded output for better readability (Windows 10+ and Unix-like systems).

## Usage

### Command-Line Options

* `[-l] <rom>`: Display contents of a ROM image (-l can be omitted).
* `-x <rom>`: Extract files from a ROM image into a folder named `ext_<romname>`.
* `-g <conf> <folder> <rom>`: Generate a ROM image from a configuration file.
* `-a <rom>`: Adds files to a ROM image.
* `[-h]`: Display help message (-h can be omitted).

### Examples

#### Generate a ROM Image

```sh
romman -g config.csv rom.bin folder/
```

#### Extract Files from a ROM Image

```sh
romman -x rom.bin
```

#### Display Contents of a ROM Image

```sh
romman rom.bin
romman -l rom.bin
```

## Configuration File Format

The configuration file should be a CSV file with the following columns:

* **Name**: Name of the module. 10 characters max, capital letters, first module name must start with RESET, other module names cannot start with RESET.
* **FixedOffset**: Offset at which the file should be placed (optional).
* **Date**: Date of the file (optional) in format dd/mm/yyyy. If "-" then the date attribute will be forcefully removed.
* **Version**: Version of the file (optional).
* **Comment**: Comment for the file (optional, cannot contain commas).

Note: It is the user's responsibility to put the modules in the correct order. Ensure that modules between FixedOffset do not overlap. The app will not optimize the image; this should be done by the user.

### Example:

```csv
Name,FixedOffset,Date,Version,Comment
FILE1,0x1000,22/02/2023,1.0,First file
FILE2,,-,1.1,Second file
FILE3,0x2000,30/12/2004,2.0,Third file with different date format
FILE4,,,,Empty fields example
```

## TODO

* Add support for DECKARD module, as it uses big endian instead of little endian. Endianess can be detected by first module entry: RESET - is little endian, RESETB - big endian
* Add information about subroms into the main configuration file. So you will not need to recreate each subrom, but can just call -g on the main script.
* Check if it is possible to detect where erom starts in DVD images.
* Pack/unpack compressed modules, add info about the pack.
* Automatically check and maybe fix if the configuration file contains modules with FixedOffset in incorrect order.
* Check if it is possible to use overflow in ROMDIR to mimic symlink behavior. ROMDIR contains entries with module size in bytes but doesn't contain offsets. The offset is calculated by summarizing all previous offsets and aligning each step with 16 bytes. The idea (credits to @uyjulian) is to create a dummy entry with a size big enough to overflow an unsigned int. This way, the actual offset of the next module will be set before the current module. Then, we add a symlinked module entry without adding its data. Finally, we add one more dummy entry into ROMDIR to get back to the first address. This can be useful if we want to add same module with different name.

Source code is partially based on [ROMIMG](https://sites.google.com/view/ysai187/home/projects/romimg) from @sp193, initial C++ rewrite done by @israpps, critical error fix, improving commands, inventing new -g command done by @akuhak.
