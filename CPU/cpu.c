/*
 * cpu.c
 *
 *  Created on: 01/06/2014
 *      Author: utnso
 */

#include "cpu.h"

static const int CONTENIDO_VARIABLE = 20;
static const int POSICION_MEMORIA = 0x10;

AnSISOP_funciones functions = {
		.AnSISOP_definirVariable		= definirVariable,
		.AnSISOP_obtenerPosicionVariable= obtenerPosicionVariable,
		.AnSISOP_dereferenciar			= dereferenciar,
		.AnSISOP_asignar				= asignar,
		.AnSISOP_obtenerValorCompartida = obtenerValorCompartida,
		.AnSISOP_asignarValorCompartida = asignarValorCompartida,
		.AnSISOP_irAlLabel              = irAlLabel,
		.AnSISOP_llamarSinRetorno       =llamarSinRetorno,
		.AnSISOP_llamarConRetorno       = llamarConRetorno,
		.AnSISOP_finalizar              = finalizar,
		.AnSISOP_retornar               = retornar,
		.AnSISOP_imprimir				= imprimir,
		.AnSISOP_imprimirTexto			= imprimirTexto,
		.AnSISOP_entradaSalida = entradaSalida};

AnSISOP_kernel kernel_functions = {
		.AnSISOP_wait = wait,
		.AnSISOP_signal =signal};


int main(){

	/*levanta archivo de configuracion para obtener ip y puerto de kernel y umv*/
	t_config *unaConfig = config_create("config.conf");//debe obtener este archivo por la direcc en la variable de enterno
	char *ipKernel=config_get_string_value(unaConfig, "IPKERNEL");
	int puertoKernel = config_get_int_value(unaConfig, "PUERTOKERNEL");
	char *ipUmv=config_get_string_value(unaConfig, "IPUMV");
	int puertoUmv = config_get_int_value(unaConfig, "PUERTOUMV");

	//u_int32_t tipoCPU = 2; //para tipo de conexion cpu
	//int cantIns = 0 // cantidad de instruccion ansisop que ejecuta

	/*conexion con el kernel*/
	struct sockaddr_in infoConexion;
	int socketCpu = servidor(ipKernel, puertoKernel);
	listen(socketCpu, 1);
	socklen_t sin_size = sizeof(struct sockaddr_in);
	int socketKernel = accept(socketCpu, (struct sockaddr *)&infoConexion, &sin_size);
	//queda a la espera que el pcp  envie el pcb

	/*conexion con la umv*/
	int socketUmv = conectarse(ipUmv,puertoUmv);
	//falta handshake para indicar tipo de conexion:CPU

	//recibir pcb
	//recv()
	//crear diccionario de variables

	//mientras (cantIns <= quantum )
		//para saber la instruccion que tengo que ejecutar
		//solicito a la umv base= base seg de codigo offset=program counter tamaño= ?
		//proxSentencia solicitarProxSentencia(indice_codigo);
	 	//con indice de codigo del pcb solicita prox sentencia a ejecutar
		//recibo de umv una linea de codigo a ejecutar
		//si devuelve error
		//if(no es ultimaSentencia)
			//analizadorLinea(strdup(instruccion), &functions, &kernel_functions);
			//incremento el program counter/ pido al kernel/umv que aumente el pc
			//aumento cantIns
			//else
				//analizadorLinea
				//aviso a umv para que destruya segmentos
				//aviso a pcp que termino la ejecucion

	//aviso al pcp que termino el quantum
	//destruyo diccionario de datos




	/* si recibe señal sigusr1 debe desconectarse del pcp
	 *atendida por un thread del proceso
	 *signal(SIGUSR1, rutina);
	 * void rutina{}
	 * continua donde el programa fue interrumpido
	 *
	 *
	 * si umv devuelve un mensaje de excepcion, se muestra por pantalla de programa y concluye ejecucion avisando al pcp
	 */
close(socketKernel);
close(socketCpu);
close(socketUmv);

return 0;

}




t_puntero definirVariable(t_nombre_variable identificador_variable){
	//obtener del pcb el pointer al contexto actual
	//obtener posicion a la que escribir por pointer de ejecucion actual + desplazamiento tamaño de contexto
	//chequea que no se pase del tamño del stack
	//escribe identificador_variable en stack
	//agregar a diccionario de variables: identificador y posicion de memoria dictionary_put()
	// aumentar tamaño de contexto actual
	//devolver puntero a la nueva variable
	//aumentar program counter
	printf("definir la variable %c\n",identificador_variable);
	return POSICION_MEMORIA;
}


t_puntero obtenerPosicionVariable(t_nombre_variable identificador_variable){
	//return dictionary_get(diccionario, identificador de variable)
	// si no esta, devolver -1
	printf("Obtener posicion de %c\n", identificador_variable);
	return POSICION_MEMORIA;
}



t_valor_variable dereferenciar(t_puntero direccion_variable){
	//obtiene los cuatro bytes siguientes a direccion_variable
	//solictar a umv base=contexto_actual  offset=direccion_variable tamaño=4 bytes
	//recibir de umv 4 bytes de valor_variable
	// return valor_variable
	printf("Dereferenciar %d y su valor es: %d\n",direccion_variable, CONTENIDO_VARIABLE);
	return CONTENIDO_VARIABLE;
}


void asignar(t_puntero direccion_variable, t_valor_variable valor){
	//envio a la umv para almacenar base= contexto actual offset= direccion_variable tamaño=4bytes buffer= valor
	printf("Asignando en el valor \n");
}



t_valor_variable obtenerValorCompartida(t_nombre_compartida variable){
	printf("Obteniendo el valor de compartida");
	return CONTENIDO_VARIABLE;
}

t_valor_variable asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor){
	printf("Asignando a variable compartida ");
	return CONTENIDO_VARIABLE;
	}


void irAlLabel(t_nombre_etiqueta t_nombre_etiqueta){
	printf("Yendo al label \n");

}

void llamarSinRetorno(t_nombre_etiqueta etiqueta){
	printf("Llamando sin retorno a etiqueta\n");
	}

void llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar){
	printf("Llamando con retorno a etiqueta");
}

void finalizar(void){
	printf("Finalizando");
}

void retornar(t_valor_variable retorno){
	printf("Retornando valor de variable%d\n", retorno);
}

void imprimir(t_valor_variable valor_mostrar){
	printf("Imprimiendo valor de variable %d\n", valor_mostrar);
}


void imprimirTexto(char* texto){
	printf("Imprimiendo texto: %s", texto);
	}

void entradaSalida(t_nombre_dispositivo dispositivo, int tiempo){
	printf("Saliendo a entrada/salida en dispositivo por esta cantidad de tiempo\n");
}

void wait(t_nombre_semaforo identificador_semaforo){
	printf("Haciendo wait a semaforo\n");
}
void signal(t_nombre_semaforo identificador_semaforo){
	printf("Haciendo signal a semaforo\n");
}

