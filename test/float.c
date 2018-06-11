#include <stdio.h>
#include <unistd.h>

int main(void)
{
	int x;
	float pi;
	pid_t pid;


	printf("Please input a integer number: ");
	scanf("%d", &x);
	printf("%d\n", x);
	printf("Please input a float number: ");
	scanf("%f", &pi);
	printf("%f\n", pi);

	if ((pid = fork()) == 0)
	{
		float f;

		f = 2.3;
		f *= f;

		printf("f * f = %f\n", f);
	}
	else
	{
		printf("\npi * pi %f\n", pi * pi);
	}

	return 0;
}
