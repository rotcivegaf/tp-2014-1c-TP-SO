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
		.AnSISOP_entradaSalida 			= entradaSalida};

AnSISOP_kernel kernel_functions = {
		.AnSISOP_wait 	= wait,
		.AnSISOP_signal = mi_signal};

enum ShapeType {
	ERROR,
	OK
};

t_dictionary *dic_Variables;
t_pcb *pcb;
int32_t socketKernel, socketUmv;
int32_t quantum_actual;
int32_t quantum_max;
char* etiquetas;
int32_t fueFinEjecucion = 0;
FILE *cpu_file_log;
int32_t pid_cpu;

int main(){
	got_usr1 = 0;



	sa.sa_handler = signal_handler; // puntero a una funcion handler del signal
	sa.sa_flags = 0; // flags especiales para afectar el comportamiento de la señal
	sigemptyset(&sa.sa_mask); // conjunto de señales a bloquear durante la ejecucion del signal-catching function

	cpu_file_log = txt_open_for_append("./CPU/logs/cpu.log");
	txt_write_in_file(cpu_file_log,"---------------------Nueva ejecucion--------------------------------------------------------------------------------------------\n");

	txt_write_in_file(cpu_file_log, "Cargo la configuracion desde el archivo\n");

	t_config *unaConfig = config_create("./CPU/cpu_config");
	char *puertoKernel = config_get_string_value(unaConfig, "Puerto_Kernel");
	char *ipKernel = config_get_string_value(unaConfig, "IP_Kernel");
	char *puertoUmv = config_get_string_value(unaConfig, "Puerto_UMV");
	char *ipUmv = config_get_string_value(unaConfig, "IP_UMV");
	imprimo_config(puertoKernel, ipKernel, puertoUmv, ipUmv);

	printf("El PID de este CPU es %d \n", getpid());

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
		fueFinEjecucion = 0;
		/*recibe un pcb del kernel*/
		recibirUnPcb();

		/* le digo a la UMV q cambie el proceso activo */
		cambio_PA(pcb->id);

		traerIndiceEtiquetas();
		/*se crea un diccionario para guardar las variables del contexto*/
		crearDiccionario();

		for (quantum_actual = 0;(quantum_actual<=quantum_max) && (!fueFinEjecucion); quantum_actual++){//aca cicla hasta q el haya terminado los quantums
			char* proxInstrucc = solicitarProxSentenciaAUmv();
			analizadorLinea(proxInstrucc, &functions, &kernel_functions);
			free(proxInstrucc);
		}
		if(!fueFinEjecucion){
			salirPorQuantum();
		}
		free(etiquetas);
		free(pcb);
		cambio_PA(0);//lo cambio a 0 asi la UMV puede comprobar q hay un error
		destruir_dic_Variables();
	}

	socket_cerrar(socketKernel);
	socket_cerrar(socketUmv);
	return 0;
}

void traerIndiceEtiquetas(){
	int32_t base, offset, tam;
	base =pcb->dir_primer_byte_umv_indice_etiquetas;
	offset=0;
	tam=pcb->tam_indice_etiquetas;

	enviar_men_cpu_umv_destruir(SOL_BYTES, base, offset, tam, NULL);

	t_men_comun *rec_etiq = socket_recv_comun(socketUmv);
	if(rec_etiq->tipo == R_SOL_BYTES){
		etiquetas = malloc(rec_etiq->tam_dato);
		memcpy(etiquetas ,rec_etiq->dato, rec_etiq->tam_dato);
	}
	if(rec_etiq->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
	}
	destruir_men_comun(rec_etiq);
}

void recibirUnPcb(){
	t_men_quantum_pcb *m = socket_recv_quantum_pcb(socketKernel);

	pcb = malloc(sizeof(t_pcb));
	if((m->tipo==CONEC_CERRADA) || (m->tipo!=PCB_Y_QUANTUM)){
		printf("ERROR se ha perdido la coneccion con el Kernel o el tipo de dato recibido es erroneo\n");
	}else{
		memcpy(pcb, m->pcb, sizeof(t_pcb));
		memcpy(&quantum_max , &(m->quantum), sizeof(int32_t));
	}

	destruir_quantum_pcb(m);
}

void crearDiccionario(){
	dic_Variables = dictionary_create();
	int32_t base, offset, tam;
	//si se trata de un programa con un  stack que ya tiene variables
	int32_t tam_contexto = pcb->cant_var_contexto_actual;
	int32_t i;

	if (tam_contexto > 0){//regenerar diccionario
		base = pcb->dir_primer_byte_umv_segmento_stack;
		for(i=0;i<tam_contexto;i++){
			offset= (i)*5;
			tam=1;

			enviar_men_cpu_umv_destruir(SOL_BYTES, base, offset, tam, NULL);

			t_men_comun *rec_var = socket_recv_comun(socketUmv);

			if(rec_var->tipo == SEGMEN_FAULT){
				manejarSegmentationFault();
			}
			if(rec_var->tipo== R_SOL_BYTES){
				char key[2];
				key[0] = rec_var->dato[0];
				key[1] = '\0';
				int32_t *pos = malloc(sizeof(int32_t));
				*pos = base + offset;
				dictionary_put(dic_Variables, key, pos );
			}
			destruir_men_comun(rec_var);
		}
	}
}

