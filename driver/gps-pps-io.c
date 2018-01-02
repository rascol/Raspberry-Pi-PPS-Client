/*
 * gps-pps-io.c
 *
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
 * Derived from code from the book "Linux Device Drivers" by
 * Alessandro Rubini and Jonathan Corbet, published by O'Reilly
 * & Associates.
 *
 * Notes:
 *
 * Compile on Raspberry Pi 2 to create gps-pps-io.ko. On
 * installation gps-pps-io.ko must be copied to
 *  /lib/modules/`uname -r`/kernel/drivers/misc/gps-pps-io.ko
 *
 * 1. When an interrupt is received on PPS_GPIO this driver records
 * the reception time. That time can then be read from the driver
 * with the read() function.
 *
 * 2. This driver can also record the reception time of a second
 * interrupt on INTRPT_GPIO that is initiated from within the driver.
 * That requires an external wired connection between OUTPUT_GPIO
 * and INTRPT_GPIO.
 *
 * With that connection in place, writing a 1 to the driver
 * will disable the interrupt on PPS_GPIO, will cause OUTPUT_GPIO to
 * request an interrupt on INTRPT_GPIO and will record the time the
 * write arrived at OUTPUT_GPIO.
 *
 * The write time to OUTPUT_GPIO and the reception time of the
 * interrupt on INTRPT_GPIO can then be read from the driver with
 * the read() function.
 *
 * Writing a 0 to the driver will then re-enable the interrupt
 * on PPS_GPIO.
 *
 * 3. This driver can also be used to inject an offset in whole
 * seconds to the system time by writing a pair of integers to
 * the driver with the first being an identifier value greater
 * than 1 and the second being the integer offset time.
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

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

/* The text below will appear in output from 'cat /proc/interrupt' */
#define INTERRUPT_NAME "gps-pps-io"

const char *version = "gps-pps-io v1.0.0";

static int major = 0;							/* dynamic by default */
module_param(major, int, 0);					/* but can be specified at load time */

static int PPS_GPIO = 0;
module_param(PPS_GPIO, int, 0);					/* Specify PPS_GPIO at load time */

static int OUTPUT_GPIO = 0;
module_param(OUTPUT_GPIO, int, 0);				/* Specify OUTPUT_GPIO at load time */

static int INTRPT_GPIO = 0;
module_param(INTRPT_GPIO, int, 0);				/* Specify INTRPT_GPIO at load time */

volatile int pps_irq1 = -1;
volatile int pps_irq2 = -1;
volatile int gpio_out = 0;

MODULE_AUTHOR ("Raymond Connell");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("0.2.0");

int *pps_buffer = NULL;
DECLARE_WAIT_QUEUE_HEAD(pps_queue);

int read1_OK = 0;
int read2_OK = 0;
bool readIntr2 = false;

unsigned long j_delay;

static atomic_t driver_available = ATOMIC_INIT(1);

/**
 * Permits open() only by the first caller until
 * that caller closes the driver.
 */
int pps_open (struct inode *inode, struct file *filp)
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
int pps_release (struct inode *inode, struct file *filp)
{
	/**
	 * Sets driver_available to 1 so the next caller
	 * can open the driver again after this close.
	 */
	atomic_inc(&driver_available);
	return 0;
}

/**
 * Reads the reception times of interrupts on PPS_GPIO and INTRPT_GPIO
 * and the time an output write arrived at OUTPUT_GPIO from
 * __user *buf.
 *
 * When reading the time of an interrupt on PPS_GPIO __user *buf is
 * interpreted to be a two-element int array mapping a struct timeval
 * tv. The int with index 0 contains tv.tv_sec and the int with index
 * 1 contains tv.tv_usec. In this case count is 2 * sizeof(int).
 *
 * When reading the time of an interrupt on INTRPT_GPIO __user *buf is
 * interpreted to be a six-element int array mapping three struct
 * timeval objects as int pairs. The first int pair is not used.
 * The second int pair contains the time a write arrived at OUTPUT_GPIO,
 * as above, and the third int pair contains the time the INTRPT_GPIO
 * interrupt was recognized. In this case count is 6 * sizeof(int).
 *
 * If this function is called before an interrupt is triggered then the
 * reading process is put to sleep and if the interrupt is triggered
 * within a 200 msec timeout then this function wakes up the reading
 * process, copies the time captured by the interrupt to buf and returns.
 *
 * If an interrupt was triggered before this function is called this
 * function does not put the reading process to sleep but only copies
 * the time captured by the interrupt to buf and returns.
 *
 * Returns the number of bytes read or zero if the timeout occurs before
 * an interrupt is triggered. Can also return a negative value if for any
 * reason the system is unable to respond to the read request. One possible
 * error is -ERESTARTSYS (-256) on an unhandled signal.
 *
 * Is called from user space as a normal file read operation on the device
 * driver file.
 */
