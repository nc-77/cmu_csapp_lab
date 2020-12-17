#指定编译器
CC = gcc

#指定编译参数
CFLAGS = -O2 -g

#指定生成可执行文件名称
EXEC = hostinfo
 
#指定由哪些文件来生成可执行文件
COBJS = hostinfo.c
 
#make执行动作
all:
	$(CC) -o $(EXEC) $(COBJS) $(CFLAGS)
 
#make clean动作
clean:
	rm -rf ${EXEC}