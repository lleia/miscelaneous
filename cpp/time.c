#include <stdio.h>
#include <sys/time.h>

char            fmt[64], buf[64];
struct timeval  tv;
struct tm       *tm;

int main()
{
	gettimeofday(&tv, NULL);
	if((tm = localtime(&tv.tv_sec)) != NULL) {
		strftime(fmt, sizeof fmt, "%Y-%m-%d %H:%M:%S.%%06u %z", tm);
		snprintf(buf, sizeof buf, fmt, tv.tv_usec);
		printf("'%s'\n", buf); 
	}
	return 0;
}