void manejarSegmentationFault(){
	//informar por pantalla el error
	printf("Hubo un error en la solictud de memoria, se finalizará la ejecucion del programa actual %d\n", pcb->id);
	txt_write_in_file(cpu_file_log, "Hubo un error en la solictud de memoria, se finalizará la ejecucion del programa actual\n");

	quantum_actual++;
	fueFinEjecucion =1;
	finalizarContexto(SEGMEN_FAULT);
}

char* solicitarProxSentenciaAUmv(){// revisar si de hecho devuelve la prox instruccion(calculos)
	int32_t base, offset, tam;
	char *proxInst;
	t_men_comun *men_base_offset;

	//con el indice de codigo y el pc obtengo la posicion de la proxima instruccion a ejecutar
	base = pcb->dir_primer_byte_umv_indice_codigo;
	offset = pcb->program_counter*(sizeof(int32_t)*2);
	tam = sizeof(int32_t)*2;
	enviar_men_cpu_umv_destruir(SOL_BYTES, base, offset, tam, NULL);

	men_base_offset = socket_recv_comun(socketUmv);

	memcpy(&offset, men_base_offset->dato, sizeof(int32_t));
	memcpy(&tam, men_base_offset->dato+sizeof(int32_t), sizeof(int32_t));

	if((men_base_offset->tam_dato != 8) || (men_base_offset->tipo!=R_SOL_BYTES))//por si la UMV me llega a mandar un tamanio distinto al q pedi o si el tipo de dato es diferente
		printf("ERROR el tamanio recibido:%i es distinto a 8, o el tipo de dato:%i es distinto a R_SOL_BYTES\n",men_base_offset->tam_dato,men_base_offset->tipo);


	destruir_men_comun(men_base_offset);

	base = pcb->dir_primer_byte_umv_segmento_codigo;

	//le mando a la umv base, offset y tamanio de la proxima instruccion
	enviar_men_cpu_umv_destruir(SOL_BYTES, base, offset, tam, NULL);

	//recibir la instruccion a ejecutar
	t_men_comun *rec_inst = socket_recv_comun(socketUmv);

	if(rec_inst->tipo == R_SOL_BYTES){
		proxInst = malloc(rec_inst->tam_dato);
		memcpy(proxInst, rec_inst->dato,rec_inst->tam_dato);
		char *proxInst_sin_blancos = sacar_caracteres_escape(proxInst);
		free(proxInst);
		txt_write_in_file(cpu_file_log, "Instruccion que voy a ejecutar\n");
		printf("Instruccion que voy a ejecutar:%s\n",proxInst_sin_blancos);
		pcb->program_counter ++;
		destruir_men_comun(rec_inst);
		return proxInst_sin_blancos;
	}
	if(rec_inst->tipo ==SEGMEN_FAULT){
		manejarSegmentationFault();
		destruir_men_comun(rec_inst);
		return NULL;
	}
	printf("ERROR tipo de dato:%i, recibido es erroneo\n", rec_inst->tipo);
	return NULL;
}

void salirPorQuantum(){
	//mando el pcb con un tipo de mensaje que sali por quantum
	char *aux_string = string_itoa(pcb->id);
	enviar_men_comun_destruir(socketKernel, FIN_QUANTUM, aux_string, string_length(aux_string));
	enviar_pcb_destruir();
	free (aux_string);
}

void enviar_pcb_destruir(){
	t_men_quantum_pcb *men = crear_men_quantum_pcb(PCB_Y_QUANTUM,0, pcb);
	socket_send_quantum_pcb(socketKernel, men);
	destruir_quantum_pcb(men);
}

void signal_handler(int sig){
	got_usr1 =1;

	txt_write_in_file(cpu_file_log, "Se recibio la SIGUSR1 \n");
	printf("Se recibio la SIGUSR1");

	while(quantum_actual <= quantum_max){
		char* proxInstrucc = solicitarProxSentenciaAUmv();
		analizadorLinea(proxInstrucc, &functions, &kernel_functions);
		free(proxInstrucc);
		quantum_actual ++;
	}
	if(!fueFinEjecucion){
		salirPorQuantum();
	}
	txt_write_in_file(cpu_file_log, "SIGUSR1 - Se termina de ejecutar el quantum actual \n");
	printf("SIGUSR1 - Se termina de ejecutar el quantum actual");

	free(etiquetas);
	free(pcb);
	cambio_PA(0);
	destruir_dic_Variables();

	enviar_men_comun_destruir(socketKernel, SIGUSR1_CPU_DESCONEC, NULL, 0);

	enviar_pcb_destruir();

	txt_write_in_file(cpu_file_log, "Se desconecta la CPU por recibir la señal SIGUSR1. Chau \n");
	printf("Se desconecta la CPU por recibir la señal SIGUSR1. Chau \n");
}

