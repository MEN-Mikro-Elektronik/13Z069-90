/*********************  P r o g r a m  -  M o d u l e ***********************/
/*!
 *        \file  men_z069_reset_wdg.c
 *
 *      \author  thomas.schnuerer@men.de
 *
 *      \brief   reset and watchdog driver
 *
 *---------------------------------------------------------------------------
 * Copyright 2019, MEN Mikro Elektronik GmbH
 ****************************************************************************/
 /*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/moduleparam.h>
#include <linux/miscdevice.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/watchdog.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,12,0)
#include <asm/uaccess.h>
#else
#include <linux/uaccess.h>
#endif
#include <linux/fs.h>
#include <asm/io.h>
#include <MEN/men_typs.h>
#include <MEN/men_chameleon.h>
#include "men_z069_reset_wdg_int.h"


/*
 * Defines
 */

#ifndef Z069_RST_WDG_BIT
# define Z069_RST_WDG_BIT 	0xffffffff
#endif

#define Z069_WDTRIG_VAL_AAAA	0xaaaa
#define WATCHDOG_MINOR 		130
#define PFX 			"men_z069_reset_wdg: "
#define STR_HELPER(x) 		#x
#define M_INT_TO_STR(x) 	STR_HELPER(x)

#ifdef DBG
#define DEBUG_DEFAULT 1
#else
#define DEBUG_DEFAULT 0
#endif

