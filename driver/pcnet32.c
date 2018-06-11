#include <asm/io.h>
#include <asm/system.h>
#include <cnix/types.h>
#include <cnix/string.h>
#include <cnix/kernel.h>
#include <cnix/driver.h>
#include <cnix/mm.h>
#include <cnix/net.h>
#include <cnix/pcnet32.h>

#define MAC_16		0x0
#define RDP_16		0x10
#define RAP_16		0x12
#define RESET_16	0x14
#define BDP_16		0x16

#define DRX		(1)		// disable receive
#define DTX		(1 << 1)	// disable transimit
#define LOOP		(1 << 2)
#define PROMIS		(1 << 15)	// Promiscuous mode

#define	COMP_RTX_LEN(r, t) ((r << 4) | (t << 12))

#define	IS_OWN(x)	(((x) >> 15) & 1)
#define SET_OWN(x)	(x |= (1 << 15))
#define CLR_OWN(x)	(x &= ~(1 << 15))

#define SET_STP(x)	(x |= (1 << 9))
#define IS_STP(x)	(((x) << 9) & 1)

#define SET_ENP(x)	(x |= (1 << 8))
#define IS_ENP(x)	(((x) << 8) & 1)

#define SET_SEND(x)	(x |= (1 << 3))

#define IS_BAM(x)	(((x) >> 4) & 1) // boardcast address match
#define IS_LAFM(x)	(((x) >> 5) & 1) // logical address filter match
#define IS_PAM(x)	(((x) >> 6) & 1) // physical address match
#define IS_ERR(x)	(((x) >> 14) & 1) // receive error

#define SET_STOP(x)	((x) |= 1 << 2)
#define SET_INIT(x)	((x) |= 1)

#define INIT	0
#define STRT	1
#define STOP 	2
#define TDMD	3
#define TXON	4
#define RXON	5
#define IENA	6
#define INTR	7
#define IDON	8
#define TINT	9
#define RINT	10
#define MERR	11
#define MISS	12
#define CERR	13
#define BABL	14
#define ERR	15

