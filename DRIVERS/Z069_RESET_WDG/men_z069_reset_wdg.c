/*********************  P r o g r a m  -  M o d u l e ***********************/
/*!
 *        \file  men_z069_reset_wdg.c
 *
 *      \author  aw
 *        $Date: 2010/12/03 19:18:47 $
 *    $Revision: 1.6 $
 *
 *      \brief   reset and watchdog driver
 *
 *     Required: -
 *
 *     \switches Z069_RST_WDG_BIT - Define the watchdog bit (board specific)
 */
 /*-------------------------------[ History ]--------------------------------
 *
 * $Log: men_z069_reset_wdg.c,v $
 * Revision 1.6  2010/12/03 19:18:47  ts
 * R: Z069 instance in F11S is located in an I/O mapped BAR, not supported yet
 * M: added support for IO mapped Z069 (inw/outw instead readw/writew on x86)
 *
 * Revision 1.5  2009/02/06 17:58:23  rt
 * R:1. Cosmetics
 * M:1. Added Z069_RST_WDG_BIT comment
 *
 * Revision 1.4  2008/10/06 15:57:57  aw
 * R: ioctl doesn't used get_user function to copy the variable from user to kernel space
 * M: use now get_user function
 *
 * Revision 1.3  2008/07/25 10:35:23  aw
 * cosmetics
 *
 * Revision 1.2  2008/07/24 13:09:26  aw
 * R: reset driver was needed
 * M: added reset functions to men_z069_reset_wdg driver
 *
 * Revision 1.1  2008/07/14 09:30:11  aw
 * Initial Revision
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2008 by MEN Mikro Elektronik GmbH, Nuremberg, Germany
 ****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <MEN/men_typs.h>
#include <MEN/sysdep.h>
#include <MEN/men_chameleon.h>
#include "men_z069_reset_wdg_int.h"


#define WATCHDOG_MINOR 130
#define Z069_RST_WDG_BIT 0xffffffff

#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif /* DEBUG */
#define PFX "men_z069_reset_wdg: "

#ifdef CONFIG_WDT_DEFAULT_TIMEOUT
static int G_defaultTimeout = CONFIG_WDT_DEFAULT_TIMEOUT;
#else
static int G_defaultTimeout = 0;
#endif

static struct semaphore G_openSem; /**< allows only one open  */
static struct semaphore G_wdtLock; /**< locks accesses wdog to regs  */
static struct semaphore G_ledLock; /**< locks accesses ledcr reg  */

static int G_expectClose = 0;	/**< user has written a "V"  */
static int G_wdMargin;			/**< current timeout in counter units  */

static int G_unitType;			/**< FPGA unit type found */
static char *G_wdBase;			/**< mapped wdog reg base   */
static u32 G_wdPhys;			/**< phys wdog reg base   */

static u32 G_ioMapped;			/**< nonzero if Z69 is IO mapped  */
static u32 G_RegsSize;

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

module_param(nowayout, int, 0664);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");


/*******************************************************************/
/** Wrapper to perform 16bit writes to the Z069, depending on
 *  memmapped or iomapped IP core
 *
 */
void Z69WRITE_D16( char *base , unsigned int offs, u_int16 val)
{
	//printk("Z69WRITE_D16: %p = 0x%x\n", (char*)(base + offs), val);
	if (G_ioMapped)
		outw(val, (unsigned int)(base + offs));
	else
		writew(val, (char*)(base+offs));
}


/*******************************************************************/
/** Wrapper to perform 16bit reads from the Z069, depending on
 *  memmapped or iomapped IP core
 *
 */
u_int16 Z69READ_D16(char *base, unsigned int offs)
{
	u16 retval;
    // *value = Z69READ_D16( G_wdBase, Z069_RST_RCR );
	if (G_ioMapped)
		retval = inw((unsigned int)(base+offs));
	else
		retval = readw((char*)(base+offs));

	//printk("Z69READ_D16: 0x%p = 0x%x\n", (char*)(base + offs), retval);
	
	return retval;
}


/*******************************************************************/
/** Trigger the watchdog
 */
static void wdt_trigger(void)
{
	u16 val;
	DBG("wdt_trigger\n");
	down( &G_wdtLock );

	val = Z69READ_D16(G_wdBase,Z069_RST_WVR);
	Z69WRITE_D16( G_wdBase, Z069_RST_WVR, val ^ 0xffff );

	up( &G_wdtLock );
}

/*******************************************************************/
/** Compute timer value for time
 *
 *  \param  time  \IN	Value in 1/100s
 *
 *  \return val   \OUT  Raw counter value or negative Linux error number
 */
