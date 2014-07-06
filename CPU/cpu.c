/*
 * cpu.c
 *
 *  Created on: 01/06/2014
 *      Author: utnso
 */

#include "cpu.h"



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
		.AnSISOP_signal = mi_signal};



int main(){
	prox_inst = malloc(0);
	//t_pcb *pcb = malloc(sizeof(t_pcb));
	got_usr1 = 0;
	etiquetas = NULL;
	base=0;
	offset=0;
	tam = 0;


	sa.sa_handler = signal_handler; // puntero a una funcion handler del signal
	sa.sa_flags = 0; // flags especiales para afectar el comportamiento de la señal
	sigemptyset(&sa.sa_mask); // conjunto de señales a bloquear durante la ejecucion del signal-catching function


	t_config *unaConfig = config_create("config.conf");
	char *ipKernel=config_get_string_value(unaConfig, "IPKERNEL");
	char *puertoKernel = config_get_string_value(unaConfig, "PUERTOKERNEL");
	char *puertoUmv = config_get_string_value(unaConfig, "PUERTOUMV");
	char *ipUmv=config_get_string_value(unaConfig, "IPKERNEL");

	/*conexion con el kernel*/

	handshake_kernel(puertoKernel, ipKernel);

	/*conexion con la umv*/
	handshake_umv(puertoUmv, ipUmv);

	/*maneja si recibe la señal SIGUSR, hay que revisarlo todavia*/
	if(sigaction(SIGUSR1, &sa, NULL) == -1){// sigaction permite llamar o especificar la accion asociada a la señal
			perror("sigaction");
			exit(1);
			}


	while(!got_usr1){
		traerIndiceEtiquetas();


		while(quit_sistema){

		/*recibe un pcb del kernel*/
		recibirUnPcb();

		/*se crea un diccionario para guardar las variables del contexto*/
		crearDiccionario();



		char* proxInstrucc = solicitarProxSentenciaAUmv();

		for(;quantum>0;quantum--){//ejecuta tantas instrucciones como quantums tenga
			parsearUnaInstruccion(proxInstrucc);
			fueFinEjecucion = 0;
			}
		if(!fueFinEjecucion){
			salirPorQuantum();
		}


		dictionary_destroy(dic_Variables);


		}


	socket_cerrar(socketKernel);
	socket_cerrar(socketUmv);
	}



	return 0;

	}


void handshake_umv(char *ip_umv, char *puerto_umv){
	//envio a la UMV
	socketUmv = socket_crear_client(puerto_umv,ip_umv);
	t_men_comun *mensaje_inicial = crear_men_comun(HS_CPU,"",string_length(""));
	socket_send_comun(socketUmv,mensaje_inicial);
	//espero coneccion de la UMV
	mensaje_inicial = socket_recv_comun(socketUmv);
	if(mensaje_inicial->tipo == HS_UMV){
		printf("UMV conectada\n");
	}else{
		printf("ERROR HANDSHAKE");
	}
}


void handshake_kernel(char *ip_k, char *puerto_k){
	//envio al kernel
	socketKernel = socket_crear_client(puerto_k,ip_k);
	t_men_comun *mensaje_inicial = crear_men_comun(HS_CPU,"",string_length(""));
	socket_send_comun(socketKernel,mensaje_inicial);
	//espero conexion de kernel
	mensaje_inicial = socket_recv_comun(socketKernel);
	if(mensaje_inicial->tipo == HS_KERNEL){
		printf("Kernel conectado\n");
	}else{
		printf("ERROR HANDSHAKE");
	}
}


void traerIndiceEtiquetas(){
	t_men_cpu_umv *cambio_pa= crear_men_cpu_umv(CAMBIO_PA, 0, 0, 0, proc_id);
	socket_send_cpu_umv(socketUmv, cambio_pa);
	destruir_men_cpu_umv(cambio_pa);

	base =pcb->dir_primer_byte_umv_indice_etiquetas;
	offset=0;
	tam=pcb->tam_indice_etiquetas;
	t_men_cpu_umv *sol_etiquetas = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_etiquetas);
	destruir_men_cpu_umv(sol_etiquetas);

	t_men_comun *rec_etiq = socket_recv_comun(socketUmv);
	if(rec_etiq->tipo == R_SOL_BYTES){
	etiquetas = malloc(rec_etiq->tam_dato);
	etiquetas = rec_etiq->dato;
	}
	if(rec_etiq->tipo == SEGMEN_FAULT){
		exit(1);
	}

}


