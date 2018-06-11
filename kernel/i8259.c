#include <asm/io.h>
#include <asm/regs.h>
#include <asm/system.h>
#include <cnix/head.h>
#include <cnix/kernel.h>

#define IRQ0		0x20

#define INT1_CTL	0x20
#define INT1_MASK	0x21
#define INT2_CTL	0xa0
#define INT2_MASK	0xa1
#define ENABLE_INT	0x20

#define IRQ_NUM		16

extern void hw_int0(void);
extern void hw_int1(void);
extern void hw_int2(void);
extern void hw_int3(void);
extern void hw_int4(void);
extern void hw_int5(void);
extern void hw_int6(void);
extern void hw_int7(void);
extern void hw_int8(void);
extern void hw_int9(void);
extern void hw_int10(void);
extern void hw_int11(void);
extern void hw_int12(void);
extern void hw_int13(void);
extern void hw_int14(void);
extern void hw_int15(void);

int nested_intnum;

static void (*irq_table[IRQ_NUM])(struct regs_t *);

static irqhandler_t intr_gate[IRQ_NUM] = {
	hw_int0, hw_int1, hw_int2, hw_int3, hw_int4, hw_int5, hw_int6, hw_int7,
	hw_int8, hw_int9, hw_int10, hw_int11, hw_int12, hw_int13, hw_int14, 
	hw_int15
};

void disable_irq(int irq)
{
	unsigned char value;
	unsigned int port;
	
	port = irq < 8 ? INT1_MASK : INT2_MASK;

	irq = irq % 8;
	value = inb(port) | (1 << irq);
	outb(value, port);
}

void enable_irq(int irq)
{
	unsigned char value;
	unsigned int port;

	port = irq < 8 ? INT1_MASK : INT2_MASK;

	irq = irq % 8;
	value = inb(port) & (~(1 << irq));
	outb(value, port);
}

static void ack_irq(int irq)
{
	outb(ENABLE_INT, INT1_CTL);

	/* if will delay. */
	if(irq > 8)
		outb(ENABLE_INT, INT2_CTL);
}

static void default_handler(struct regs_t * regs)
{
	printk("unexpected irq: %d\n", regs->index);
}

void intr_init(void)
{
	int i;

	nested_intnum = 1;

	for(i = 0; i < IRQ_NUM; i++)
		irq_table[i] = default_handler;

	for(i = 0; i < IRQ_NUM; i++)
		set_intr_gate(IRQ0 + i, intr_gate[i]);
}

int put_irq_handler(int irq, void (*handler)(struct regs_t * regs))
{
	if(irq < 0 || irq >= IRQ_NUM)
		DIE("invalid irq %d\n", irq);
	
	if(irq_table[irq] == handler)
		return 0;

	if(irq_table[irq] != default_handler)
		DIE("already has handler on irq %d\n", irq);
	
	disable_irq(irq);
	irq_table[irq] = handler;
	enable_irq(irq);

	return 0;
}

void do_with_irq(struct regs_t regs)
{
	nested_intnum++;
	disable_irq(regs.index);
	ack_irq(regs.index);
	sti();
	irq_table[regs.index](&regs);
	cli();
	enable_irq(regs.index);
	nested_intnum--;
}