void preservarContexto(){
	int32_t base, offset, tam;
	base = pcb->dir_primer_byte_umv_segmento_stack;
	offset = pcb->dir_primer_byte_umv_contexto_actual - pcb->dir_primer_byte_umv_segmento_stack + (pcb->cant_var_contexto_actual*5);	// la posicion siguiente al ultimo valor de la ultima varaible
	tam = sizeof(int32_t);

	char *buffer = copiar_int_to_buffer(pcb->dir_primer_byte_umv_contexto_actual);
	enviar_men_cpu_umv_destruir(ALM_BYTES, base, offset, tam, buffer);
	free(buffer);

	t_men_comun *r_con = socket_recv_comun(socketUmv);
	if (r_con->tipo == R_ALM_BYTES){
		destruir_men_comun(r_con);
		offset = offset + 4;
		char *buffer = copiar_int_to_buffer(pcb->program_counter);
		enviar_men_cpu_umv_destruir(ALM_BYTES, base, offset, tam, buffer);
		free(buffer);

		t_men_comun *r_proxins = socket_recv_comun(socketUmv);
		if(r_proxins->tipo == R_ALM_BYTES){
			txt_write_in_file(cpu_file_log, "Se almaceno correctamente el contexto\n");
			printf("Se almaceno correctamente el contexto\n");
			destruir_men_comun(r_proxins);
			return;
		}
		if(r_proxins->tipo == SEGMEN_FAULT){
			manejarSegmentationFault();
			destruir_men_comun(r_proxins);
			return;
		}
	}
	if(r_con->tipo == SEGMEN_FAULT){
		destruir_men_comun(r_con);
		manejarSegmentationFault();
		return;
	}
	printf("ERROR tipo de dato:%i, recibido es erroneo\n", r_con->tipo);
}

void finalizarContexto(int32_t tipo_fin){
	int32_t i, base, offset;

	if(tipo_fin == SEM_INEX){
		printf("ERROR,el semaforo es inexistente\n");
		quantum_actual= quantum_max;
		fueFinEjecucion=1;
		return;
	}

	if (tipo_fin == SEM_BLOQUEADO){
		quantum_actual = quantum_max;
		fueFinEjecucion=1;
		enviar_pcb_destruir();
	}

	if(tipo_fin == VAR_INEX){
		printf("ERROR,la variable global es inexistente\n");
		quantum_actual= quantum_max;
		fueFinEjecucion=1;
		return;
	}

	if (tipo_fin == ERROR){
		printf("ERROR, la etiqueta no se ha encontrado pero creo q esta fuera del alcanse del tp\n");
		fueFinEjecucion = 1;
		return;
	}
	if (tipo_fin == SEGMEN_FAULT){
		char *aux_string = string_itoa(pcb->id);
		enviar_men_comun_destruir(socketKernel, SEGMEN_FAULT, aux_string, string_length(aux_string));
		free(aux_string);
		fueFinEjecucion = 1;
	}
	if (tipo_fin == OK){
		printf("Imprmiendo variables y finalizando proceso\n");
		char *string = "\n----------Imprimo el estado final de las variable----------\n";

		imprimirTexto(string);

		for(i=0;i<pcb->cant_var_contexto_actual;i++){//esta MIERDA de for es para imprimir vas variables en la consola del proceso progrma
			base = pcb->dir_primer_byte_umv_contexto_actual;
			offset = (5*i);
			enviar_men_cpu_umv_destruir(SOL_BYTES, base, offset, 1, NULL);
			t_men_comun *men_nombre_var = socket_recv_comun(socketUmv);

			base = pcb->dir_primer_byte_umv_contexto_actual;
			offset = (5*i)+1;

			enviar_men_cpu_umv_destruir(SOL_BYTES, base, offset, 4, NULL);
			t_men_comun *men_cont_var = socket_recv_comun(socketUmv);


			int32_t cont_var;
			memcpy(&cont_var,men_cont_var->dato,4);

			char *aux_cont_var =string_itoa(cont_var);

			int32_t tam_string = string_length(aux_cont_var)+4;// 1(tab)+1(nombre de la variable)+1(el igual)+string_length(aux_cont_var)+1(el \n)+1(el \0)
			char var_a_imp_con_contexto[tam_string];
			var_a_imp_con_contexto[0] = '\t';
			var_a_imp_con_contexto[1] = men_nombre_var->dato[0];
			var_a_imp_con_contexto[2] = '=';
			memcpy(var_a_imp_con_contexto+3,aux_cont_var,string_length(aux_cont_var));
			var_a_imp_con_contexto[tam_string-1] = '\n';
			var_a_imp_con_contexto[tam_string] = '\0';
			free (aux_cont_var);

			imprimirTexto(var_a_imp_con_contexto);
			destruir_men_comun(men_cont_var);
			destruir_men_comun(men_nombre_var);
		}
		char *aux_string = string_itoa(pcb->id);
		enviar_men_comun_destruir(socketKernel, FIN_EJECUCION, aux_string, string_length(aux_string));
		free(aux_string);
		fueFinEjecucion = 1;
	}
	txt_write_in_file(cpu_file_log, "Mandando a Kernel-PCP el pcb de un programa que se termino de ejecutar\n");
}

