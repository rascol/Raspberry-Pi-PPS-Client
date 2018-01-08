# PPS-Client v1.5.0

<p align="center"><img src="figures/RPi_with_GPS.jpg" alt="Raspberry Pi with GPS" width="400"/></p>

The PPS-Client daemon is a fast, microsecond accuracy Pulse-Per-Second system clock synchronizer for Raspberry Pi that synchronizes the Raspberry Pi system time clock to a GPS time clock. 

- [Summary](#summary)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
  - [Operating System](#operating-system)
  - [No Internet Time](#no-internet-time)
  - [GPS Only](#gps-only)
  - [The chkconfig system services manager](#the-chkconfig-system-services-manager)
- [Installing](#installing)
  - [Get the Files](#get-the-files)
  - [Compile the Kernel](#compile-the-kernel)
  - [Build PPS-Client](#build-pps-client)
- [Uninstalling](#uninstalling)
- [Reinstalling](#reinstalling)
- [Running PPS-Client](#running-pps-client)
- [Practical Limits to Time Measurement](#practical-limits-to-time-measurement)
  - [Flicker Noise](#flicker-noise)
  - [Linux OS Real-Time Latency](#linux-os-real-time-latency)
  - [Measurements of Noise and Latency](#measurements-of-noise-and-latency)

# Summary
---
The PPS-Client daemon provides timekeeping synchronization precision of 1 microsecond and a typical average timekeeping accuracy of 2 microseconds on the Raspberry Pi 3 (verified on 10 test units). Precision and accuracy at this level is possible because the PPS-Client controller uses design techniques not previously employed to discipline an application processor clock to a PPS signal.

The PPS-client controller is continuously self-calibrating and does not require precision temerature control. The following results were recorded while the test Raspberry Pi test units were in a partially heated room where the temperature varied over a range of about 60 to 75 deg F through 24 hours. 

Figure 1 is a distribution of time adjustments made by the PPS-Client controller to the system clock. 

<p align="center"><img src="figures/offset-distrib.png" alt="Jitter and Corrections Distrib" width="608"/></p>

This data was captured from a Raspberry Pi 3 running Raspian with a standard 4.4.14-v7+ Linux kernel. The time corrections required to keep the rollover of the second synchronized to the rising edge of the PPS signal never exceeded 1 microsecond in this 24 hour period. This was true for all test units.

Figure 2 shows the system clock frequency set by the controller and the resulting [Allan deviation](https://en.wikipedia.org/wiki/Allan_variance) for the test unit with the largest error of the ten that were tested.

<p align="center"><img src="figures/frequency-vars.png" alt="Frequency Vars over 24 hours" width="685"/></p>

Although the clock frequency drifted slightly between each one minute frequency correction, the maximum Allan deviation of 0.045 ppm over this 24 hour period shows it to be unlikely that the clock ever drifted more than 0.100 ppm from the control point. That corresponds to a time drift of less than 0.1 microseconds per second (A clock offset of 1 ppm corresponds to a time drift of 1 microsecond per sec.)

Since the time slew adjustments necessary to keep the system time synchronized to the PPS never exceeded 1 microsecond each second and the clock oscillator time drift never exceeded 0.1 microsecond each second, the timekeeping control **precision** illustrated in Figure 3 was 1 microsecond over this 24 hour period for all test units. 

As shown in Figure 3, timekeeping **accuracy** is the time offset at the rollover of the second which is also the offset between the true time and the measured time at any point in time.

<p align="center"><img src="figures/time.png" alt="Interpretation of accuracy and precision" width=""/></p>

Figure 4 is the distribution of measured times relative to a true time of 800,000 microseconds into each second for a typical Raspberry Pi 3 from 10 units tested over a 24 hour period.

<p align="center"><img src="figures/InterruptTimerDistrib.png" alt="Time Parameters" width=""/></p>

The peak of the distribution in Figure 4 is the average error for this test unit and is about 0.28 microsecond below 800,000 microseconds. For the ten test units the median average error was -0.25 microsecond and the maximum average error was -0.76 microseconds. 

Figure 4 also shows that there are limits to accurate single-event time measurement set by clock oscillator jitter and the response time (latency) of the Linux kernel. This is discussed below in [Practical Limits to Time Measurement](#practical-limits-to-time-measurement).

For a detailed description of the PPS-Client controller and accuracy testing run Doxygen in `/usr/share/doc/pps-client` on the RPi to generate the documentation or visit the [PPS-Client-Pages](https://rascol.github.io/Raspberry-Pi-PPS-Client) website.

# Hardware Requirements
---

1. A Raspberry Pi 3 or Pi 2 Model B.

2. A GPS module that provides a PPS output. Development was done with the [Adafruit Ultimate GPS module](https://www.adafruit.com/products/2324). Others providing compatible logic levels can also be used.

3. A wired connection from a PPS source with 3.3 Volt logic outputs to GPIO 4 (pin 7) on the RPi header.

4. A wired connection from GPIO_17 (pin 11) to GPIO_22 (pin 15) to support self calibration (Note the yellow jumper in the photo above).
 
# Software Requirements
---
## Operating System

Versions of Linux kernel 4.1 and later are supported. Currently PPS-Client v1.5 and later runs on **Raspian** and **Ubuntu Mate** on the Raspberry Pi. At the moment, PPS-Client is restricted to Debian based Linux operating systems because the file locations required by the installer (and test files) are hard coded. If there is enough interest in using other OS's, these install locations could be determined by the PPS-Client config file.

## No Internet Time

As of PPS-Client v1.5.0 the default for setting whole second time-of-day updates is to query NIST time servers over the Internet. You may want use some other program to handle setting time-of-day updates. To tell PPS-Client that it should totally ignore how the clock seconds are set, after installation use a setting in **/etc/pps-client.conf**:
```
~ $ sudo nano /etc/pps-client.conf
```
Scroll down to the line,
```
#sntp=disable
```
and uncomment it. PPS-Client will no longer care about whole-second time of day or how it gets set. But PPS-Client will continue to synchronize the roll-over of the second to the PPS signal. 

## GPS Only

PPS-Client can also be configured to have the GPS receiver that is providing the PPS signal provide the whole-second time of day updates as well. This enables operation with no internet connection. A Raspberry Pi configured this way could, for example, be used as a Stratum 1 time server for a LAN that is not connected to the Internet. 

To configure this mode after installation,
```
~ $ sudo nano /etc/pps-client.conf
```
Scroll down to the line,
```
#serial=enable
```
and uncomment it. This will set PPS-Client to read GPS time messages from the serial port. This, of course, requires that the GPS receiver is connected to the serial port of the Raspberry Pi and that the serial port has been set for this purpose. Setting the serial port can be done on **Raspian** by using the **raspi-config** command.  Details are [here](https://learn.adafruit.com/adafruit-ultimate-gps-hat-for-raspberry-pi/pi-setup).



**GPS Only** can be demonstrated with no additional hardware connections if the [Adafruit Ultimate GPS module](https://www.adafruit.com/products/2324) (visible in the picture at the top) is used because this device connects directly to the serial port of the RPi through its GPIO pins.

## The chkconfig system services manager 
 
```
~ $ sudo apt-get install chkconfig
```

This is necessary if you want to install PPS-Client as a system service that will automatically start on system boot.

# Installing
---

The PPS-Client program has a built-in Linux kernel driver. It is a Linux requirement that kernel drivers must be compiled on the Linux version on which they are used. This means that there is a different version of PPS-Client for every version of Linux that has been released since Linux 4.0. Because Linux versions roll over very frequently, it is impractical to provide a pre-compiled PPS-Client installer for every one. Consequently, the PPS-Client installer is built from source. 

This situation is far from ideal because it means that PPS-Client has to be reinstalled whenever the Linux kernel in the RPi is upgraded. If there is interest in this project, the driver may be accepted into mainline in the upstream kernel and the versioning problem will go away.

In principle the build can be done on a cross-compiler. However, bulding directly on the Rasperry Pi is less error prone. The kernel source is downloaded and compiled and then the PPS-Client installer is compiled using the kernel build system.

The steps below don't do a complete kernel installation. Only enough is done to get the object files that are necessary for compiling a kernel driver. The entire installation takes about 40 minutes on Raspberry Pi 3.

## Get the Files

Previous versions of PPS-Client required the installation of NTP. PPS-Client now uses **timedatectl** from **systemd** to disable NTP clock discipline. This is currently the preferred way to do any kind of time control (see man timedatectl). But **timedatectl** will only take control if NTP is **not** installed. To insure that is the case, from the Raspberry Pi do,
```
~ $ sudo apt-get remove ntp
```
Also, before compiling the kernel, be certain your system and tools are up to date on the Raspberry Pi. The reboot is necessary in case the Linux kernel version was updated and, if necessary, to complete the removal of NTP.
```
~ $ sudo apt-get update
~ $ sudo apt-get upgrade
~ $ sudo reboot
```

In your home folder on your Raspberry Pi, you might want to first set up a build folder:
```
~ $ mkdir rpi
~ $ cd rpi
```
First get missing dependencies:
```
~/rpi $ sudo apt-get install bc
```
You might also need to install git:
```
~ $ sudo apt-get install git
```
For retrieving the Linux source get the rpi-source script:
```
~/rpi $ sudo wget https://raw.githubusercontent.com/notro/rpi-source/master/rpi-source -O /usr/bin/rpi-source && sudo chmod +x /usr/bin/rpi-source && /usr/bin/rpi-source -q --tag-update
```
Run the script to download the Linux source that matches the installed version of Linux on your RPi:
```
~/rpi $ rpi-source -d ./ --nomake --delete
```
The script might complain about not getting the version of the compiler. If so, re-run it with the suggested flag. 

You should now have the kernel source. But before you go further, **double check that the script down loaded the correct source version**. To determine your RPi kernel version use
```
~/rpi $ uname -r
```
Then run
```
~/rpi $ nano linux/Makefile
```

You should see something like this at the top of the Makefile
```
VERSION = 4
PATCHLEVEL = 9
SUBLEVEL = 59
EXTRAVERSION =
NAME = Roaring Lionus
 ...
```
The revision numbers must match the version of your Linux kernel exactly. If they do not, don't use this kernel source. Delete it and, as a fall back, retrieve the Linux kernel source for your RPi from https://github.com/raspberrypi/linux/releases. Sources are indexed by creation date. To determine the creation date of the Linux kernel on your RPi run
```
~ $ uname -a
```
Locate the source you need, download it and unzip it into a directory called "linux" on the RPi in the `~/rpi` directory.

## Compile the Kernel

On the RPi,
```
~/rpi $ cd linux
~/rpi/linux $ KERNEL=kernel7
~/rpi/linux $ make bcm2709_defconfig
```

Now compile the kernel (takes about half an hour on Pi 2, 20 minutes on Pi 3):
```
~/rpi/linux $ make -j4 zImage
```
If there are no compile errors, the last message from the compiler will be,
```
Kernel: arch/arm/boot/zImage is ready
```
If all went well, you have the necessary kernel object files to build the PPS-Client driver. The current version of gcc seems prone to randomly segfaulting. If that happens try `make clean` and re-run the make command. 

## Build PPS-Client

If you have not already downloaded the PPS-Client project, do it now:
```
~/rpi/linux $ cd ..
~/rpi $ git clone --depth=1 https://github.com/rascol/Raspberry-Pi-PPS-Client
~/rpi $ cd Raspberry-Pi-PPS-Client
```
Now make the PPS-Client installer. The `KERNELDIR` argument must point to the folder containing the compiled Linux kernel. Type or copy the commands below exactly as shown (using **back quotes** which cause the back quotes and text between to be replaced with the correct kernel version). If you are recompiling, first `make clean`. 

```
~/rpi/Raspberry-Pi-PPS-Client $ make KERNELDIR=~/rpi/linux KERNELVERS=`uname -r`
```
That will build the installer. Run it on the RPi as root:
```
~/rpi/Raspberry-Pi-PPS-Client $ sudo ./pps-client-`uname -r`
```
That completes the PPS-Client installation.

# Uninstalling
---

Uninstall PPS-Client on the RPi with:
```
~ $ sudo pps-client-stop
~ $ sudo pps-client-remove
```
This removes everything **except** the configuration file which you might want to keep if it has been modified and you intend to reinstall PPS-Client. A reinstall will not replace a PPS-Client configuration file, **/etc/pps-client.conf**, that has been modified.

To remove everything do:
```
~ $ sudo pps-client-remove -a
```
# Reinstalling
---

To reinstall, or to install a new version of PPS-Client over an old version, first uninstall as [described above](#uninstalling) then install. If you kept a modified **/etc/pps-client.conf** file, the new install will **not** replace it. Instead the new config file will be copied to **/etc/pps-client.conf.default** to give you the choice of replacing it later.


# Running PPS-Client
---

PPS-Client requires that a PPS hardware signal is available from a GPS module and all wired connections for the GPS module [are in place](#hardware-requirements). Once the GPS is connected and the PPS output is present on GPIO 4 you can do a quick try-out with,
```
~ $ sudo pps-client
```
That installs pps-client as a daemon. To watch the controller acquire you can subsequently enter
```
~ $ pps-client -v
```
That runs a secondary copy of PPS-Client that just displays a status printout that the PPS-Client daemon continuously generates and saves to a memory file. When PPS-Client starts up you can expect to see something like the following in the status printout:

<p align="center"><img src="figures/StatusPrintoutOnStart.png" alt="Status Printout on Startup" width="634"/></p>

The `jitter` value is showing the fractional second offset of the PPS signal according to the system clock. That value will decrease second by second as the controller locks to the PPS signal. After about 10 minutes the status printout will look like this:

<p align="center"><img src="figures/StatusPrintoutAt10Min.png" alt="Status Printout after 10 Min" width="634"/></p>

The `jitter` is displaying small numbers. The time of the rising edge of the PPS signal is shown in the second column. The `clamp` value on the far right indicates that the maximum time correction applied to the system clock is being limited to one microsecond. The system clock is synchronized to the PPS signal to a precision of one microsecond (but with an absolute accuracy limited by clock oscillator noise which could have as much as 1 microsecond of [RMS](https://en.wikipedia.org/wiki/Root_mean_square) jitter).

It can take as long as 20 minutes for PPS-Client to fully acquire the first time it runs. This happens if the `jitter` shown in the status printout is on the order of 100,000 microseconds or more. It's quite common for the NTP fractional second to be off by that amount on a cold start. In this case PPS-Client may restart several times as it slowly reduces the `jitter` offset. That happens because system functions that PPS-Client calls internally prevent time changes of more than about 500 microseconds in each second.

These are the parameters shown in the status printout:

 * First two columns - date and time of the rising edge of the PPS signal.
 * Third column - the sequence count, i.e., the total number of PPS interrupts received since PPS-Client was started.
 * jitter - the time deviation in microseconds recorded at the reception of the PPS interrupt.
 * freqOffset - the frequency offset of the system clock in parts per million of the system clock frequency.
 * avgCorrection - the second-by-second average over the previous minute of the time correction (in parts per million) that is applied once per minute to the system clock.
 * clamp - the hard limit (in microsecs) applied to the raw time error to convert it to a time correction.

About every sixth line, interrupt delay parameters are also shown. The interval will larger when PPS-Client discards a PPS interrupt delayed more than 3 microseconds.

To stop the display type ctrl-c.

The daemon will continue to run until you reboot the system or until you stop the daemon with
```
~ $ sudo pps-client-stop
```

To have the PPS-Client daemon be installed as a system service and loaded on system boot, from an RPi terminal enter:
```
~ $ sudo chkconfig --add pps-client
```
If you have installed PPS-Client as a system service you should start it with 
```
~ $ sudo service pps-client start
```
and you should stop it with
```
~ $ sudo service pps-client stop
```
The "`pps-client -v`" command continues to work as described above.

# Practical Limits to Time Measurement
---

While PPS-Client will synchronize the system clock to a GPS clock with an average accuracy of two microseconds, there are practical limits imposed by the hardware and the operating system that limit single-event timing accuracy. The hardware limit is [flicker noise](https://en.wikipedia.org/wiki/Flicker_noise), a kind of low frequency noise that is present in all crystal oscillators. The operating system limit is the real-time performance of the Linux OS.

## Flicker Noise

The Raspberry Pi is an ARM processor that generates all internal timing with a conventional integrated circuit oscillator timed by an external crystal. Consequently the RPi is subject to crystal oscillator flicker noise. In the case of the RPi, flicker noise appears as a random deviation of system time from the PPS signal of up to a few microseconds. Even though flicker noise is always present, it is not evident when timing intervals between events occurring in software running on the same system. It only becomes evident when timing events external to the processor or between two systems. 

Flicker noise sets the absolute limit on the accuracy of the system clock. This is true not only of the RPi ARM processor but also of all conventional application processors.

## Linux OS Real-Time Latency

The Linux OS was never designed to be a real-time operating system. Nevertheless, because of considerable interest in upgrading it to provide real-time performance, real-time performance improved significantly between versions 3 and 4. As a result, median system latency in responding to an external interrupt on the RPi ARM processor is currently about 6 microseconds - down from about 23 microseconds in Linux 3. As yet, however, longer sporadic delays still occur. 

## Measurements of Noise and Latency

<p align="center"><img src="figures/SingleEventTimerDistrib.png" alt="Jitter Distribution" width=""/></p>

Figure 5 is a typical 24-hour accumulation of single-event timings for external interrupts at 800,000 microseconds after the PPS interrupt. The main peak is the result of reasonably constant system latency and clock oscillator flicker noise having a standard deviation of about 0.8 microsecond. The secondary peak at about 800,003 microseconds is one of many such features introduced by OS latency that can appear for hours or days or disappear altogether. The jitter samples to the right of the main peak that can only be seen in the logarithmic plot were delayed time samples of the PPS signal also introduced by OS latency.

Consequently, while flicker noise limits synchronization accuracy of events on different Raspberry Pi computers timed by the system click to a few microseconds (~1 μsec SD), the real-time performance of the Linux OS (as of v4.4.14-v7+) sets the accuracy of timing external events to about 20 microseconds (Pi 3) because of sporadic system interrupt latency.

