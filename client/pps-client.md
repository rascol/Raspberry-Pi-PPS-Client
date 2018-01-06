# Raspberry Pi PPS Client Documentation (rev. c) {#mainpage}

- [Uses](#uses)
- [How To Do High Accuracy Timekeeping](#how-to-do-high-accuracy-timekeeping)
  - [Noise](#noise)
    - [Raspberry Pi 2](#raspberry-pi-2)
    - [Raspberry Pi 3](#raspberry-pi-3)
    - [Jitter Spikes](#jitter-spikes)
- [The PPS-Client Controller](#the-pps-client-controller)
  - [Feedback Controller](#feedback-controller)
  - [Feedforward Compensator](#feedforward-compensator)
  - [Driver](#driver)
  - [Controller Behavior on Startup](#controller-behavior-on-startup)
  - [Performance Under Stress](#performance-under-stress)
  - [Error Handling](#error-handling)
- [Testing](#testing)
  - [Performance Evaluation](#performance-evaluation)
    - [Configuration File](#configuration-file)
    - [Command Line](#command-line)
  - [Accuracy Validation](#accuracy-validation)
    - [The pulse-generator Utility](#the-pulse-generator-utility)
    - [The interrupt-timer Utility](#the-interrupt-timer-utility)
    - [The NormalDistribParams Utility](#normaldistribparams-utility)
    - [Testing Accuracy](#testing-accuracy)
      - [Test Setup](#test-setup)
      - [Test Results](#test-results)

# Uses {#uses}

The PPS-Client source code is the reference design and proof of concept for a general technique that provides high accuracy timekeeping. It was implemented on a relatively slow ARM processor which should illustrate that a similar software solution can be successful on almost any application processor.

Although the goal of high accuracy computer timekeeping, evident from even a cursory search on the Internet, has been around since at least the introduction of the Network Time Protocol, general support for high precision timekeeping over the Internet is still not available. However, GPS reception is available everywhere and in conjunction with a daemon like PPS-Client can be used for that purpose now. Indeed, an Internet search found commercial GPS repeaters that can bring GPS reception indoors with local coverage up to at least 30 meters.

The ability to time synchronize multiple computers with microsecond accuracy is particularly important for small embedded processors like the RPi that can be used to used to construct distributed systems with a large number of individual cores. This works very well when the individual RPis are time synchronized because that makes applications possible that would otherwise be difficult or impractical. One example is a multiple video camera system that synchronizes the video frames from the individual RPis each handling a different camera. In this kind of application a robot with twin cameras for eyes could easily synchronize the cameras from the system time on each RPi.

There are other uses for time synchronized computers: Network administrators might find it useful to have the possibility of making one-way, single-path time measurements of network delays. That becomes possible if the computers at the endpoints are accurately synchronized to GPS time - which is rarely the case at present. Also, certain kinds of scientific applications require accurate synchronization to a GSP clock. One sometimes of interest to amateur astronomers is [occultation timing](https://en.wikipedia.org/wiki/Occultation). Another is collection of distributed seismic data with remote computers in the study of earthquakes or for substratum mapping. If computers were routinely provided with microsecond-accuracy timekeeping other applications would soon appear.

# How To Do High Accuracy Timekeeping {#how-to-do-high-accuracy-timekeeping}

The PPS-Client daemon is implemented as a [proportional-integral (PI) controller](https://en.wikipedia.org/wiki/PID_controller) (a.k.a. type 2 Servo) with proportional and integral feedback provided each second but with the value of the integral feedback adjusted once per minute. The PI controller model provides a view of time synchronization as a linear feedback system in which gain coefficients can be adjusted to provide a good compromise among stability, transient response and amount of noise introduced in the error signal. 

The error signal is the time difference between the one-second interval provided by a PPS signal and the length of the second reported by the system clock. The noise of concern is the second-to-second variation in time reported by the system because of the corresponding variation of system delay in responding to the PPS interrupt. This variable component of the delay, referred to as jitter, is a combination of clock oscillator jitter, PPS signal jitter and system latency in responding the the interrupt.

Because the error signal has jitter and is being used to control synchronization, the jitter component has the effect of a **false error signal** that causes the time and frequency synchronization to fluctuate as the controller attempts to follow the jitter. The conventional approach to reducing jitter is to low-pass or median filter the error signal. But filtering has the serious disadvantage that reduction of the jitter must be traded off against time delay introduced by the filter. Additional time delay in the feedback loop inevitably degrades controller performance. The PPS-Client program uses a much better technique that introduces **no** delay. To reduce the jitter, the time values returned by the system are passed through a hard limiter that clips them before applying them as time corrections.

Indeed, individual time corrections constitute jitter added to a very small time correction. Consequently, each individual correction is mostly jitter and thus is wrong by nearly the amount that it deviates from zero. Because of that, the limiting employed in this system clips the maximum time corrections when the controller has fully stabilized to the time resolution of the system clock, 1 microsecond.

It might seem that such extreme limiting would remove the desired error signal along with the noise. But that doesn't happen because the true time error is a slowly varying quasi-stationary (DC) level. Limiting only slices off the dynamic (AC) component of the error signal. The DC component remains. (To see what limiting does see Figures 3, 4, and 5 and the relevant discussion.) If the jitter were not limited, the controller would make the sum of the positive and negative jitter zero. That would be undesirable even after filtering because the noise has significant components in time periods that extend to well beyond one minute. Filtering would remove noise only for time intervals within the cut-off region of the filter. Longer period noise would remain. 

On the other hand, instead of zeroing the **sum** of negative and positive jitter and thereby allowing the difference to be introduced as noise, hard limiting causes the controller to make the **number** of positive and negative excursions to be equal around zero. That happens because the clipped positive and negative amplitudes are identical (1 microsecond). Thus, making the sum zero makes the count equal. As a result, the varying magnitude of the jitter around the control point is ignored and the reported time delay of the PPS rising edge adjusts to its median value, i.e., the delay at which there were as many shorter as longer reported delays over the previous minute.

The only disadvantage of hard limiting is that it reduces the amount of time correction that can be applied each each second. But that limitation is easily circumvented by allowing the hard limit level to track the amount of required time correction. This insures that the hard limit level does not prevent larger time corrections when they are necessary.

As described, hard limiting removes the error that would be introduced by the magnitude of jitter around the control point. However noise that is outside the control region of the controller, that is, noise with a period appreciably longer than one minute would remain. The time delay corresponding to PPS interrupt latency contains noise of that kind which appears as a slow variation in the latency which changes with processor activity (and flicker noise). This latency is corrected in PPS-Client by **measuring and compensating for** the system latency with a variable called `sysDelay`. The `sysDelay` value directly determines the control point of the controller because the time of the roll over of the second is determined as the reported time of the interrupt **minus** the value of `sysDelay`.

## Noise {#noise}

As an introduction to how PPS-Client corrects for the varying value of system latency with `sysDelay`, it is useful to consider the nature of the noise in more detail because measurements revealed that system latency is a combination of a static **constant component** and three identifiable noise components. The constant component is just the minimum time required to process the PPS interrupt request which is determined by the frequency of the program clock and the minimum number of program steps required to generate the PPS interrupt. The noise components are

1. a **variable delay** component introduced primarily by 
	
    a. changes in the number of program steps to generate the interrupt caused by branching in the scheduler as the program load changes from second to second and
    
    b. slow drift (and occasionally abrupt micro-shifts) in the system clock oscillator frequency caused by the 1/f nature of [flicker noise](https://en.wikipedia.org/wiki/Flicker_noise) in the clock oscillator,

2. an approximately **Gaussian** random component composed of 

	a. second-by-second delay changes introduced by program branching as in 1.a above with a positive-only noise contribution,
    
    b. jitter from flicker noise in the system clock oscillator and 
    
    c. jitter in the PPS signal source.

3. sporadically occurring **spikes** of longer duration latency caused by less-than-optimal OS performance as other system applications or kernel processes take time from real-time applications like PPS-Client.

For reference these will be called Type 1, Type 2 and Type 3 noise components. The **constant** component, the Type 1 **variable delay** component and the median of the Type 2 **Gaussian** component completely characterize the `sysDelay` value that is applied in the controller. As described previously, the **Gaussian** component is reduced to its median with a residual 1 microsecond jitter around the control point. The **constant** and **variable delay** components are determined by incorporating a system latency measurement into the PPS-Client code.

The latency measurement is calculated as the median of the time difference between the initiation and reception times of a calibration interrupt triggered across a pair of GPIO pins in the PPS-client driver. Doing a direct measurement in this way guarantees that all of the static and time-varying contributions to processing delay are corrected by the resulting `sysDelay` value. More details are provided below in the description of the [Feedforward Compensator](#feedforward-compensator).

That accounts for the Type 1 and Type 2 noise components. Type 3, described as latency **spikes**, is treated as [noise that is to be removed](#jitter-spikes) by simply excluding it from the controller feedback loop.

To see how this presents in practice, typical measured performance of Raspberry Pi processors is shown next. While qualitatively similar, the quantitative noise characteristics of Raspberry Pi 2 and Raspberry Pi 3 are sufficiently different that they are described in separate sections.

### Raspberry Pi 2 {#raspberry-pi-2}

Raspberry Pi 2 Type 2 noise is shown in Figure 1a for an RPi 2 unit showing typical characteristics from a batch of 5 test units. 

<a name="timed-event"></a>
![Raspberry Pi 2 Jitter Distribution](pps-jitter-distrib.png) 

Figure 1a, the internal PPS signal jitter, is data that was captured from the test unit over 24 hours to the file `/var/local/pps-jitter-distrib` by setting `jitter-distrib=enable` in `/etc/pps-client.conf` and is typical data that can be easily generated on any RPi 2.

Figure 1a shows a delay peak at zero (relative to the current `sysDelay` value) followed by infrequent sporadic Type 3 latency **spikes** in the log plot. The nearly **Gaussian** distribution of the Type 2 noise is also quite evident. This distribution is a combination of all three components of Type 2 noise.

Of the three components of Type 2 noise, it is possible to see the Type 2.a noise separately from the others in a plot of the calibration interrupt. Because this interrupt is generated entirely within the RPi 2, it is unaffected by PPS source jitter. Nor is it affected by flicker noise in the clock oscillator because the measurement is made internal to the processor. Any time measurement made totally within the processor is blind to the flicker noise of its own clock oscillator because time is being measured relative to a time reference warped by flicker noise.

In Figure 1b the Type 2 Gaussian noise of Figure 1a is superimposed on a distribution of calibration interrupt jitter.

![Calibration Interrupt and PPS Interrupt Delay Comparison](interrupt-delay-comparison.png)

The calibration interrupt delay distribution was captured in the file `/var/local/pps-intrpt-distrib` by setting `interrupt-distrib=enable` in `/etc/pps-client.conf`. Also The jitter was collected while `sysDelay` took on the value of 8 μsecs with 45,343 samples collected.

If, as argued, the calibration interrupt noise is entirely Type 2.a noise, it should be skewed to the right since it should consist predominantly of the `sysDelay` value along with sporadically occurring longer Type 2.a processing delays. Looking closely at the distribution, that is probably the case if in fact the `sysDelay` value is actually closer to about 7.6 to 7.8 microseconds (Time can be only be resolved whole microseconds here but the true delays can have nanosecond resolution).

### Raspberry Pi 3 {#raspberry-pi-3}

Raspberry Pi 3 PPS Type 2 noise is shown in Figure 2a for an RPi 3 unit showing typical characteristics from a batch of 10 test units.

![Raspberry Pi 3 Jitter Distribution](pps-jitter-distrib-RPi3.png)

Figure 2a is internal PPS signal jitter that was captured from the test unit over 24 hours to the file `/var/local/pps-jitter-distrib` by setting `jitter-distrib=enable` in `/etc/pps-client.conf` and is typical data that is easily generated on any RPi 3.

As was done for the RPi 2, the RPi 3 internal PPS jitter captured in Figure 2a is superimposed on a distribution of calibration interrupt jitter in Figure 2b.

![Calibration Interrupt and PPS Interrupt Delay Comparison](interrupt-delay-comparison-RPi3.png)

The calibration interrupt delay distribution was captured in the file `/var/local/pps-intrpt-distrib` by setting `interrupt-distrib=enable` in `/etc/pps-client.conf`. This was data collected while sysDelay took on the value of 5 μsecs with 50,212 samples collected.

It is quite evident from Figure 2b that the calibration interrupt delay is indeed completely skewed to the right as would be expected if it is Type 2.a processing-delay jitter. In this example, the true `sysDelay` value happened to be very close to 5 microseconds. 

Because these distributions approximate a [Gaussian distribution](#https://en.wikipedia.org/wiki/Normal_distribution) or half-Gaussian distribution for Type 2.a calibration delay jitter, a utility program, [NormalDistribParams](#normaldistribparams-utility), that is provided with the PPS-Client package can provide approximate Standard Deviation (SD) values from the discrete values shown in the distributions. This utility can be used to identify the contributions of some of the components of the Type 2 noise in the internal PPS jitter which contains all three.

For the RPi 2 test unit, the SD of the internal PPS jitter in Figure 1a was <I>σ<sub> t</sub></I> = 1.02 microsecond while the SD of the calibration interrupt was about <I>σ</I><sub> 1</sub> = 0.79 microsecond (45,343 samples) with relative fits to Gaussians of 0.99. 

Since the SDs of Gaussian distributions add in root-sum-square fashion, 

<center><I>σ<sub> t</sub></I><sup> 2</sup> = <I>σ</I><sub> 1</sub><sup> 2</sup> + <I>σ</I><sub> 2</sub><sup> 2</sup></center>

the combined flicker noise and PPS source jitter in the RPi 2 was calculated to be <I>σ</I><sub>2</sub> = **0.65** microsecond SD. 

On repeating the calculation for the RPi 3 unit with PPS signal jitter <I>σ<sub> t</sub></I> = 0.82 microsecond and calibration interrupt jitter <I>σ</I><sub> 1</sub> = 0.45 microsecond (50,212 samples) with relative fits to Gaussian distributions of 0.98, the calculated combined flicker noise and PPS source jitter was about <I>σ</I><sub>2</sub> = **0.69** microsecond SD.

In summary the combined clock oscillator flicker noise and PPS source jitter in our test of an RPi 2 processor and RPi 3 processor was on the order of 0.7 microsecond SD with good agreement between both processors. 

Type 2.a processing delay jitter contributed about 0.8 microsecond SD in the RPi 2 unit but only about 0.45 microsecond SD in the RPi 3 unit, clearly showing the RPi 3 processor to require significantly fewer clock cycles than the RPi 2 to process interrupts.

### Jitter Spikes {#jitter-spikes}

A typical jitter spike is evident in the PPS-Client status printout shown below which shows low jitter values except for the delay spike in the middle line of the image.

![Jitter Spike in Status Printout](jitter-spike.png)

Since jitter spikes are easily identified by the length of delay, they are removed by suspending controller time and frequency updating when delay duration exceeds the controller `noiseLevel` value mentioned above which is hardcoded to 3 microseconds. From the SD values of the Gaussian Type 2 noise determined above, it is the jitter starting at 3 to 4 standard deviations from the center of the Gaussian Type 2 noise region that is treated as removable jitter spikes.

# The PPS-Client Controller {#the-pps-client-controller}

With previous discussion as background, the sections below provide a cross-referenced summary of the source code. While Doxygen does a credible job of cross-referencing, this summary probably only makes sense while referencing a side-by-side open copy of the source code set up as a project in a cross-referencing editor like [Eclipse](http://www.eclipse.org/cdt/).

The PPS-Client controller can be thought of as consisting of two conceptual components, a feedback controller and a feedforward compensator.

## Feedback Controller {#feedback-controller}

The PPS-Client controller algorithm processes timestamps of interrupts from a hardware GPIO pin triggered by the rising edges of a PPS signal. These PPS timestamps are recorded by a kernel device driver, `pps-gps-io.ko`, that was specifically designed to record the timestamps as closely as possible to when the processor responded to the PPS interrupts. This is important because the time at which the system responded to the interrupt minus the time at which it actually toggled the input pin is the interrupt delay that must be compensated by the control point constant, `G.sysDelay`, and this delay must correspond as accurately as possible to that time difference in order to reduce the uncertainty of the delay value.

The `makeTimeCorrection()` routine is the central controller routine and it waits in the timer loop of the function, `waitForPPS()` and inside the `readPPS_SetTime()` routine, until a PPS timestamp becomes available from the PPS-Client device driver. At that instant the timestamp is passed into `makeTimeCorrection()` where the fractional part of the second becomes available as the variable `G.interruptTime` which is converted to the controller error variable as,

G.rawError = G.interruptTime - G.sysDelay

Each `G.rawError` is a time measurement corrupted by jitter. Thus the value of `G.rawError` generated each second can be significantly different from the true time correction. To extract the time correction, `G.rawError` is passed into the `removeNoise()` routine that contains the first noise processing routine, `detectDelaySpike()`, that determines (when `G.rawError` is sufficiently small) is `G.rawError` spike noise? in which case further processing in the current second is skipped. If it's not spike noise, the average time slew, `G.avgSlew` is updated by `G.rawError`. The `G.avgSlew` value along with the average time correction up to the current second, `G.avgCorrection`, determines the current hard limit value that will be applied in the final noise removal routine, `clampJitter()`. Then `G.rawError` limited by `clampJitter()` is returned from `removeNoise()` as `G.zeroError` which is then modified by the proportional gain value and then sign reversed to generate `G.timeCorrection` for the current second.

The sign reversal on `G.timeCorrection` is necessary in order to provide a proportional control step that subtracts the time correction from the current time slew, making a time slew that is too large smaller and vice versa. That happens by passing `G.timeCorrection` to the system `adjtimex()` routine which slews the time by exactly that value unless the magnitude is greater than about 500 μsecs, in which case the slew adjustment is restricted to 500 μsecs by `adjtimex()`. This is usually what happens when PPS-Client starts. After several minutes of 500 μsec steps, `G.timeCorrection` will be in a range to allow the integral control step to begin.

But before the integral control step can begin, an average of the second-by-second time corrections over the previous minute must be available to form the integral. That average is constructed in the `getAverageCorrection()` routine which sequences the time corrections through a circular buffer `G.correctionFifo` and simultaneously generates a rolling sum in `G.correctionAccum` which is scaled to form a rolling average of time corrections that is returned as `G.avgCorrection` by `getAverageCorrection()` each second. At feedback convergence, the rolling sum of *unit* `G.timeCorrection` values makes `G.avgCorrection` the *median* of `G.timeCorrection` values. 

At the end of each minute the integral control step in the `makeAverageIntegral()` routine sums `G.avgCorrection` into one of 10 accumulators `G.integral[i]` each of which accumulates a separate integral that is offset by one second from the others. At the end of the minute those are averaged into `G.avgIntegral`.

Also at the end of the minute (actually after 60 time corrections have been averaged as determined by `integralIsReady()`), `G.avgIntegral` is returned from `getIntegral()` and multiplied by `G.integralGain` to create `G.freqOffset` which, after scaling by `ADJTIMEX_SCALE` that is required by `adjtimex()`, is passed to `adjtimex()` to provide the integral control. 

## Feedforward Compensator {#feedforward-compensator}

The specific purpose of the feedback controller described above is to adjust the system time second by second to satisfy this local "equation of time":

0 = median(G.interruptTime) - G.sysDelay

It does that by setting the local clock so that the difference between `G.sysDelay` and the median of `G.interruptTime` is zero. For this to succeed in adjusting the local time to the PPS, `G.sysDelay` must be the median of the time delay at which the system responded to the rising edge of the PPS interrupt. But the median value of `G.sysDelay` can't be determined by the feedback controller. As indicated by the equation, all the controller can do is satisfy the equation of time.

In order to independently determine the `G.sysDelay` value, a calibration interrupt is made every second immediately following the PPS interrupt. These time measurements are requested from the `gps-pps-io.ko` device driver in the `getInterruptDelay()` routine. That routine calculates `G.intrptDelay` from the time measurements and calls `removeIntrptNoise()` with that value. The `removeIntrptNoise()` routine generates `G.delayMedian`, an estimate of the median of the `G.intrptDelay` value, in a parallel way and using the same routines that `removeNoise()` uses to generate the median of `G.interruptTime`. The `G.delayMedian` value is then assigned to `G.sysDelay`.

## Driver {#driver}

It should be evident by now that the PPS-Client deamon was written almost entirely in user space. That made the daemon much easier to design and test. Moreover it makes the code very easy to maintain and to customize for different processors, different flavors of Linux and maybe eventually different operating systems. Indeed, it would have be preferable to do all of the code in user space. However, the controller needs to capture timestamps of the PPS interrupt and the calibration interrupt with the shortest possible time delays between the events and recording the time stamps of them. Currently, this can only be realized by capturing the timestamps in kernel space. 
 

## Controller Behavior on Startup {#controller-behavior-on-startup}

Figure 3 shows the behavior of the controller when PPS-Client is started. The figure shows frequency offset and corresponding time corrections recorded to `/etc/local/pps-offsets` when saving this file is requested from the [command line](#command-line) as `pps-offsets`.

During the first 120 seconds (not shown in the figure), the controller made time corrections to get the time offset into a reasonable range but made no frequency corrections. Frequency offset correction was enabled at sequence number (second) 120. Over the next 600 seconds the PPS-Client controller adjusted the frequency offset to bring the system clock frequency into sync with the PPS.

![Offsets to 720 secs](pps-offsets-to-720.png)

The expanded view of the same data over the first 300 seconds in Figure 4 shows in more detail what is happening. In general, proportional feedback is correcting the time by the (gain-scaled and limited) time error measured each second. The integral feedback is updated each minute with the average of the time corrections over the previous minute. As the integral is formed the result is to move the frequency offset minute by minute towards synchronization. It should be clear that in any minute where the system clock frequency is not perfectly in sync with the PPS, the average length of the second will be either longer or shorter than one second. For example, if the average length of the second differed by 1 microsecond from the true length of the second, that would indicate that the system clock was in error by 1 part per million, in which case an adjustment of the frequency offset by 1 ppm might be expected. 

![Offsets to 300 secs](pps-offsets-to-300.png)

Now notice that, in the minute between sequence number 120 and 180, there is a clearly visible positive bias in the time correction values. Averaging those time corrections (literally adding them from the figure and dividing by 60), gets a 3.22 microsecond average bias indicating that the system clock is in error by 3.22 parts per million over that minute. However, in the next minute (between 180 and 240), the frequency offset is changed by only about 1.8 ppm. In other words, frequency offset change is only about 0.55 of the amount needed to do a full frequency correction and, moreover, it is easily verified that same fractional correction is made in every succeeding minute. 

The 0.55 fractional adjustment is the damping ratio fixed by the integral gain of the PI controller. That damping value has been chosen to keep the loop stable and, in fact, to set it below the maximum acquire rate provided by a ratio of 1 which corresponds to the full frequency correction.

But why not apply the full frequency correction each second? The reason is that the correction is always made too late. It would have been correct for the minute in which it was measured but by the time it has been measured it can only be an estimate for the next minute. If the estimate is even slightly too large then the estimation error will be integrated along with the estimate and will become progressively larger in each succeeding minute until the result is an oscillation of the frequency around its stable value. The design decision is that, considering noise and other system uncertainties, it is better to have the controller acquire more slowly with a lower damping value than to risk oscillation with a higher value.

Once the controller has acquired, it continues to average the time errors that occurred over the past minute and to apply the scaled integral of the average as the frequency correction for the next minute. So theoretically the controller never acquires. Rather it is constantly chasing the value to be acquired with a somewhat low estimate of that value. This seems to argue for a [Zeno's paradox](https://en.wikipedia.org/wiki/Zeno%27s_paradoxes). In practice, however, the difference between the estimate and the target value soon drops below the noise level so that any practical measurement would indicate that the controller had, indeed, acquired.

The startup transient in Figure 3 is the largest adjustment in frequency the controller ever needs to make and in order to make that adjustment relatively large time corrections are necessary. Once the control loop has acquired, however, then by design the time corrections will exceed 1 microsecond only when the controller must make larger than expected frequency offset corrections. In that case, the controller will simply adjust to larger corrections by raising its hard limit level. 

## Performance Under Stress {#performance-under-stress}

To get some idea of what the worst case corrections might be, Figure 5 demonstrates how the PPS-Client control loop responds to stress. In this case a full processor load (100% usage on all four cores) was suddenly applied at sequence number 1900. The loading raised the processor temperature causing a total shift in frequency offset of about 1.7 ppm from onset to a stable value near sequence number 2500. The time corrections increased to 2 microseconds only in the region of steepest ascent. Since the transients caused by extreme load changes occur infrequently, it is likely that a time correction more than 1 microsecond would only occasionally occur in normal operation. Moreover it is unlikely that a precision time measurement would be required simultaneously with the onset of full processor load.

![PPS offsets to Stress](pps-offsets-stress.png)

## Error Handling {#error-handling}

All trapped errors are reported to the log file `/var/log/pps-client.log`. In addition to the usual suspects, PPS-Client also reports interrupt dropouts. Also because sustained dropouts may indicate a fault with the PPS source, there is a provision to allow hardware enunciation of PPS dropouts. Setting the configuration option `alert-pps-lost=enable` will cause RPi GPIO header pin 15 to go to a logic HIGH on loss of the PPS interrupt and to return to a logic LOW when the interrupt resumes.

# Testing {#testing}

Before performing any test, make sure that the test environment is clean. At a minimum, if not starting fresh, **reboot the `RPi`s that are being used in the tests**. This can eliminate a lot of unexpected problems.

The simplest test is to run PPS-Client and verify with the status printout that the controller locks to the PPS signal to a precision of one microsecond. From a terminal, that can be done at any time while PPS-Client is running with,

    $ pps-client -v

That runs a secondary copy of PPS-Client that just displays a status printout that the PPS-Client daemon continuously generates. When PPS-Client starts up you can expect to see something like the following in the status printout:

![Status Printout on Startup](StatusPrintoutOnStart.png)

The `jitter` value is showing the fractional second offset of the PPS signal according to the system clock. That value will decrease second by second as the controller locks to the PPS signal. After about 10 minutes the status printout will look like this:

![Status Printout after 10 Min](StatusPrintoutAt10Min.png)

The `jitter` is displaying small numbers. The time of the rising edge of the PPS signal is shown in the second column. The `clamp` value on the far right indicates that the maximum time correction applied to the system clock is being limited to one microsecond. The system clock is synchronized to the PPS signal to a precision of one microsecond.

It can take as long as 20 minutes for PPS-Client to fully acquire the first time it runs. This happens if the `jitter` shown in the status printout is on the order of 100,000 microseconds or more. It's quite common for the NTP fractional second to be off by that amount. In this case PPS-Client may restart several times as it slowly reduces the `jitter` offset. That happens because the system function `adjtimex()` that PPS-Client calls internally prevents time changes of more than about 500 microseconds each second.

Here are the parameters shown in the status printout:

 * First two columns - date and time of the rising edge of the PPS signal.
 * Third column - the sequence number giving the total PPS interrupts received since PPS-Client was started.
 * `jitter` - the time deviation in microseconds recorded at the reception of the PPS interrupt.
 * `freqOffset` - the frequency offset of the system clock in parts per million of the system clock frequency.
 * `avgCorrection` - the time corrections (in microseconds) averaged over the previous minute.
 * `clamp` - the hard limit (in microseconds) applied to the raw time error to convert it to a time correction.

Every sixth line, interrupt delay parameters are also shown. If SNTP is being used to provide whole-second time of day then about every 17 minutes, an SNTP time query will be made and the results of that will be shown, but will have no effect unless a time update is required. Similarly if GPS is used to provide whole seconds, a "Requesting a GPS time check" message will appear about every 10 minutes.

To stop the display type ctrl-c.

The PPS-Client daemon writes the timestamp and sequence number of the PPS rising edge to an in-memory file that changes every second. You can verify that the time is being controlled and that the controller is currently active by entering this a few times:

    $ cat /run/shm/pps-assert

That would generate something like this:

    pi@raspberrypi:~ $ cat /run/shm/pps-assert
    1460044256.000001#173028
    pi@raspberrypi:~ $ cat /run/shm/pps-assert
    1460044259.000000#173031

The timestamp is displayed in seconds to the nearest microsecond. This is probably the most foolproof way of determining that PPS-Client is currently running. If you get the same numbers twice in succession or none at all you know it's not.

Another way to tell that PPS-Client is running is to get the process id with,

    $ pidof pps-client

which will only return a PID if PPS-Client is an active process.

## Performance Evaluation {#performance-evaluation}

Data can be collected while PPS-Client is running either by setting specific data files to be saved in the PPS-Client configuration file or by requesting others from the command line of a terminal that is communicating with the RPi.

### Configuration File {#configuration-file}

Data that can be collected using the configuration file is enabled with settings in `/etc/pps-client.conf`. These instruct the PPS-Client daemon to generate data files, some of which provided the data used to generate the spreadsheet graphs shown on this page and in the project README file. Generating a particular file requires setting a flag. All of these files are disabled by default. But they can be enabled or disabled at any time, including while the PPS-Client daemon is running, by editing and saving the config file. Here are the flags you can use to enable them:

* `error-distrib=enable` generates `/var/local/pps-error-distrib-forming` which contains the currently forming distribution of time corrections to the system clock. When 24 hours of corrections have been accumulated, these are transferred to `/var/local/pps-error-distrib` which contains the cumulative distribution of time corrections applied to the system clock over 24 hours.

* `jitter-distrib=enable` generates `/var/local/pps-jitter-distrib-forming` which contains the currently forming distribution of jitter values. When 24 hours of corrections have been accumulated, these are transferred to `/var/local/pps-jitter-distrib` which contains the cumulative distribution of all time (jitter) values recorded at reception of the PPS interrupt over 24 hours.

* `interrupt-distrib=enable` generates `/var/local/pps-intrpt-distrib-forming` which contains the currently forming distribution of calibration interrupt delays. When 24 hours of these have been accumulated they are transferred to `/var/local/pps-intrpt-distrib` which contains a cumulative distribution of recorded calibration interrupt delays that occurred over 24 hours.

* `sysdelay-distrib=enable` generates `/var/local/pps-sysDelay-distrib-forming` which contains the currently forming distribution of `sysDelay` values. When 24 hours of these have been accumulated they are transferred to `/var/local/pps-sysDelay-distrib` which contains a cumulative distribution of `sysDelay` values that were applied to the PPS-Client controller over 24 hours.

Note that while the turnover interval for some of the files above is given as 24 hours, the interval will usually be slightly longer than 24 hours because PPS-Client runs on an internal count, `G.activeCount`, that does not count lost PPS interrupts or skipped jitter spikes.

### Command Line {#command-line}

Some of the data that can be saved by a running PPS-Client daemon is of the on-demand type. This is enabled by executing PPS-Client with the `-s` flag while the daemon is running. For example,

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
* `rawError` writes an exponentially decaying distribution of unprocessed PPS jitter values as they enter the controller. These are relative to the current value of `sysDelay`. Each jitter value that is added to the distribution has a half-life of one hour. So the distribution is almost completely refreshed every four to five hours.

* `intrptError` writes an exponentially decaying distribution of unprocessed calibration interrupt delay values. The description is otherwise the same as for `"rawError"`.

* `frequency-vars` writes the last 24 hours of clock frequency offset and Allan deviation of one-minute samples in each five-minute interval indexed by the timestamp at each interval.

* `pps-offsets` writes the previous 10 minutes of recorded time offsets and applied frequency offsets indexed by the sequence number (seq_num) each second.

## Accuracy Validation {#accuracy-validation}

Time accuracy is defined as the absolute time error at any point in time relative to the PPS time clock. The limit to time accuracy on any processor that uses a conventional integrated circuit crystal oscillator is [flicker noise](https://en.wikipedia.org/wiki/Flicker_noise) in the oscillator. At the 1 Hz operating frequency of the PPS-Client controller, flicker noise is evident as [part of the random component](#noise) of second-to-second jitter. The integrator in the control loop removes it from the system clock frequency adjustment and the proportional adjustment only allows a 1 microsecond adjustment each second which ignores all but 1 microsecond of it. 

As a result, any internal time measurement made on the system clock will see at most ± 1 microsecond of noise. Since that is the the resolution limit of time measurements, the processor clock is blind to its own flicker noise. Flicker noise is only evident when the RPi system clock is timing an external event, in which case the system clock will [see its own noise](#timed-event) plus any noise in the event being timed. Thus any time measurements indicated below as "on the local clock" are internal and are blind to local flicker noise.

It has been determined that the system clock oscillator synchronized by PPS-Client has a maximum drift on the order of less than 100 nanoseconds per second (on the local clock). That can be verified on any RPi that has been running for at least a day by saving a file of frequency data and examining Allan deviation:

    $ pps-client -s frequency-vars
    
24 hours later:
    
    $ cat /var/local/pps-frequency-vars

Consequently, any interval measurement of one second or less on the system clock will have an average error smaller than 1 microsecond (on the local clock). That will, in fact, be true of any time measurement because whole seconds are synchronized to the PPS signal. This verifies that the RPi system clock has a precision of 1 microsecond with respect to the independent `sysDelay` value determined by the [feedforward compensation](#feedforward-compensator) mechanism, but does not verify that the `sysDelay` value is correct.

To verify absolute time accuracy, a pair of repetitive pulses is necessary with the first pulse replacing the PPS time source and the second pulse providing a time value to be measured by the RPi system clock. If the time reported by the RPi system clock agrees with the known time interval between the pulses to within an acceptable error then absolute time accuracy is verified. 

Ideally, the pulses would be generated by laboratory equipment. But precision test gear is not generally available and, fortunately, is not really necessary. Since RPi processors will have flicker noise that is about the same from unit to unit then one RPi can be configured as a timing pulse generator to test a second RPi. The result of the timing will have the combined flicker noise of both RPi units. Since the noise distributions of these independent noise sources add in [RMS](https://en.wikipedia.org/wiki/Root_mean_square) fashion, then if the flicker noise in the units is about equal then the combined distribution will be wider than that of a single unit by about √2. Since all tests will be the result of evaluating the peaks of noise distributions over a large number of trials, the widening of the distribution will have a negligible effect.

### The pulse-generator Utility {#the-pulse-generator-utility}

The pulse generator used for testing is a small program that is installed along with PPS-Client. It can generate repeating single pulses or pulse pairs each second at times specified in microseconds and with a precision of 1 microsecond (on the local clock). Pulses have a duration of 10 microseconds. To use the utility, load the driver, specifying the GPIO outputs or single output that will provide the pulses. 

Because pulse-generator runs as a real-time process, pulses should not be generated at zero in order to avoid conflict with PPS-Client.

    $ sudo pulse-generator load-driver <GPIO_num1> [GPIO_num2]

Then run the utility, specifying the times of the output pulses with,

    $ sudo pulse-generator <time1> [time2]

You can stop it with ctrl-c. However, when pulse-generator is started over a terminal and the terminal disconnects for any reason, that will also stop it. To prevent it from stopping during a long test it can be started as a detached process:

    $ sudo pulse-generator <time1> [time2] &

In this case it can be stopped with

    $ sudo kill `pidof pulse-generator`

Notice that back quotes are used instead of single quotes.

When finished, unload the driver with

    $ sudo pulse-generator unload-driver

Pulses are generated by a kernel driver in a spin loop that constantly checks the system time with a kernel function `do_gettimeofday()` that returns the time within about half a microsecond so that, as soon as the specified times occur, the GPIO output(s) are asserted. This technique inherently has high linearity because time on the local clock is linear to within the error introduced by flicker noise in the clock oscillator.

The pulse-generator does not use interrupts - a fact that appears to minimize sporadic system latency. After each pulse is generated, pulse-generator prints the time of occurrence to the console. If a pulse is delayed more than 1 microsecond by latency it is omitted and a message to that effect is printed followed by the time the omitted pulse would have occurred. This typically occurs only a few times in 24 hours.

### The interrupt-timer Utility {#the-interrupt-timer-utility}

To time interrupts generated by external pulses, a second utility, "interrupt-timer" is provided that is also installed on the RPi along with PPS-Client. The interrupt-timer utility is insensitive to omitted interrupt pulses. When it receives a pulse, it prints the reception time of the interrupt to the terminal and also records a distribution of the fractional seconds part of the time to a file `/var/local/timer-distrib-forming` that is copied to `/var/local/timer-distrib` every 24 hours. This timing-collection mode is used to validate the accuracy of PPS-Client. For the details see the [Testing Accuracy](#testing-accuracy) section below.

The interrupt-timer can be run from the command line of any terminal communicating with the RPi. To use it, load the driver specifying the GPIO number of the pin that will provide the interrupt:

    $ sudo interrupt-timer load-driver <GPIO number>

Then run the `interrupt-timer` with,

    $ sudo interrupt-timer

You can stop it with ctrl-c. However, when interrupt-timer is started over a terminal and the terminal disconnects for any reason, that will also stop it. To prevent it from stopping during a long test it can be started as a detached process:

    $ sudo interrupt-timer &

In this case it can be stopped with

    $ sudo kill `pidof interrupt-timer`

Notice that back quotes are used instead of single quotes.

When finished, unload the driver with,

    $ sudo interrupt-timer unload-driver

Because any interrupt time reported by the system is always later by the delay introduced by system latency, interrupt-timer compensates for system interrupt delay by reading the `sysDelay` value recorded by PPS-Client and subtracting it from the measured time of the interrupt internally recorded by interrupt-timer. This generates a reported time of the external interrupt <I>t<sub> r</sub></I> that adjusts the measured time of the interrupt <I>t<sub> m</sub></I> by the value of `sysDelay`, <I>d<sub> sys</sub></I>,

<center><I>t<sub> r</sub></I> = <I>t<sub> m</sub></I> - <I>d<sub> sys</sub></I></center>

in exactly the same way that the reported time of the zero crossing of the second, <I>t<sub> r0</sub></I>, is the measured time of the PPS interrupt, <I>t<sub> m0</sub></I>, adjusted for `sysDelay`:

<center><I>t<sub> r0</sub></I> = <I>t<sub> m0</sub></I> - <I>d<sub> sys</sub></I></center>

But since the [feedforward compensator](#feedforward-compensator) determines the value of <I>d<sub> sys</sub></I> and the feedback controller forces <I>t<sub> r0</sub></I> to be zero then the only question is did the feedforward compensator determine the correct value for <I>d<sub> sys</sub></I> If it did then the reported value of the time of the interrupt <I>t<sub> r</sub></I> in the first equation should be the true time of the interrupt relative to the PPS. That is exactly what accuracy testing establishes.

### The NormalDistribParams Utility {#normaldistribparams-utility}

The distributions obtained in testing PPS-Client are usually narrow which makes it difficult to estimate offset errors and standard deviations. Moreover there is ample evidence that the random component of the PPS-Client distributions is well-modeled by a normal distribution but is also effectively binned by the 1 microsecond resolution of the system clock. The `NormalDistribParams` program makes it possible to directly compute normal distribution parameters from binned values of a sample distribution. 

The program fits an ideal normal distribution to the three binned sample values at or near the peak of the distribution. If the sample distribution departs from normal, there will be a conformance error listed as relative fit which is a measure of the reliability of the calculated values of both ideal mean and ideal SD. Roughly speaking, relative fit is the probability that the samples actually are drawn from a normal distribution.

The program can be used to determine mean and SD for any of the sample distributions collected in testing PPS-Client including positive-only distributions like those of calibration interrupts which fit a half-normal distribution in the direction of increasing delay. For example, the jitter distribution in [Figure 2b](#raspberry-pi-3) was evaluated for mean and standard deviation by providing the sample numbers for the sample bins from zero forward like this,
```
	pi@raspberrypi:~ $ NormalDistribParams 34483 0 12958 1 2279 2 50212
	Relative to the best fit normal distribution:
	mean:  0.210884
	stddev: 0.448001
	Relative fit of samples: 0.974911
```
In this example, sample numbers were normalized to a total sample size of 50,212 instead of the default size of 86,400.

### Testing Accuracy {#testing-accuracy}

To minimize the effects of flicker noise and latency, accuracy testing consists of making a large number of independent time interval measurements and then statistically evaluating the results. This averages out flicker noise in the oscillators of both the RPi unit under test and the RPi unit used to provide timing pulses. 

The pulse-generator utility runs in two-pulse mode on an RPi identified as RPi-1 that is connected to a GPS receiver providing the PPS signal. The pulse-generator on RPi-1 provides both Pulse 1 as the PPS signal to RPi-2 and Pulse 2 as the pulse to be timed. The interrupt-timer utility is used on the RPi-2 to time the reception of Pulse 2.

Timing pulses are received each second by interrupt-timer on RPi-2. Validation is successful if the average time of reception of Pulse 2 is equal to the time difference between the times of Pulse 2 and Pulse 1 (generated on RPi-1) with sufficiently small error.

In order to get low-latency timings it was necessary for interrupt-timer to sleep until just before the expected time of arrival of Pulse 2. So that technique is built into interrupt-timer.

#### Test Setup {#test-setup}

![Accuracy Testing](accuracy_verify.jpg)

In these tests, GPIO_23 and GPIO_25 are used for Pulse 1 and Pulse 2, respectively. Pulse 1 provides the PPS signal to RPi-2 and Pulse 2 goes to the GPIO input that is timed by interrupt-timer. The RPi units are wired as follows:

    RPi-1 0V      ---> RPi-2 0V      (Signal Ground)
    RPi-1 GPIO_23 ---> RPi-2 GPIO_4  (PPS input)
    RPi-1 GPIO_25 ---> RPi-2 GPIO_24 (interrupt-timer input)

This connection is also made on both RPi-1 and RPi-2 to support self calibration:

    RPi-2 GPIO_17 ---> GPIO_22

The diagram below shows those connections (the self-calibration connection for RPi-1 is already made on the GPS board by the yellow jumper in the photo).

![Accuracy Test Wiring](wiring.png)

After running PPS-Client on RPi-1 to at least a sequence count of 1200 to allow it to stabilize, the pulse-generator is started on RPi-1 with pulses at 100 μsec and 900 μsec with,

    RPi-1:~ $ sudo pulse-generator load-driver 23 25
    RPi-1:~ $ sudo pulse-generator -p 100000 900000 &

Once the pulse generator is providing the PPS input, PPS-Client is started on RPi-2 and is allowed to stabilize to a minimum sequence count of 1200. At that point the interrupt-timer utility is loaded on RPi-2 with

    RPi-2:~ $ sudo interrupt-timer load-driver 24

Then interrupt-timer is started in timing collection mode and is allowed to collect interrupt times for 24 hours from GPIO 24 on RPi-2 with

    RPi-2:~ $ sudo interrupt-timer &

The test is completed when the the `timer-distrib` file, containing the distribution of recorded interrupt times, becomes available in the `/var/local` directory.

#### Test Results {#test-results}

This test as described above was performed on ten Raspberry Pi 3 processors under low system load. Timings were collected over a period of 24 hours with typical results like that shown in Figure 6.

![System Clock Accuracy](InterruptTimerDistrib.png)

The distribution shows the average recorded pulse time for this distribution (RPi3#2) to be about 0.16 microsecond lower than the ideal time of 800,000 microseconds. The log plot shows that for this unit, pulses were received with a delay as much as 20 microseconds because of sporadic Linux system interrupt latency.

The results collected for all ten units are shown in the table below. The relative fit to an ideal normal distribution was provided by the `NormalDistribParams` utility.
```
  Clock Error from ideal time of 800,000 usecs
    UNIT#  Error(usec)  stddev  relative fit
    -----------------------------------------
    RPi3#1   -0.76     0.997     0.997
    RPi3#2   -0.16     1.006     0.998
    RPi3#3   -0.29     1.030     0.998
    RPi3#4   -0.40     0.960     0.9996
    RPi3#5   -0.60     1.048     0.9993
    RPi3#6   -0.42     1.088     0.99999
    RPi3#7   -0.16     1.047     0.996
    RPi3#8   +0.22     1.007     0.989
    RPi3#9   -0.22     1.084     0.998
    RPi3#10  -0.21     0.916     0.997
```