#define Z069DBG(fmt, args...) \
	do { \
		if (debug) { \
			printk( KERN_DEBUG fmt, ## args ); \
		} \
	} while (0)

/*
 * module parameters
 */
static int debug = DEBUG_DEFAULT; /**< enable debug printouts */
module_param(debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Enable debugging printouts (default " M_INT_TO_STR(DEBUG_DEFAULT) ")");

static int device = 0; /**< attach the driver to this Z069 device instance */
module_param(device, int, S_IRUGO);
MODULE_PARM_DESC(device, "Attach the driver to this Z069 device instance (default 0 = first detected device)");

/*
 * about CONFIG_WATCHDOG_NOWAYOUT from menuconfig:
 * The default watchdog behaviour (which you get if you say N here) is
 * to stop the timer if the process managing it closes the file
 * /dev/watchdog. It's always remotely possible that this process might
 * get killed. If you say Y here, the watchdog cannot be stopped once
 * it has been started.
 */
#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif
module_param(nowayout, int, 0664);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");


#ifdef CONFIG_WDT_DEFAULT_TIMEOUT
static int G_defaultTimeout = CONFIG_WDT_DEFAULT_TIMEOUT;
#else
static int G_defaultTimeout = 0;
#endif

static struct semaphore G_openSem; /**< allows only one open  */
static struct semaphore G_wdtLock; /**< locks accesses wdog to regs  */

static int G_expectClose = 0; /**< user has written a "V"  */
static char *G_wdBase; 	/**< mapped wdog reg base   */
static u32 G_ioMapped; 	/**< nonzero if Z69 is IO mapped  */
static u32 G_wdMargin;

static int G_device_cnt = 0; /**< increment this variable after each call of z069_probe */

/*
 * Prototypes
 */
static int z069_probe(CHAMELEON_UNIT_T *chu);
static int z069_remove(CHAMELEON_UNIT_T *chu);
static ssize_t z069_read(struct file *file, char *buf, size_t count, loff_t *ptr);
static ssize_t z069_write(struct file *file, const char *buf, size_t count, loff_t *ppos);
static long z069_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int z069_open(struct inode *inode, struct file *file);
static int z069_release(struct inode *inode, struct file *file);

/*
 * Typedefs
 */
static struct file_operations z069_fops = {
	owner: 		THIS_MODULE,
	llseek: 	no_llseek,
	read: 		z069_read,
	write: 		z069_write,
	unlocked_ioctl: z069_ioctl,
	open: 		z069_open,
	release:	z069_release
};

static u16 G_modCodeArr[] = {
		CHAMELEON_16Z069_RST,
		CHAMELEON_MODCODE_END
};

static CHAMELEON_DRIVER_T G_driver = {
	.name 		= "men_z069_wdg_reset",
	.modCodeArr 	= G_modCodeArr,
	.probe 		= z069_probe,
	.remove 	= z069_remove
};

static struct miscdevice z069_watchdog_miscdev=
{
	WATCHDOG_MINOR,
	"watchdog",
	&z069_fops
};


/*******************************************************************/
/** Wrapper to perform 16bit writes to the Z069, depending on
 *  memmapped or iomapped IP cor
 */
void Z69WRITE_D16(char *base, unsigned int offs, u_int16 val)
{
	if(G_ioMapped)
		outw(val, (unsigned long)(base + offs));
	else
		writew(val, (char*)(base + offs));
}

/*******************************************************************/
/** Wrapper to perform 16bit reads from the Z069, depending on
 *  memmapped or iomapped IP core
 */
u_int16 Z69READ_D16(char *base, unsigned int offs)
{
	u16 retval;

	if(G_ioMapped)
		retval = inw((unsigned long)(base + offs));
	else
		retval = readw((char*)(base + offs));
	return retval;
}

/*******************************************************************/
/** Trigger the watchdog
 *
 *  triggers with the alternating 0xAAAA,0x5555 sequence
 */
static void wdt_trigger(void)
{
	u16 val;
	val = Z69READ_D16(G_wdBase, Z069_RST_WVR);
	Z069DBG("wdt_trigger: Z069_RST_WVR = 0x%04x\n", val);
	Z69WRITE_D16(G_wdBase, Z069_RST_WVR, val ^ 0xffff);
}

/*******************************************************************/
/** Compute timer value for time
 *
 *  \param  time  \IN	Value in 1/100s
 *
 *  \return val   \OUT  Raw counter value or negative Linux error number
 */
static int wdt_time2val(int time)
{
	int val = time * Z069_WDT_TIMER_FREQUENZ / 100;

	if(time != ((val * 100) / Z069_WDT_TIMER_FREQUENZ)) {
		return -EINVAL; /* value overflow */
	}

	if((val < Z069_WDT_COUNTER_MIN) || (val > Z069_WDT_COUNTER_MAX))
		return -EINVAL;

	return val;
}

/*******************************************************************/
/** Compute time for watchdog timer value
 *
 *  \param val 	\IN 	In counts of watchdog timer
 *
 *  \return Time in 1/100s
 */
static int wdt_val2time(int val)
{
	return ((val * 100) / Z069_WDT_TIMER_FREQUENZ);
}

/*******************************************************************/
/** load timeout value into watchdog.
 *
 *  \param time  \IN	Timeout value in 1/100s. 0 means disable watchdog
 *
 *  \return The real timeout value in 1/100s or negative Linux error number
 */
static int wdt_timer_load(int time)
{
	int val;

	Z069DBG("wdt_timer_load %d\n", time);

	if(time) {
		if((val = wdt_time2val(time)) < 0)
			return val;
	} else
		val = 0;

	down(&G_wdtLock);

	if(val) {
		Z69WRITE_D16(G_wdBase, Z069_RST_WTR, val | Z069_RST_WTR_WDEN );
	} else {
		/* disable watchdog */
		Z69WRITE_D16(G_wdBase, Z069_RST_WTR, (u_int16)~Z069_RST_WTR_WDEN);
	}

	up(&G_wdtLock);
	wdt_trigger();

	return val;
}

static ssize_t z069_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int i=0;
	Z069DBG("z069_write:\n");

	if (count) {
		if (!nowayout) {
			G_expectClose = 0;
			for (i = 0; i != count; i++) {
				char c;
				if(get_user(c, buf+i))
					return -EFAULT;
				if (c == 'V') {
					Z069DBG("got 'V'\n");
					G_expectClose = 1;
				}
			}
		}
		wdt_trigger();
	}
	return count;
}

static ssize_t z069_read(struct file *file, char *buf, size_t count,
		loff_t *ptr)
{
	return -EINVAL;
}

/*******************************************************************/
/** z069_SetResetMask:
 *	\param value \IN    value to be set to reset mask register
 *
 *	\return always 0
 */
int z069_SetResetMask(u_int32 value)
{
	Z069DBG("Z069_SetResetMask := 0x%08x\n", value);
	Z69WRITE_D16(G_wdBase, Z069_RST_RMR, value);
	return 0;
}

/*******************************************************************/
/**	Z069_GetResetMask:
 *	\param value \OUT    read value from reset mask register
 *
 *	\return always 0
 */
int z069_GetResetMask(u_int32 *value)
{
	int retVal=Z69READ_D16(G_wdBase, Z069_RST_RMR);
	Z069DBG("Z069_GetResetMask := 0x%08x\n", retVal);
	*value = retVal;
	return 0;
}

/*******************************************************************/
/** z069_SetResetCause:
 *	\param value \IN    value to be set to reset cause register
 *
 *	\return always 0
 */
int z069_SetResetCause(u_int32 value)
{
	Z069DBG("Z069_SetResetCause := 0x%08x\n", value);
	Z69WRITE_D16(G_wdBase, Z069_RST_RCR, value);
	return 0;
}

/*******************************************************************/
/** z069_GetResetCause:
 *	\param value \OUT    value to be set to reset cause register
 *
 *	\return always 0
 */
int z069_GetResetCause(u_int32 *value)
{
	int retVal = Z69READ_D16(G_wdBase, Z069_RST_RCR);
	Z069DBG("Z069_GetResetCause := 0x%08x\n", retVal);
	*value = retVal;
	return 0;
}

/*******************************************************************/
/** z069_SetResetRequest:
 *	\param value \IN    value to be set to reset request register
 *
 *	\return always 0
 */
int z069_SetResetRequest(u_int32 value)
{
	Z069DBG("Z069_SetResetRequest := 0x%08x\n", value);
	Z69WRITE_D16(G_wdBase, Z069_RST_RRR, value);
	return 0;
}

/*******************************************************************/
/** z069_GetResetRequest:
 *	\param value \IN    value to be set to reset request register
 *
 *	\return always 0
 */
int z069_GetResetRequest(u_int32 *value)
{
	int retVal = Z69READ_D16(G_wdBase, Z069_RST_RRR);
	Z069DBG("Z069_GetResetRequest := 0x%08x\n", retVal);
	*value = retVal;
	return 0;
}

/*******************************************************************/
/** file ops: open the device
 *
 * The watchdog device is single open and on opening we activate
 * the watchdog with its default value.
 *
 */
static int z069_open(struct inode *inode, struct file *file)
{
	u_int16 maskReg = 0;

	Z069DBG("z069_open\n");

	if(down_trylock(&G_openSem))
		return -EBUSY;

	/* now activate watchdog with default timeout */
	if(wdt_timer_load(G_defaultTimeout) < 0)
		wdt_timer_load(wdt_val2time( Z069_WDT_COUNTER_MAX));

	/* init WD trigger value register */
	Z69WRITE_D16(G_wdBase, Z069_RST_WVR, Z069_WDTRIG_VAL_AAAA);

	maskReg = Z69READ_D16(G_wdBase, Z069_RST_RMR);

	/* set Reset mask Register, considering the Z069_RST_WDG_BIT */
	Z69WRITE_D16(G_wdBase, Z069_RST_RMR, maskReg & ~Z069_RST_WDG_BIT);

	wdt_trigger();
	G_expectClose = 0;

	return 0;

}

/*******************************************************************/
/** file ops: release the device
 *
 */
static int z069_release(struct inode *inode, struct file *file)
{
	if(down_trylock(&G_openSem))
		return -EBUSY;

	Z069DBG("z069_release\n");

	if(G_expectClose) {
		wdt_timer_load(0); /* disable wdog */
	} else {
		printk(KERN_DEBUG PFX "Unexpected close, not stopping timer!\n");
		wdt_trigger();
	}
	up(&G_openSem);
	return 0;
}

static const struct watchdog_info z069_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
	.identity = "Z069 WDT",
	.firmware_version = 1,
};


