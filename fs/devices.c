#include <cnix/types.h>
#include <cnix/devices.h>

#define OPEN_FUNC(name) extern int name(dev_t, int)
#define CLOSE_FUNC(name) extern int name(dev_t, int)
#define READ_FUNC(name) \
	extern ssize_t name(dev_t, char *, size_t, off_t, int *, int)
#define WRITE_FUNC(name) \
	extern ssize_t name(dev_t, const char *, size_t, off_t, int *, int)
#define IOCTL_FUNC(name) \
	extern int name(dev_t, int, void *, int)

OPEN_FUNC(tty_open);
CLOSE_FUNC(tty_close);
READ_FUNC(tty_read);
WRITE_FUNC(tty_write);
IOCTL_FUNC(tty_ioctl);

OPEN_FUNC(mem_open);
CLOSE_FUNC(mem_close);
READ_FUNC(mem_read);
WRITE_FUNC(mem_write);
IOCTL_FUNC(mem_ioctl);

OPEN_FUNC(hd_open);
CLOSE_FUNC(hd_close);
READ_FUNC(hd_read);
WRITE_FUNC(hd_write);
IOCTL_FUNC(hd_ioctl);

struct dev_struct dev_table[] = {
	{ NULL, NULL, NULL, NULL, NULL }, /* not used */
	{ mem_open, mem_read, mem_write, mem_close, mem_ioctl},
	{ NULL, NULL, NULL, NULL, NULL }, /* /dev/fdX */
	{ hd_open, hd_read, hd_write, hd_close, hd_ioctl},
	{ NULL, NULL, NULL, NULL, NULL }, /* not used */
	{ tty_open, tty_read, tty_write, tty_close, tty_ioctl },
};
int get_dev_nr(void)
{
	return (sizeof(dev_table) / sizeof(struct dev_struct));
}
