/*
 * funciones.h
 *
 *  Created on: 27/05/2014
 *      Author: utnso
 */

#ifndef FUNCIONES_H_
#define FUNCIONES_H_

#include <stdio.h>
#include <string.h>		//para el memset
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>		//para el close
#include <arpa/inet.h>	//para el inet_addr


//Se le manda la ip y el puerto al que se quiere conectar y devuelve el socket
int conectarse(char* ip,int puerto);

//Abre un srv en la ip y puerto que se le manda y devuelve el socket
int servidor(char *ip, int puerto);

#endif /* FUNCIONES_H_ */