/*******************************************************************/
/** file ops: ioctl handling
 *
 */
static long z069_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int margin;
	int retVal = 0;

	Z069DBG("wdt_ioctl: ");
	switch(cmd) {

	/* official watchdog-API commands */
	case WDIOC_GETSUPPORT:
		Z069DBG("WDIOC_GETSUPPORT\n");
		retVal = copy_to_user((struct watchdog_info *)arg, &z069_wdt_info, sizeof(z069_wdt_info)) ? -EFAULT : 0;
		break;
	case WDIOC_KEEPALIVE:
		Z069DBG("WDIOC_KEEPALIVE\n");
		wdt_trigger();
		break;
	case WDIOC_SETTIMEOUT:
		Z069DBG("WDIOC_SETTIMEOUT\n");
		if(get_user(margin, (int *)arg))
			return -EFAULT;
		G_wdMargin = margin;
		retVal = wdt_timer_load(margin * 100 );
		break;
	case WDIOC_GETTIMEOUT:
		Z069DBG("WDIOC_GETTIMEOUT\n");
		margin = wdt_val2time(G_wdMargin) / 100;
		retVal = put_user(margin, (int *)arg);
		break;

	/* MEN reset controller ioctl's */
	case RSTIOC_SET_RESET_MASK:
		Z069DBG("RSTIOC_SET_RESET_MASK\n");
		get_user(margin, (int *)arg);
		z069_SetResetMask(margin);
		break;
	case RSTIOC_GET_RESET_MASK:
		Z069DBG("RSTIOC_GET_RESET_MASK\n");
		z069_GetResetMask((u_int32 *)&margin);
		retVal = put_user(margin, (int *)arg);
		break;
	case RSTIOC_SET_RESET_CAUSE:
		Z069DBG("RSTIOC_SET_RESET_CAUSE\n");
		get_user(margin, (int *)arg);
		z069_SetResetCause(margin);
		break;
	case RSTIOC_GET_RESET_CAUSE:
		Z069DBG("RSTIOC_GET_RESET_CAUSE\n");
		z069_GetResetCause((u_int32 *)&margin);
		retVal = put_user(margin, (int *)arg);
		break;
	case RSTIOC_SET_RESET_REQUEST:
		Z069DBG("RSTIOC_SET_RESET_REQUEST\n");
		get_user(margin, (int *)arg);
		z069_SetResetRequest(margin);
		break;
	case RSTIOC_GET_RESET_REQUEST:
		Z069DBG("RSTIOC_GET_RESET_REQUEST\n");
		z069_GetResetRequest((u_int32 *)&margin);
		retVal = put_user(margin, (int *)arg);
		break;
	default:
		Z069DBG("not supported\n");
		return -ENOTTY;
	}
	return retVal;
}

