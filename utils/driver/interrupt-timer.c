/*
 * interrupt-timer.c
 *
 *  Created on: Apr 8, 2016
 *      Author: ray
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
#define INTERRUPT_NAME "interrupt-timer"

static int major = 0;							/* dynamic by default */
module_param(major, int, 0);					/* but can be specified at load time */

/* The interrupt is undefined by default. */
static int gpio_num = -1;

volatile int timer_irq = -1;

module_param(gpio_num, int, 0);					/* Specify the interrupt number at load time */

MODULE_AUTHOR ("Raymond Connell");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("0.2.0");

int *timer_buffer = NULL;
DECLARE_WAIT_QUEUE_HEAD(timer_queue);

int read_OK = 0;

int gpio_out = 0;
unsigned long j_delay;

static atomic_t driver_available = ATOMIC_INIT(1);

/**
 * Permits open() only by the first caller until
 * that caller closes the driver.
 */
int timer_open(struct inode *inode, struct file *filp)
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
int timer_release (struct inode *inode, struct file *filp)
{
	/**
	 * Sets driver_available to 1 so the next caller
	 * can open the driver again after this close.
	 */
	atomic_inc(&driver_available);
	return 0;
}

/**
 * Reads the reception times of interrupts on gpio_num.
 *
 * When reading the time of an interrupt on gpio_num __user *buf is
 * interpreted to be a two-element int array mapping a struct timeval
 * tv. The int with index 0 contains tv.tv_sec and the int with index
 * 1 contains tv.tv_usec. Count is 2 * sizeof(int).
 *
 * If this function is called before an interrupt is triggered then the
 * reading process is put to sleep and if the interrupt is triggered
 * within a 10 second timeout then this function wakes up the reading
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
ssize_t timer_i_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t rv = 0;
	int wr = 0;

	while (read_OK == 0){
		wr = wait_event_interruptible_timeout(timer_queue, read_OK == 1, j_delay);
		if (wr == 0){							// read_OK == 0 after j_delay
			break;
		}
		if (wr != 1){							// read_OK == 1 before j_delay
			break;
		}
	}

	if (read_OK == 1){
		if (copy_to_user(buf, timer_buffer, count)){
			rv = -EFAULT;
		}
		else {
			rv = count;
		}
	}
	else {
		rv = -wr;
	}

	timer_buffer[0] = 0;
	timer_buffer[1] = 0;
	timer_buffer[2] = 0;
	timer_buffer[3] = 0;
	timer_buffer[4] = 0;
	timer_buffer[5] = 0;

	read_OK = 0;
	return rv;
}

struct file_operations timer_i_fops = {
	.owner	 = THIS_MODULE,
	.read	 = timer_i_read,
	.open	 = timer_open,
	.release = timer_release,
};

/**
 * On recognition of the interrupt copies the
 * time of day to timer_buffer[0]-[1], sets the
 * read_OK flag and wakes up the reading process.
 */
irqreturn_t timer_interrupt(int irq, void *dev_id)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	timer_buffer[0] = tv.tv_sec;
	timer_buffer[1] = tv.tv_usec;

	read_OK = 1;
	wake_up_interruptible(&timer_queue); 				/* Wake up the reading process now */

	return IRQ_HANDLED;
}

/**
 * Requests a GPIO,  maps the GPIO number to the interrupt and gets
 * the interrupt number.
 */
int configureInterruptOn(int gpio_num) {

   if (gpio_request(gpio_num, INTERRUPT_NAME)) {
      printk(KERN_INFO "interrupt-timer: GPIO request failed on GPIO %d\n", gpio_num);
      return -1;
   }

   if (gpio_direction_input(gpio_num)){
	   printk(KERN_INFO "interrupt-timer: GPIO pin direction 'INPUT' on GPIO %d was denied\n", gpio_num);
	   return -1;
   }
	   	   	   	   	   	   	   	   	   	   // Map gpio pin to its corresponding irq number
   if ( (timer_irq = gpio_to_irq(gpio_num)) < 0 ) {
	  printk(KERN_INFO "interrupt-timer: GPIO to IRQ mapping failed\n");
	  return -1;
   }

   printk(KERN_INFO "interrupt-timer: Mapped int %d\n", timer_irq);

   if (request_irq(timer_irq,
				   (irq_handler_t) timer_interrupt,
				   IRQF_TRIGGER_RISING,
				   INTERRUPT_NAME,
				   NULL) != 0) {
	  printk(KERN_INFO "interrupt-timer: request_irq() failed\n");
	  return -1;
   }

   enable_irq(timer_irq);

   return 0;
}

void timer_cleanup(void)
{
	if (timer_irq >= 0) {
		free_irq(timer_irq, NULL);
	}

	flush_scheduled_work();

	unregister_chrdev(major, "interrupt-timer");

	if (timer_buffer)
		free_page((unsigned long)timer_buffer);

	gpio_free(gpio_num);

	printk(KERN_INFO "interrupt-timer: removed\n");
}

int timer_init(void)
{
	int result;

	struct timespec value;
	value.tv_sec = 10;
	value.tv_nsec = 0;

	j_delay = timespec_to_jiffies(&value);

	result = register_chrdev(major, "interrupt-timer", &timer_i_fops);
	if (result < 0) {
		printk(KERN_INFO "interrupt-timer: can't get major number\n");
		return result;
	}

	if (major == 0)
		major = result; /* dynamic */

	timer_buffer = (int *)__get_free_pages(GFP_KERNEL,0);

	if (configureInterruptOn(gpio_num) == -1){
		printk(KERN_INFO "interrupt-timer: failed installation\n");
		timer_cleanup();
		return -1;
	}

	printk(KERN_INFO "interrupt-timer: installed\n");

	return 0;
}

module_init(timer_init);
module_exit(timer_cleanup);




