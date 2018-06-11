#ifndef PARAM_H
#define PARAM_H
struct hsc{
	unsigned char head;
	unsigned short sc __attribute__ ((packed));
};

struct partition{
	unsigned char bootind;
	struct hsc 	start_hsc;
	unsigned char sysind;
	struct hsc	end_hsc;
	unsigned int  start_sec;
	unsigned int  size_sec;
};

#define SPT 63		/* You should modify these for your own system */
#define CLY 130
#define HEAD 16
#define MAKECYL(hsc) 	(((hsc.sc) & 0xff00) >> 8| ((hsc.sc) & 0xB0) << 8)
#define MAKEHEAD(hsc)	(hsc.head)
#define MAKESECT(hsc)	((hsc.sc) & 0x3F)
int check_part_entry(const struct partition *pe)
{
	int cyl, head, sect;
	unsigned int start_sect;

	cyl = MAKECYL(pe->start_hsc);
	head = MAKEHEAD(pe->start_hsc);
	sect = MAKESECT(pe->start_hsc);

	start_sect = cyl * HEAD * SPT  + head * SPT + sect - 1;

	if (start_sect != pe->start_sec){
		printf("parameter error!\n");
		return 0;
	}
	if (pe->size_sec <= 0){
		printf("No size!\n");
		return 0;
	}
	if (pe->size_sec < 32){
		printf("The size of this partition is too small (<16K)!\n");
		return 0;
	}
	return 1;		/* success */
}
#endif
