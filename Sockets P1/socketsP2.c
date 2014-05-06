/*
 * socketsP2.c
 *
 *  Created on: 04/05/2014
 *      Author: utnso
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/un.h>

#define IP INADDR_ANY  //INADDR_ANY guarda mi IP (ESTO DEBERIA SER UNA VARIABLE GLOBAL DE TODO EL SIST PORQUE ES EL IP DEL PROCESO AL QUE ME VOY A CONECTAR)
#define PUERTO 3550  //El puerto al que me voy a conectar (idem)

/*int main(){

	int descriptorSocket;
	struct sockaddr_in cliente;

	descriptorSocket =socket(AF_INET, SOCK_STREAM, 0);	//crea el socket y me devuelve su direcion o '-1' si da error
	if (descriptorSocket < 0) {
		printf("Error en la creación del socket \n");
		exit(1);
	}

	if (connect(descriptorSocket, (struct sockaddr *)&descriptorSocket,sizeof(struct sockaddr_in))<0)
	{
		printf("Error en connect \n");
		exit(0);
	}



}*/