void recibirUnPcb(){
	t_men_quantum_pcb *m = socket_recv_quantum_pcb(socketKernel);
	if(m->tipo==CONEC_CERRADA){
		//avisar al pcp
	}else {
		pcb = m->pcb;
		quantum = m->quantum;
		proc_id= string_itoa(pcb->id);
	}

}

void crearDiccionario(){
	dic_Variables = dictionary_create();

	//si se trata de un programa con un  stack que ya tiene variables
	int32_t tam_contexto = pcb->cant_var_contexto_actual;
	int32_t i = 1;
	if (tam_contexto > 0){//regenerar diccionario
		while(i<=tam_contexto){
			t_men_cpu_umv *cambio_pa= crear_men_cpu_umv(CAMBIO_PA, 0, 0, 0, proc_id);
			socket_send_cpu_umv(socketUmv, cambio_pa);
			destruir_men_cpu_umv(cambio_pa);


			base = pcb->dir_primer_byte_umv_contexto_actual;
			offset= (i-1)*5;
			tam=1;

			t_men_cpu_umv *sol_var = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
			socket_send_cpu_umv(socketUmv, sol_var);
			destruir_men_cpu_umv(sol_var);

			t_men_comun *rec_var = socket_recv_comun(socketUmv);
			if(rec_var->tipo == SEGMEN_FAULT){//manejar

			}
			if(rec_var->tipo== R_SOL_BYTES){
				char *key = malloc(rec_var->tam_dato);
				key = rec_var->dato;
				char *data = malloc(sizeof(string_itoa(base + offset)));
				data = string_itoa(base + offset);

				dictionary_put(dic_Variables, key, data);

				free(data);
				free(key);
			}

			destruir_men_comun(rec_var);

			i++;

		}

		}


	}



char* solicitarProxSentenciaAUmv(){// revisar si de hecho devuelve la prox instruccion(calculos)
	char* proxInst = NULL;

	//mandarle a la umv el cambio de proceso activo  pcb->id
	//con el indice de codigo y el pc obtengo la posicion de la proxima instruccion a ejecutar
	base = pcb->dir_primer_byte_umv_segmento_codigo;
	offset = (pcb->dir_primer_byte_umv_indice_codigo)+ 8*(1-pcb->program_counter);
	tam = sizeof(uint32_t)*4;

	//le mando a la umv base, offset y tamanio de la proxima instruccion
	t_men_cpu_umv *cambio_pa= crear_men_cpu_umv(CAMBIO_PA, 0, 0, 0, proc_id);
	socket_send_cpu_umv(socketUmv, cambio_pa);
	destruir_men_cpu_umv(cambio_pa);
	t_men_cpu_umv *sol_inst = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_inst);
	destruir_men_cpu_umv(sol_inst);

	//recibir la instruccion a ejecutar
	t_men_comun *rec_inst = socket_recv_comun(socketUmv);

	if(rec_inst->tipo == R_SOL_BYTES){
		char* proxInst = rec_inst->dato;
		printf("Instruccion que voy a ejecutar %s\n",proxInst);
	}
	if(rec_inst->tipo == CONEC_CERRADA){
		printf("Se cerro la conexion de la umv\n");
	}
	if(rec_inst->tipo ==SEGMEN_FAULT){
		printf("SEGMENTATION FAULT");// que hacer si hay seg fault? aviso al kernel y desconecto?
	}

	pcb->program_counter ++;
	return proxInst;
}


void parsearUnaInstruccion(char* unaIns){
	analizadorLinea(strdup(unaIns), &functions, &kernel_functions);
}



void salirPorQuantum(){
	//mando el pcb con un tipo de mensaje que sali por quantum
	socket_send_quantum_pcb(socketKernel, crear_men_quantum_pcb(FIN_QUANTUM,0, pcb));


}



void signal_handler(int sig){
	got_usr1 =1;

	t_men_quantum_pcb *m = crear_men_quantum_pcb(CPU_DESCONEC, 0, pcb);
	socket_send_quantum_pcb(socketKernel, m);
	destruir_quantum_pcb(m);

	fueFinEjecucion = 1;

	printf("Se recibio la SIGUSR1");

	}