ssize_t pps_i_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t rv = 0;
	int wr = 0;

	if (readIntr2 == false){
		while (read1_OK == 0){
			wr = wait_event_interruptible_timeout(pps_queue, read1_OK == 1, j_delay);
			if (wr == 0){							// read1_OK == 0 after j_delay
				break;
			}
			if (wr != 1){							// read1_OK == 1 before j_delay
				break;
			}
		}

		if (read1_OK == 1){
			if (copy_to_user(buf, pps_buffer, count)){
				rv = -EFAULT;
			}
			else {
				rv = count;
			}
		}
		else {
			rv = -wr;
		}
	}
	else {
		readIntr2 = false;

		while (read2_OK == 0){
			wr = wait_event_interruptible_timeout(pps_queue, read2_OK == 1, j_delay);
			if (wr == 0){
				break;
			}
			if (wr != 1){
				break;
			}
		}

		if (read2_OK == 1){
			if (copy_to_user(buf, pps_buffer, count)){
				rv = -EFAULT;
			}
			else {
				rv = count;
			}
		}
		else {
			rv = -wr;
		}
	}

	pps_buffer[0] = 0;
	pps_buffer[1] = 0;
	pps_buffer[2] = 0;
	pps_buffer[3] = 0;
	pps_buffer[4] = 0;
	pps_buffer[5] = 0;

	read1_OK = 0;
	read2_OK = 0;
	return rv;
}

/**
 * Provides three functions:
 *   1. Writing an integer with a value of 1 to __user *buf
 *   records the time of the write to pps_buffer[2]-[3],
 *   disables pps_irq1 then sets OUTPUT_GPIO (high). This allows
 *   pps_irq2 to be used alternately with pps_irq1. count
 *   is provided with a value of sizeof(int).
 *
 *   2. Writing an integer with a value of 0 to __user *buf
 *   enables pps_irq1 and resets OUTPUT_GPIO (low). count is
 *   provided with a value of sizeof(int).
 *
 *   3. Writing a pair of integers where the first is greater
 *   than 1 to __user *buf causes the second integer to be used
 *   as an offset in seconds to the system time and this offset
 *   is applied immediately. count is provided with a value of
 *   2 * sizeof(int).
 */
ssize_t pps_i_write (struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
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

	struct timeval tv;
	struct timespec tv2;

	int *val = (int *)buf;

	if (val[0] == 1){

		disable_irq_nosync(pps_irq1);

		tv.tv_sec = 0;
		tv.tv_usec = 0;

		while (tv.tv_usec < 600){		// Spin to 600 microseconds before
			do_gettimeofday(&tv);		// writing to the output pin.
		}

		readIntr2 = true;

		do_gettimeofday(&tv);

		pps_buffer[2] = tv.tv_sec;
		pps_buffer[3] = tv.tv_usec;

		gpio_set_value(gpio_out, 1);
	}
	else if (val[0] == 0){
		gpio_set_value(gpio_out, 0);

		readIntr2 = false;
		enable_irq(pps_irq1);
	}
	else {								// val[0] > 1
		tv2.tv_nsec = 0;
		tv2.tv_sec = val[1];

		timekeeping_inject_offset(&tv2);
	}

	return 0;
}

struct file_operations pps_i_fops = {
	.owner	 = THIS_MODULE,
	.read	 = pps_i_read,
	.write   = pps_i_write,
	.open	 = pps_open,
	.release = pps_release,
};

/**
 * On recognition of the interrupt on PPS_GPIO copies
 * the time of day to pps_buffer[0]-[1], sets the
 * read1_OK flag and wakes up the reading process.
 */
irqreturn_t pps_interrupt1(int irq, void *dev_id)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	pps_buffer[0] = tv.tv_sec;
	pps_buffer[1] = tv.tv_usec;

	read1_OK = 1;
	wake_up_interruptible(&pps_queue); 				/* Wake up the reading process now */

	return IRQ_HANDLED;
}

/**
 * On recognition of the interrupt on INTRPT_GPIO copies
 * the time of day to pps_buffer[4]-[5], sets the
 * read2_OK flag and wakes up the reading process.
 */
irqreturn_t pps_interrupt2(int irq, void *dev_id)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	pps_buffer[4] = tv.tv_sec;
	pps_buffer[5] = tv.tv_usec;

	read2_OK = 1;
	wake_up_interruptible(&pps_queue); 				/* Wake up the reading process now */

	return IRQ_HANDLED;
}

/**
 * Requests a GPIO,  maps the GPIO number to the interrupt and gets
 * the interrupt number.
 */
