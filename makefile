CC = gcc
FLAGS = 
#-Wall -ansi

SOCKET_LIB_OBJS = sockets_lib.o
SOCKET_LIB_C = sockets_lib/sockets_lib.c
SOCKET_LIB_H = sockets_lib/sockets_lib.h
SOCKET_LIB_SO = libsockets_lib.so

UMV_OBJS = UMV.o
UMV_C = UMV/UMV.c
UMV_H = UMV/UMV.h

CPU_OBJS = cpu.o
CPU_C = CPU/cpu.c
CPU_H = CPU/cpu.h

KERNEL_OBJS = kernel.o
KERNEL_C = kernel/kernel.c
KERNEL_H = kernel/kernel.h

PROGRAMA_OBJS = programa.o
PROGRAMA_C = programa/programa.c
PROGRAMA_H = programa/programa.h

# ALL
all: coverflow

# ALLC (make all y make clean)
allc: all clean variables_entorno

# coverflow
coverflow:\
	umv\
	cpu\
	kernel\
	programa

# SOCKET_LIB
sockets_lib.o:  $(SOCKET_LIB_C) $(SOCKET_LIB_H)
	$(CC) -c -fPIC $(SOCKET_LIB_C)
libsockets_lib.so: $(SOCKET_LIB_OBJS)
	$(CC) -shared -o libsockets_lib.so $(SOCKET_LIB_OBJS)

# UMV
UMV.o: $(UMV_C) $(UMV_H) 
	$(CC) -I"./sockets_lib" $(FLAGS) -c $(UMV_C)
umv: $(SOCKET_LIB_SO) $(UMV_OBJS)
	$(CC) -L"./" -o umv.ansisop $(UMV_OBJS) -lsockets_lib -lcommons -lpthread

# CPU
cpu.o: $(CPU_C) $(CPU_H)
	$(CC) -I"./sockets_lib" $(FLAGS) -c $(CPU_C)
cpu: $(SOCKET_LIB_SO) $(CPU_OBJS)
	$(CC) -L"./" -o cpu.ansisop $(CPU_OBJS) -lsockets_lib -lcommons -lparser-ansisop

# KERNEL
kernel.o: $(KERNEL_C) $(KERNEL_H)
	$(CC) -I"./sockets_lib" $(FLAGS) -c $(KERNEL_C)
kernel: $(SOCKET_LIB_SO) $(KERNEL_OBJS)
	$(CC) -L"./" -o kernel.ansisop $(KERNEL_OBJS) -lsockets_lib -lcommons -lpthread -lparser-ansisop

# PROGRAMA
programa.o: $(PROGRAMA_C) $(PROGRAMA_H)
	$(CC) -I"./sockets_lib" $(FLAGS) -c $(PROGRAMA_C)
programa: $(SOCKET_LIB_SO) $(PROGRAMA_OBJS)
	$(CC) -L"./" -o programa.ansisop $(PROGRAMA_OBJS) -lsockets_lib -lcommons

# DECLARO VARIABLES DE ENTORNO
variables_entorno: 
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:~/tp-2014-1c-hashtaggers/
	export ANSISOP_CONFIG=./programa/programa_config

# CLEAN
clean:
	rm $(SOCKET_LIB_OBJS) $(UMV_OBJS) $(CPU_OBJS) $(KERNEL_OBJS) $(PROGRAMA_OBJS)


# CLEAR COVERFLOW