void preservarContexto(){
	// guardar en stack ptro contexto actual
	t_men_cpu_umv *cambio_pa= crear_men_cpu_umv(CAMBIO_PA, 0, 0, 0, proc_id);
	socket_send_cpu_umv(socketUmv, cambio_pa);
	destruir_men_cpu_umv(cambio_pa);

	base = pcb->dir_primer_byte_umv_contexto_actual;
	offset = base + (pcb->cant_var_contexto_actual)+1;	// la posicion siguiente al ultimo valor de la ultima varaible
	tam = sizeof(int32_t);
	char * buffer = malloc(tam);
	buffer = string_itoa(base);

	t_men_cpu_umv *save_context = crear_men_cpu_umv(ALM_BYTES,base, offset, tam, buffer);
	socket_send_cpu_umv(socketUmv, save_context);
	destruir_men_cpu_umv(save_context);

	t_men_comun *r_con = socket_recv_comun(socketUmv);
	if (r_con->tipo == R_ALM_BYTES){
			//program counter + 1 (siguiente instruccion a ejecutar)
		t_men_cpu_umv *cambio_pa= crear_men_cpu_umv(CAMBIO_PA, 0, 0, 0, proc_id);
		socket_send_cpu_umv(socketUmv, cambio_pa);
		destruir_men_cpu_umv(cambio_pa);

		offset = offset + 4;
		buffer = string_itoa(pcb->program_counter);
		t_men_cpu_umv *save_proxins = crear_men_cpu_umv(ALM_BYTES, base, offset, tam, buffer);
		socket_send_cpu_umv(socketUmv, save_proxins);
		destruir_men_cpu_umv(save_proxins);

		t_men_comun *r_proxins = socket_recv_comun(socketUmv);
		if(r_proxins->tipo == R_ALM_BYTES){
				printf("Se almaceno correctamente el conexto");
			}
		if(r_proxins->tipo == MEM_OVERLOAD){
				printf("MEMORY OVERLOAD");
				finalizarContexto();
			}

		free(buffer);
	}
	if(r_con->tipo == MEM_OVERLOAD){
			printf("MEMORY OVERLOAD");
			finalizarContexto();
		}

}


void finalizarContexto(){
	t_men_quantum_pcb *fin_ej= crear_men_quantum_pcb(FIN_EJECUCION,0, pcb);
	socket_send_quantum_pcb(socketKernel, fin_ej);
	destruir_quantum_pcb(fin_ej);

	fueFinEjecucion = 1;
}

//Primitivas ANSISOP


t_puntero definirVariable(t_nombre_variable identificador_variable){
	t_men_cpu_umv *cambio_pa= crear_men_cpu_umv(CAMBIO_PA, 0, 0, 0, proc_id);
	socket_send_cpu_umv(socketUmv, cambio_pa);
	destruir_men_cpu_umv(cambio_pa);

	base = pcb->dir_primer_byte_umv_contexto_actual;
	offset= 1+ (pcb->cant_var_contexto_actual)*5 ;
	int32_t tam = sizeof(int32_t)+1;
	char* buffer = malloc(1);
	buffer = string_itoa(identificador_variable);

	t_men_cpu_umv *alm_bytes = crear_men_cpu_umv(ALM_BYTES, base, offset,tam , buffer);
	socket_send_cpu_umv(socketUmv, alm_bytes);
	destruir_men_cpu_umv(alm_bytes);
	free(buffer);

	t_men_comun *r_alm = socket_recv_comun(socketUmv);
	if(r_alm->tipo == R_ALM_BYTES){
		char* key= string_itoa(identificador_variable);
		char* data = string_itoa(base + offset);
		dictionary_put(dic_Variables, key, data);
		(pcb->cant_var_contexto_actual) ++;
		printf("Definir la variable %c en la posicion %c\n",identificador_variable, base +offset);

	}
	if(r_alm->tipo == MEM_OVERLOAD){
		printf("MEMORY OVERLOAD");
		base=0;
		offset=0;
		finalizarContexto();
	}


	return base + offset;


}


t_puntero obtenerPosicionVariable(t_nombre_variable identificador_variable){
	/*
	 * Se fija en el diccionario de variables la posicion
	 * la posicion va a ser el data del elemento
	 */
	t_puntero posicion;
	char *key = string_itoa(identificador_variable);
	t_hash_element *e = dictionary_get(dic_Variables,key);


	if (e->data == NULL){
		posicion = -1;
		}else {
			posicion= atoi(e->data);
		}

	printf("La posicion de la variable %c es %d", identificador_variable, posicion);

	return posicion ;



}



