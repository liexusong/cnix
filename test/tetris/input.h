#ifndef INPUT_H
#define INPUT_H

/*======================================================================
        Key code definitions
  ===*/
#define PF1           0x0100
#define PF2           0x0101
#define PF3           0x0102
#define PF4           0x0103
#define PF5           0x0104
#define PF6           0x0105
#define PF7           0x0106
#define PF8           0x0107
#define PF9           0x0108
#define PF10          0x0109
#define PF11          0x010A
#define PF12          0x010B

#define OPF1          0x0110
#define OPF2          0x0111
#define OPF3          0x0112
#define OPF4          0x0113
#define OPF5          0x0114
#define OPF6          0x0115
#define OPF7          0x0116
#define OPF8          0x0117
#define OPF9          0x0118
#define OPF10         0x0119
#define OPF11         0x011A
#define OPF12         0x011B

#define OOPF1          0x0120
#define OOPF2          0x0121
#define OOPF3          0x0122
#define OOPF4          0x0123
#define OOPF5          0x0124
#define OOPF6          0x0125
#define OOPF7          0x0126
#define OOPF8          0x0127
#define OOPF9          0x0128
#define OOPF10         0x0129
#define OOPF11         0x012A
#define OOPF12         0x012B

#define OOOPF1         0x0130
#define OOOPF2         0x0131
#define OOOPF3         0x0132
#define OOOPF4         0x0133
#define OOOPF5         0x0134
#define OOOPF6         0x0135
#define OOOPF7         0x0136
#define OOOPF8         0x0137
#define OOOPF9         0x0138
#define OOOPF10        0x0139
#define OOOPF11        0x013A
#define OOOPF12        0x013B

#define PF2OPF(x)	(x + 0x10)
#define PF2OOPF(x)	(x + 0x20)
#define PF2OOOPF(x)	(x + 0x30)


/*-- some control codes for arrow keys.. see read_char */
#define KEY_UP		0x0140
#define KEY_DOWN	0x0141
#define KEY_RIGHT	0x0142
#define KEY_LEFT	0x0143
#define KEY_JUNK	0x0144
#define KEY_RESIZE	0x0145  /* Fake key to cause resize */
#define KEY_HOME	0x0146  /* Extras that aren't used outside DOS */
#define KEY_END		0x0147
#define KEY_PGUP	0x0148
#define KEY_PGDN	0x0149
#define KEY_DEL		0x014A

#define NO_OP_COMMAND		'\0'    /* no-op for short timeouts   */

#define NO_OP_IDLE    		0x200   /* no-op for timeouts longer than 25 seconds  */

#define ctrl(x)		((x) - 0100)

///
int tty_cbreak(int fd);

///
int tty_raw(int fd);

///
int tty_reset(int fd);

///
void sig_catcher(int);

///
int read_char(int time_out);

#endif
