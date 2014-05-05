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
#include <netdb.h>
#include <sys/un.h>

#define PUERTO 3550  //El puerto que será abierto
#define BACKLOG 2 //Es la cantidad de conexiones permitidas
#define IP INADDR_ANY  //INADDR_ANY guarda mi IP

int main (){


	int descriptorSocket; // aca se guarda el descriptor que va a devolver la funcion socket
	struct sockaddr_in servidor;	//estructura p guardar info del socket creado
	int tamanio;	// tamaño de la estructura de la conexion entrate
	int descriptorSocketCliente; //aca se guarda la info de la conexion entrante
	char buffer[tamanio];
	int n; //cantidad de bytes leidos en READ

	// SOCKET crea el socket y me devuelve su direcion o '-1' si da error
	descriptorSocket =socket(AF_INET, SOCK_STREAM, 0);	//crea el socket y me devuelve su direcion o '-1' si da error
	if (descriptorSocket < 0) {
	printf("Error en la creación del socket \n");
	exit(1);
	}

	servidor.sin_family = AF_UNIX;
	servidor.sin_addr= IP;
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
	tamanio=sizeof(struct sockaddr_in);
	if ((descriptorSocketCliente=accept(descriptorSocket,(struct sockaddr *)&servidor,&tamanio))<0)
	{
		printf("error en accept()\n");
		exit(0);
	}

	//READ recibe datos a travez del descriptor de socket
	if ((n=read(descriptorSocket, buffer, tamanio))<0)
	{
		printf("error en read()\n");
		exit(0);
	}

	int a=buffer[0];
	int b=0;

	//CLOSE cierra el socket
	if (close(descriptorSocket)!=0)
	{
		printf("error en close()\n");
		exit(0);
	}

	if (a>0){
		b=2*a;
	}
	printf("%d\n",b);
	return a;
}


