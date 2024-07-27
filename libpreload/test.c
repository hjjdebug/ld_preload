#include <stdio.h>
#include <stdlib.h>

int main()
{
	char *p1,*p2,*p3,*p4,*p5;
//方便函数调用栈查询,
	//但printf会有一次内存分配,大小1024
	printf("main addr:%p\n",&main);
	//自己分配的内存都可以被监视到,
	p1 = malloc(100);
	printf("p1 is %p\n",p1);
	p2 = calloc(1,200);
	printf("p2 is %p\n",p2);
	p3=realloc(p2,300);
	printf("p3 is %p\n",p4);
	posix_memalign((void **)&p4,16,400);
	printf("p4 is %p\n",p4);
//故意丢失
	free(p1); 
//	free(p2); //p2 realloc 时已经释放
//	free(p3);
//	free(p4);
	return 0;
}