t_valor_variable dereferenciar(t_puntero direccion_variable){
	//obtiene los cuatro bytes siguientes a direccion_variable
	t_valor_variable valor;

	t_men_cpu_umv *cambio_pa= crear_men_cpu_umv(CAMBIO_PA, 0, 0, 0, proc_id);
	socket_send_cpu_umv(socketUmv, cambio_pa);
	destruir_men_cpu_umv(cambio_pa);

	int32_t base = pcb->dir_primer_byte_umv_contexto_actual;
	int32_t offset = 1 + direccion_variable; // son los cuatro bytes siguientes a la variable
	int32_t tam= sizeof(int32_t);

	t_men_cpu_umv *sol_val = crear_men_cpu_umv(SOL_BYTES,base, offset, tam, NULL);

	socket_send_cpu_umv(socketUmv,sol_val);
	destruir_men_cpu_umv(sol_val);

	//recibir de umv 4 bytes de valor_variable

	t_men_comun *men_comun = socket_recv_comun(socketUmv);

	if (men_comun->tipo == R_SOL_BYTES){

		valor = atoi(men_comun->dato);
	}

	if(men_comun->tipo ==SEGMEN_FAULT){
			printf("SEGMENTATION FAULT");// que hacer si hay seg fault? aviso al kernel y desconecto?
		}

	printf("Dereferenciar %d y su valor es: %d\n",direccion_variable,valor);

	return valor;
}


void asignar(t_puntero direccion_variable, t_valor_variable valor){
	//envio a la umv para almacenar base= contexto actual offset= direccion_variable tamaño=4bytes buffer= valor
	t_men_cpu_umv *cambio_pa= crear_men_cpu_umv(CAMBIO_PA, 0, 0, 0, proc_id);
	socket_send_cpu_umv(socketUmv, cambio_pa);
	destruir_men_cpu_umv(cambio_pa);

	int32_t base = pcb->dir_primer_byte_umv_contexto_actual;
	int32_t offset = 1 + direccion_variable;
	int32_t tam = sizeof(t_valor_variable);
	char* buffer = malloc(tam);
	buffer = string_itoa(valor);

	t_men_cpu_umv  *men_asig= crear_men_cpu_umv(ALM_BYTES, base, offset, tam, buffer);
	socket_send_cpu_umv(socketUmv, men_asig);
	destruir_men_cpu_umv(men_asig);

	t_men_comun *r_alm = socket_recv_comun(socketUmv);

	if(r_alm->tipo == R_ALM_BYTES){
	printf("Asignando en la direccion %d el valor %d \n", direccion_variable, valor);
	}
	if(r_alm->tipo == MEM_OVERLOAD){
		printf("MEMORY OVERLOAD");
		finalizarContexto();
		exit(1);
	}
}



t_valor_variable obtenerValorCompartida(t_nombre_compartida variable){
	//le mando al kernel el nombre de la variable compartida
	t_men_comun *m = crear_men_comun(OBTENER_VALOR, variable, sizeof(variable));
	socket_send_comun(socketKernel, m);

	//recibo el valor
	t_men_comun *respuesta = socket_recv_comun(socketKernel);
	char *c = respuesta->dato;
	t_valor_variable valor = atoi(c);

	printf("Obteniendo el valor de compartida %s que es %c", variable, valor);
	return valor;
}

t_valor_variable asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor){
	//le mando al kernel el nombre de la variable compartida
	//le mando el valor que le quiero asignar

	t_men_comun *var =crear_men_comun(GRABAR_VALOR, variable, sizeof(variable));

	char *c =  string_itoa(valor);
	t_men_comun *val_asignado = crear_men_comun(VALOR_ASIGNADO, c, sizeof(c));

	socket_send_comun(socketKernel,var);
	socket_send_comun(socketKernel, val_asignado);

	destruir_men_comun(var);
	destruir_men_comun(val_asignado);


	printf("Asignando a variable compartida %s el valor %d \n", variable, valor);
	return valor;
	}


void irAlLabel(t_nombre_etiqueta t_nombre_etiqueta){//revisar

	// primer instruccion ejecutable de etiqueta y -1 en caso de error


	t_puntero_instruccion pos_etiqueta= metadata_buscar_etiqueta(t_nombre_etiqueta, etiquetas, pcb->tam_indice_etiquetas);

	if(pos_etiqueta != -1){
		pcb->program_counter = pos_etiqueta;
	}else{
		printf("Hubo un error en la busqueda de la etiqueta /n");
		finalizarContexto();
	}


	printf("Yendo al label %s\n", t_nombre_etiqueta);

}