#define DUMP_FLAG(val, x)	do 				\
				{ 				\
					if ((val >> x) & 1)	\
					{ 			\
						printk(#x" ");	\
					}			\
				} while (0)
				
#define SSIZE32	8
#define SWSTYLE	2

#define COMPLEMENT(x)	(~(-x) + 1)

#define AMD_CHIP_MASK	0x003
#define AMD_PART_MASK	0xffff
#define AM79C960	0x0003
#define AM79C961	0x2260
#define AM79C961A	0x2261
#define AM79C965	0x2430
#define AM79C970	0x0242
#define AM79C970A	0x2621
#define AM79C971	0x2623
#define AM79C972	0x2624
#define AM79C973	0x2625
#define AM79C978	0x2626
#define AM79C975	0x2627

#define UnknownChip	0
#define LANCE           1        /* Am7990	*/
#define C_LANCE         2        /* Am79C90	*/
#define PCnet_ISA       3        /* Am79C960	*/
#define PCnet_ISAplus   4        /* Am79C961	*/
#define PCnet_ISA_II    5        /* Am79C961A	*/
#define PCnet_32        6        /* Am79C965	*/
#define PCnet_PCI       7        /* Am79C970	*/
#define PCnet_PCI_II    8        /* Am79C970A	*/
#define PCnet_FAST      9        /* Am79C971	*/
#define PCnet_FASTplus  10       /* Am79C972	*/
#define PCnet_Home	11	 /* Am79C978	*/
#define PCnet_FAST_III	12	 /* Am79C973/Am79C975 */

#define PCI_CONFIG_ADDR 	0x0CF8
#define PCI_CONFIG_DATA		0x0CFC
#define PCNET32_VENDOR_ID	0x1022

typedef struct _init_block_struct_32
{
	u16_t mode;	// the mode, load into CSR15
	u16_t rtx_len;	// rcv ring length : CSR76
			// xmt ring length : CSR78
	u32_t paddr0;	// physical address of adapter: CSR12,13,14
	u32_t paddr1;
	u32_t lpadrf0;	// logical address filter: CSR8,9,10,11
	u32_t lpadrf1;
	rcv_desc_ring_entry *rdra; // receive descriptor ring address: CSR24,25
	xmt_desc_ring_entry *tdra; // transmit descriptor ring address: CSR30,31
} init_block_struct  __attribute__ ((aligned(16)));

typedef struct _pcnet32_pci_config
{
	u16_t pci_vendor_id;
	u16_t pci_device_id;
	u16_t pci_command;
	u16_t pci_status;
	
	u8_t pci_revision;
	u8_t pci_program;
	u8_t pci_subclass;
	u8_t pci_baseclass;
	
	u8_t pci_cache;
	u8_t pci_latency;
	u8_t pci_header_type;
	u8_t pci_reserved0;
	
	u32_t pci_base_addr;
	u32_t pci_map_addr;
	
	u8_t pci_reserved1[20];
	
	u16_t pci_subsys_vendor;
	u16_t pci_subsys_id;
	u32_t pci_rom_addr;
	
	u8_t pci_capablity_pointer;
	u8_t pci_reserved3[7];
	
	u8_t pci_intr_line;
	u8_t pci_intr_pin;
	u8_t pci_min_gnt;
	u8_t pci_max_lat;
	
	u8_t pci_reserved4[4];
	
	u8_t pci_capability;
	u8_t pci_next_item;
	u16_t pci_power_manager;
	u16_t pci_power_status;
	
	u8_t pci_bridge_support;
	u8_t pci_data;
	u8_t padding[180];
} pcnet32_pci_config_t;

typedef struct pci_dev
{
	int bus_no;
	int dev_no;
	int func_no;
} pci_dev_t;

static pci_dev_t pci;
static init_block_struct pcnet32_init_data;
static pcnet32_pci_config_t pcnet32_pci_cfg;
BOOLEAN pcnet32_debug = FALSE;
static BOOLEAN transmit_one_packet(ether_pcnet_t *netif);
static void dump_cs0_status(ether_pcnet_t *pdev);

static int pcnet32_ioctl(netif_t *netif, int flag);

static char* chipIdent[] = 
{
	"Unknown",
	"LANCE",
	"C-LANCE",
	"PCnet-ISA",
	"PCnet-ISA+",
	"PCnet-ISA II",
	"PCnet-32 VL-Bus",
	"PCnet-PCI",
	"PCnet-PCI II",
	"PCnet-FAST",
	"PCnet-FAST+",
	"PCnet-Home",
	"PCnet-Fast III"
};


static void pci_write_word(pci_dev_t *dev, int addr, unsigned long val)
{
	unsigned long pci_addr;

	pci_addr = 0x80000000 | (dev->bus_no << 16) | (dev->dev_no << 11) |\
		(dev->func_no << 8) | addr;
	
	outl(pci_addr, PCI_CONFIG_ADDR);
	outw(val, PCI_CONFIG_DATA);
}

static unsigned long pci_read_word(pci_dev_t *dev, int addr)
{
	unsigned long val = 0;
	unsigned long pci_addr;
	
	pci_addr = 0x80000000 | (dev->bus_no << 16) | (dev->dev_no << 11) |\
		(dev->func_no << 8) | addr;

	outl(pci_addr, PCI_CONFIG_ADDR);
	val = inw(PCI_CONFIG_DATA);

	return val;
}

static BOOLEAN get_pci_config(pcnet32_pci_config_t *cfg, u16_t vendor_id)
{
	int bus;
	int dev;
	int func;
	int index;
	unsigned long *ptr;
	unsigned int odata;
	unsigned int idata;

	ptr = (unsigned long *)cfg;	
	for (bus = 0; bus < 256; bus++)
	{
		for (dev = 0; dev < 32; dev++)
		{
			for (func = 0; func < 8; func++)
			{
				ptr = (unsigned long *)cfg;
				for (index = 0; index < 64; index++)
				{
					odata = 1 << 31;
					odata |= bus << 16;
					odata |= dev << 11;
					odata |= func << 8;
					odata |= index << 2;

					outl(odata, 0x0cf8);
					idata = inl(0x0cfc);

					*ptr++ = idata;
				}

				if (cfg->pci_vendor_id == PCNET32_VENDOR_ID)
				{
					pci.bus_no = bus;
					pci.dev_no = dev;
					pci.func_no = func;
					
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

static int get_chip_id(u32_t addr)
{
	unsigned short val;
	unsigned short ret;
	
	outw(88, addr + RAP_16);
	val = inw(addr + RDP_16);
	ret = (val >> 12) & 0xF;

	outw(89, addr + RAP_16);
	val = inw(addr + RDP_16);

	ret |= (val << 4);

	switch (ret & AMD_PART_MASK)
	{
		case AM79C960:  
			return PCnet_ISA;
			
		case AM79C961:  
			return PCnet_ISAplus;
			
		case AM79C961A: 
			return PCnet_ISA_II;
			
		case AM79C965: 
			return PCnet_32;
			
		case AM79C970: 
			return PCnet_PCI;
			
		case AM79C970A: 
			return PCnet_PCI_II;
			
		case AM79C971: 
			return PCnet_FAST;
			
		case AM79C972: 
			return PCnet_FASTplus;
			
		case AM79C978:  
			return PCnet_Home;
			
		case AM79C973:
		case AM79C975: 
			return PCnet_FAST_III;
			
		default:
			break;
	}

	return UnknownChip;
}

// 48 bit IEEE 
static void get_mac_address(mac_addr_t *paddr, unsigned long io_base)
{
	int i;
	unsigned char *ptr = &paddr->mac_addr[0];
	unsigned short val;

	for (i = 0; i < 6; i++)
	{
		*ptr = (inb(io_base + i) & 0xFF);
		printk("%x:", *ptr);
		ptr++;
	}

	printk("\n");
	val = inb(io_base + 14);
	if (((val & 0xFF) != 'W') || (((val = inb(io_base + 15)) & 0xFF) != 'W'))
	{
		printk("eeprom something wrong?!\n");
	}
}

// allocate buffer of receive & transmit descriptor ring
static void init_rtx_ring_buffer(rtx_ring_buffer_t *pring)
{
	unsigned long pg;
	rcv_desc_ring_entry *prcv;
	xmt_desc_ring_entry *pxmt;
	int i;
	int page_offset;

	prcv = &pring->rcv_ring[0];
	if (prcv->rcv_buffer)
	{
		return;			// has initialized.
	}

	if (!(pg = get_one_page()))
	{
		panic("No free page!\n");
	}

	memset(pring, 0, sizeof(rtx_ring_buffer_t));

	pring->xmt_head_index = 0;
	pring->xmt_tail_index = 0;
	pring->rcv_index = 0;
	
	page_offset = 0;
	for (i = 0, prcv = &pring->rcv_ring[0]; i < NR_DRES; i++, prcv++)
	{
		if ((page_offset + RTX_BUFF_SIZE) > PAGE_SIZE)
		{
			if (!(pg = get_one_page()))
			{
				panic("no free page!\n");
			}

			page_offset = 0;
		}
		prcv->rcv_buffer = (char *)(pg + page_offset);
		prcv->rcv_buffer_len = (-RTX_BUFF_SIZE) & 0xFFFF;
		SET_OWN(prcv->rcv_flags);

		page_offset += RTX_BUFF_SIZE;

		printk("rcv dre addr = 0x%08x\n", prcv);
	}
	
	for (i = 0, pxmt = &pring->xmt_ring[0]; i < NR_DRES; i++, pxmt++)
	{
		if ((page_offset + RTX_BUFF_SIZE) > PAGE_SIZE)
		{
			if (!(pg = get_one_page()))
			{
				panic("no free page!\n");
			}

			page_offset = 0;
		}
		pxmt->xmt_buffer = (char *)(pg + page_offset);
		pxmt->xmt_len = (-RTX_BUFF_SIZE) & 0xFFFF;

		page_offset += RTX_BUFF_SIZE;

		printk("xmt dre addr = 0x%08x\n", pxmt);
	}
}

// init the init block data
static void init_block_data(init_block_struct *pinit, ether_pcnet_t *pdev)
{
	rtx_ring_buffer_t *pring;
	
	memset(pinit, 0, sizeof(init_block_struct));

	pring = &pdev->rtx_ring_buffer;
	init_rtx_ring_buffer(pring);
	pinit->rtx_len = (LOG_NR_DRES << 12) | (LOG_NR_DRES << 4);
	pinit->mode = 0;
	pinit->rdra = &pring->rcv_ring[0];
	pinit->tdra = &pring->xmt_ring[0];

	if (pcnet32_debug)
	{
		printk("pinit = 0x%08x, rdra = 0x%08x, tdra = 0x%08x\n",
			pinit, 
			pinit->rdra,
			pinit->tdra);
	}

	get_ether_mac_addr((ether_netif_t *)pdev,
			(mac_addr_t *)&pinit->paddr0);
}

static void enable_intr(ether_pcnet_t *pdev)
{
	outw(0, pdev->io_base_addr + RAP_16);
	outw((1 << IENA), pdev->io_base_addr + RDP_16);
}

static BOOLEAN initialized(ether_pcnet_t *pdev)
{
	unsigned long val;

	outw(0, pdev->io_base_addr + RAP_16);
	val = inw(pdev->io_base_addr + RDP_16);
	val &= 0xFFFF;
	
	dump_cs0_status(pdev);

	return (val >> IDON) & 1;
}

static unsigned long read_csr(ether_pcnet_t *pdev, int num)
{
	unsigned long val = 0;

	outw(num, pdev->io_base_addr + RAP_16);
	val = inw(pdev->io_base_addr + RDP_16);
	val &= 0xFFFF;
	
	return (val & 0xFFFF);
}

#if 0
static void write_csr(ether_pcnet_t *pdev, unsigned long val, int num)
{
	outw(num, pdev->io_base_addr + RAP_16);
	outw(val & 0xFFFF, pdev->io_base_addr + RDP_16);
}
#endif

static void write_bcr(ether_pcnet_t *pdev, unsigned long val, int num)
{
	outw(num, pdev->io_base_addr + RAP_16);
	outw(val & 0xFFFF, pdev->io_base_addr + BDP_16);
}

static void emit_send_command(ether_pcnet_t *pdev)
{
	outw(0, pdev->io_base_addr + RAP_16);
	outw(((1 << IENA) | (1 << TDMD)), pdev->io_base_addr + RDP_16);
}

static void check_transmit(ether_pcnet_t *pdev)
{
	short tmp;
	int head_index;
	xmt_desc_ring_entry *pentry;
	rtx_ring_buffer_t *pring;

	if (!pdev)
	{
		return;
	}

	pring = &pdev->rtx_ring_buffer;
	head_index = pring->xmt_head_index;
	pentry = &pring->xmt_ring[head_index];
	if (IS_OWN(pentry->xmt_flags))
	{
		printk("error? transmit flags = 0x%x\n",
			pentry->xmt_flags & 0xFFFF);
		return;		// NOT send
	}

	if (IS_ERR(pentry->xmt_flags))
	{
		pdev->tx_error_count++;
		printk("transmit error count = %d\n", pdev->tx_error_count);
	}
	else
	{
		pdev->tx_frame_count++;

		tmp = pentry->xmt_len;
		tmp = -tmp;
		pdev->tx_byte_count += tmp;
#if 0
		printk("total transmit bytes = %d, frame count = %d\n",
			pdev->tx_byte_count,
			pdev->tx_frame_count);
#endif
	}

	memset(pentry->xmt_buffer, 0, RTX_BUFF_SIZE);

	pring->xmt_head_index++;
	if (pring->xmt_head_index == NR_DRES)
	{
		pring->xmt_head_index = 0;
	}

	if (!list_empty(&pdev->out_queue))
	{
		if(!transmit_one_packet(pdev))
			DIE("BUG: cannot happen\n");
	}
}

static void check_receive(ether_pcnet_t *pdev)
{
	int head_index;
	rcv_desc_ring_entry *pentry;
	rtx_ring_buffer_t *pring;
	
	pring = &pdev->rtx_ring_buffer;

	head_index = pring->rcv_index;
	pentry = &pring->rcv_ring[head_index];

	while(!IS_OWN(pentry->rcv_flags))
	{
		if (pcnet32_debug)
		{
			printk("rcv addr=0x%08x, flags=%x, recv_len=0x%x\n", 
				pentry->rcv_buffer,
				pentry->rcv_flags,
				pentry->rcv_msg_len);
		}

		if (IS_ERR(pentry->rcv_flags))
		{
			pdev->rx_error_count++;
			printk("rcv error count = %d\n", pdev->rx_error_count);
		}
		else if (
			(pentry->rcv_msg_len < 46 + ETHER_LNK_HEAD_LEN + 4)
			||
			(pentry->rcv_msg_len > pdev->netif_mtu + ETHER_LNK_HEAD_LEN + 4)
			)
		{
			pdev->rx_error_count++;
			printk("rcv_msg_len(%d) is wierd\n",
				pentry->rcv_msg_len);
		}
		else
		{
			skbuff_t * skb;
			unsigned char *ptr;

			pentry->rcv_msg_len -= 4; // 4 bytes for CRC

			pdev->rx_byte_count += pentry->rcv_msg_len;
			pdev->rx_frame_count++;

			skb = skb_alloc(pentry->rcv_msg_len, 0);
			if(skb)
			{
				ptr = sk_get_buff_ptr(skb);
				skb->data_ptr = 0;
				skb->dgram_len = skb->dlen - skb->data_ptr;
				skb->flag |= SK_PK_HEAD_DATA;

				memcpy(&ptr[skb->data_ptr],
					pentry->rcv_buffer,
					pentry->rcv_msg_len);

				skb->netif = (netif_t *)pdev;

				list_add_tail(&pdev->in_queue, &skb->pkt_list);

				raise_bottom(PCNET32_B);
			}
		}
	
		memset(pentry->rcv_buffer, 0, RTX_BUFF_SIZE);
		head_index++;

		if (head_index == NR_DRES)
		{
			head_index = 0;
		}

		SET_OWN(pentry->rcv_flags);
		pentry->rcv_buffer_len = (-RTX_BUFF_SIZE) & 0xFFFF;

		pentry = &pring->rcv_ring[head_index];
	}

	pring->rcv_index = head_index;
}

static void pcnet32_bottom(void)
{
	process_all_input();
}

static void pcnet32_intr(struct regs_t * regs)
{
	unsigned long val;
	ether_pcnet_t *pdev;

	if (!(pdev = (ether_pcnet_t *)get_netif_by_irq(regs->index)))
	{
		printk("regs->index = %d\n", regs->index);
		return;
	}

	if (pcnet32_debug)
	{
		printk("in isr\n");
		dump_cs0_status(pdev);
	}
	
	outw(0, pdev->io_base_addr + RAP_16);
	val = inw(pdev->io_base_addr + RDP_16);
	val &= 0xFFFF;
	val &= ~(1 << IENA);		// Disable interrupt
	outw(val , pdev->io_base_addr + RDP_16);

	sti();

	if ((val >> RINT) & 1)
	{
		check_receive(pdev);		
	}
	
	if ((val >> TINT) & 1)
	{
		check_transmit(pdev);
	}

	cli();
	enable_intr(pdev);
	sti();
}

static BOOLEAN setup_irq_handler(int irq)
{
	set_bottom(PCNET32_B, pcnet32_bottom);
	put_irq_handler(irq, pcnet32_intr);

	return TRUE;
}

static void dump_cs0_status(ether_pcnet_t *pdev)
{
	unsigned short val = read_csr(pdev, 0);

	DUMP_FLAG(val, INIT);
	DUMP_FLAG(val, STRT);
	DUMP_FLAG(val, STOP);
	DUMP_FLAG(val, TDMD);
	DUMP_FLAG(val, IENA);
	DUMP_FLAG(val, IDON);
	DUMP_FLAG(val, TINT);
	DUMP_FLAG(val, RINT);
	DUMP_FLAG(val, MERR);
	DUMP_FLAG(val, MISS);
	DUMP_FLAG(val, CERR);
	DUMP_FLAG(val, BABL);
	DUMP_FLAG(val, ERR);
	
	printk("\n");
}

void pcnet32_init(void)
{
	int i;
	int irq_no;
	unsigned long tmp;
	unsigned long val;
	unsigned long addr;
	unsigned long status;
	unsigned long io_base_addr;
	mac_addr_t mac;
	ether_pcnet_t *pdev;
	
	memset(&pcnet32_pci_cfg, 0, sizeof(pcnet32_pci_config_t));
	memset(&pcnet32_init_data, 0, sizeof(init_block_struct));
	
	if (!get_pci_config(&pcnet32_pci_cfg, PCNET32_VENDOR_ID))
	{
		printk("can not get pcnet32 network adapter!\n");
		return;
	}

	val = pci_read_word(&pci, 0x4);
	//printk("cmd val = %x\n", val);
	val |= 0x4 | 0x01;		// enable bus master
	pci_write_word(&pci, 0x4, val);

	status = pci_read_word(&pci, 0x4);
	//printk("status = %x\n", status);
	if (status != val)
	{
		printk("can not enable io access or bus mastering\n");
	}

	io_base_addr = pcnet32_pci_cfg.pci_base_addr & ~((1 << 5) - 1);
	irq_no = pcnet32_pci_cfg.pci_intr_line;

	printk("io base addr = 0x%08x, interrupt no = %d\n",
		io_base_addr, irq_no);

	setup_irq_handler(irq_no);

	// reset pcnet32
	inw(io_base_addr + RESET_16);
	
	outw(0, io_base_addr + RAP_16);
	tmp = inw(io_base_addr + RDP_16);
	if ((tmp & 0xF) != (1 << STOP))
	{
		printk("unexpected value!\n");
		return;
	}
	
	// get the chip id
	if ((val = get_chip_id(io_base_addr)) == UnknownChip)
	{
		printk("unkown pcnet32 chip!\n");
		return;
	}
	
	printk("chip id: %s\n", chipIdent[val]);
	get_mac_address(&mac, io_base_addr);
	
	if (!(pdev = (ether_pcnet_t *)kmalloc(sizeof(ether_pcnet_t), 0)))
	{
		PANIC("no memory for ether pcnet32\n");
	}
	ether_init((ether_netif_t *)pdev, io_base_addr, irq_no, &mac);
	
	init_block_data(&pcnet32_init_data, pdev);

	write_bcr(pdev, SWSTYLE, 20);

	// load init data into CSR1 & CSR2
	addr = (unsigned long)&pcnet32_init_data;
	printk("addr = 0x%08x\n", addr);

	outw(1, pdev->io_base_addr + RAP_16);
	outw((addr & 0xFFFF), pdev->io_base_addr + RDP_16);
	outw(2, pdev->io_base_addr + RAP_16);
	outw(((addr >> 16) & 0xFFFF), pdev->io_base_addr + RDP_16);

	// start initializing
	outw(0, pdev->io_base_addr + RAP_16);
	outw(1, pdev->io_base_addr + RDP_16);

	i = 0;
	while (!initialized(pdev))
	{
		if (i++ > 100)
		{
			break;
		}
	}

	mdelay(100);	// wait for pcnet32 interrupt

	outw(0, pdev->io_base_addr + RAP_16);
	outw(((1 << IENA) | (1 << STRT)), pdev->io_base_addr + RDP_16);

	pdev->flag |= NETIF_RUNNING | NETIF_UP;
	pdev->transmit = transmit_one_packet;
	pdev->ioctl = pcnet32_ioctl;
}

static BOOLEAN transmit_one_packet(ether_pcnet_t *netif)
{
	int i;
	int index;
	int written;
	skbuff_t *buf;
	list_t *pos;
	list_t *tmp;
	unsigned char * buffer;
	xmt_desc_ring_entry *pentry = NULL;
	rtx_ring_buffer_t *pring;
	skbuff_t *skb;
	
	pring = &netif->rtx_ring_buffer;
	i = pring->xmt_tail_index;

	pentry = &pring->xmt_ring[i];

	if (IS_OWN(pentry->xmt_flags))
	{
		return FALSE;
	}

	// get one skbuff from out queue
	tmp = netif->out_queue.next;
	list_del1(tmp);

	skb = list_entry(tmp, pkt_list, skbuff_t);
	
	if (skb->dgram_len > RTX_BUFF_SIZE)
	{
		PANIC("BUG: can not happend!\n");
	}
	
	index = 0;
	foreach (tmp, pos, &skb->list)
	{
		buf = list_entry(tmp, list, skbuff_t);
		if (!(buffer = sk_get_buff_ptr(buf)))
		{
			break;
		}

		written = buf->dlen - buf->data_ptr;
		memcpy(&pentry->xmt_buffer[index],
			&buffer[buf->data_ptr], 
			written);

		index += written;
	}
	endeach(&skb->list);

	pentry->xmt_len = (-index) & 0xFFFF;
	pentry->xmt_reserved = 0;
	SET_STP(pentry->xmt_flags);
	SET_ENP(pentry->xmt_flags);
	SET_OWN(pentry->xmt_flags);

	i++;
	if (i == NR_DRES)
	{
		i = 0;
	}

	pring->xmt_tail_index = i;

	if (pentry && pcnet32_debug)
	{
		printk("transmit addr = 0x%08x, len = %x, flag = %x\n",
			pentry->xmt_buffer,
			pentry->xmt_len & 0xFFFF, 
			pentry->xmt_flags & 0xFFFF);
	}

	emit_send_command(netif);

	skb_free_packet(skb);

	return TRUE;
}

void switch_pcnet32_debug(void)
{
	pcnet32_debug = !pcnet32_debug;
}

static int pcnet32_ioctl(netif_t *netif, int flag)
{
#if 0
	ether_pcnet_t *pdev = (ether_pcnet_t *)netif;
#endif
	return 1;
}
