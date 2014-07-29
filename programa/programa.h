#ifndef PROGRAMA_H_
#define PROGRAMA_H_

	#include "sockets_lib.h"
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <unistd.h>

	#include "commons/config.h"
	#include "commons/collections/dictionary.h"
	#include "commons/log.h"

	int tamanioArchivo(FILE *archivo);
	void handshake_kernel();

#endif /* PROGRAMA_H_ */