int configureInterruptOn(int gpio_num) {

   if (gpio_request(gpio_num, INTERRUPT_NAME)) {
      printk(KERN_INFO "gps-pps-io: GPIO request failed on GPIO %d\n", gpio_num);
      return -1;
   }

   if (gpio_direction_input(gpio_num)){
	   printk(KERN_INFO "gps-pps-io: GPIO pin direction 'INPUT' on GPIO %d was denied\n", gpio_num);
	   return -1;
   }

   if (gpio_num == PPS_GPIO){
	   if ( (pps_irq1 = gpio_to_irq(PPS_GPIO)) < 0 ) {
	      printk(KERN_INFO "gps-pps-io: GPIO to IRQ mapping failed\n");
	      return -1;
	   }

	   printk(KERN_INFO "gps-pps-io: Mapped int %d\n", pps_irq1);

	   if (request_irq(pps_irq1,
					   (irq_handler_t) pps_interrupt1,
					   IRQF_TRIGGER_RISING | IRQF_NO_THREAD,
					   INTERRUPT_NAME,
					   NULL) != 0) {
		  printk(KERN_INFO "gps-pps-io: request_irq() failed\n");
		  return -1;
	   }
   }

   if (gpio_num == INTRPT_GPIO){
	   if ( (pps_irq2 = gpio_to_irq(INTRPT_GPIO)) < 0 ) {
	      printk(KERN_INFO "gps-pps-io: GPIO to IRQ mapping failed\n");
	      return -1;
	   }

	   printk(KERN_INFO "gps-pps-io: Mapped int %d\n", pps_irq2);

	   if (request_irq(pps_irq2,
					   (irq_handler_t) pps_interrupt2,
					   IRQF_TRIGGER_RISING | IRQF_NO_THREAD,
					   INTERRUPT_NAME,
					   NULL) != 0) {
		  printk(KERN_INFO "gps-pps-io: request_irq() failed\n");
		  return -1;
	   }
   }

   return 0;
}

int configureWriteOn(int gpio_num){

	if (gpio_request(gpio_num, INTERRUPT_NAME)) {
		printk(KERN_INFO "gps-pps-io: GPIO request failed on GPIO %d\n", gpio_num);
		return -1;
	}

	if (gpio_direction_output(gpio_num, 0)){
		printk(KERN_INFO "gps-pps-io: GPIO pin direction 'OUTPUT' on GPIO %d was denied\n", gpio_num);
		return -1;
	}
	gpio_out = gpio_num;

	return 0;
}

void pps_cleanup(void)
{
	if (pps_irq1 >= 0) {
		free_irq(pps_irq1, NULL);
	}
	if (pps_irq2 >= 0) {
		free_irq(pps_irq2, NULL);
	}

	flush_scheduled_work();

	unregister_chrdev(major, "gps-pps-io");

	if (pps_buffer)
		free_page((unsigned long)pps_buffer);

	gpio_free(PPS_GPIO);
	gpio_free(INTRPT_GPIO);
	gpio_free(OUTPUT_GPIO);

	printk(KERN_INFO "gps-pps-io: removed\n");
}

struct file* file_open(const char* path, int flags, int rights) {
    struct file* filp = NULL;
    mm_segment_t oldfs;
    int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, flags, rights);
    set_fs(oldfs);
    if(IS_ERR(filp)) {
        err = PTR_ERR(filp);
        return NULL;
    }
    return filp;
}

int file_read(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_read(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
}

void file_close(struct file* file) {
    filp_close(file, NULL);
}

int kstrncmp(const char *str1, const char *str2, int n){
	int i;
	for (i = 0; i < n; i++){
	    if (str1[i] != str2[i])
	        return (str1[i] - str2[i]);
	}
	return (0);
}

int pps_init(void)
{
	int result;

	struct timespec value;
	value.tv_sec = 0;
	value.tv_nsec = 200000000;							// 200 millisecs

	j_delay = timespec_to_jiffies(&value);

	result = register_chrdev(major, "gps-pps-io", &pps_i_fops);
	if (result < 0) {
		printk(KERN_INFO "gps-pps-io: can't get major number\n");
		return result;
	}

	if (major == 0)
		major = result; /* dynamic */

	pps_buffer = (int *)__get_free_pages(GFP_KERNEL,0);

	if (configureInterruptOn(PPS_GPIO) == -1){
		printk(KERN_INFO "gps-pps-io: failed installation\n");
		pps_cleanup();
		return -1;
	}

	if (configureWriteOn(OUTPUT_GPIO) == -1){
		printk(KERN_INFO "gps-pps-io: failed installation\n");
		pps_cleanup();
		return -1;
	}

	if (configureInterruptOn(INTRPT_GPIO) == -1){
		printk(KERN_INFO "gps-pps-io: failed installation\n");
		pps_cleanup();
		return -1;
	}

	printk(KERN_INFO "gps-pps-io: installed\n");

	return 0;
}

module_init(pps_init);
module_exit(pps_cleanup);



