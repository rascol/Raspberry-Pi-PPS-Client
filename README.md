# Raspberry Pi PPS Client

<center><img src="/images/RPi_with_GPS.jpg" alt="Raspberry Pi with GPS" style="width: 400px;"/></center>

The pps-client daemon is a fast, high accuracy Pulse-Per-Second system clock synchronizer for Raspberry Pi that synchronizes the Raspberry Pi system clock to a GPS time clock. 

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
  - [Locate the Kernel Source](#locate-the-kernel-source)
  - [Building on the RPi](#building-on-the-rpi)
  - [Building on a Linux Workstation](#building-on-a-linux-workstation)
- [Running pps-client](#running-pps-client)
- [Practical Limits to Time Measurement](#practical-limits-to-time-measurement)
  - [Flicker Noise](#flicker-noise)
  - [Linux OS Real-Time Latency](#linux-os-real-time-latency)
  - [Measurements of Noise and Latency](#measurements-of-noise-and-latency)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# Summary
---
The pps-client daemon provides timekeeping synchronization precision of 1 microsecond and a typical average accuracy of 2 microseconds on the Raspberry Pi 3 (verified on 10 test units).

Figure 1 is a distribution of time adjustments made by the pps-client controller to the system clock. 

<center><img src="/images/offset-distrib.png" alt="Jitter and Corrections Distrib" style="width: 608px;"/></center>

This data was captured from a Raspberry Pi 3 running Raspian with a 4.4.14-v7+ Linux kernel. The time corrections required to keep the rollover of the second synchronized to the rising edge of the PPS signal never exceeded 1 microsecond in this 24 hour period. This was true for all test units.

Figure 2 shows the system clock frequency set by the controller and the resulting [Allan deviation](https://en.wikipedia.org/wiki/Allan_variance) for the test unit with the largest error of the ten that were tested.

<center><img src="/images/frequency-vars.png" alt="Frequency Vars over 24 hours" style="width: 685px;"/></center>

Although the clock frequency drifted slightly between each frequency correction, the maximum Allan deviation of 0.045 ppm over this 24 hour period shows it to be unlikely that the clock ever drifted more than 0.100 ppm from the control point. That corresponds to a time drift of less than 0.1 microseconds per second (A clock offset of 1 ppm corresponds to a time drift of 1 microsecond per sec.)

Since the time slew adjustments necessary to keep the system time synchronized to the PPS never exceeded 1 microsecond each second and the time drift never exceeded 0.1 microsecond each second, the timekeeping control **precision** illustrated in Figure 3 was 1 microsecond over this 24 hour period for all test units. 

As shown in Figure 3, timekeeping **accuracy** is the time offset at the rollover of the second which is also the offset between the true time and the measured time at any point in time.

<center><img src="/images/time.png" alt="Interpretation of accuracy and precision" style=""/></center>

Figure 4 is the distribution of measured times relative to a true time of 800,000 microseconds into each second for a typical Raspberry Pi 3 from the units tested.

<center><img src="/images/InterruptTimerDistrib.png" alt="Time Parameters" style=""/></center>

The peak of the distribution in Figure 4 is the average error for this test unit and is about 0.28 microsecond below 800,000 microseconds. For the ten test units the median error was -0.28 microsecnd and the maximum error was -1.1 microseconds. 

Figure 4 also shows that there are limits to accurate single-event time measurement set by clock oscillator jitter and the response time (latency) of the Linux kernel. This is discussed below in [](#practical-limits-to-time-measurement).

For a detailed description of the pps-client controller and accuracy testing run Doxygen to generate the documentation or visit the [PPS-Client-Pages](https://rascol.github.io/Raspberry-Pi-PPS-Client) website.

# Hardware Requirements
---

1. A Raspberry Pi 3 or Pi 2 Model B. All data included in this file is for Raspberry Pi 3. Please note that jitter and noise are lowest in the Raspberry Pi 3.

2. A GPS module that provides a PPS output. Development was done with the [Adafruit Ultimate GPS module](http://www.adafruit.com/product/746). Others providing compatible logic levels can also be used.

3. A wired connection from a PPS source with 3.3 Volt logic outputs to GPIO 4 (pin 7) on the RPi header.

4. A wired connection from GPIO_17 (pin 11) to GPIO_22 (pin 15) to support self calibration (Note the yellow jumper in the photo above).
 
# Software Requirements
---
## The Raspian OS

Versions of Linux kernel 4.1 and later are supported. The Raspian OS is required only because the RPi file locations required by the installer (and test files) are hard coded. If there is enough interest in using alternative OS's, these install locations could be determined by the pps-client config file.

## The NTP daemon

NTP is provided out the box on Raspian. NTP sets the whole seconds of the initial date and time and SNTP maintains the date and time through DST and leap second changes.

## The chkconfig system services manager 
 
`$ sudo apt-get install chkconfig`

This is necessary if you want to install pps-client as a system service.

# Installing
---

The pps-client has a few pre-compiled Linux 4 versions of the installer on the server. The pps-client version must match the version of the Linux kernel on your RPi. Version matching is necessary because pps-client contains a kernel driver that will only be recognized by the matching version of the Linux kernel.

The kernel version of your RPi can be found by running `uname -r` from a connected terminal. The first two numbers in the kernel version on your RPi must match those in the pps-client installer that you select. If the third number mismatches that means the kernel version on your RPi is not the latest bugfix version of the Linux kernel in that series. In that case, you will need to update the kernel on your RPi to that version.

Bugfix kernel updates within the same kernel series, as for example from version `4.1.19` to `4.1.21`, are unlikely to adversely affect the operation of software already installed on your RPi. But in the exceptional case that you run into a problem later, the kernel you replaced has been saved on your RPi so you can always revert to it.

If you need to update your kernel, then on your RPi install `rpi-update` with
```
$ sudo apt-get install rpi-update
```
Then run `rpi-update` as follows with the second number of the kernel on your RPi substituted for `X`,
```
$ sudo BRANCH=rpi-4.X.y rpi-update
```
To finish the install, copy the appropriate installer to your RPi and run it. (Type the command below exactly as shown (using **back quotes** which cause the back quotes and text between to be replaced with the kernel version of your RPi).
```
$ sudo ./pps-client-`uname -r`
```
Installers for a few different Linux kernels are currently on the server and more will be. This is not an ideal solution because it means that pps-client has to be re-installed if the kernel version is updated. That will not automatically happen unless you run `rpi-update`. If there is enough interest in this project, the driver may be accepted into mainline in the upstream kernel and the versioning problem will go away.

# Uninstalling
---

Uninstall pps-client on the RPi with:
```
$ sudo pps-client-stop
$ sudo pps-client-remove
```
This removes everything **except** the configuration file which you might want to keep if it has been modified and you intend to reinstall pps-client. To remove everything do:

```
$ sudo pps-client-remove -a
```

# Reinstalling
---

To reinstall, first uninstall as [described above](#uninstalling) then install.

# Building from Source
---
Because pps-client contains a Linux kernel driver, building pps-client requires that a compiled Linux kernel with the same version as the version present on the RPi must also be available during the compilation of pps-client. The pps-client project can be built directly on the RPi or on a Linux workstation with a cross-compiler. Building on the RPi is slower but more reliable. In either case you will first need to download and compile the Linux kernel that corresponds to the kernel version on your RPi.

The steps below don't do a complete kernel installation. Only enough is done to get the object files that are necessary for compiling a kernel driver.

If you don't have git,
```
$ sudo apt-get install git
```

## Locate the Kernel Source

First determine the Linux kernel version by running `uname -r` from a terminal on your RPi. 

Then from a browser go to, for example, https://github.com/raspberrypi/linux/commits/rpi-4.4.y (the second number must agree with your Linux kernel version). Scroll down the page and click on the Linux commit entry (`Linux 4.4.21` in this example). That will take you to the latest commit page. Click on the `Browse files` tab on the right side. That will take you to the source page you need in the build steps below.

The third number, `y`, in the Linux version identifies bugfix updates. If that number in the page you found above is later than the version on your RPi then before you use the downloaded kernel you will need to update your kernel to that bugfix update. Bugfix updates are unlikely to adversely affect the operation of software already on your RPi. But if you later run into a problem, the kernel you replaced has been saved on your RPi so you can always revert to it.

To update your RPi kernel you will need to use the `rpi-update` utility. If necessary install it with `sudo apt-get install rpi-update`.

Then update to the latest bugfix for your kernel version with (replace `X` with the corresponding number from your kernel)
```
$ sudo BRANCH=rpi-4.X.y rpi-update
```

## Building on the RPi

You might want to create a folder to hold the kernel and pps-client project. For example,
```
$ mkdir ~/rpi
$ cd ~/rpi
```
Download the version of the kernel sources corresponding to your RPi as described above in "Locate the Kernel Source", substituting the appropriate source page:
```
$ git clone --depth=1 [your-rpi-kernel-source-page]
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
Compile the kernel (takes about half an hour on Pi 2, about 20 minutes on Pi 3):
```
$ make -j4 zImage
```
That will provide the necessary kernel object files to build the pps-client driver. If you have not already downloaded the pps-client project, do it now:
```
$ cd ..
$ git clone --depth=1 https://github.com/rascol/PPS-Client
$ cd PPS-Client
```
Now make the pps-client project. The `KERNELDIR` argument must point to the folder containing the compiled Linux kernel. Type the commands below exactly as shown (using **back quotes** which cause the back quotes and text between to be replaced with the correct kernel version).
```
$ make KERNELDIR=~/rpi/linux KERNELVERS=`uname -r`
```
That will build the installer. Run it on the RPi as root:
```
$ sudo ./pps-client-`uname -r`
```
That completes the pps-client installation.

## Building on a Linux Workstation

This is decidedly for the adventurous. We've had problems with the cross-compiler where it looked like everything was fine but the code didn't run on the RPi. Very hard to debug. On the other hand, it can be much easier to get code though the compile stage on a workstation.

You might want to create a folder to hold the kernel and tools. For example,
```
$ mkdir ~/rpi
$ cd ~/rpi
```
On a workstation you will need the latest version of the cross-compiler:
```
$ git clone https://github.com/raspberrypi/tools
```
That should create a `tools` folder in `/rpi`.

Download the version of the kernel sources corresponding to your RPi as described above in "Locate the Kernel Source", substituting the appropriate source page:
```
$ git clone --depth=1 [your-rpi-kernel-source-page]
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
Now (assuming that the kernel source version is 4.4.14) make the pps-client project. The `KERNELDIR` argument must point to the folder containing the compiled Linux kernel. If not, change it to point to the correct location of the "linux" folder.
```
$ make CROSS_COMPILE=$CROSS_COMPILE KERNELDIR=~/rpi/linux KERNELVERS=4.4.14-v7+
```
That will build the installer, `pps-client-4.4.14-v7+`. Copy this to the RPi. Run it on the RPi as root:
```
$ sudo ./pps-client-4.4.14-v7+
```
That completes the pps-client installation.

# Running pps-client
---

The pps-client requires that a PPS hardware signal is available from a GPS module and all wired connections for the GPS module [are in place](#hardware-requirements). Once the GPS is connected and the PPS output is present on GPIO 4 you can do a quick try-out with,
```
$ sudo pps-client
```
That installs pps-client as a daemon. To watch the controller acquire you can subsequently enter
```
$ pps-client -v
```
That runs a secondary copy of pps-client that just displays a status printout that the pps-client daemon continuously generates and saves to a memory file. When pps-client starts up you can expect to see something like the following in the status printout:

<center><img src="/images/StatusPrintoutOnStart.png" alt="Status Printout on Startup" style="width: 634px;"/></center>

The `jitter` value is showing the fractional second offset of the PPS signal according to the system clock. That value will decrease second by second as the controller locks to the PPS signal. After about 10 minutes the status printout will look like this:

<center><img src="/images/StatusPrintoutAt10Min.png" alt="Status Printout after 10 Min" style="width: 634px;"/></center>

The `jitter` is displaying small numbers. The time of the rising edge of the PPS signal is shown in the second column. The `clamp` value on the far right indicates that the maximum time correction applied to the system clock is being limited to one microsecond. The system clock is synchronized to the PPS signal to a precision of one microsecond (but with an absolute accuracy limited by clock oscillator noise which has ≈1 microsecond [RMS](https://en.wikipedia.org/wiki/Root_mean_square) jitter).

It can take as long as 20 minutes for pps-client to fully acquire the first time it runs. This happens if the `jitter` shown in the status printout is on the order of 100,000 microseconds or more. It's quite common for the NTP fractional second to be off by that amount on a cold start. In this case pps-client may restart several times as it slowly reduces the `jitter` offset. That happens because system functions that pps-client calls internally prevent time changes of more than about 500 microseconds in each second.

These are the parameters shown in the status printout:

 * First two columns - date and time of the rising edge of the PPS signal.
 * Third column - the sequence count, i.e., the total number of PPS interrupts received since pps-client was started.
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

# Practical Limits to Time Measurement
---

While pps-client will synchronize the system clock to a GPS clock with an average accuracy of two microseconds, there are practical limits imposed by the hardware and the operating system that limit single-event timing accuracy. The hardware limit is [flicker noise](https://en.wikipedia.org/wiki/Flicker_noise), a kind of low frequency noise that is present in all crystal oscillators. The operating system limit is the real-time performance of the Linux OS.

## Flicker Noise

The Raspberry Pi is an ARM processor that generates all internal timing with a conventional integrated circuit oscillator timed by an external crystal. Consequently the RPi is subject to crystal oscillator flicker noise. In the case of the RPi, flicker noise appears as a random deviation of system time from the PPS signal of up to several microseconds. Even though flicker noise is always present, it is not evident when timing intervals between events occurring in software running on the same system. It only becomes evident when timing events external to the processor or between two systems. 

Flicker noise sets the absolute limit on the accuracy of the system clock. This is true not only of the RPi ARM processor but also of all conventional application processors.

## Linux OS Real-Time Latency

The Linux OS was not designed to be a real-time operating system. Nevertheless, there is considerable interest in upgrading it to provide real-time performance. Consequently, real-time performance improved significantly between versions 3 and 4. As a result of that, median system latency in responding to an external interrupt on the RPi ARM processor is currently about 6 microseconds - down from about 23 microseconds in Linux 3. As yet, however, longer sporadic delays occur frequently. 

## Measurements of Noise and Latency

As it maintains clock synchronization, the pps-client daemon continuously measures jitter and interrupt delay. Jitter is predominantly a combination flicker noise in the clock oscillator and OS response latency. A jitter plot recorded second by second over 24 hours on a Raspberry Pi 3 Model B is shown in Figure 3.

<center><img src="/images/pps-jitter-distrib.png" alt="Jitter Distribution" style=""/></center>

This is a spreadsheet plot of the data file `/var/local/pps-jitter-distrib` that was generated when `jitter-distrib=enable` was set in in the pps-client config file, `/etc/pps-client.conf`. 

The shape of the main peak is consistent with clock oscillator flicker noise having a standard deviation of about 0.8 microsecond. The jitter samples to the right of the main peak that can only be seen in the logarithmic plot were delayed time samples of the PPS signal introduced by OS latency. There were about 1500 of these out of a total sample population of 86,400. So about 1.7% of the time Linux system latency delayed the sample as much as 15 microseconds.

Consequently, while flicker noise limits synchronization accuracy of events on different Raspberry Pi computers timed by the system click to a few microseconds, the real-time performance of the Linux OS (as of v4.4.14-v7+) sets the timing accuracy of external events to about 20 microseconds.



