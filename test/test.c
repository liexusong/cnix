#include <stdio.h>

int main(void)
{
	FILE *fp;

	fp = fopen("a.txt", "w");
	if (!fp)
	{
		printf("open a.txt for writing failed.\n");
		return 1;
	}
	fprintf(fp, "helloworld");

	fclose(fp);

	return 0;
}
