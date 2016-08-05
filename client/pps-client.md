# Raspberry Pi PPS Client {#mainpage}

- [Uses](#uses)
- [Achieving High Accuracy Timekeeping](#achieving-high-accuracy-timekeeping)
  - [Noise](#noise)
- [The pps-client Controller](#the-pps-client-controller)
  - [Feedback Controller](#feedback-controller)
  - [Feedforward Compensator](#feedforward-compensator)
  - [Controller Behavior on Startup](#controller-behavior-on-startup)
  - [Performance Under Stress](#performance-under-stress)
  - [Kernel Device Driver](#kernel-device-driver)
  - [Error Handling](#error-handling)
- [Testing](#testing)
  - [Performance Evaluation](#performance-evaluation)
    - [Configuration File](#configuration-file)
    - [Command Line](#command-line)
  - [Accuracy Validation](#accuracy-validation)
    - [The pulse-generator Utility](#the-pulse-generator-utility)
    - [The interrupt-timer Utility](#the-interrupt-timer-utility)
    - [Testing Accuracy](#testing-accuracy)

# Uses {#uses}

This project is the reference design for a general technique that provides high accuracy timekeeping. It was implemented on an ARM processor that is relatively slow. That should illustrate that a similar software solution can be used on most application processors. On Linux systems, the userland code in the project `client` folder, could be used with minor changes on other processors. The GPIO code of the kernel driver in the `driver` folder is unique to the RPi processor but also could easily be rewritten for other processors.

Although the goal of high accuracy computer timekeeping, evident from even a cursory search on the internet, has been around since at least the introduction of the Network Time Protocol, general support for high precision timekeeping over the internet is still not available. However, GPS reception is available everywhere and in conjunction with a daemon like pps-client can be used for that purpose now. Indeed, an internet search found GPS repeaters for sale that can bring GPS reception indoors with local coverage up to at least 30 meters.

The ability to time synchronize multiple computers is particularly important for small embedded processors like the RPi that can be used to used to construct distributed systems with a large number of individual cores. This works very well when the individual RPis are time synchronized because that makes applications possible that would otherwise be difficult or impractical. One example is a multiple video camera system that synchronizes the video frames from the individual RPis each handling a different camera. In this kind of application a robot with twin cameras for eyes could easily synchronize the cameras from the system time on each RPi.

There are other uses for time synchronized computers: Network administrators would find it useful to have the possibility of making one-way, single-path time measurements of network delays. That becomes possible if the computers at the endpoints are accurately synchronized to GPS time - which is rarely the case at present. Also, certain kinds of scientific applications require accurate synchronization to an external clock. One of interest to amateur astronomers is occultation timings. Another is collection of distributed seismic data in the study of earthquakes or for substratum mapping. There are many others.

# Achieving High Accuracy Timekeeping {#achieving-high-accuracy-timekeeping}

The pps-client is implemented as a [proportional-integral (PI) controller](https://en.wikipedia.org/wiki/PID_controller) (a.k.a. a Type 2 Servo) with proportional and integral feedback provided each second but with the value of the integral feedback adjusted once per minute. The PI controller model provides a view of time synchronization as a linear feedback system in which gain coefficients can be adjusted to provide a good compromise among stability, transient response and amount of noise introduced in the error signal. 

The error signal is the time difference between the one-second interval provided by a PPS signal and the length of the second reported by the system clock. The noise of concern is the second-to-second variation in time reported by the system because of the corresponding variation of system delay in responding to the PPS interrupt. This variable component of the delay, referred to as jitter, is a combination of clock oscillator jitter, PPS signal jitter and system latency in responding the the interrupt.

Because the error signal has jitter and is being used to control synchronization, the jitter component has the effect of a false error signal that causes the time and frequency synchronization to fluctuate as the controller attempts to follow the jitter. The conventional approach to reducing jitter is to low-pass or median filter the error signal. But filtering has the serious disadvantage that reduction of the jitter must be traded off against time delay introduced by the filter. Additional time delay in the feedback loop inevitably degrades controller performance. The pps-client program uses a much better technique that introduces no delay. To reduce the jitter, the time values returned by the system are passed through a hard limiter that clips extreme values before applying them as time corrections.

In fact, the individual time corrections constitute jitter added to a very small time correction. Consequently, each individual correction is mostly jitter and thus is wrong by nearly the amount it deviates from zero. Because of that, the limiting employed in this system clips the maximum time corrections when the controller has fully stabilized to the time resolution of the system clock which is 1 microsecond.

It might seem that such extreme limiting would remove the desired error signal along with the noise. But that doesn't happen because the true time error is a slowly varying quasi-stationary (DC) level. Limiting only slices off the dynamic (AC) component of the error signal. The DC component remains. (To see what limiting does see Figures 3, 4, and 5 and the relevant discussion.) If the jitter were not limited, the controller would make the sum of the positive and negative jitter zero. That would be undesirable even after filtering because the noise has significant components in time periods that extend to well beyond one minute. Filtering would remove noise only for time intervals within the cut-off region of the filter. Longer period noise would remain. 

On the other hand, instead of zeroing the sum of negative and positive jitter and thereby allowing the difference to be introduced as noise, once the controller has acquired, hard limiting causes the controller to make the number of positive and negative excursions to be equal around zero. That happens because the clipped positive and negative amplitudes are identical (1 microsecond). Thus, making the sum zero makes the count equal. As a result, the varying magnitude of the jitter around the control point is ignored and the reported time delay of the PPS rising edge adjusts to its median value, the delay at which there were as many shorter as longer reported delays over the previous minute.

The only (almost insignificant) disadvantage of hard limiting is that it reduces the amount of time correction that can be applied each each second. But that limitation is easily circumvented by allowing the hard limit level to track the amount of required time correction. This insures that the hard limit level does not prevent larger time corrections when they are necessary.

As described, hard limiting removes the error that would be introduced by the magnitude of jitter around the control point. However noise that is outside the control region of the controller, that is, noise with a period appreciably longer than one minute would remain. The time delay corresponding to PPS interrupt latency contains noise of that kind which appears as a slow variation in the latency which changes with processor activity. This latency is compensated in pps-client by a variable called `sysDelay`. The `sysDelay` value directly determines the control point of the controller because the time of the rollover of the second is determined as the reported time of the interrupt minus the value of `sysDelay`.

## Noise {#noise}

The situation with regard to jitter and latency (noise) in the PPS interrupt response is complicated. It is not only necessary to characterize `sysDelay` but also to remove as much of the noise as possible. In order to characterize `sysDelay`, measurements of interrupt latency were made by timing the triggering of the hardware interrupt from a GPIO pin tied across to the GPIO pin that requested the interrupt.  Those measurements revealed that interrupt latency has four distinct components: (1) a constant component corresponding to the minimum time required to process the interrupt interrupt request, (2) an approximately Gaussian random component introduced by random jitter in the system clock oscillator and in the PPS signal, (3) components consisting of intermittently occurring long sequences of constant latency of unknown origin and (4) intermittently occurring spikes of long duration latency.

The constant and average random components of interrupt latency completely characterize `sysDelay` and are determined by incorporating the interrupt latency measurement into the RPi hardware. The pps-client driver that records the reception time of a PPS interrupt also has the ability to measure the initiation and reception times of a test interrupt triggered across a pair of GPIO pins. The time difference in those is used to determine the PPS interrupt latency for a particular RPi 2 and Linux kernel. The median of the latency measurement then becomes the applied `sysDelay` value. This cancels the median of the delay from the PPS rising edge generated by the hard limiting in the controller.

Components (3) and (4) are treated as noise that is to be removed. Component (3) is not always present. Moreover it comes and goes. But when this component is present, it can have a complicated structure. Figure 2 is an example of this.

![Raw Error Distribution](raw-error-distrib.png) 

The figure is a raw error distribution of PPS delay relative to the `sysDelay` value captured from the command line with `pps-client -s "rawError"` and written to `/var/local/pps-rawError-distrib`. Several recurring constant delay components are evident. After the main PPS peak at zero, there is a peak at 6 usecs, another at 21 usecs and another at 27 usecs. These are probably signatures of specific processes running on the RPi. 

The delay peak at 6 usecs differs from the ones at 21 and 27 usecs in that it was found to be strongly correlated with a similar constant delay in timings made by the [interrupt-timer](#the-interrupt-timer-utility) utility. This indicates that this latency component corresponds to an increase in system delay persisting up to several seconds each time it occurs. Thus it was possible to eliminate it by shifting the jitter delay by the delay of the response peak whenever the jitter delay exceeded the controller `noiseLevel` value that delimits the upper limit of random noise at around 3 usecs.

The remaining peaks at 21 and 27 and others like it probably have sources like the sources of the jitter spikes of component (4). A typical jitter spike is evident in the pps-client status printout shown below which shows low jitter values except in the line in the center. 

![Jitter Spike in Status Printout](jitter-spike.png)

Since jitter spikes are easily identified by the length of delay, they are removed by suspending controller time and frequency updates as long as delay duration exceeds the controller `noiseLevel` value mentioned above. 

# The pps-client Controller {#the-pps-client-controller}

With the discussion above as background, the sections below summarize the source code.

The pps-client controller can be thought of as consisting of two conceptual components, a feedback controller and a feedforward compensator.

## Feedback Controller {#feedback-controller}

The pps-client controller algorithm processes timestamps of interrupts from a hardware GPIO pin triggered by the rising edges of a PPS signal. These PPS timestamps are recorded by a kernel device driver, `pps-client.ko`, that was specifically designed to record the timestamps as closely as possible to when the processor responded to the PPS interrupts. This is important because the time at which the system responded to the interrupt minus the time at which it actually toggled the input pin is the interrupt delay that must be compensated by a control point constant, `G.sysDelay`, and this delay must correspond as accurately as possible to that time difference in order to reduce the uncertainty of the delay value.

The `makeTimeCorrection()` routine is the central controller routine and it waits in the timer loop, `waitForPPS()` inside the `readPPS_SetTime()` routine, until a PPS timestamp becomes available from the pps-client device driver. At that instant the timestamp is passed into `makeTimeCorrection()` where the fractional part of the second becomes available as the variable `G.interruptTime` which is converted to the controller error variable as,

G.rawError = G.interruptTime - G.sysDelay

Each `G.rawError` is a time measurement corrupted by jitter. Thus the value of `G.rawError` generated each second can be significantly different from the true time correction. To extract the time correction, `G.rawError` is passed into the `removeNoise()` routine which has accumulated a raw jitter distribution in `detectDelayPeak()` which looks for a [second delay peak](#noise) in the distribution. If the jitter distribution does have a second delay peak following the PPS delay peak and if the incoming `G.rawError` exceeds the `G.noiseLevel` threshold then `G.rawError` is shifed by `G.delayShift`, which is the delay difference between the delay peak and the PPS peak. This momentarily rebiases `G.rawError` to the `G.delayShift` value so that processing is done relative to the new control point determined by `G.delayShift`. On average this corrects for the lengthening of the PPS delay caused by the delay peak. At the same time `G.sysDelay` modified by `G.delayShift` is saved to a file by `writeSysDelay()`. The use of this modified `G.sysDelay` value for interrupt timing significantly reduces the width of interrupt time distributions.

The next processing routine `detectDelaySpike()` determines (when `G.rawError` is sufficiently small) is `G.rawError` spike noise? in which case further processing in the current second is skipped. If it's not spike noise, the average time slew, `G.avgSlew` is updated by `G.rawError`. The `G.avgSlew` value along with the average time correction up to the current second, `G.avgCorrection`, determines the current hard limit value that will be applied in the final noise removal routine, `clampJitter()`. Then `G.rawError` limited by `clampJitter()` is returned from `removeNoise()` as `G.zeroError` which is then modified by the proportional gain value and then sign reversed to generate `G.timeCorrection` for the current second.

The sign reversal of `G.timeCorrection` is necessary in order to provide a proportional control step that subtracts the time correction from the current time slew, making a time slew that is too large smaller and vice versa. That happens by passing `G.timeCorrection` to the system `adjtimex()` routine which slews the time by exactly that value unless the magnitude is greater than about 500 usec, in which case the slew adjustment is restricted to 500 usec by `adjtimex()`. This is usually what happens when pps-client starts. After several minutes of 500 usec steps, `G.timeCorrection` will be in a range to allow the integral control step to begin.

But before the integral control step can begin, an average of the second-by-second time corrections over the previous minute must be available to form the integral. That average is constructed in the `getAverageCorrection()` routine which sequences the time corrections through a circular buffer `G.correctionFifo` and simultaneously generates a rolling sum in `G.correctionAccum` which is scaled to form a rolling average of time corrections that is returned as `G.avgCorrection` by `getAverageCorrection()` each second. At feedback convergence, the rolling sum of *unit* `G.timeCorrection` values makes `G.avgCorrection` the *median* of `G.timeCorrection` values. 

At the end of each minute the integral control step in the `makeAverageIntegral()` routine sums `G.avgCorrection` into one of 10 accumulators `G.integral[i]` each of which accumulates a separate integral that is offset by one second from the others. At the end of the minute those are averaged into `G.avgIntegral`.

Also at the end of the minute (actually after 60 time corrections have been averaged as determined by `integralIsReady()`), `G.avgIntegral` is returned from `getIntegral()` and multiplied by `G.integralGain` to create `G.freqOffset` which, after additional scaling (`ADJTIMEX_SCALE`) that is required by `adjtimex()`, is passed to `adjtimex()` to provide the integral control. 

## Feedforward Compensator {#feedforward-compensator}

The specific purpose of the feedback controller described above is to adjust the system time second by second to satisfy this local "equation of time":

0 = median(G.interruptTime) - G.sysDelay

It does that by setting the local clock so that the difference between `G.sysDelay` and the median of `G.interruptTime` is zero. For this to succeed in adjusting the local time to the PPS, `G.sysDelay` must be the median of the time delay at which the system responded to the rising edge of the PPS interrupt. But the median value of `G.sysDelay` can't be determined by the feedback controller. As indicated by the equation, all the controller can do is satisfy the equation of time.

In order to independently determine the `G.sysDelay` value, a calibration interrupt is made every second immediately following the PPS interrupt. These time measurements are requested from the `pps-client.ko` device driver in the `getInterruptDelay()` routine. That routine calculates `G.intrptDelay` from the time measurements and calls `removeIntrptNoise()` with that value. The `removeIntrptNoise()` routine generates `G.delayMedian`, an estimate of the median of the `G.intrptDelay` value, in a parallel way and using the same routines that `removeNoise()` uses to generate the median of `G.interruptTime`. The `G.delayMedian` value is then assigned to `G.sysDelay`.

## Controller Behavior on Startup {#controller-behavior-on-startup}

Figure 3 shows the behavior of the controller when pps-client is started. The figure shows frequency offset and corresponding time corrections recorded to `/etc/local/pps-offsets` when `pps-offsets=enable` was set in the config file. During the first 120 seconds (not shown in the figure), the controller made time corrections to get the time offset into a reasonable range but made no frequency corrections. Frequency offset correction was enabled at sequence number (second) 120. Over the next 600 seconds the pps-client controller adjusted the frequency offset to bring the system clock frequency into sync with the PPS. But how is it actually doing that?

![Offsets to 720 secs](pps-offsets-to-720.png)

The expanded view of the same data over the first 300 seconds in Figure 4 shows in more detail what is happening. In general, proportional feedback is correcting the time by the (gain-scaled and limited) time error measured each second. The integral feedback is updated each minute with the average of the time corrections over the previous minute. As the integral is formed the result is to move the frequency offset minute by minute towards synchronization. It should be clear that in any minute where the system clock frequency is not perfectly in sync with the PPS, the average length of the second will be either longer or shorter than one second. For example, if the average length of the second differed by 1 microsecond from the true length of the second, that would indicate that the system clock was in error by 1 part per million, in which case an adjustment of the frequency offset by 1 ppm might be expected. 

![Offsets to 300 secs](pps-offsets-to-300.png)

Now notice that, in the minute between sequence number 120 and 180, there is a clearly visible positive bias in the time correction values. Averaging those time corrections (adding them and dividing by 60), gets a 3.22 microsecond average bias indicating that the system clock is in error by 3.22 parts per million over that minute. However, in the next minute (between 180 and 240), the frequency offset is changed by only about 1.8 ppm. In other words, frequency offset change is only about 0.55 of the amount needed to do a full frequency correction and, moreover, it is easily verified that same fractional correction is made in every succeeding minute. 

The 0.55 fractional adjustment is the damping ratio fixed by the integral gain of the PI controller. That damping value has been chosen to keep the loop stable and, in fact, to set it below the maximum acquire rate provided by a ratio of 1 which corresponds to the full frequency correction.

But why not apply the full frequency correction each second? The reason is that the correction is always made too late. It would have been correct for the minute in which it was measured but by the time it has been measured it can only be an estimate for the next minute. If the estimate is in the proper direction but is even slightly too large then the estimation error will be integrated along with the estimate and will become progressively larger in each succeeding minute until the result is an oscillation of the frequency around its stable value. The design decision is that, considering noise and other system uncertainties, it is better to have the controller acquire more slowly with a lower damping value than to risk oscillation with a higher value.

Once the controller has acquired, it continues to average the time errors that occurred over the past minute and to apply the scaled integral of the average as the frequency correction for the next minute. So theoretically the controller never acquires. Rather it is constantly chasing the value to be acquired with a somewhat low estimate of that value. This seems to argue for a [Zeno's paradox](https://en.wikipedia.org/wiki/Zeno%27s_paradoxes). In practice, however, the difference between the estimate and the target value soon drops below the noise so that any practical measurement would indicate that the controller had, indeed, acquired.

The startup transient in Figure 3 is the largest adjustment in frequency the controller ever needs to make and in order to make that adjustment relatively large time corrections are necessary. Once the control loop has acquired however, by design, the time corrections will exceed 1 microsecond only when the controller must make larger than expected frequency offset corrections. In that case, the controller will simply adjust to larger corrections by raising its hard limit level. 

## Performance Under Stress {#performance-under-stress}

To get some idea of what the worst case corrections might be, Figure 5 demonstrates how the pps-client control loop responds to stress. In this case a full processor load (100% usage on all four cores) was suddenly applied at sequence number 1900. The loading raised the processor temperature causing a total shift in frequency offset of about 1.7 ppm from onset to a stable value near sequence number 2500. The time corrections increased to 2 microseconds only in the region of steepest ascent. Since the transients caused by extreme load changes occur infrequently, it is likely that a time correction more than 1 microsecond would only occasionally occur in normal operation. Moreover it is unlikely that a precision time measurement would be required simultaneously with the onset of full processor load.

![PPS offsets to Stress](pps-offsets-stress.png)

## Kernel Device Driver {#kernel-device-driver}

## Error Handling {#error-handling}

All trapped errors are reported to the log file `/var/log/pps-client.log`. In addition to the usual suspects, pps-client also reports interrupt dropouts. Also because sustained dropouts may indicate a fault with the PPS source, there is a provision to allow hardware enunciation of PPS dropouts. Setting the configuration option `alert-pps-lost=enable` will cause RPi GPIO header pin 15 to go to a logic HIGH on loss of the PPS interrupt and to return to a logic LOW when the interrupt resumes.

# Testing {#testing}

The simplest test is to run pps-client and verify with the status printout that the controller locks to the PPS signal to a precision of one microsecond. From a terminal, that can be done at any time while pps-client is running with,

    $ pps-client -v

That runs a secondary copy of pps-client that just displays a status printout that the pps-client daemon continuously generates. When pps-client starts up you can expect to see something like the following in the status printout:

![Status Printout on Startup](StatusPrintoutOnStart.png)

The `jitter` value is showing the fractional second offset of the PPS signal according to the system clock. That value will decrease second by second as the controller locks to the PPS signal. After about 10 minutes the status printout will look like this:

![Status Printout after 10 Min](StatusPrintoutAt10Min.png)

The `jitter` is displaying small numbers. The time of the rising edge of the PPS signal is shown in the second column. The `clamp` value on the far right indicates that the maximum time correction applied to the system clock is being limited to one microsecond. The system clock is synchronized to the PPS signal to a precision of one microsecond.

It can take as long as 20 minutes for pps-client to fully acquire the first time it runs. This happens if the `jitter` shown in the status printout is on the order of 100,000 microseconds or more. It's quite common for the NTP fractional second to be off by that amount. In this case pps-client may restart several times as it slowly reduces the `jitter` offset. That happens because system functions that pps-client calls internally prevent time changes of more than about 500 microseconds each second.

Here are the parameters shown in the status printout:

 * First two columns - date and time of the rising edge of the PPS signal.
 * Third column - the sequence number giving the total PPS interrupts received since pps-client was started.
 * jitter - the time deviation in microseconds recorded at the reception of the PPS interrupt.
 * freqOffset - the frequency offset of the system clock in parts per million of the system clock frequency.
 * avgCorrection - the time corrections (in microseconds) averaged over the previous minute.
 * clamp - the hard limit (in microsecs) applied to the raw time error to convert it to a time correction.

Every sixth line, interrupt delay parameters are also shown. About every 17 minutes, an SNTP time query will be made and the results of that will be shown, but will have no effect unless a time update is required.

To stop the display type ctrl-c.

The pps-client daemon writes the timestamp and sequence number of the PPS rising edge to an in-memory file that changes every second. You can verify that the time is being controlled and that the controller is currently active by entering this a few times:

    $ cat /run/shm/pps-assert

That would generate something like this:

    pi@raspberrypi:~ $ cat /run/shm/pps-assert
    1460044256.000001#173028
    pi@raspberrypi:~ $ cat /run/shm/pps-assert
    1460044259.000000#173031

The timestamp is displayed in seconds to the nearest microsecond. This is probably the most foolproof way of determining that pps-client is currently running. If you get the same numbers twice in succession or none at all you know it's not.

Another way to tell that pps-client is running is to get the process id with,

    $ pidof pps-client

which will only return a PID if pps-client is an active process.

## Performance Evaluation {#performance-evaluation}

Data can be collected while pps-client is running either by setting specific data files to be saved in the pps-client configuration file or by requesting others from the command line of a terminal that is communicating with the RPi.

### Configuration File {#configuration-file}

Data that can be collected using the configuration file is enabled with settings in `/etc/pps-client.conf`. These instruct the pps-client daemon to generate data files, some of which provided the data used to generate the spreadsheet graphs shown on this page and in the project README file. Generating a particular file requires setting a flag. All of these files are disabled by default. But they can be enabled or disabled at any time, including while the pps-client daemon is running, by editing and saving the config file. Here are the flags you can use to enable them:

* `error-distrib=enable` generates `/var/local/pps-error-distrib-forming` which contains the currently forming distribution of time corrections to the system clock. When 24 hours of corrections have been accumulated, these are transferred to `/var/local/pps-error-distrib` which contains the cumulative distribution of time corrections applied to the system clock over 24 hours.

* `jitter-distrib=enable` generates `/var/local/pps-jitter-distrib-forming` which contains the currently forming distribution of jitter values. When 24 hours of corrections have been accumulated, these are transferred to `/var/local/pps-jitter-distrib` which contains the cumulative distribution of all time (jitter) values recorded at reception of the PPS interrupt over 24 hours.

Unless `calibrate=disable` is set in `/etc/pps-client.conf` these files may also be saved:

* `interrupt-distrib=enable` generates `/var/local/pps-intrpt-distrib-forming` which contains the currently forming distribution of calibration interrupt delays. When 24 hours of these have been accumulated they are transferred to `/var/local/pps-intrpt-distrib` which contains a cumulative distribution of recorded calibration interrupt delays that occurred over 24 hours.

* `sysdelay-distrib=enable` generates `/var/local/pps-sysDelay-distrib-forming` which contains the currently forming distribution of `sysDelay` values. When 24 hours of these have been accumulated they are transferred to `/var/local/pps-sysDelay-distrib` which contains a cumulative distribution of `sysDelay` values that were applied to the pps-client controller over 24 hours.

Note that while the turnover interval for some of the files above is given as 24 hours, the interval will usually be longer than 24 hours because pps-client runs on an internal count that does not count lost PPS interrupts or skipped jitter spikes.

### Command Line {#command-line}

Some of the data that can be saved by a running pps-client daemon is of the on-demand type. This is enabled by executing pps-client with the `-s` flag while the daemon is running. For example,

    $ pps-client -s frequency-vars

will return something like this

    pps-client v1.1.0 is running.
    Writing to default file: /var/local/pps-frequency-vars

You can write to a different filename or location by using the `-f` flag followed by the 
desired path and filename:

    $ pps-client -s frequency-vars -f data/freq-vars-01.txt

The specified directories must already exist. You may also include the `-v` flag if you want the status display to start as soon as the requested file is written to disk.

As an aid to remembering what can be requested, omitting the type of data will print a list of what's available. Currently that would result in something like,

    $ pps-client -s
    pps-client v1.1.0 is running.
    Error: Missing argument for -s.
    Accepts any of these:
    rawError
    intrptError
    frequency-vars
    pps-offsets

described as,
* `rawError` writes an exponentially decaying distribution of unprocessed PPS jitter values as they enter the controller. Each jitter value that is added to the distribution has a half-life of one hour. So the distribution is almost completely refreshed every four to five hours.

* `intrptError` writes an exponentially decaying distribution of unprocessed test interrupt delay values. The description is otherwise the same as for `"rawError"`.

* `frequency-vars` writes the last 24 hours of clock frequency offset and Allan deviation of one-minute samples in each five-minute interval indexed by the timestamp at each interval.

* `pps-offsets` writes the previous 10 minutes of recorded time offsets and applied frequency offsets indexed by the sequence number (seq_num) each second.

## Accuracy Validation {#accuracy-validation}

Time accuracy is defined as the absolute time error at any point in time relative to the PPS time clock. Since whole seconds are synchronized by SNTP, this section addresses the fractional second accuracy which is controlled by pps-client. 

It has been determined that the system clock oscillator synchronized by pps-client has a maximum drift on the order of less than 50 nanoseconds per second. That can be verified on any RPi that has been running for at least a day by saving a file of frequency data and examining Allan deviation:

    $ pps-client -s "frequency-vars"

Consequently, any interval measurement of one second or less on the system clock will have an average error smaller than 1 microsecond (on the local system clock). That will, in fact, be true of any time measurement because whole seconds are synchronized to the PPS signal. Absolute accuracy will be a few microseconds because of jitter in the RPi clock oscillator. 

Accuracy validation requires an external source of timing pulses that are connected to the RPi under test (RUT) through its GPIO inputs. These inputs trigger interrupts that are timestamped by the RUT. The interval accuracy of the system clock on an RPi controlled by pps-client made it possible to use an RPi synchronized by pps-client as a time interval generator to test and verify the time accuracy of pps-client running on the RUT.

### The pulse-generator Utility {#the-pulse-generator-utility}

The utility that was used for testing is a small program that is installed along with pps-client. It can generate repeating single pulses or pulse pairs each second at times specified in microseconds. To use the utility, load the driver, specifying the GPIO outputs or single output that will provide the pulses. (Pulses have 10 usecs duration so they should be spaced no closer than about 15 usecs.)

    $ pulse-generator load-driver <GPIO_num1> [GPIO_num2]

Then run the utility, specifying the times of the output pulses with,

    $ pulse-generator <time1> [time2]

When finished, unload the driver with

    $ pulse-generator unload-driver

Pulses are generated by a kernel driver in a spin loop that constantly checks the system time with a kernel function `do_gettimeofday()` that returns the time within about half a microsecond so that, as soon as the specified times occur, the GPIO output(s) are asserted. There is a small constant delay in transferring each pulse to the output. But in two pulse mode, where the critical time interval is the interval between the first and second pulse, the delays in these cancel so that the interval in microseconds is the difference between the specified times with an error of less than 1 microsecond as measured on the local clock. Unavoidably, because of jitter in the local clock, pulse-generator also has a random error from second to second that is invisible to the clock on the RPi that is the pulse generator. This error is on the order of a few microseconds standard deviation. That error will be quantified in the measurements made below.

### The interrupt-timer Utility {#the-interrupt-timer-utility}

To time interrupts generated by external pulses, a second utility, "interrupt-timer" is provided that is also installed on the RPi along with pps-client. With no command line args, interrupt-timer prints the reception time of an external interrupt to the terminal but also records a distribution of the fractional seconds part of the time to a file `/var/local/timer-distrib-forming` that is copied to `/var/local/timer-distrib` every 24 hours. This timing-collection mode was used to validate the accuracy of pps-client. For the details see the "Testing Accuracy" section below.

The interrupt-timer can be run from the command line of any terminal connected to the RPi. To use it, load the driver specifying the GPIO number of the pin that will provide the interrupt:

    $ sudo interrupt-timer load-driver <GPIO number>

Then run the `interrupt-timer` with,

    $ sudo interrupt-timer

You can stop it with ctrl-c. When finished, unload the driver with,

    $ sudo interrupt-timer unload-driver

Under the hood, interrupt-timer compensates for system interrupt delay by reading the `sysDelay` value recorded by pps-client and correcting the time of the interrupt by `sysDelay`.

### Testing Accuracy {#testing-accuracy}

The absolute limit to time accuracy on any processor that uses a conventional integrated circuit crystal oscillator is flicker noise in the oscillator. At the 1 Hz operating frequency of the pps-client controller, flicker noise is apparent as the random component of second-to-second jitter. To minimise the effects of flicker noise, accuracy testing consists of making a large number of independent time interval measurements and then statistically evaluating the results. This reduces the average error in the oscillators of both the RPi under test (RUT) and one used to provide timing pulses. The pulse-generator utility runs in two-pulse mode on an RPi identified as RPi-1. The interrupt-timer utility is used on the RUT, RPi-2, to time the reception of pulse 2 from pulse-generator while the pulse 1 provides the PPS signal to the RUT. 

Validation is successful if the average time of reception of pulse 2 is equal to the time difference between the times of pulse 2 and pulse 1 with sufficiently small error.

In the tests, GPIO_23 and GPIO_25 were used for pulse 1 and pulse 2, respectively. Pulse 1 provides the PPS signal to the RUT and pulse 2 goes to the GPIO input that is timed by interrupt-timer. The connections look like this:

    RPi-1 GPIO_23 ---> RPi-2 GPIO_4 (PPS input)
    RPi-1 GPIO_25 ---> RPi-2 GPIO_24 (interrupt-timer input)

After running pps-client on RPi-1 for 20 minutes, the pulse-generator was started on RPi-1 with pulses at 100 usec and 900 usec with,

    RPi-1:~ $ sudo pulse-generator load-driver 23 25
    RPi-1:~ $ sudo pulse-generator -p 100000 900000

Infrequently, system latency will delay the generation of a pulse by pulse-generator. To prevent that from affecting the measurements of interrupt times on the RUT, a file share between the two RPis was set up using Network File System (NFS) to provide a shared file, `/mnt/usbstorage/PulseVerify` on a USB disk. The pulse-generator on RPi-1 checked each pulse and recorded the pulse status to the file as one of the asci text values 1 (delayed), 2 (missing) or 3 (on time). On RPi-2 after receiving the pulse, interrupt-timer read the number from the file and recorded the pulse time only if the status was 3.

The pps-client was next started on RPi-2 and was allowed to settle for 20 minutes. At that point the interrupt-timer utility was started on RPi-2 with

    RPi-2:~ $ sudo interrupt-timer load-driver 24

Then interrupt-timer was started in timing collection mode and was allowed to collect interrupt times from GPIO 24 on RPi-2 with

    RPi-2:~ $ sudo interrupt-timer

0 = median(G.interruptTime) - G.sysDelay

It does that by setting the local clock so that the difference between `G.sysDelay` and the median of `G.interruptTime` is zero. For this to succeed in adjusting the local time to the PPS, `G.sysDelay` must be the median of the times at which the system responded to the rising edge of the PPS interrupt. But the median value of `G.sysDelay` can't be determined by the feedback controller. As shown by the equation, all the controller can do is adjust local time to satisfy the equation.

In order to independently determine the `G.sysDelay` value, a calibration interrupt is made every second immediately following the PPS interrupt. These time measurements are requested from the `pps-client.ko` device driver in the `getInterruptDelay()` routine. That routine calculates `G.intrptDelay` from the time measurements and calls `removeIntrptNoise()` with that value. The `removeIntrptNoise()` routine generates `G.delayMedian`, an estimate of the median of the `G.intrptDelay` value, in a parallel way and using the same routines that `removeNoise()` uses to generate the median of `G.interruptTime`. The `G.delayMedian` value is then assigned to `G.sysDelay`.



