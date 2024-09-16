# ROM Manager script language
to facilitate usage of this program in complex/automated situations. a rather simple/rudimentary scripting "language" was made

the design is simple and will allow you to (re)build all kinds of ROM images

the supported 'instructions' are:
- `CreateImage("filename")`: tells the program the filename for the new image
- `AddFile("filename")`: add a file to the new image
- `AddFixedFile("filename", offset)`: add a file that must exist at a fixed offset on the image. offset parameter must be a positive number, either decimal or hexadeciam (with the appropiate `0x` prefix)
- `WriteImage()`: creates the image
- `SortBySize()`: organize the declared files up to this line by their size (larger files will be written first). this helps the program to optimize size if we are dealing with fixed offset size. __the usage of this instruction when building IOPRP images is TOTALLY discouraged__!!