//Primitivas ANSISOP
t_puntero definirVariable(t_nombre_variable identificador_variable){
	int32_t base, offset, tam;
	base = pcb->dir_primer_byte_umv_segmento_stack;
	printf("Base del stack:%i", base);
	offset= (pcb->dir_primer_byte_umv_contexto_actual-pcb->dir_primer_byte_umv_segmento_stack)+(pcb->cant_var_contexto_actual)*5;

	tam = sizeof(int32_t)+1;
	int32_t pos_mem = base + offset;

	printf("Definir la variable %c en la posicion %i, offset:%i\n",identificador_variable, pos_mem,offset);
	char string_id_var[2];
	string_id_var[0]=identificador_variable;
	string_id_var[1]= '\0';
	enviar_men_cpu_umv_destruir(ALM_BYTES, base, offset, tam, string_id_var);


	t_men_comun *r_alm = socket_recv_comun(socketUmv);

	if(r_alm->tipo == R_ALM_BYTES){
		int32_t *pos = malloc(sizeof(int32_t));
		*pos = pos_mem;
		dictionary_put(dic_Variables, string_id_var, pos);
		(pcb->cant_var_contexto_actual) ++;
		txt_write_in_file(cpu_file_log, "Definiar la variable \n");
		txt_write_in_file(cpu_file_log, "en la posicion \n");
		destruir_men_comun(r_alm);
		return pos_mem;
	}
	if(r_alm->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
		base=0;
		offset=0;
		destruir_men_comun(r_alm);
		return 0;
	}
	printf("ERROR tipo de dato:%i, recibido es erroneo\n", r_alm->tipo);
	destruir_men_comun(r_alm);
	return -1;
}

t_puntero obtenerPosicionVariable(t_nombre_variable identificador_variable){
	/*Se fija en el diccionario de variables la posicion la posicion va a ser el data del elemento*/
	char key[2];
	key[0] = identificador_variable;
	key[1] = '\0';

	int32_t *e = dictionary_get(dic_Variables,key);

	if (e == NULL)
		printf("ERROR no se ha podido encontrar la key:%s\n",key);

	txt_write_in_file(cpu_file_log, "La posicion de la variable\n");
	txt_write_in_file(cpu_file_log, "es\n");
	printf("La posicion de la variable: %s, es %i\n", key, *e);

	return *e;
}

t_valor_variable dereferenciar(t_puntero direccion_variable){
	//obtiene los cuatro bytes siguientes a direccion_variable
	t_valor_variable valor;

	int32_t base = pcb->dir_primer_byte_umv_segmento_stack;
	int32_t offset = direccion_variable-base+1;
	int32_t tam= sizeof(int32_t);

	enviar_men_cpu_umv_destruir(SOL_BYTES, base, offset, tam, NULL);

	//recibir de umv 4 bytes de valor_variable
	t_men_comun *men_comun = socket_recv_comun(socketUmv);

	if (men_comun->tipo == R_SOL_BYTES){
		memcpy(&valor, men_comun->dato,men_comun->tam_dato);
		txt_write_in_file(cpu_file_log, "Dereferenciar \n");
		txt_write_in_file(cpu_file_log, "y su valor es \n");
		printf("Dereferenciar %d, offset:%i y su valor es: %d\n",direccion_variable, offset,valor);
	}
	if(men_comun->tipo ==SEGMEN_FAULT){
		manejarSegmentationFault();
	}
	destruir_men_comun(men_comun);
	return valor;
}

void asignar(t_puntero direccion_variable, t_valor_variable valor){
	//envio a la umv para almacenar base= contexto actual offset= direccion_variable tamaño=4bytes buffer= valor
	int32_t base = pcb->dir_primer_byte_umv_segmento_stack;
	int32_t offset = direccion_variable-base+1;

	int32_t tam = sizeof(t_valor_variable);
	char *buffer = copiar_int_to_buffer(valor);
	enviar_men_cpu_umv_destruir(ALM_BYTES, base, offset, tam, buffer);
	free(buffer);

	t_men_comun *r_alm = socket_recv_comun(socketUmv);

	if(r_alm->tipo == R_ALM_BYTES){
		txt_write_in_file(cpu_file_log, "Asignando en la direccion \n");
		txt_write_in_file(cpu_file_log, "el valor \n");
		printf("Asignando en la direccion %d, offset:%i, el valor %d \n", direccion_variable, offset, valor);
	}
	if(r_alm->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
	}
	destruir_men_comun(r_alm);
}