void llamarSinRetorno(t_nombre_etiqueta etiqueta){
	/*Preserva el contexto de ejecución actual para poder retornar luego al mismo.*/

	preservarContexto();


	pcb->cant_var_contexto_actual = 0;
	pcb->dir_primer_byte_umv_contexto_actual = (pcb->dir_primer_byte_umv_contexto_actual)+5*(pcb->cant_var_contexto_actual)+1;

	irAlLabel(etiqueta);

	printf("Llamando sin retorno a etiqueta %s\n", etiqueta);

	}



void llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar){
	//preservar el contexto

	preservarContexto();

	// dir de retorno
	t_men_cpu_umv *cambio_pa= crear_men_cpu_umv(CAMBIO_PA, 0, 0, 0, proc_id);
	socket_send_cpu_umv(socketUmv, cambio_pa);
	destruir_men_cpu_umv(cambio_pa);

	offset = offset + 4;
	char* buffer = malloc(sizeof(int32_t));
	buffer = string_itoa(donde_retornar);
	t_men_cpu_umv *save_retorno = crear_men_cpu_umv(ALM_BYTES, base, offset, tam, buffer);
	socket_send_cpu_umv(socketUmv, save_retorno);
	destruir_men_cpu_umv(save_retorno);

	free(buffer);

	t_men_comun *r_alm = socket_recv_comun(socketUmv);
	if(r_alm->tipo == R_ALM_BYTES){
		printf("Se almaceno la direccion de retorno");

		pcb->cant_var_contexto_actual = 0;
		pcb->dir_primer_byte_umv_contexto_actual = (pcb->dir_primer_byte_umv_contexto_actual)+5*(pcb->cant_var_contexto_actual)+1;
		irAlLabel(etiqueta);

		printf("Llamando con retorno a etiqueta %s y retorna en %d", etiqueta, donde_retornar);

	}
	if(r_alm->tipo == MEM_OVERLOAD){
		printf("MEMORY OVERLOAD no se pudo guardar contexto");
		finalizarContexto();
	}



}

void finalizar(void){
	t_men_cpu_umv *cambio_pa= crear_men_cpu_umv(CAMBIO_PA, 0, 0, 0, proc_id);
	socket_send_cpu_umv(socketUmv, cambio_pa);
	destruir_men_cpu_umv(cambio_pa);

	base=pcb->dir_primer_byte_umv_contexto_actual;
	offset= base -4;
	tam=sizeof(int32_t);

	t_men_cpu_umv *sol_progcount = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_progcount);
	destruir_men_cpu_umv(sol_progcount);

	t_men_comun *rec_progcount= socket_recv_comun(socketUmv);
	if(rec_progcount->tipo == R_SOL_BYTES){
		int32_t prog_counter = atoi(rec_progcount->dato);
		pcb->program_counter= prog_counter;
	}
	if(rec_progcount->tipo == SEGMEN_FAULT){
		printf("SEGMENTATION FAULT");
		exit(1);
	}

	offset= offset-4;

	t_men_cpu_umv *sol_context = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_context);
	destruir_men_cpu_umv(sol_context);

	t_men_comun *rec_context= socket_recv_comun(socketUmv);
	if(rec_context->tipo == R_SOL_BYTES){
		int32_t context = atoi(rec_context->dato);
		pcb->dir_primer_byte_umv_contexto_actual = context;
	}

	if(rec_context->tipo == SEGMEN_FAULT){
		printf("SEGMENTATION FAULT");
		exit(1);
	}

	if(pcb->dir_primer_byte_umv_contexto_actual == pcb->dir_primer_byte_umv_segmento_stack){

	finalizarContexto();

	}

	printf("Finalizando el programa actual");
}

