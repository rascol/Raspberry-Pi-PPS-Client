/*
 * pulse-generator.c
 *
 * Created on: Apr 9, 2016
 * Copyright (C) 2016  Raymond S. Connell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * pulse-generator.ko must be copied to
 *  /lib/modules/`uname -r`/kernel/drivers/misc/pulse-generator.ko
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/delay.h>	/* udelay */
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/param.h>
#include <asm/gpio.h>
#include <asm/atomic.h>
#include <asm/io.h>
#include <../kernel/time/timekeeping.h>

/* The text below will appear in output from 'cat /proc/interrupt' */
#define MODULE_NAME "pulse-generator"

const char *version = "pulse-generator-driver v1.0.0";

static int major = 0;								/* dynamic by default */
module_param(major, int, 0);						/* but can be specified at load time */

static int gpio_num1 = 0;
module_param(gpio_num1, int, 0);						/* Specify gpio 1 at load time */

static int gpio_num2 = 0;
module_param(gpio_num2, int, 0);						/* Specify gpio 2 at load time */

MODULE_AUTHOR ("Raymond Connell");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("0.1.0");

int gpio_out1 = 0;
int gpio_out2 = 0;

int pulseTime[2];

static atomic_t driver_available = ATOMIC_INIT(1);

/**
 * Permits open() only by the first caller until
 * that caller closes the driver.
 */
int pulsegen_open (struct inode *inode, struct file *filp)
{
	/**
	 * The following statement fails if driver_available
	 * is 1 and driver_available then gets set to zero.
	 *
	 * The statement succeeds if driver_available is 0
	 * and driver_available then gets set to -1.
	 */
    if (! atomic_dec_and_test(&driver_available)) {

    	/**
    	 * If driver_available was initially 0 then got
    	 * here and driver_available was set to -1. But
    	 * the next statment sets driver_available back
    	 * to 0. So every subsequent open call comes here.
    	 */

        atomic_inc(&driver_available);
        return -EBUSY; 						/* already open */
    }

    /**
     * If driver_available was initially 1 then got
     * here and driver_available was set to 0
     * by atomic_dec_and_test().
     */
    return 0;
}

/**
 * Closes the driver but keeps it active so the next
 * caller can open it again.
 */
int pulsegen_release(struct inode *inode, struct file *filp)
{
	/**
	 * Sets driver_available to 1 so the next caller
	 * can open the driver again after this close.
	 */
	atomic_inc(&driver_available);
	return 0;
}

/**
 * Asserts a 10 usec pulse at the fractional second time
 * requested by timeout on the gpio given by gpio_out.
 */
void generate_pulse(int timeout, int *gpio_out)
{
	struct timeval tv;
	int startTime, lastTime, time;

	do_gettimeofday(&tv);
	startTime = tv.tv_usec;
	if (startTime > timeout){				// At the rollover of the second,
		timeout += 1000000;					// extend the timeout value
	}

	lastTime = startTime;

	do {									// Spin until time of day >= timeout
		do_gettimeofday(&tv);

		time = tv.tv_usec;
		if (time < lastTime){				// If the second rolled over,
			time += 1000000;				// extend the time value.
		}

		lastTime = time;

	} while (time < timeout);

	pulseTime[0] = tv.tv_sec;				// Record time of rising edge
	pulseTime[1] = tv.tv_usec;

	gpio_set_value(*gpio_out, 1);

	timeout += 10;

	do {									// Spin until time of day >= timeout
		do_gettimeofday(&tv);

		time = tv.tv_usec;
		if (time < lastTime){				// If the second rolled over,
			time += 1000000;				// extend the time value.
		}

		lastTime = time;

	} while (time < timeout);

	gpio_set_value(*gpio_out, 0);
}

/**
 * Asserts up to two 50 usec pulses at the fractional second
 * times requested by buf as a pair of integers. The count
 * value must be 2 * sizeof(int).
 */
ssize_t pulsegen_i_write (struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
   /**
	*  For future reference:
	*
	*  void gpio_set_value(unsigned gpio, int value)
	*
	*  int gpio_get_value(unsigned gpio)
	*
	*  are the functions to set output and get input
	*  values from appropriate gpio pins.
	*/

	int *timeData = (int *)buf;				// timeData[0] : first or second GPIO, timeData[1] : pulse time

	if (timeData[0] == 0){
		generate_pulse(timeData[1], &gpio_out1);
	}
	else{
		generate_pulse(timeData[1], &gpio_out2);
	}

	return 0;
}

ssize_t pulsegen_i_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	if (copy_to_user(buf, (char *)pulseTime, count)){
		return -EFAULT;
	}

	return count;
}

struct file_operations pulsegen_i_fops = {
	.owner	 = THIS_MODULE,
	.write   = pulsegen_i_write,
	.read	 = pulsegen_i_read,
	.open	 = pulsegen_open,
	.release = pulsegen_release,
};

int configureWriteOn(int gpio_num){

	if (gpio_request(gpio_num, MODULE_NAME)) {
		printk(KERN_INFO "pulse-generator: GPIO request failed on GPIO %d\n", gpio_num);
		return -1;
	}

	if (gpio_direction_output(gpio_num, 0)){
		printk(KERN_INFO "pulse-generator: GPIO pin direction 'OUTPUT' on GPIO %d was denied\n", gpio_num);
		return -1;
	}

	return 0;
}

void pulsegen_cleanup(void)
{
	flush_scheduled_work();

	unregister_chrdev(major, "pulse-generator");

	gpio_free(gpio_num1);

	if (gpio_num2 > 0){
		gpio_free(gpio_num2);
	}

	printk(KERN_INFO "pulse-generator: removed\n");
}

int pulsegen_init(void)
{
	int result;

	result = register_chrdev(major, "pulse-generator", &pulsegen_i_fops);
	if (result < 0) {
		printk(KERN_INFO "pulse-generator: can't get major number\n");
		return result;
	}

	if (major == 0)
		major = result; /* dynamic */

	if (configureWriteOn(gpio_num1) == -1){
		printk(KERN_INFO "pulse-generator: failed installation\n");
		pulsegen_cleanup();
		return -1;
	}
	gpio_out1 = gpio_num1;

	if (gpio_num2 > 0){
		if (configureWriteOn(gpio_num2) == -1){
			printk(KERN_INFO "pulse-generator: failed installation\n");
			pulsegen_cleanup();
			return -1;
		}
		gpio_out2 = gpio_num2;
	}

	printk(KERN_INFO "pulse-generator: installed\n");

	pulseTime[0] = 0;
	pulseTime[1] = 0;

	return 0;
}

module_init(pulsegen_init);
module_exit(pulsegen_cleanup);