t_valor_variable obtenerValorCompartida(t_nombre_compartida variable){
	//le mando al kernel el nombre de la variable compartida
	enviar_men_comun_destruir(socketKernel, OBTENER_VALOR, variable, string_length(variable));

	//recibo el valor
	t_men_comun *respuesta = socket_recv_comun(socketKernel);
	t_valor_variable valor;

	if((respuesta->tipo != VAR_INEX) || (respuesta->tipo != R_OBTENER_VALOR))
		printf("ERROR se ha recibido un tipo de dato erroneo\n");

	if(respuesta->tipo == VAR_INEX){
		txt_write_in_file(cpu_file_log, "Acceso a variable global inexistente \n");
		printf("Error en acceso a la variable %s, no existe", variable);
		valor = 0;//todo asigno 0 si la variable no existe
		finalizarContexto(VAR_INEX);//todo romper
	}else{
		memcpy(&valor, respuesta->dato,respuesta->tam_dato);

		txt_write_in_file(cpu_file_log, "Obteniendo el valor de compartida \n");
		txt_write_in_file(cpu_file_log, "el valor \n");
		printf("Obteniendo el valor de compartida %s que es %c", variable, valor);
	}
	destruir_men_comun(respuesta);

	return valor;
}

t_valor_variable asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor){
	//le mando al kernel el nombre de la variable compartida
	//le mando el valor que le quiero asignar

	enviar_men_comun_destruir(socketKernel, GRABAR_VALOR, variable, string_length(variable));

	t_men_comun *men_resp = socket_recv_comun(socketKernel);
	if(men_resp->tipo == VAR_INEX){
		txt_write_in_file(cpu_file_log, "Acceso a variable global inexistente \n");
		printf("Error en acceso a la variable %s, no existe", variable);
		finalizarContexto(VAR_INEX);
		destruir_men_comun(men_resp);
		return -1;
	}
	if(men_resp->tipo == R_GRABAR_VALOR){
		char *c =  string_itoa(valor);
		enviar_men_comun_destruir(socketKernel, VALOR_ASIGNADO, c, string_length(c));
		free(c);
		//todo sincronizar
		return valor;
	}
	printf("ERROR el kernel me mando:%i y se esperaba otro tipo\n",men_resp->tipo);
	return -1;
}

void irAlLabel(t_nombre_etiqueta nombre_etiqueta){//revisar
	// primer instruccion ejecutable de etiqueta y -1 en caso de error
	char *etiq_sin_blancos = sacar_caracteres_escape(nombre_etiqueta);

	//t_puntero_instruccion pos_etiqueta= metadata_buscar_etiqueta("inicio", etiquetas, pcb->tam_indice_etiquetas);
	t_puntero_instruccion pos_etiqueta= metadata_buscar_etiqueta(etiq_sin_blancos, etiquetas, pcb->tam_indice_etiquetas);

	if(pos_etiqueta != -1){
		pcb->program_counter = pos_etiqueta;
		txt_write_in_file(cpu_file_log, "Yendo al label \n");
		printf("Yendo al label: %s\n", etiq_sin_blancos);
	}else{
		txt_write_in_file(cpu_file_log, "Hubo un error en la busqueda de la etiqueta\n");
		printf("ERROR en la busqueda de la etiqueta:%s \n",etiq_sin_blancos);
		finalizarContexto(ERROR);
	}
	free(etiq_sin_blancos);
}

void llamarSinRetorno(t_nombre_etiqueta etiqueta){
	/*Preserva el contexto de ejecución actual para poder retornar luego al mismo.*/
	preservarContexto();

	//actualizo las estructuras
	pcb->cant_var_contexto_actual = 0;
	pcb->dir_primer_byte_umv_contexto_actual = (pcb->dir_primer_byte_umv_contexto_actual)+5*(pcb->cant_var_contexto_actual)+8;
	destruir_dic_Variables();
	crearDiccionario();

	txt_write_in_file(cpu_file_log, "Llamando sin retorno a etiqueta\n");
	printf("Llamando sin retorno a etiqueta %s\n", etiqueta);
	irAlLabel(etiqueta);
}

void llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar){
	int32_t base, offset, tam;
	//preservar el contexto, agrega al segmento stack la dir de memoria del contexto anterior y la cantidad de variables del contex anterior
	preservarContexto();
	//aca le digo a la umv q me guarde en el stack la dir de la variable a la q tengo q asignarle el valor retornado por la "funcion"

	base = pcb->dir_primer_byte_umv_segmento_stack;
	offset = (pcb->dir_primer_byte_umv_contexto_actual - pcb->dir_primer_byte_umv_segmento_stack) + (pcb->cant_var_contexto_actual*5)+8;
	tam = sizeof(int32_t);

	char *buffer = copiar_int_to_buffer(donde_retornar);
	enviar_men_cpu_umv_destruir(ALM_BYTES, base, offset, tam, buffer);
	free(buffer);

	t_men_comun *r_alm = socket_recv_comun(socketUmv);
	if(r_alm->tipo == R_ALM_BYTES){
		txt_write_in_file(cpu_file_log, "Se almaceno la direccion de retorno \n");
		printf("Se almaceno la direccion de retorno\n");

		//actualizo las estructuras
		pcb->cant_var_contexto_actual = 0;
		pcb->dir_primer_byte_umv_contexto_actual = (pcb->dir_primer_byte_umv_contexto_actual)+offset+4;
		destruir_dic_Variables();
		crearDiccionario();
		irAlLabel(etiqueta);

		txt_write_in_file(cpu_file_log, "Llamando con retorno a la etiqueta\n");
		printf("Llamando con retorno a etiqueta %s y retorna en %i\n", etiqueta, donde_retornar);
		return;
	}
	if(r_alm->tipo == SEGMEN_FAULT){
		manejarSegmentationFault();
		return;
	}
	printf("ERROR tipo de dato:%i, recibido es erroneo\n", r_alm->tipo);
}

void finalizar(){
	int32_t base, offset, tam;

	//si es el fin de programa, le digo al kernel q finalizo y salgo de la funcion
	if(pcb->dir_primer_byte_umv_contexto_actual == pcb->dir_primer_byte_umv_segmento_stack){
		finalizarContexto(OK);
		txt_write_in_file(cpu_file_log, "Finalizando el programa actual\n");
		printf("Finalizando el programa actual\n");
		return;
	}

	int32_t prog_counter;
	int32_t dir_context_ant;
	//consigo 2 int, el primero para el program counter a retornar y el segundo para la dir de memoria al contexto anterior
	tam= 8;
	base= pcb->dir_primer_byte_umv_segmento_stack;
	offset= pcb->dir_primer_byte_umv_contexto_actual-pcb->dir_primer_byte_umv_segmento_stack- 8;

	enviar_men_cpu_umv_destruir(SOL_BYTES, base, offset, tam, NULL);

	t_men_comun *rec_ret= socket_recv_comun(socketUmv);
	if(rec_ret->tipo == R_SOL_BYTES){
		memcpy(&dir_context_ant, rec_ret->dato,sizeof(int32_t));
		memcpy(&prog_counter, rec_ret->dato+4,sizeof(int32_t));
	}else{
		printf("ERROR se esperaba un tipo de dato R_SOL_BYTES y recibo un:%i\n", rec_ret->tipo);
	}

	//actualizar el pcb
	int32_t cant_var = (pcb->dir_primer_byte_umv_contexto_actual - dir_context_ant-8)/5;
	actualizar_pcb(cant_var,prog_counter,dir_context_ant);

	regenerar_dicc_var();
}

void retornar(t_valor_variable retorno){
	int32_t base, offset, tam;
	int32_t dir_retorno;
	int32_t prog_counter;
	int32_t dir_context_ant;

	//consigo 3 int, el primero para asignarle el valor retorno, el segundo para el program counter a retornar y el tercero para la dir de memoria al contexto anterior
	tam= 12;
	base= pcb->dir_primer_byte_umv_segmento_stack;
	offset= pcb->dir_primer_byte_umv_contexto_actual-pcb->dir_primer_byte_umv_segmento_stack- 12;

	enviar_men_cpu_umv_destruir(SOL_BYTES, base, offset, tam, NULL);

	t_men_comun *rec_ret= socket_recv_comun(socketUmv);
	if(rec_ret->tipo == R_SOL_BYTES){
		memcpy(&dir_context_ant, rec_ret->dato,sizeof(int32_t));
		memcpy(&prog_counter, rec_ret->dato+4,sizeof(int32_t));
		memcpy(&dir_retorno, rec_ret->dato+8,sizeof(int32_t));
	}else{
		printf("ERROR se esperaba un tipo de dato R_SOL_BYTES y recibo un:%i\n", rec_ret->tipo);
	}

	//actualizar el pcb
	int32_t cant_var = (pcb->dir_primer_byte_umv_contexto_actual - dir_context_ant-12)/5;
	actualizar_pcb(cant_var,prog_counter,dir_context_ant);

	regenerar_dicc_var();

	//almaceno la variable retorno en el contexto anterior
	char *buffer = copiar_int_to_buffer(retorno);

	enviar_men_cpu_umv_destruir(ALM_BYTES, dir_retorno, 1, sizeof(int32_t), buffer);

	free(buffer);
	t_men_comun *rec_alm = socket_recv_comun(socketUmv);
	if(rec_alm->tipo == R_ALM_BYTES){
		txt_write_in_file(cpu_file_log, "Retornando valor de variable\n");
		printf("Retornando valor de variable:%d en la direccion %d\n", retorno, dir_retorno);
	}else{
		printf("ERROR se esperaba un tipo de dato R_SOL_BYTES y recibo un:%i\n", rec_ret->tipo);
	}
}

