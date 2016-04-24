
# Raspberry Pi PPS Client

<center><img src="/images/RPi_with_GPS.jpg" alt="Raspberry Pi with GPS" style="width: 400px;"/></center>

The pps-client daemon is a fast, high accuracy Pulse-Per-Second system clock synchronizer for Raspberry Pi 2 that synchronizes the Raspberry Pi 2 system clock to a GPS time clock. 

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->

- [Summary](#summary)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
  - [The Raspian OS](#the-raspian-os)
  - [The NTP daemon](#the-ntp-daemon)
  - [The chkconfig system services manager](#the-chkconfig-system-services-manager)
- [Installing](#installing)
- [Uninstalling](#uninstalling)
- [Reinstalling](#reinstalling)
- [Building from Source](#building-from-source)
  - [Building on the RPi](#building-on-the-rpi)
  - [Building on a Linux Workstation](#building-on-a-linux-workstation)
- [Running pps-client](#running-pps-client)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# Summary
---
The pps-client daemon provides synchronization precision of 1 microsecond and a timekeeping accuracy of 5 microseconds by design. The figures below illustrate this clearly. This data was captured over a 24 hour period from a Raspberry Pi running Raspian with a [3.18.9-rt kernel](http://docs.emlid.com/Downloads/Real-time-Linux-RPi2/). Figure 1 is a distribution of time adjustments made by the pps-client controller to the system clock (dark blue) superimposed on raw system time measurements (light blue) made over the same period. 

<center><img src="/images/offset-distrib.png" alt="Jitter and Corrections Distrib" style="width: 608px;"/></center>

Figure 2 shows the system clock frequency set by the controller which held the Allan deviation of the frequency to less than 20 parts per billion in each minute over the same period. This corresponds to a time drift of less than 20 nanoseconds per second (A clock offset of 1 ppm corresponds to a time drift of 1 microsec per sec.) The controller adjusted the frequency offset against temperature changes to the value that kept the system clock synchronized to the PPS within a fraction of a microsecond.

<center><img src="/images/frequency-vars.png" alt="Frequency Vars over 24 hours" style="width: 685px;"/></center>

The combination of time slew adjustments never needing to exceed 1 microsecond each second and time drift never exceeding 20 nanoseconds each second certifies a timekeeping precision of 1 microsecond over this 24 hour period. The claimed absolute accuracy limit of 5 microseconds is because of uncertainties in determining the precise value of the PPS interrupt delay and because allowance must be made for occasional time adjustments that could slightly exceed 1 microsecond (but didn't here). This is discussed in detail in the docs. In any case, the performance shown here can be considered to be worst case if Linux 4.1.y kernels are used because these have even better timekeeping performance than the 3.18.9-rt kernel from which this data was collected.

# Hardware Requirements
---

1. A Raspberry Pi 2 Model B or later.

2. A GPS module that provides a PPS output. Development was done with the [Adafruit Ultimate GPS module](http://www.adafruit.com/product/746). Others providing compatible logic levels will also work.

3. A wired connection from a PPS source with 3.3 Volt logic outputs to GPIO 4 (pin 7) on the RPi header.

4. A wired connection from GPIO_17 (pin 11) to GPIO_22 (pin 15) to support self calibration (Note the yellow jumper in the photo above).
 
# Software Requirements
---
## The Raspian OS

Version 3.18.9-rt5 and later versions of Linux kernel 4.1.y are supported. Initial development was done with the [EMLID Navio kernel](http://docs.emlid.com/Downloads/Real-time-Linux-RPi2/) because it had real-time patches that were necessary for Linux 3.18.x. The stock 4.1.y kernel now provides equal or better timekeeping performance.

The Raspian OS is required only because the RPi file locations required by the installer (and test files) are hard coded into the installer. If there is enough interest in using alternative OS's, these install locations could be determined by the pps-client config file, `\etc\pps-client.conf`.

## The NTP daemon

NTP is provided out the box on Raspian. NTP sets the whole seconds of the initial date and time and SNTP maintains the date and time through DST and leap second changes.

## The chkconfig system services manager 
 
`$ sudo apt-get install chkconfig`

This is necessary if you want to install pps-client as a system service.

# Installing
---

The pps-client has several versions of the installer. Copy the appropriate one to the RPi and run it from a terminal:
```
$ sudo ./pps-client-4.1.20-v7+
```
The pps-client version must match the version of the Linux kernel on the RPi. The kernel version can be determined by running "`uname -r`" on an RPi terminal. Version matching is necessary because pps-client contains a kernel driver and all kernel drivers are compiled against a specific version of the Linux kernel and can only run on that kernel version.

A few different Linux kernels are currently supported and more will be. This is not the best solution because it means that pps-client has to be re-installed every time Raspian updates the kernel version. The hope is that if there is enough interest in this project, the driver will be accepted into mainline in the upstream kernel.

# Uninstalling
---

Uninstall pps-client on the RPi with:
```
$ sudo pps-client-remove
```
# Reinstalling
---

If you want to keep your current pps-client configuration file rename it before you perform a subsequent install of pps-client. Otherwise it will be overwritten with the default. For example, you could preserve it by doing something like,
```
$ sudo mv /ect/pps-client.conf /etc/pps-client.conf.orig
```
and then, after installing pps-client,
```
$ sudo mv /etc/pps-client.conf.orig  /ect/pps-client.conf
```
# Building from Source
---
Because pps-client contains a Linux kernel driver, building pps-client requires that a compiled Linux kernel with the same version as the version present on the RPi must also be available during the compilation of pps-client. The pps-client project can be built directly on the RPi or on a Linux workstation with a cross-compiler. Building on the RPi is slower but more reliable. In either case you will first need to download and compile the Linux kernel that corresponds to the kernel version on your RPi.

The steps below don't do a complete kernel installation. Only enough is done to get the object files that are necessary for compiling a kernel driver. If you are unable to match your kernel version to the source found [here](https://github.com/raspberrypi/linux) instructions for doing a complete kernel install can be found [here](https://www.raspberrypi.org/documentation/linux/kernel/building.md). Otherwise follow the steps below.

If you need git:
```
$ sudo apt-get install git
```
## Building on the RPi

You might want to create a folder to hold the kernel and pps-client project. For example,
```
$ mkdir ~/rpi
$ cd ~/rpi
```
Download the current version of the kernel sources:
```
$ git clone --depth=1 https://github.com/raspberrypi/linux
```
Get missing dependencies:
```
$ sudo apt-get install bc
```
Configure the kernel:
```
$ cd linux
$ KERNEL=kernel7
$ make bcm2709_defconfig
```
Compile the kernel (takes about half an hour):
```
$ make -j4 zImage
```
That will provide the necessary kernel object files to build the pps-client driver. If you have not already downloaded the pps-client project, do it now:
```
$ cd ..
$ git clone --depth=1 https://github.com/rascol/PPS-Client
$ cd PPS-Client
```
Now (assuming that the kernel source version is 4.1.20) make the pps-client project. The `KERNELDIR` argument must point to the folder containing the compiled Linux kernel. If not, change it to point to the correct location of the "linux" folder.
```
$ make KERNELDIR=~/rpi/linux KERNELVERS=4.1.20-v7+
```
That will build the installer, `pps-client-4.1.20-v7+`. Run it on the RPi as root:
```
$ sudo ./pps-client-4.1.20-v7+
```
That completes the pps-client installation.

## Building on a Linux Workstation

This is decidedly for the adventurous. We've had problems with the cross-compiler where it looked like everything was fine but the code didn't run on the RPi. Very hard to debug. On the other hand, it can be much easier to get code though the compile stage on a workstation.

You might want to create a folder to hold the kernel and tools. For example,
```
$ mkdir ~/rpi
$ cd ~/rpi
```
On a workstation you will need the cross-compiler:
```
$ git clone https://github.com/raspberrypi/tools
```
That should create a `tools` folder in `/rpi`.

Download the current version of the kernel sources:
```
$ git clone --depth=1 https://github.com/raspberrypi/linux
```
This environment variable will be needed both to configure the kernel and to build pps-client:
```
$ export CROSS_COMPILE=~/rpi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-
```
The `export` command above is temporary. It will survive only until you close the terminal window. To make it persistent you can optionally add the `export` command to the `.bashrc` file in your home directory.

Now configure the kernel:
```
$ cd linux
$ KERNEL=kernel7
$ make ARCH=arm CROSS_COMPILE=$CROSS_COMPILE bcm2709_defconfig
```
and compile it:
```
$ make -j6 ARCH=arm CROSS_COMPILE=$CROSS_COMPILE zImage
```
That will provide the necessary kernel object files to build the driver. If you have not already downloaded the pps-client project, do it now:
```
$ cd ~/rpi
$ git clone --depth=1 https://github.com/rascol/PPS-Client
$ cd PPS-Client
```
Now (assuming that the kernel source version is 4.1.20) make the pps-client project. The `KERNELDIR` argument must point to the folder containing the compiled Linux kernel. If not, change it to point to the correct location of the "linux" folder.
```
$ make CROSS_COMPILE=$CROSS_COMPILE KERNELDIR=~/rpi/linux KERNELVERS=4.1.20-v7+
```
That will build the installer, `pps-client-4.1.20-v7+`. Copy this to the RPi. Run it on the RPi as root:
```
$ sudo ./pps-client-4.1.20-v7+
```
That completes the pps-client installation.

# Running pps-client
---

The pps-client requires that a PPS hardware signal is available from a GPS module. Once the GPS is connected and the PPS output is present on GPIO 4 you can do a quick try-out with,
```
$ sudo pps-client
```
That installs pps-client as a daemon. To watch the controller acquire you can subsequently enter
```
$ pps-client -v
```
That runs a secondary copy of pps-client that just displays a status printout that the pps-client daemon continuously generates. When pps-client starts up you can expect to see something like the following in the status printout:

<center><img src="/images/StatusPrintoutOnStart.png" alt="Status Printout on Startup" style="width: 634px;"/></center>

The `jitter` value is showing the fractional second offset of the PPS signal according to the system clock. That value will decrease second by second as the controller locks to the PPS signal. After about 10 minutes the status printout will look like this:

<center><img src="/images/StatusPrintoutAt10Min.png" alt="Status Printout after 10 Min" style="width: 634px;"/></center>

The `jitter` is displaying small numbers. The time of the rising edge of the PPS signal is shown in the second column. The `clamp` value on the far right indicates that the maximum time correction applied to the system clock is being limited to one microsecond. The system clock is synchronized to the PPS signal to a precision of one microsecond.

It can take as long as 20 minutes for pps-client to fully acquire the first time it runs. This happens if the `jitter` shown in the status printout is on the order of 100,000 microseconds or more. It's quite common for the NTP fractional second to be off by that amount. In this case pps-client may restart several times as it slowly reduces the `jitter` offset. That happens because system functions that pps-client calls internally prevent time changes of more than about 500 microseconds in each second.

These are the parameters shown in the status printout:

 * First two columns - date and time of the rising edge of the PPS signal.
 * Third column - the total number of PPS interrupts received since pps-client was started.
 * jitter - the time deviation in microseconds recorded at the reception of the PPS interrupt.
 * freqOffset - the frequency offset of the system clock in parts per million of the system clock frequency.
 * avgCorrection - the time corrections (in microseconds) averaged over the previous minute.
 * clamp - the hard limit (in microsecs) applied to the raw time error to convert it to a time correction.

Every sixth line, interrupt delay parameters are also shown. About every 17 minutes, an SNTP time query will be made and the results of that will be shown, but will have no effect unless a time update is required.

To stop the display type ctrl-c.

The daemon will continue to run until you reboot the system or until you stop the daemon with
```
$ sudo pps-client-stop
```

To have the pps-client daemon be installed as a system service and loaded on system boot, from an RPi terminal enter:
```
$ sudo chkconfig --add pps-client
```
If you have installed pps-client as a system service you should start it with 
```
$ sudo service pps-client start
```
and you should stop it with
```
$ sudo service pps-client stop
```
The "`pps-client -v`" command continues to work as described above.

If your have questions, we have answers. Be sure to check out the docs.
