# VirtualScreenGTK
A virtual screen (in a window) you can feed from a FIFO for GTK3

Developing this project https://github.com/abbati-simone/Yoshis-Nightmare-Altera and not having (at the time) a CRT/LCD Monitor with VGA input, I developed this little program to show an image starting from a "bitstream" (RGB data sequence).

**Requires GTK3**

The program can be compiled like so:
```bash
$ cd src
$ ./configure
$ make
$ sudo make install #Optional, executable is inside src subdirecotry. Use su -c 'make install' if your system does not have sudo
```

And tested like so:

*Shell 1*
```bash
$ mkfifo ~/fifo #Create a FIFO in home directory with name fifo
$ virtualScreenGtk -f ~/fifo -x 640 -y 480 -p 25 -m RGB_4_4_4
```

*Shell 2*
```bash
$ cat /dev/urandom > ~/fifo
```


You should see something like:

![VirtualScreenGTK random data test](https://github.com/abbati-simone/VirtualScreenGTK/blob/master/doc/images/Screenshot_1.png "VirtualScreenGTK random data test")

The refresh button on the top is used to force window update with the data acquired. This is not useful when feeding data from /dev/urandom device, because it is fast.
For example the following screenshot show partial frame data generated from the Icarus Verilog Compiler (see linked project above).

![VirtualScreenGTK partial frame](https://github.com/abbati-simone/VirtualScreenGTK/blob/master/doc/images/Screenshot_2.png "VirtualScreenGTK partial frame")

A trace line is visible on the screenshot above and is useful when a manual refresh is done and a previous frame is already on background.
The following screenshot shows exactly this:

![VirtualScreenGTK partial frame 2](https://github.com/abbati-simone/VirtualScreenGTK/blob/master/doc/images/Screenshot_3.png "VirtualScreenGTK partial frame 2")

You can see the refresh line on the top.

The following one is a frame screenshot captured via the specific command line option:

![VirtualScreenGTK screenshot via option](https://github.com/abbati-simone/VirtualScreenGTK/blob/master/doc/images/Screenshot_4.jpg "VirtualScreenGTK screenshot via option")

Parameters
----------
 * The FIFO is read from is specified via *-f* parameter
 * Window/Image Width is specified via *-x* parameter
 * Window/Image Height is specified via *-y* parameter
 * Color data sequence and number of bits per color is specified via *-m* parameter (RGB_4_4_4, BGR_4_4_4, GBR_8_8_8, and so on)
 * Maximum FPS is specified via *-p* parameter
 * Trace line is enabled via *-t* parameter
 * Screenshot capturing and path is specified via *-s* parameter (do not specify to disable screenshot capturing)
 * Help show is enabled via *-h* parameter

Problems and future development
-------------------------------
I read FIFO in a non-blocking way. A better implementation should use of *poll*/*select*.
I did tested only a few color sequences (*-m* parameter) without any problem, further testing is needed to say it works well.