static int wdt_time2val( int time )
{
	int val = time * Z069_WDT_TIMER_FREQUENZ / 100;

	DBG("wdt_time2val\n");

	if( time != ((val * 100) / Z069_WDT_TIMER_FREQUENZ) ){
		return -EINVAL;			/* value overflow */
	}

	if( (val < Z069_WDT_COUNTER_MIN) || (val > Z069_WDT_COUNTER_MAX))
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
static int wdt_val2time( int val )
{
	DBG("wdt_val2time\n");
	return ((val * 100) / Z069_WDT_TIMER_FREQUENZ);
}

/*******************************************************************/
/**
 *	wdt_timer_load:
 *
 *	Load timeout value into watchdog.
 *
 *  \param time  \IN	Timeout value in 1/100s. 0 means disable watchdog
 *  \return The real timeout value in 1/100s or negative Linux error number
 */
static int wdt_timer_load( int time )
{
	int val;

	DBG("wdt_timer_load %d\n", time );

	if( time ){
		if(( val = wdt_time2val( time )) < 0 )
			return val;
	}
	else
		val = 0;

	down( &G_wdtLock );

	if( val ){
		Z69WRITE_D16( G_wdBase, Z069_RST_WTR, val );
		Z69WRITE_D16( G_wdBase, Z069_RST_WTR, val | Z069_RST_WTR_WDEN );
	}
	else {
		/* disable watchdog */
		Z69WRITE_D16(G_wdBase, Z069_RST_WTR, 0xffff );
		Z69WRITE_D16(G_wdBase, Z069_RST_WTR, (u_int16)~Z069_RST_WTR_WDEN );
	}
	DBG( "wdt_timer_load read val = 0x%04x\n",
		 Z69READ_D16( G_wdBase, Z069_RST_WTR ));
	G_wdMargin = val;

	up( &G_wdtLock );

	wdt_trigger();

	return val;
}


/*******************************************************************/
/**
 *	Z069_write:
 *	\param file     \IN     File handle to the watchdog
 *	\param buf      \OUT    Buffer to write (unused as data does not matter here)
 *	\param count    \IN     Count of bytes
 *	\param ppos     \IN     Pointer to the position to write. No seeks allowed.
 *
 *	A write to a watchdog device is defined as a keep-alive signal. Any
 *	write of data will do, as we don't define content meaning.
 */
static ssize_t Z069_write(
	struct file *file,
	const char *buf,
	size_t count,
	loff_t *ppos)
{
	DBG("Z069_write\n" );

#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,5,0)
	/*  Can't seek (pwrite) on this device. (Always fails on 2.6.x.)  */
	if (ppos != &file->f_pos)
		return -ESPIPE;
#endif

	if (count) {
		if (!nowayout) {
			size_t i;

			G_expectClose = 0;

			for (i = 0; i != count; i++) {
				char c;
				if(get_user(c, buf+i))
					return -EFAULT;
				if (c == 'V')
					G_expectClose = 1;
			}
		}
		wdt_trigger();
	}

	return count;
}

/*******************************************************************/
/**
 *	Z069_read:
 *	\param file     \IN     File handle to the watchdog board
 *	\param buf      \IN     Buffer to write 1 byte into
 *	\param count    \IN     Length of buffer
 *	\param ptr      \IN     Offset (no seek allowed)
 *
 *	Nothing to read here.
 */
static ssize_t Z069_read(
	struct file *file,
	char *buf,
	size_t count,
	loff_t *ptr)
{
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,5,0)
	/*  Can't seek (pread) on this device. (Always fails on 2.6.x.)  */
	if (ptr != &file->f_pos)
		return -ESPIPE;
#endif

	return -EINVAL;
}

/*******************************************************************/
/**
 *	Z069_SetResetMask:
 *	\param value \IN    value to be set to reset mask register
 *
 *	\return always 0
 */
int Z069_SetResetMask( u_int32 value )
{
    DBG("Z069_SetResetMask\n");
    Z69WRITE_D16( G_wdBase, Z069_RST_RMR, value );
    
    return 0;
}

/*******************************************************************/
/**
 *	Z069_GetResetMask:
 *	\param value \OUT    read value from reset mask register
 *
 *	\return always 0
 */
int Z069_GetResetMask( u_int32 *value )
{
    DBG("Z069_GetResetMask\n");
    *value = Z69READ_D16( G_wdBase, Z069_RST_RMR );
    
    return 0; 
}

