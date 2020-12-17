#指定编译器
CC = gcc

#指定编译参数
CFLAGS = -O2 -g -lpthread

#指定生成可执行文件名称
EXEC = client 
 
#指定由哪些文件来生成可执行文件
COBJS = client.c 
 
#make执行动作
all:
	$(CC) -o tiny main.c doit.c  csapp.c $(CFLAGS)
	
#make clean动作
clean:
	
	rm -rf doit
	rm -rf main