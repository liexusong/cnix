#include <asm/io.h>
#include <cnix/types.h>
#include <cnix/kernel.h>
#include <cnix/driver.h>

unsigned char cfgspace[256];

int pci_ide_detect(void)
{
	unsigned long odata, idata;
	int bus, dev, func, index;
	unsigned long * lcs = (unsigned long *)cfgspace;
	unsigned short * scs = (unsigned short *)&cfgspace[0x0a];

	for(bus = 0; bus < 256; bus++){
		for(dev = 0; dev < 32; dev++){
			for(func = 0; func < 8; func++){
				for(index = 0; index < 64; index++){
					odata = 1 << 31;
					odata |= bus << 16;
					odata |= dev << 11;
					odata |= func << 8;
					odata |= index << 2;

					outl(odata, 0x0cf8);
					idata = inl(0x0cfc);

					lcs[index] = idata;
				}

				/*
				 * base-class: 0x01, mass storage controller
				 * sub-class:
				 * 	0x00 scsi
				 * 	0x01 ide
				 * 	0x02 floppy
				 */
				if(*scs == 0x0101) 
					goto found;

				/* mutli func */
				if(!func && !(cfgspace[0x0e] & 0x80))
					break;
			}
		}
	}

	return 0;

found:

#if 0
	{
		int i, j;

		printk("\n");
		for(i = 0; i < 16; i++){
			for(j = 0; j < 16; j++)
				printk("%x ", cfgspace[i * 16 + j]);
			printk("\n");
		}
	}
#endif

	return 1;
}