/*******************************************************************/
/**
 *	Z069_SetResetCause:
 *	\param value \IN    value to be set to reset cause register
 *
 *	\return always 0
 */
int Z069_SetResetCause( u_int32 value )
{
    DBG("Z069_SetResetCause\n");
    Z69WRITE_D16( G_wdBase, Z069_RST_RCR, value );
    
    return 0;    
}

/*******************************************************************/
/**
 *	Z069_GetResetCause:
 *	\param value \OUT    value to be set to reset cause register
 *
 *	\return always 0
 */
int Z069_GetResetCause( u_int32 *value )
{
    DBG("Z069_GetResetCause\n");
    *value = Z69READ_D16( G_wdBase, Z069_RST_RCR );
    
    return 0;    
}

/*******************************************************************/
/**
 *	Z069_SetResetRequest:
 *	\param value \IN    value to be set to reset request register
 *
 *	\return always 0
 */
int Z069_SetResetRequest( u_int32 value )
{
    DBG("Z069_SetResetRequest\n");
    Z69WRITE_D16( G_wdBase, Z069_RST_RRR, value );
    
    return 0; 
}

/*******************************************************************/
/**
 *	Z069_GetResetRequest:
 *	\param value \IN    value to be set to reset request register
 *
 *	\return always 0
 */
int Z069_GetResetRequest( u_int32 *value )
{
    DBG("Z069_GetResetRequest\n");
    *value = Z69READ_D16( G_wdBase, Z069_RST_RRR );
    
    return 0; 
}

/*******************************************************************/
/**
 *	Z069_ioctl:
 *	\param inode    \IN     Inode of the device
 *	\param file     \IN     File handle to the device
 *	\param cmd      \IN     Watchdog command
 *	\param arg      \INOUT  Argument pointer
 *
 */
static int Z069_ioctl(
	struct inode *inode,
	struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	int margin;
	static struct watchdog_info ident=
	{
		WDIOF_SETTIMEOUT|WDIOF_MAGICCLOSE,
		1,
		"MEN Z069_RESET"
	};

	DBG("wdt_ioctl " );
	switch(cmd)
	{
		default:
			DBG("not supported\n");
			return -ENOTTY;
		case WDIOC_GETSUPPORT:
			DBG("WDIOC_GETSUPPORT\n" );
			return copy_to_user((struct watchdog_info *)arg, &ident,
								sizeof(ident))?-EFAULT:0;
		case WDIOC_KEEPALIVE:
			DBG("WDIOC_KEEPALIVE\n" );
			wdt_trigger();
			return 0;
		case WDIOC_SETTIMEOUT:
			DBG("WDIOC_SETTIMEOUT\n" );
			if (get_user(margin, (int *)arg))
				return -EFAULT;

			/* convert to 1/100s */
			margin *= 100;

			wdt_timer_load( margin );
			/* Fall */
		case WDIOC_GETTIMEOUT:
			DBG("WDIOC_GETTIMEOUT\n" );
			margin = wdt_val2time( G_wdMargin ) / 100;
			return put_user(margin, (int *)arg);
		case RSTIOC_SET_RESET_MASK:
		    DBG("RSTIOC_SET_RESET_MASK\n" );
		    get_user(margin, (int *)arg);
		    Z069_SetResetMask( margin );
		    return 0;
		case RSTIOC_GET_RESET_MASK:
		    DBG("RSTIOC_GET_RESET_MASK\n" );
		    Z069_GetResetMask((u_int32 *)&margin);
		    return put_user(margin, (int *)arg);
		case RSTIOC_SET_RESET_CAUSE:
		    DBG("RSTIOC_SET_RESET_CAUSE\n" );
		    get_user(margin, (int *)arg);
		    Z069_SetResetCause(margin);
		    return 0;
		case RSTIOC_GET_RESET_CAUSE:
		    DBG("RSTIOC_GET_RESET_CAUSE\n" );
		    Z069_GetResetCause((u_int32 *)&margin);
		    return put_user(margin, (int *)arg);
		case RSTIOC_SET_RESET_REQUEST:
		    DBG("RSTIOC_SET_RESET_REQUEST\n" );
		    get_user(margin, (int *)arg);		    
		    Z069_SetResetRequest(margin);
		    return 0;
		case RSTIOC_GET_RESET_REQUEST:
		    DBG("RSTIOC_GET_RESET_REQUEST\n" );
		    Z069_GetResetRequest((u_int32 *)&margin);
		    return put_user(margin, (int *)arg);
	}
}