void imprimir(t_valor_variable valor_mostrar){
	char *aux = string_itoa(valor_mostrar);
	imprimirTexto(aux);
	free(aux);
}

void imprimirTexto(char* texto){
	char *texto_sin_blancos = sacar_caracteres_escape(texto);

	enviar_men_comun_destruir(socketKernel, IMPRIMIR_TEXTO, texto_sin_blancos, string_length(texto_sin_blancos));
	char *aux_string = string_itoa(pcb->id);
	enviar_men_comun_destruir(socketKernel, ID_PROG, aux_string, string_length(aux_string));
	free(aux_string);
	recibir_resp_kernel(R_IMPRIMIR);

	txt_write_in_file(cpu_file_log, "Imprimiendo texto\n");
	printf("Imprimiendo texto: %s\n", texto_sin_blancos);
	free(texto_sin_blancos);
}

void entradaSalida(t_nombre_dispositivo dispositivo, int tiempo){
	enviar_men_comun_destruir(socketKernel, IO_ID, dispositivo, string_length(dispositivo));
	char *aux_tiempo = string_itoa(tiempo);
	enviar_men_comun_destruir(socketKernel, IO_CANT_UNIDADES,aux_tiempo , string_length(aux_tiempo));
	free(aux_tiempo);

	enviar_pcb_destruir();

	txt_write_in_file(cpu_file_log, "Saliendo a i/o en dispositivo\n");
	printf("Saliendo a entrada/salida en dispositivo %s por esta cantidad de tiempo%d\n", dispositivo, tiempo);
}

void wait(t_nombre_semaforo identificador_semaforo){
	enviar_men_comun_destruir(socketKernel, WAIT, identificador_semaforo, string_length(identificador_semaforo));
	txt_write_in_file(cpu_file_log, "Haciendo wait a semaforo\n");
	printf("Haciendo wait a semaforo %s\n", identificador_semaforo);

	t_men_comun *men_resp = socket_recv_comun(socketKernel);

	switch(men_resp->tipo){
		case SEM_OK:
			//El semaforo esta disponible, seguir procesando o lo que sea
			printf("Se recibio el mensaje SEM_OK, se continuara con la ejecucion\n");
			txt_write_in_file(cpu_file_log, "El semaforo esta disponible, se continua con la ejecucion.");
			//No hago nada, continua con la ejecucion normalmente
			break;
		case SEM_BLOQUEADO:
			finalizarContexto(SEM_BLOQUEADO); // para que no busque la proxima instruccion, y se quede bloqueado
			// no hace falta mandar nada al kernel, ya que al avisar que lo bloquea lo manda a bloqueado por get_cpu
			printf("Se recibio el mensaje SEM_BLOQUEADO del semaforo,se bloquea el programa n° %i\n", pcb->id);
			txt_write_in_file(cpu_file_log, "Se bloqueo el programa por un semaforo, se desaloja de la cpu.");
			//El semaforo esta bloqueado, desalojar(suponiendo que tiene que cambiar de proceso, si es asi, mandar el pcb)
			break;
		case SEM_INEX:
			printf("Acceso a semaforo %s inexistente \n",identificador_semaforo);
			txt_write_in_file(cpu_file_log,"Acceso a semaforo inexistente");
			finalizarContexto(SEM_INEX);
			break;
		default:
			printf("El tipo de dato recibido: %i es erroneo\n",men_resp->tipo);
			txt_write_in_file(cpu_file_log,"Se recibio un msj del kernel de tipo erroneo");
			break;
	}
	destruir_men_comun(men_resp);
}

void mi_signal(t_nombre_semaforo identificador_semaforo){
	enviar_men_comun_destruir(socketKernel, SIGNAL, identificador_semaforo, string_length(identificador_semaforo));

	txt_write_in_file(cpu_file_log,"Haciendo signal a semaforo\n");
	printf("Haciendo signal a semaforo %s\n", identificador_semaforo);

	t_men_comun *respuesta = socket_recv_comun(socketKernel);

	if(respuesta->tipo == SEM_INEX){
		txt_write_in_file(cpu_file_log,"Signal a semaforo inexistente\n");
		printf("Signal a semaforo %s inexistente \n", identificador_semaforo);
		finalizarContexto(SEM_INEX);
		}
	destruir_men_comun(respuesta);
}

void logear_int(FILE* destino,int32_t un_int){
	char *aux_string = string_itoa(un_int);
	txt_write_in_file(destino,aux_string);
	free(aux_string);
}

void logear_char(FILE* destino,char un_char){
	if (un_char == '\0'){
		char *aux_string = "\\0";
		txt_write_in_file(destino, aux_string);
	}else{
		char *aux_string = string_itoa((int)un_char);
		txt_write_in_file(destino, aux_string);
		free(aux_string);
	}
	txt_write_in_file(destino,"-");
}

