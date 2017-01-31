# Raspberry Pi PPS Client

<p align="center"><img src="figures/RPi_with_GPS.jpg" alt="Raspberry Pi with GPS" width="400"/></p>

The pps-client daemon is a fast, high accuracy Pulse-Per-Second system clock synchronizer for Raspberry Pi that synchronizes the Raspberry Pi system time clock to a GPS time clock. 

- [Summary](#summary)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
  - [The Raspian OS](#the-raspian-os)
  - [The NTP daemon](#the-ntp-daemon)
  - [The chkconfig system services manager](#the-chkconfig-system-services-manager)
- [Installing](#installing)
- [Uninstalling](#uninstalling)
- [Reinstalling](#reinstalling)
- [Running pps-client](#running-pps-client)
- [Practical Limits to Time Measurement](#practical-limits-to-time-measurement)
  - [Flicker Noise](#flicker-noise)
  - [Linux OS Real-Time Latency](#linux-os-real-time-latency)
  - [Measurements of Noise and Latency](#measurements-of-noise-and-latency)

# Summary
---
The pps-client daemon provides timekeeping synchronization precision of 1 microsecond and a typical average timekeeping accuracy of 2 microseconds on the Raspberry Pi 3 (verified on 10 test units).

Figure 1 is a distribution of time adjustments made by the pps-client controller to the system clock. 

<p align="center"><img src="figures/offset-distrib.png" alt="Jitter and Corrections Distrib" width="608"/></p>

This data was captured from a Raspberry Pi 3 running Raspian with a 4.4.14-v7+ Linux kernel. The time corrections required to keep the rollover of the second synchronized to the rising edge of the PPS signal never exceeded 1 microsecond in this 24 hour period. This was true for all test units.

Figure 2 shows the system clock frequency set by the controller and the resulting [Allan deviation](https://en.wikipedia.org/wiki/Allan_variance) for the test unit with the largest error of the ten that were tested.

<p align="center"><img src="figures/frequency-vars.png" alt="Frequency Vars over 24 hours" width="685"/></p>

Although the clock frequency drifted slightly between each frequency correction, the maximum Allan deviation of 0.045 ppm over this 24 hour period shows it to be unlikely that the clock ever drifted more than 0.100 ppm from the control point. That corresponds to a time drift of less than 0.1 microseconds per second (A clock offset of 1 ppm corresponds to a time drift of 1 microsecond per sec.)

Since the time slew adjustments necessary to keep the system time synchronized to the PPS never exceeded 1 microsecond each second and the time drift never exceeded 0.1 microsecond each second, the timekeeping control **precision** illustrated in Figure 3 was 1 microsecond over this 24 hour period for all test units. 

As shown in Figure 3, timekeeping **accuracy** is the time offset at the rollover of the second which is also the offset between the true time and the measured time at any point in time.

<p align="center"><img src="figures/time.png" alt="Interpretation of accuracy and precision" width=""/></p>

Figure 4 is the distribution of measured times relative to a true time of 800,000 microseconds into each second for a typical Raspberry Pi 3 from the units tested.

<p align="center"><img src="figures/InterruptTimerDistrib.png" alt="Time Parameters" width=""/></p>

The peak of the distribution in Figure 4 is the average error for this test unit and is about 0.28 microsecond below 800,000 microseconds. For the ten test units the median average error was -0.25 microsecond and the maximum average error was -0.76 microseconds. 

Figure 4 also shows that there are limits to accurate single-event time measurement set by clock oscillator jitter and the response time (latency) of the Linux kernel. This is discussed below in [Practical Limits to Time Measurement](#practical-limits-to-time-measurement).

For a detailed description of the pps-client controller and accuracy testing run Doxygen in `/usr/share/doc/pps-client` on the RPi to generate the documentation or visit the [PPS-Client-Pages](https://rascol.github.io/Raspberry-Pi-PPS-Client) website.

# Hardware Requirements
---

1. A Raspberry Pi 3 or Pi 2 Model B.

2. A GPS module that provides a PPS output. Development was done with the [Adafruit Ultimate GPS module](https://www.adafruit.com/products/2324). Others providing compatible logic levels can also be used.

3. A wired connection from a PPS source with 3.3 Volt logic outputs to GPIO 4 (pin 7) on the RPi header.

4. A wired connection from GPIO_17 (pin 11) to GPIO_22 (pin 15) to support self calibration (Note the yellow jumper in the photo above).
 
# Software Requirements
---
## The Raspian OS

Versions of Linux kernel 4.1 and later are supported. The Raspian OS is required only because the RPi file locations required by the installer (and test files) are hard coded. If there is enough interest in using alternative OS's, these install locations could be determined by the pps-client config file.

## The NTP daemon

NTP is provided out the box on Raspian. NTP sets the whole seconds of the initial date and time and SNTP maintains the date and time through DST and leap second changes.

## The chkconfig system services manager 
 
`~ $ sudo apt-get install chkconfig`

This is necessary if you want to install pps-client as a system service.

# Installing
---

The pps-client program has a built-in Linux kernel driver. It is a Linux requirement that kernel drivers must be compiled on the Linux version on which they are used. This means that there is a different version of PPS-Client for every version of Linux that has been released since Linux 4.0. Because Linux versions roll over very frequently, it is impractical to provide a pre-compiled pps-client installer for every one. Consequently, the pps-client installer will be built from source. 

This situation is far from ideal because it means that pps-client has to be reinstalled whenever the Linux kernel in the RPi is upgraded. If there is interest in this project, the driver may be accepted into mainline in the upstream kernel and the versioning problem will go away.

In principle the build can be done on a cross-compiler. However, bulding directly on the Rasperry Pi is less error prone. The kernel source is downloaded and compiled and then the pps-client installer is compiled using the kernel build system.

The steps below don't do a complete kernel installation. Only enough is done to get the object files that are necessary for compiling a kernel driver. The entire installation takes about 40 minutes on Raspberry Pi 3.

Before compiling the kernel be certain your system and tools are up to date on the Raspberry Pi. The reboot is necessary in case the Linux kernel version was updated.
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
For retrieving the Linux source get the rpi-source script:
```
~/rpi $ sudo wget https://raw.githubusercontent.com/notro/rpi-source/master/rpi-source -O /usr/bin/rpi-source && sudo chmod +x /usr/bin/rpi-source && /usr/bin/rpi-source -q --tag-update
```

Run the script to download the Linux source matching the installed version of Linux on your RPi:
```
~/rpi $ rpi-source -d ./ --nomake --delete
```
Get missing dependencies:
```
~/rpi $ sudo apt-get install bc
```
Configure the kernel:
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
If all went well, you have the necessary kernel object files to build the pps-client driver. If you have not already downloaded the pps-client project, do it now:
```
~/rpi/linux $ cd ..
~/rpi $ git clone --depth=1 https://github.com/rascol/Raspberry-Pi-PPS-Client
~/rpi $ cd Raspberry-Pi-PPS-Client
```
Now make the pps-client installer. The `KERNELDIR` argument must point to the folder containing the compiled Linux kernel. Type or copy the commands below exactly as shown (using **back quotes** which cause the back quotes and text between to be replaced with the correct kernel version).
```
~/rpi/Raspberry-Pi-PPS-Client $ make KERNELDIR=~/rpi/linux KERNELVERS=`uname -r`
```
That will build the installer. Run it on the RPi as root:
```
~/rpi/Raspberry-Pi-PPS-Client $ sudo ./pps-client-`uname -r`
```
That completes the pps-client installation.

# Uninstalling
---

Uninstall pps-client on the RPi with:
```
~ $ sudo pps-client-stop
~ $ sudo pps-client-remove
```
This removes everything **except** the configuration file which you might want to keep if it has been modified and you intend to reinstall pps-client. To remove everything do:

```
~ $ sudo pps-client-remove -a
```

# Reinstalling
---

To reinstall, first uninstall as [described above](#uninstalling) then install.


# Running pps-client
---

The pps-client requires that a PPS hardware signal is available from a GPS module and all wired connections for the GPS module [are in place](#hardware-requirements). Once the GPS is connected and the PPS output is present on GPIO 4 you can do a quick try-out with,
```
~ $ sudo pps-client
```
That installs pps-client as a daemon. To watch the controller acquire you can subsequently enter
```
~ $ pps-client -v
```
That runs a secondary copy of pps-client that just displays a status printout that the pps-client daemon continuously generates and saves to a memory file. When pps-client starts up you can expect to see something like the following in the status printout:

<p align="center"><img src="figures/StatusPrintoutOnStart.png" alt="Status Printout on Startup" width="634"/></p>

The `jitter` value is showing the fractional second offset of the PPS signal according to the system clock. That value will decrease second by second as the controller locks to the PPS signal. After about 10 minutes the status printout will look like this:

<p align="center"><img src="figures/StatusPrintoutAt10Min.png" alt="Status Printout after 10 Min" width="634"/></p>

The `jitter` is displaying small numbers. The time of the rising edge of the PPS signal is shown in the second column. The `clamp` value on the far right indicates that the maximum time correction applied to the system clock is being limited to one microsecond. The system clock is synchronized to the PPS signal to a precision of one microsecond (but with an absolute accuracy limited by clock oscillator noise which could have as much as 1 microsecond of [RMS](https://en.wikipedia.org/wiki/Root_mean_square) jitter).

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
~ $ sudo pps-client-stop
```

To have the pps-client daemon be installed as a system service and loaded on system boot, from an RPi terminal enter:
```
~ $ sudo chkconfig --add pps-client
```
If you have installed pps-client as a system service you should start it with 
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

While pps-client will synchronize the system clock to a GPS clock with an average accuracy of two microseconds, there are practical limits imposed by the hardware and the operating system that limit single-event timing accuracy. The hardware limit is [flicker noise](https://en.wikipedia.org/wiki/Flicker_noise), a kind of low frequency noise that is present in all crystal oscillators. The operating system limit is the real-time performance of the Linux OS.

## Flicker Noise

The Raspberry Pi is an ARM processor that generates all internal timing with a conventional integrated circuit oscillator timed by an external crystal. Consequently the RPi is subject to crystal oscillator flicker noise. In the case of the RPi, flicker noise appears as a random deviation of system time from the PPS signal of up to a few microseconds. Even though flicker noise is always present, it is not evident when timing intervals between events occurring in software running on the same system. It only becomes evident when timing events external to the processor or between two systems. 

Flicker noise sets the absolute limit on the accuracy of the system clock. This is true not only of the RPi ARM processor but also of all conventional application processors.

## Linux OS Real-Time Latency

The Linux OS was never designed to be a real-time operating system. Nevertheless, because of considerable interest in upgrading it to provide real-time performance, real-time performance improved significantly between versions 3 and 4. As a result, median system latency in responding to an external interrupt on the RPi ARM processor is currently about 6 microseconds - down from about 23 microseconds in Linux 3. As yet, however, longer sporadic delays still occur. 

## Measurements of Noise and Latency

<p align="center"><img src="figures/SingleEventTimerDistrib.png" alt="Jitter Distribution" width=""/></p>

Figure 5 is a typical accumulation of single-event timings for external interrupts at 800,000 microseconds after the PPS interrupt. The main peak is the result of reasonably constant system latency and clock oscillator flicker noise having a standard deviation of about 0.8 microsecond. The secondary peak at about 800,003 microseconds is one of many such features introduced by OS latency that can appear for hours or days or disappear altogether. The jitter samples to the right of the main peak that can only be seen in the logarithmic plot were delayed time samples of the PPS signal also introduced by OS latency.

Consequently, while flicker noise limits synchronization accuracy of events on different Raspberry Pi computers timed by the system click to a few microseconds (~1 μsec SD), the real-time performance of the Linux OS (as of v4.4.14-v7+) sets the timing accuracy of external events to about 20 microseconds (Pi 3) because of sporadic system interrupt latency.

