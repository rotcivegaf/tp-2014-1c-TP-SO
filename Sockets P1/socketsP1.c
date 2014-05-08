/*
 * socketsP1.c
 *
 *  Created on: 04/05/2014
 *      Author: utnso
 */

//SERVIDOR

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>


#define PUERTO 6667  //El puerto que será abierto
#define BACKLOG 1 //Es la cantidad de conexiones permitidas
#define IP "127.0.0.1"
#define BUFFERSIZE 1024 //tamaño de lo que se recibe

int srvBasico (){


	int descriptorSocket; // aca se guarda el descriptor que va a devolver la funcion socket
	struct sockaddr_in servidor,cliente;	//estructura p guardar info de las conexiones
	int descriptorSocketCliente;
	char buffer[BUFFERSIZE];
	int readSize; //tamanio de lo que llega

	// SOCKET crea el socket y me devuelve su direcion o '-1' si da error
	descriptorSocket =socket(AF_INET, SOCK_STREAM, 0);	//crea el socket y me devuelve su direcion o '-1' si da error
	if (descriptorSocket < 0) {
	printf("Error en la creación del socket \n");
	exit(1);
	}

	servidor.sin_family = AF_INET;
	servidor.sin_addr.s_addr= inet_addr(IP);
	memset(&servidor.sin_zero, 0, sizeof servidor.sin_zero[8]); //memset inicializa el vector con ceros
	servidor.sin_port= htons(PUERTO); //asigno un puerto

	// BIND asocia una direccion al socket y devuelve 0 si sale bien o '-1' por error
	if(bind(descriptorSocket,(struct sockaddr*)&servidor,sizeof(struct sockaddr))==-1) {
	      printf("error en bind() \n");
	      exit(-1);
	   }

	//LISTEN
	if(listen(descriptorSocket,BACKLOG) == -1) {
	      printf("error en listen()\n");
	      exit(-1);
	   }

	// ACCEPT las coneciones de clientes al sistema operativo
	socklen_t sin_size = sizeof(struct sockaddr_in);
	descriptorSocketCliente=accept(descriptorSocket,(struct sockaddr *)&cliente,&sin_size);


	//RECV recibe datos a travez del descriptor de socket

	while((readSize = recv(descriptorSocket, (void*) buffer, BUFFERSIZE, 0))>0){
		buffer[readSize]='\0';
		printf("%s", buffer);
	}

	if (readSize==0)
		printf("Cliente desconectado");

	//CLOSE cierra el socket
	close(descriptorSocket);
	close(descriptorSocketCliente);

	return 0;
}

