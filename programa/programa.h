#ifndef PROGRAMA_H_
#define PROGRAMA_H_

	#include "sockets_lib.h"
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <unistd.h>

	#include <commons/string.h>
	#include <commons/config.h>
	#include <commons/collections/dictionary.h>

	int tamanioArchivo(FILE *archivo);
	void handshake_kernel(int soc_kernel);

#endif /* PROGRAMA_H_ */