/*******************************************************************/
/**
 *	Z069_open:
 *	\param inode    \IN     Inode of device
 *	\param file     \IN     File handle to device
 *
 * The watchdog device is single open and on opening we activate
 * the watchdog with its default value.
 *
 */
static int Z069_open(struct inode *inode, struct file *file)
{
    u_int16 maskReg = 0;
    
	DBG("Z069_open\n" );
	
	switch(MINOR(inode->i_rdev))
	{
		case WATCHDOG_MINOR:
			if (down_trylock(&G_openSem))
				return -EBUSY;

			if (nowayout) {
				MOD_INC_USE_COUNT;
			}

			/* now activate watchdog with default timeout */
			if( wdt_timer_load( G_defaultTimeout ) < 0 )
				wdt_timer_load( wdt_val2time( Z069_WDT_COUNTER_MAX ));

			/* init WD trigger value register */
			Z69WRITE_D16( G_wdBase, Z069_RST_WVR, 0xaaaa );
			
			maskReg = Z69READ_D16( G_wdBase, Z069_RST_RMR );
			
			/* set Reset mask Register, considering the Z069_RST_WDG_BIT */
			Z69WRITE_D16( G_wdBase, Z069_RST_RMR, maskReg & ~Z069_RST_WDG_BIT );

			wdt_trigger();
			G_expectClose = 0;
			return 0;
		case Z069_RESET_MINOR:
		    return 0;

		default:
			return -ENODEV;
	}
}

/*******************************************************************/
/**
 *	Z069_release:
 *	\param inode    \IN     Inode to board
 *	\param file     \IN     File handle to board
 *
 *	The watchdog has a configurable API. There is a religious dispute
 *	between people who want their watchdog to be able to shut down and
 *	those who want to be sure if the watchdog manager dies the machine
 *	reboots. In the former case we disable the counters, in the latter
 *	case you have to open it again very soon.
 */
static int Z069_release(struct inode *inode, struct file *file)
{
    DBG("Z069_release\n");
	if (MINOR(inode->i_rdev)==WATCHDOG_MINOR) {
		if (G_expectClose) {
			wdt_timer_load( 0 ); /* disable wdog */
		} else {
			printk(KERN_CRIT PFX "Unexpected close, not stopping timer!\n");
			wdt_trigger();
		}
		up(&G_openSem);
	}
	
	return 0;
}

/******************************************************************/
/**
 *	Z069_notify_sys:
 *	\param this     \IN     Our notifier block
 *	\param code     \IN     The event being reported
 *	\param unused   \IN     Unused
 *
 *	Our notifier is called on system shutdowns. We want to turn the card
 *	off at reboot, otherwise the machine will reboot again during memory
 *	test or worse yet during the following fsck. This would suck, in fact
 *	trust me - if it happens it does suck.
 */
static int Z069_notify_sys(struct notifier_block *this, unsigned long code,
	void *unused)
{
	DBG("Z069_notify_sys code=%ld\n", code );

	if (code==SYS_DOWN || code==SYS_HALT || code==SYS_POWER_OFF) {
		wdt_timer_load( 0 ); /* disable wdog */
	}
	return NOTIFY_DONE;
}

/*
 *	Kernel Interfaces
 */


static struct file_operations Z069_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		Z069_read,
	write:		Z069_write,
	ioctl:		Z069_ioctl,
	open:		Z069_open,
	release:	Z069_release,
};

static struct miscdevice Z069_watchdog_miscdev=
{
	WATCHDOG_MINOR,
	"watchdog",
	&Z069_fops
};

static int Z069_probe( CHAMELEON_UNIT_T *chu );
static int Z069_remove( CHAMELEON_UNIT_T *chu );

static u16 G_modCodeArr[] = {
	CHAMELEON_16Z069_RST,
	CHAMELEON_MODCODE_END
};

static CHAMELEON_DRIVER_T G_driver = {
	.name		=		"men_z069_wdg_reset",
	.modCodeArr = 		G_modCodeArr,
	.probe		=		Z069_probe,
	.remove		= 		Z069_remove
};

/*******************************************************************/
/** PNP function for Z069
 *
 * This gets called when the Chameleon PNP subsystem starts and
 * is called for each Z069_RESET unit.
 *
 * Does not touch the current state of the watchdog.
 *
 * \param chu		\IN Z069 unit found
 * \return 0 on success or negative Linux error number
 */