static int z069_probe(CHAMELEON_UNIT_T *chu)
{
	int ret = 0;
	int memReq = 0;

	sema_init(&G_openSem, 1);
	sema_init(&G_wdtLock, 1);

	/*--- check for the right Z069 device instance ---*/
	if ( device != G_device_cnt++ ) {
		return -ENODEV;
	}

	/*--- are we io-mapped ? ---*/
	G_ioMapped = pci_resource_flags(chu->pdev, chu->bar) & IORESOURCE_IO;

	printk(KERN_INFO "MEN 16Z069 Watchdog/Reset IP core driver.\n" );
	printk(KERN_INFO "Found 16Z069 unit @ %p using %s-mapped access\n", chu->phys, G_ioMapped ? "IO" : "mem" );

	if ( G_ioMapped ) {
		if( request_region( (unsigned long)chu->phys, (unsigned long)Z069_REG_SIZE, "Z069_WDG") == NULL ) {
			printk (KERN_ERR PFX " error on request_region\n");
			goto out;
		}
		memReq++;
		G_wdBase = (char*)chu->phys; /* IO-mapped addresses are used directly via inb/w/l, outb/w/l */
	} else {
		if ( request_mem_region((unsigned long)chu->phys, (unsigned long)Z069_REG_SIZE, "Z069_WDG" ) < 0 ) {
			printk (KERN_ERR PFX " error on request_mem_region\n");
			goto out;
		}

		memReq++;
		if((G_wdBase = (char*)ioremap((unsigned long)chu->phys, Z069_REG_SIZE)) == NULL)
			goto out;
	}

	if(G_defaultTimeout == 0)
		G_defaultTimeout = wdt_val2time( Z069_WDT_COUNTER_MAX);

	Z069DBG("Default timeout=%d\n", G_defaultTimeout);

	ret = misc_register(&z069_watchdog_miscdev);
	if ( ret ) {
		printk (KERN_ERR PFX "Cannot register watchdog misc device (error code %d)\n", ret );
		goto out;
	}

	return ret;
out:
	printk(KERN_ERR PFX "Unable to register driver, z069_probe failed\n");
	if(G_wdBase)
		iounmap(G_wdBase);

	if(memReq) {
		if(G_ioMapped) {
			release_region((unsigned long)chu->phys, (unsigned long)Z069_REG_SIZE);
		} else {
			release_mem_region((unsigned long)chu->phys, (unsigned long)Z069_REG_SIZE);
		}
	}
	return -ENODEV;
}

static int z069_remove(CHAMELEON_UNIT_T *chu)
{
	Z069DBG("z069_remove\n");
	misc_deregister(&z069_watchdog_miscdev);
	if(G_ioMapped) {
		release_region( (unsigned long)chu->phys, (unsigned long)Z069_REG_SIZE);
	} else {
		iounmap(G_wdBase);
		release_mem_region((unsigned long)chu->phys, (unsigned long)Z069_REG_SIZE);
	}
	return 0;
}

/* module stuff */
static int __init z069_init(void)
{
	men_chameleon_register_driver( &G_driver );
	return 0;
}

static void __exit z069_cleanup(void)
{
	men_chameleon_unregister_driver( &G_driver );
}

module_init( z069_init );
module_exit( z069_cleanup );

MODULE_LICENSE( "GPL" );
MODULE_DESCRIPTION( "MEN watchdog/Reset IP core driver" );
MODULE_AUTHOR("Thomas Schnuerer <thomas.schnuerer@men.de>");
#ifdef MAK_REVISION
MODULE_VERSION(MENT_XSTR(MAK_REVISION));
#endif