void retornar(t_valor_variable retorno){
	int32_t dir_retorno = 0;
	int32_t context = 0;

	t_men_cpu_umv *cambio_pa= crear_men_cpu_umv(CAMBIO_PA, 0, 0, 0, proc_id);
	socket_send_cpu_umv(socketUmv, cambio_pa);
	destruir_men_cpu_umv(cambio_pa);

	base=pcb->dir_primer_byte_umv_contexto_actual;
	offset= base -4;
	tam=sizeof(int32_t);


	t_men_cpu_umv *sol_ret = crear_men_cpu_umv(SOL_BYTES,base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_ret);
	destruir_men_cpu_umv(sol_ret);

	t_men_comun *rec_ret= socket_recv_comun(socketUmv);
	if(rec_ret->tipo == R_SOL_BYTES){
		dir_retorno = atoi(rec_ret->dato);
	}

	if (rec_ret->tipo == SEGMEN_FAULT){
		printf("SEGMENTATION FAULT");
		exit(1);
	}


	offset= offset-4;

	t_men_cpu_umv *sol_progcount = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_progcount);
	destruir_men_cpu_umv(sol_progcount);

	t_men_comun *rec_progcount= socket_recv_comun(socketUmv);
	if(rec_progcount->tipo == R_SOL_BYTES){
		int32_t prog_counter = atoi(rec_progcount->dato);
		pcb->program_counter= prog_counter;
	}

	if(rec_progcount->tipo == SEGMEN_FAULT){
		printf("SEGMENTATION FAULT");
		exit(1);
	}

	offset= offset-4;

	t_men_cpu_umv *sol_context = crear_men_cpu_umv(SOL_BYTES, base, offset, tam, NULL);
	socket_send_cpu_umv(socketUmv, sol_context);
	destruir_men_cpu_umv(sol_context);

	t_men_comun *rec_context= socket_recv_comun(socketUmv);
	if(rec_context->tipo == R_SOL_BYTES){
		int32_t context = atoi(rec_context->dato);
		pcb->dir_primer_byte_umv_contexto_actual = context;

	}
	if (rec_context->tipo == SEGMEN_FAULT){
		printf("SEGMENTATION FAULT");
		exit(1);
	}


	base = context;
	offset= dir_retorno +1;
	char* buffer = string_itoa(retorno);
	t_men_cpu_umv *alm_val = crear_men_cpu_umv(ALM_BYTES, base, offset, tam, buffer);
	socket_send_cpu_umv(socketUmv, alm_val);
	destruir_men_cpu_umv(alm_val);

	t_men_comun *rec_alm = socket_recv_comun(socketUmv);
	if(rec_alm->tipo == MEM_OVERLOAD){
		printf("MEMORY OVERLOAD no se pudo guardar");
		exit(1);
	}
	if(rec_alm->tipo == R_ALM_BYTES){

	}

	printf("Retornando valor de variable%d en la direccion %d\n", retorno, dir_retorno);
}

void imprimir(t_valor_variable valor_mostrar){
	char *aux = string_itoa(valor_mostrar);
	t_men_comun *men = crear_men_comun(IMPRIMIR_VALOR,aux ,string_length(aux));
	socket_send_comun(socketKernel,men);

	men = crear_men_comun(ID_PROG,string_itoa(pcb->id) ,string_length(string_itoa(pcb->id)));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);
	//romper no violentamente con ### inexistente
	printf("Imprimiendo valor de variable %d\n", valor_mostrar);
}


void imprimirTexto(char* texto){
	t_men_comun *men = crear_men_comun(IMPRIMIR_TEXTO, texto,string_length(texto));
	socket_send_comun(socketKernel,men);

	men = crear_men_comun(ID_PROG,string_itoa(pcb->id) ,string_length(string_itoa(pcb->id)));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);
	//romper no violentamente con ### inexistente
	printf("Imprimiendo texto: %s", texto);
	}

void entradaSalida(t_nombre_dispositivo dispositivo, int tiempo){
	t_men_comun *men = crear_men_comun(IO_ID, dispositivo,string_length(dispositivo));
	char *cant_unidades = string_itoa(tiempo);
	men = crear_men_comun(IO_CANT_UNIDADES, cant_unidades,string_length(cant_unidades));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);


	t_men_quantum_pcb *mpcb = crear_men_quantum_pcb(PCB_Y_QUANTUM, 0, pcb);
	socket_send_quantum_pcb(socketKernel, mpcb);
	destruir_quantum_pcb(mpcb);
	//romper no violentamente con ### inexistente
	printf("Saliendo a entrada/salida en dispositivo %s por esta cantidad de tiempo%d\n", dispositivo, tiempo);
}

void wait(t_nombre_semaforo identificador_semaforo){
	t_men_comun *men = crear_men_comun(WAIT, identificador_semaforo,string_length(identificador_semaforo));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);
	//romper no violentamente con ### inexistente
	printf("Haciendo wait a semaforo%s\n", identificador_semaforo);
}
void mi_signal(t_nombre_semaforo identificador_semaforo){
	t_men_comun *men = crear_men_comun(SIGNAL, identificador_semaforo,string_length(identificador_semaforo));
	socket_send_comun(socketKernel,men);
	destruir_men_comun(men);
	//romper no violentamente con ### inexistente
	printf("Haciendo signal a semaforo%s\n", identificador_semaforo);

}