static int Z069_probe( CHAMELEON_UNIT_T *chu )
{
	static int devCount = 0;
	int ret = -ENODEV;
	//int ioMapped = 0;
	int memReq = 0, miscReg=0;

	DBG("Z069_probe\n");

	devCount++;
	if (devCount > 1) {
		printk (KERN_ERR PFX "this driver only supports 1 device\n");
		return ret;
	}
	printk(KERN_INFO "MEN Z069 RESET WDG driver built %s %s: "
		   "Found z069 module code 0x%02x\n",
		   __DATE__, __TIME__, chu->modCode );

	sema_init(&G_openSem, 1);
	sema_init(&G_wdtLock, 1);
	G_unitType = chu->modCode;

	if( G_unitType == CHAMELEON_16Z069_RST ) {
		sema_init(&G_ledLock, 1);
	}

	G_wdPhys = (u32)chu->phys;
	if( G_unitType == CHAMELEON_16Z069_RST ) {
		G_RegsSize = Z069_REGS_SIZE;
	} else {
		G_RegsSize = Z069_WDG_REGS_SIZE;
	}

	/*--- are we io-mapped ? ---*/
    G_ioMapped = pci_resource_flags( chu->pdev, chu->bar ) & IORESOURCE_IO;
    DBG( "bar=%d ioMapped=0x%x\n", chu->bar, ioMapped );

#ifdef MAC_MEM_MAPPED
	if( check_mem_region( G_wdPhys + Z069_RST_WTR, G_RegsSize ) < 0)
		goto out;

	request_mem_region( G_wdPhys + Z069_RST_WTR, G_RegsSize,
						"Z069_RESET_WDG" );
	memReq++;
	if( (G_wdBase = ioremap( G_wdPhys, 0x100 )) == NULL )
		goto out;
#else /* MAC_IO_MAPPED */

	DBG( "trying request_region(%x, %x,name)\n", G_wdPhys, G_RegsSize );

#if 1
	if( request_region( G_wdPhys + Z069_RST_WTR, G_RegsSize, "Z069_RESET_WDG") == NULL ) {
		printk (KERN_ERR PFX " error on request_region\n");
		goto out;
	}
#endif

	memReq++;
	G_wdBase = (char*)G_wdPhys;
	printk(KERN_INFO "MEN Z069_SYSTEM WDT driver: Using IO-mapped access\n");
#endif /* MAC_MEM_MAPPED */
	
	ret = misc_register (&Z069_watchdog_miscdev);
	if (ret) {
		printk (KERN_ERR PFX "[2] can't misc_register on minor=%d\n",
				WATCHDOG_MINOR);
		goto out;
	}

	if( G_defaultTimeout == 0 )
		G_defaultTimeout = wdt_val2time( Z069_WDT_COUNTER_MAX );

	DBG("Default timeout=%d\n", G_defaultTimeout );

	return 0;

 out:
 	printk(KERN_ERR PFX "Unable to register driver, Z069_probe failed\n");

	if( miscReg )
		misc_deregister(&Z069_watchdog_miscdev);

	if( G_wdBase )
		iounmap( G_wdBase );
	if( memReq )
#ifdef MAC_MEM_MAPPED
		release_mem_region( G_wdPhys + Z069_RST_WTR, G_RegsSize );
#else
		release_region( G_wdPhys + Z069_RST_WTR, G_RegsSize );
#endif

	return ret;
}

/*******************************************************************/
/** PNP function to remove Z069 driver
 *
 * \param chu		\IN wdt unit to remove
 * \return 0 on success or negative Linux error number
 */
static int Z069_remove( CHAMELEON_UNIT_T *chu )
{
	DBG("Z069_remove\n");

	misc_deregister(&Z069_watchdog_miscdev);

#ifdef MAC_MEM_MAPPED
	iounmap( G_wdBase );
	release_mem_region( G_wdPhys + Z069_RST_WTR, G_RegsSize );
#else
	release_region( G_wdPhys + Z069_RST_WTR, G_RegsSize );
#endif

	return 0;
}

/*******************************************************************/
/** Module init function
 */
static int __init Z069_init(void)
{
	DBG("Z069_init:\n" );

	men_chameleon_register_driver( &G_driver );
	return 0;
}

/**********************************************************************/
/** Driver's cleanup routine
 */
static void __exit Z069_cleanup(void)
{
	DBG("Z069_cleanup\n");

	men_chameleon_unregister_driver( &G_driver );
}

module_init(Z069_init);
module_exit(Z069_cleanup);

MODULE_LICENSE( "GPL" );
MODULE_DESCRIPTION( "MEN FPGA unit Z069_Reset watchdog and reset support" );
MODULE_AUTHOR("Anita Wanka <anita.wanka@men.de>");