void handshake_umv(char *puerto, char *ip){
	//envio a la UMV
	socketUmv = socket_crear_client(puerto,ip);

	enviar_men_comun_destruir(socketUmv, HS_CPU, NULL, 0);

	//espero coneccion de la UMV
	t_men_comun *mensaje_inicial = socket_recv_comun(socketUmv);
	if(mensaje_inicial->tipo == HS_UMV){
		txt_write_in_file(cpu_file_log, "UMV conectada \n");
		printf("UMV conectada\n");
		destruir_men_comun(mensaje_inicial);
	}else{
		txt_write_in_file(cpu_file_log, "ERROR HANDSHAKE UMV\n");
		printf("ERROR HANDSHAKE\n");
	}
}

void handshake_kernel(char *puerto, char *ip){
	//envio al kernel
	socketKernel = socket_crear_client(puerto,ip);

	enviar_men_comun_destruir(socketKernel, HS_CPU, NULL, 0);

	//espero conexion de kernel
	t_men_comun *mensaje_inicial = socket_recv_comun(socketKernel);
	if(mensaje_inicial->tipo == HS_KERNEL){
		txt_write_in_file(cpu_file_log, "Kernel conectado\n");
		printf("Kernel conectado\n");
		destruir_men_comun(mensaje_inicial);
	}else{
		txt_write_in_file(cpu_file_log, "ERROR HANDSHAKE KERNEL \n");
		printf("ERROR HANDSHAKE\n");
	}
}

void imprimo_config(char *puertoKernel, char *ipKernel, char *puertoUmv, char *ipUmv){
	printf("\n\n------------------------------Archivo Config----------------------------------------\n");
	printf("	Puerto Kernel	= %s\n", puertoKernel);
	printf("	IP Kernel	= %s\n", ipKernel);
	printf("	Puerto UMV	= %s\n", puertoUmv);
	printf("	IP UMV		= %s\n", ipUmv);
	printf("------------------------------------------------------------------------------------\n\n");
}

void destruir_dic_Variables(){
	void _liberar_pos(int32_t *pos){
		free(pos);
	}
	dictionary_destroy_and_destroy_elements(dic_Variables, (void *)_liberar_pos);
}

void cambio_PA(int32_t id_proc){
	printf("Cambio el proceso activo al proceso nº%i\n",id_proc);
	enviar_men_cpu_umv_destruir(CAMBIO_PA, id_proc, 0, 0, NULL);
}
//recibe un int y lo transforma en un char *
char *copiar_int_to_buffer(int32_t un_int){
	char *ret = malloc(sizeof(int32_t));
	memcpy(ret, &un_int, sizeof(int32_t));
	return ret;
}
/*envia un mensaje cpu umv y lo destruye */
void enviar_men_cpu_umv_destruir(int32_t tipo, int32_t base, int32_t offset, int32_t tam, char *buffer){
	t_men_cpu_umv *men_cpu_umv = crear_men_cpu_umv(tipo, base, offset,tam , buffer);
	socket_send_cpu_umv(socketUmv, men_cpu_umv);
	destruir_men_cpu_umv(men_cpu_umv);
}

void recibir_resp_kernel(int32_t tipo_esperado){
	t_men_comun *men_sincro_ok = socket_recv_comun(socketKernel);
	if(men_sincro_ok->tipo != tipo_esperado)
		printf("ERROR el kernel me mando:%i y se esperaba %i\n",men_sincro_ok->tipo,tipo_esperado);
	destruir_men_comun(men_sincro_ok);
}

void regenerar_dicc_var(){
	int32_t base, offset,i;

	destruir_dic_Variables();
	dic_Variables = dictionary_create();

	base= pcb->dir_primer_byte_umv_segmento_stack;
	for(i=0;i<pcb->cant_var_contexto_actual;i++){
		offset= pcb->dir_primer_byte_umv_contexto_actual-pcb->dir_primer_byte_umv_segmento_stack + (i*5);

		enviar_men_cpu_umv_destruir(SOL_BYTES, base, offset, 1, NULL);

		t_men_comun *men_var = socket_recv_comun(socketUmv);
		char *key = malloc(2);
		key[0] = men_var->dato[0];
		key[1] = '\0';
		int32_t *pos = malloc(sizeof(int32_t));
		*pos = base + offset;
		dictionary_put(dic_Variables, key, pos );
	}
}

void actualizar_pcb(int32_t cant_var,int32_t program_counter,int32_t dir_primer_byte_umv){
	pcb->cant_var_contexto_actual = cant_var;
	pcb->program_counter=program_counter;
	pcb->dir_primer_byte_umv_contexto_actual=dir_primer_byte_umv;
}

char *sacar_caracteres_escape(char *un_string){
	char *aux_string = string_duplicate(un_string);
	string_trim(&aux_string);
	return aux_string;
}
