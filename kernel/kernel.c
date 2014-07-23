#include "kernel.h"

fd_set conj_soc_progs;   // conjunto maestro de sockets del PLP(sockets de progrmas)
fd_set conj_soc_cpus;    // conjunto maestro de sockets del PCP(sockets de cpus)
int32_t listener_prog;
int32_t soc_umv;
int32_t _CONTADOR_PROGRAMA = 0;
t_colas *colas;  //colas de los cinco estados de los procesos
t_list *lista_cpu; //cola donde tengo los cpu conectados
pthread_mutex_t mutex_new = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_block = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_exec = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_exit = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_uso_lista_cpu= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_io= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_dicc_var= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_dicc_sem= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_dispositivos_io= PTHREAD_MUTEX_INITIALIZER;

sem_t cant_new, cant_ready, cant_exit;
sem_t cant_multiprog, cant_cpu_libres;
int32_t quit_sistema = 1, _QUANTUM_MAX, _RETARDO, _TAMANIO_STACK;
char *PUERTO_PROG;
char *PUERTO_CPU;
t_dictionary *dicc_disp_io;
t_dictionary *dicc_var;
t_dictionary *dicc_sem;
FILE *plp_log;
FILE *pcp_log;


int main(void){
	dicc_var = dictionary_create();
	dicc_sem = dictionary_create();
	//diccionario dispositivos
	dicc_disp_io = dictionary_create();

	t_datos_config *diccionario_config = levantar_config();

	plp_log = txt_open_for_append("./kernel/logs/plp.log");
	txt_write_in_file(plp_log,"---------------------Nueva ejecucion------------------------------\n");

	pcp_log = txt_open_for_append("./kernel/logs/pcp.log");
	txt_write_in_file(pcp_log,"---------------------Nueva ejecucion------------------------------\n");

	//Conecta con la umv
	handshake_umv(diccionario_config->ip_umv, diccionario_config->puerto_umv);

	//Inicializacion semaforos
	crear_cont(&cant_new , 0);
	crear_cont(&cant_ready , 0);
	crear_cont(&cant_exit , 0);

	crear_cont(&cant_cpu_libres , 0);
	crear_cont(&cant_multiprog , diccionario_config->multiprogramacion); //cantidad de procesos que todavia entran
	free(diccionario_config);
	//Creacion de colas
	lista_cpu = list_create();
	colas = malloc(sizeof(t_colas));
	colas->cola_new = queue_create();
	colas->cola_ready = queue_create();
	colas->cola_block = queue_create();
	colas->cola_exec = queue_create();
	colas->cola_exit = queue_create();

	//Creacion de hilos
	pthread_t hilo_plp, hilo_pcp;
	pthread_create(&hilo_plp, NULL, plp, NULL);
	pthread_create(&hilo_pcp, NULL, pcp, NULL);
	menu_imp();

	queue_destroy(colas->cola_block);
	queue_destroy(colas->cola_exec);
	queue_destroy(colas->cola_exit);
	queue_destroy(colas->cola_new);
	queue_destroy(colas->cola_ready);
	list_destroy(lista_cpu);
	return EXIT_SUCCESS;
}

void *plp(){
	t_men_comun *men_prog;
	int32_t new_prog;
	pthread_t hilo_new_ready;
	fd_set read_fds; // conjunto temporal de sockets para select()
	int32_t fdmax;   // socket con el mayor valor

	//Creacion del hilo que maneja el pasaje de pcbs de new a ready
	pthread_create(&hilo_new_ready, NULL, manejador_new_ready, NULL);

	//Abre un server para escuchar las conexiones de los programas
	listener_prog = socket_crear_server(PUERTO_PROG);
	txt_write_in_file(plp_log,"Escuchando conexiones entrantes de programas \n");

	FD_ZERO(&conj_soc_progs);    // borra los conjuntos maestro y temporal de sockets
	FD_ZERO(&read_fds);
	FD_SET(listener_prog, &conj_soc_progs);

	fdmax = listener_prog; // por ahora es éste

	int32_t i;

	while(quit_sistema){
		read_fds = conj_soc_progs; // Copia el conjunto maestro al temporal
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			txt_write_in_file(plp_log,"Error en el select \n");
			perror("PLP-select");
			exit(1);
		}
		for(i = 3; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // Si hay datos entrantes en el socket i
				if (i == listener_prog) { //Si i es el socket que espera conexiones, es porque hay un nuevo prog
					new_prog = ingresar_nuevo_programa();
					if (new_prog > fdmax) // Actualiza el socket maximo
						fdmax = new_prog;
				}else{
					men_prog = socket_recv_comun(i);
					switch(men_prog->tipo){
					case CONEC_CERRADA:
						administrar_prog_cerrado(i, men_prog);
						break;
					case CODIGO_SCRIPT:
						administrar_new_script(i, men_prog);
						break;
					default:
						printf("ERROR PLP: se esperaba al recibir el tipo de dato:%i\n", men_prog->tipo);
						break;
					}
				}
			}
		}
	usleep(_RETARDO*1000);
	}
	return NULL;
}
//funciones PLP
int32_t ingresar_nuevo_programa(){
	int32_t new_soc = socket_accept(listener_prog);
	handshake_prog(new_soc);

	txt_write_in_file(plp_log,"Se acepto la conexion de un nuevo programa con socket n°:");
	logear_int(plp_log,new_soc);
	txt_write_in_file(plp_log,"\n");

	FD_SET(new_soc, &conj_soc_progs);
	return new_soc;
}

void administrar_prog_cerrado(int32_t soc_prog,t_men_comun *men_prog){
	bool _es_soc_prog(t_cpu *un_cpu){
		return un_cpu->soc_prog == soc_prog;
	}
	txt_write_in_file(plp_log,"Cerro la conexion el programa con socket n°:");
	logear_int(plp_log,soc_prog);
	txt_write_in_file(plp_log,"\n");
	printf("PLP-select: Prog desconectado socket nº%i\n", soc_prog);
	FD_CLR(soc_prog, &conj_soc_progs);
	destruir_men_comun(men_prog);

	pthread_mutex_lock(&mutex_uso_lista_cpu);

	t_cpu *aux_cpu = list_remove_by_condition(lista_cpu, (void*)_es_soc_prog);

	if (aux_cpu!=NULL){
		FD_CLR(aux_cpu->soc_cpu, &conj_soc_cpus);
		socket_cerrar(aux_cpu->soc_cpu);
	}
	pthread_mutex_unlock(&mutex_uso_lista_cpu);
	mover_pcb_exit(soc_prog);
}

void administrar_new_script(int32_t soc_prog, t_men_comun *men_prog){
	//printf("PLP-select: nuevo prog con socket n°%i\n", i);
	txt_write_in_file(plp_log,"Nuevo programa con socket n°:");
	logear_int(plp_log,soc_prog);
	txt_write_in_file(plp_log,"\n");

	_CONTADOR_PROGRAMA++;

	//Pide mem para el prog
	t_resp_sol_mem *resp_sol = solicitar_mem(men_prog);

	if (resp_sol == NULL){
		FD_CLR(soc_prog, &conj_soc_progs);
		txt_write_in_file(plp_log,"Memoria insuficiente para el programa con socket n°:");
		logear_int(plp_log,soc_prog);
		txt_write_in_file(plp_log,"\n");

		//Avisa al programa que no hay memoria
		char *aux_sring = string_itoa(_CONTADOR_PROGRAMA);
		enviar_men_comun_destruir(soc_prog,MEM_OVERLOAD,aux_sring,string_length(aux_sring)+1);
		socket_cerrar(soc_prog);
		free(aux_sring);
		free(resp_sol);
	}else{
		//Crea el pcb y lo pone en new
		t_pcb_otros *pcb_otros = malloc(sizeof(t_pcb_otros));
		pcb_otros->n_socket = soc_prog;
		pcb_otros->pcb = crear_pcb_escribir_seg_UMV(men_prog,resp_sol);
		free(resp_sol);
		pcb_otros->peso = calcular_peso(men_prog);
		pcb_otros->tipo_fin_ejecucion = -1;
		pthread_mutex_lock(&mutex_new);
		queue_push(colas->cola_new,pcb_otros);
		sem_incre(&cant_new);
		pthread_mutex_unlock(&mutex_new);

		txt_write_in_file(plp_log,"Se creo el pcb para el programa con socket n°:");
		logear_int(plp_log,soc_prog);
		txt_write_in_file(plp_log," , su id es:");
		logear_int(plp_log,pcb_otros->pcb->id);
		txt_write_in_file(plp_log,"\n");
	}
}
//funciones PCP
void *pcp(){
	int32_t i, fdmax, new_soc;
	fd_set read_fds;

	// Crea el hilo que pasa los pcb de ready a exec
	pthread_t hilo_ready_exec;
	pthread_create(&hilo_ready_exec, NULL, manejador_ready_exec,NULL);

	// Crea el hilo que maneja las llegadas de pcbs a exit
	pthread_t hilo_exit;
	pthread_create(&hilo_exit, NULL, manejador_exit, NULL);

	int32_t listener_cpu = socket_crear_server(PUERTO_CPU);
	txt_write_in_file(pcp_log,"Escuchando conexiones entrantes de cpus \n");

	FD_ZERO(&conj_soc_cpus);
	FD_ZERO(&read_fds);
	FD_SET(listener_cpu, &conj_soc_cpus);
	fdmax = listener_cpu;

	while(quit_sistema){
		read_fds = conj_soc_cpus;
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			txt_write_in_file(pcp_log,"Error en el select \n");
			perror("PCP-select");
			exit(1);
		}
		for(i = 3; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == listener_cpu) { // Si los datos son en el srv que escucha cpus(se conecta una nueva cpu)
					new_soc = ingresar_new_cpu(listener_cpu);
					if (new_soc > fdmax)
						fdmax = new_soc;
				}else{
					// Sino, es un cpu mandando datos
					t_men_comun *men_cpu = socket_recv_comun(i);
					switch(men_cpu->tipo){
					case CONEC_CERRADA:
						conec_cerrada_cpu(i, men_cpu);
						break;
					case FIN_QUANTUM:
						fin_quantum(i, men_cpu);
						break;
					case FIN_EJECUCION:
						fin_ejecucion(FIN_EJECUCION,i ,men_cpu);
						break;
					case SEGMEN_FAULT:
						fin_ejecucion(SEGMEN_FAULT,i , men_cpu);
						break;
					case IMPRIMIR_TEXTO:
						imprimir_texto(i, men_cpu);
						break;
					case IMPRIMIR_VALOR:
						imprimir_valor(i, men_cpu);
						break;
					case OBTENER_VALOR:
						obtener_valor_compartida(i, men_cpu);
						break;
					case GRABAR_VALOR:
						grabar_valor_compartida(i, men_cpu);
						break;
					case WAIT:
						wait(i ,men_cpu);
						break;
					case SIGNAL:
						signal(i, men_cpu);
						break;
					case IO_ID:
						enviar_IO(i, men_cpu);
						break;
					default:
						printf("ERROR se esperaba al recibir el tipo de dato:%i\n", men_cpu->tipo);
						break;
					}
				}
			}
		}
		usleep(_RETARDO*1000);
	}
	return NULL;
}

int32_t ingresar_new_cpu(int32_t listener_cpu){
	int32_t new_soc_cpu = socket_accept(listener_cpu);
	handshake_cpu(new_soc_cpu);

	printf("PCP-select: nueva CPU conectada socket nº%i\n", new_soc_cpu);
	txt_write_in_file(pcp_log,"Se acepto la conexion de una nueva CPU con socket n°:");
	logear_int(plp_log,new_soc_cpu);
	txt_write_in_file(pcp_log,"\n");

	t_cpu *aux_cpu = malloc(sizeof(t_cpu));
	aux_cpu->soc_cpu = new_soc_cpu;
	aux_cpu->id_prog_exec = 0;
	aux_cpu->soc_prog = 0;

	FD_SET(new_soc_cpu, &conj_soc_cpus);

	pthread_mutex_lock(&mutex_uso_lista_cpu);
	list_add(lista_cpu, aux_cpu);
	sem_incre(&cant_cpu_libres);
	pthread_mutex_unlock(&mutex_uso_lista_cpu);

	return new_soc_cpu;
}

void conec_cerrada_cpu(int32_t soc_cpu,t_men_comun *men_cpu){
	pthread_mutex_lock(&mutex_uso_lista_cpu);
	t_cpu *aux_cpu = get_cpu_soc_cpu_remove(soc_cpu);

	FD_CLR(soc_cpu, &conj_soc_cpus);
	socket_cerrar(soc_cpu);
	destruir_men_comun(men_cpu);

	txt_write_in_file(pcp_log,"Cerro la conexion el cpu con socket nº:");
	logear_int(pcp_log,soc_cpu);
	txt_write_in_file(pcp_log,"\n");
	printf("PCP-select: CPU desconectada socket nº%i\n", soc_cpu);

	if (aux_cpu->id_prog_exec != 0){
		t_pcb_otros *aux_pcb_otros = get_pcb_otros_exec(aux_cpu->id_prog_exec);
		pthread_mutex_lock(&mutex_ready);
		queue_push(colas->cola_ready,aux_pcb_otros);
		sem_incre(&cant_ready);
		pthread_mutex_unlock(&mutex_ready);
	}else{
		sem_decre(&cant_cpu_libres);
	}
	pthread_mutex_unlock(&mutex_uso_lista_cpu);
	free(aux_cpu);
}

void wait(int32_t soc_cpu,t_men_comun *men_cpu){
	t_semaforo *semaforo;
	t_cpu *aux_cpu;
	t_pcb_otros *aux_pcb_otros;
	pthread_mutex_lock(&mutex_dicc_sem);

	semaforo = dictionary_get(dicc_sem,men_cpu->dato);
	if(semaforo != NULL){
		if(semaforo->valor > 0){
			(semaforo->valor)--;
			enviar_men_comun_destruir(soc_cpu, SEM_OK, NULL, 0);
		}else{
			pthread_mutex_lock(&mutex_uso_lista_cpu);
			aux_cpu = get_cpu_soc_cpu_remove(soc_cpu);
			pthread_mutex_unlock(&mutex_uso_lista_cpu);
			enviar_men_comun_destruir(soc_cpu, SEM_BLOQUEADO, NULL, 0);
			txt_write_in_file(pcp_log,"Se bloqueo al programa por un wait, con id ");
			logear_int(pcp_log,aux_cpu->id_prog_exec);
			txt_write_in_file(pcp_log,"/n");
			aux_pcb_otros = actualizar_pcb_y_bloq(aux_cpu);
			queue_push(semaforo->procesos,aux_pcb_otros);
		}
		txt_write_in_file(pcp_log,"WAIT por cpu con socket n°:");
		logear_int(pcp_log,soc_cpu);
		txt_write_in_file(pcp_log,"\n");
	}else{
		llamada_erronea(soc_cpu, SEM_INEX);
		txt_write_in_file(pcp_log,"Error en WAIT (semaforo inexistente) por cpu con socket n°:");
		logear_int(pcp_log,soc_cpu);
		txt_write_in_file(pcp_log,"\n");
	}

	pthread_mutex_unlock(&mutex_dicc_sem);
	destruir_men_comun(men_cpu);
}

void signal(int32_t soc_cpu,t_men_comun *men_cpu){
	t_semaforo *semaforo;
	t_pcb_otros *aux_pcb_otros;
	pthread_mutex_lock(&mutex_dicc_sem);

	semaforo = dictionary_get(dicc_sem,men_cpu->dato);

	if(semaforo != NULL){
		(semaforo->valor)++;
		enviar_men_comun_destruir(soc_cpu, SEM_OK, NULL, 0);
		if(queue_size(semaforo->procesos)>0){
			aux_pcb_otros = queue_pop(semaforo->procesos);
			pasar_pcbBlock_ready(aux_pcb_otros->pcb->id);
		}
		txt_write_in_file(pcp_log,"SIGNAL por cpu con socket n°:");
		logear_int(pcp_log,soc_cpu);
		txt_write_in_file(pcp_log,"\n");
	}else{
		llamada_erronea(soc_cpu, SEM_INEX);
		txt_write_in_file(pcp_log,"Error en SIGNAL (semaforo inexistente) por cpu con socket n°:");
		logear_int(pcp_log,soc_cpu);
		txt_write_in_file(pcp_log,"\n");
	}

	pthread_mutex_unlock(&mutex_dicc_sem);
	destruir_men_comun(men_cpu);
}

t_cpu *get_cpu(int32_t soc_cpu){
	bool _es_soc_cpu(t_cpu *un_cpu){
		return un_cpu->soc_cpu == soc_cpu;
	}
	t_cpu *aux_cpu = list_find(lista_cpu, (void*)_es_soc_cpu);
	if (aux_cpu == NULL)
		printf("ERROR: get_cpu()\n");
	return aux_cpu;
}

void enviar_IO(int32_t soc_cpu, t_men_comun *men_cpu){
	t_men_comun *men;
	t_IO *aux_IO;

	pthread_mutex_lock(&mutex_uso_lista_cpu);
	t_cpu *aux_cpu = get_cpu_soc_cpu_remove(soc_cpu);
	pthread_mutex_unlock(&mutex_uso_lista_cpu);

	men = socket_recv_comun(soc_cpu);
	int32_t cant_unidades = atoi(men->dato);

	t_IO_espera *espera = malloc(sizeof(t_IO_espera));
	espera->id_prog = aux_cpu->id_prog_exec;
	espera->unidades = cant_unidades;

	pthread_mutex_lock(&mutex_io);

	aux_IO = dictionary_get(dicc_disp_io, men_cpu->dato);
	queue_push(aux_IO->procesos, espera);
	dictionary_put(dicc_disp_io, men_cpu->dato, aux_IO);

	actualizar_pcb_y_bloq(aux_cpu);

	sem_incre(&(aux_IO->cont_cant_proc));

	pthread_mutex_unlock(&mutex_io);

	destruir_men_comun(men);
	destruir_men_comun(men_cpu);
}

void fin_quantum(int32_t soc_cpu, t_men_comun *men_cpu){
	destruir_men_comun(men_cpu);
	t_pcb_otros *aux_pcb_otros;

	pthread_mutex_lock(&mutex_uso_lista_cpu);
	t_cpu *aux_cpu = get_cpu_soc_cpu_remove(soc_cpu);
	pthread_mutex_unlock(&mutex_uso_lista_cpu);

	// Actualizacion pcb
	aux_pcb_otros=get_pcb_otros_exec(aux_cpu->id_prog_exec);
	t_men_quantum_pcb *men_quantum_pcb = socket_recv_quantum_pcb(soc_cpu);

	actualizar_pcb(aux_pcb_otros->pcb,men_quantum_pcb->pcb);
	destruir_quantum_pcb(men_quantum_pcb);

	pthread_mutex_lock(&mutex_ready);
	queue_push(colas->cola_ready,aux_pcb_otros);
	sem_incre(&cant_ready);
	pthread_mutex_unlock(&mutex_ready);

	pthread_mutex_lock(&mutex_uso_lista_cpu);
	aux_cpu->id_prog_exec = 0;
	aux_cpu->soc_prog = 0;
	list_add(lista_cpu, aux_cpu);
	sem_incre(&cant_cpu_libres);
	pthread_mutex_unlock(&mutex_uso_lista_cpu);

	txt_write_in_file(pcp_log,"Termino un quantum del programa:");
	logear_int(pcp_log,aux_pcb_otros->pcb->id);
	txt_write_in_file(pcp_log,"\n");
}

void obtener_valor_compartida(int32_t soc_cpu, t_men_comun *men_obt_valor){
	int32_t *valor;
	char *string_valor;
	char *key = men_obt_valor->dato;

	pthread_mutex_lock(&mutex_dicc_var);

	if (dictionary_has_key(dicc_var, key)){
		valor = dictionary_get(dicc_var, key);
		string_valor = string_itoa(*valor);
		enviar_men_comun_destruir(soc_cpu ,R_OBTENER_VALOR,string_valor,string_length(string_valor)+1);
		txt_write_in_file(pcp_log,"OBTENER VALOR por cpu con socket n°:");
		logear_int(pcp_log,soc_cpu);
		txt_write_in_file(pcp_log,"\n");
	}else{
		llamada_erronea(soc_cpu, VAR_INEX);
		txt_write_in_file(pcp_log,"Error en OBTENER VALOR (var inexistente) por cpu con socket n°:");
		logear_int(pcp_log,soc_cpu);
		txt_write_in_file(pcp_log,"\n");
	}

	pthread_mutex_unlock(&mutex_dicc_var);

	destruir_men_comun(men_obt_valor);
}

void grabar_valor_compartida(int32_t soc_cpu, t_men_comun *men_gra_valor){
	int32_t *valor;
	char *key = men_gra_valor->dato;
	t_men_comun *men_valor;

	pthread_mutex_lock(&mutex_dicc_var);

	if (dictionary_has_key(dicc_var, key)){
		enviar_men_comun_destruir(soc_cpu,R_GRABAR_VALOR,NULL,0);
		men_valor = socket_recv_comun(soc_cpu);

		if((men_valor->tipo) != VALOR_ASIGNADO)
			printf("ERROR: esperaba recibir VALOR_ASIGNADO y recibi: %i",men_valor->tipo);

		valor = dictionary_remove(dicc_var,key);
		*valor = atoi(men_valor->dato);
		dictionary_put(dicc_var,key, valor);
		pthread_mutex_unlock(&mutex_dicc_var);

		txt_write_in_file(pcp_log,"GRABAR VALOR por cpu con socket n°:");
		logear_int(pcp_log,soc_cpu);
		txt_write_in_file(pcp_log,"\n");
		destruir_men_comun(men_valor);

		enviar_men_comun_destruir(soc_cpu,R_GRABAR_VALOR,NULL,0);
	}else{
		pthread_mutex_unlock(&mutex_dicc_var);
		llamada_erronea(soc_cpu, VAR_INEX);
		txt_write_in_file(pcp_log,"Error en GRABAR VALOR (var inexistente) por cpu con socket n°:");
		logear_int(pcp_log,soc_cpu);
		txt_write_in_file(pcp_log,"\n");
	}

	destruir_men_comun(men_gra_valor);
}

void llamada_erronea(int32_t soc_cpu,int32_t tipo_error){
	pthread_mutex_lock(&mutex_uso_lista_cpu);
	t_cpu *aux_cpu = get_cpu_soc_cpu_remove(soc_cpu);
	pthread_mutex_unlock(&mutex_uso_lista_cpu);

	t_pcb_otros *aux_pcb= get_pcb_otros_exec(aux_cpu->id_prog_exec);

	aux_pcb->tipo_fin_ejecucion=tipo_error;

	if (aux_pcb == NULL)
		printf("ERROR: llamada_erronea()\n");

	pasar_pcb_exit(aux_pcb);

	enviar_men_comun_destruir(soc_cpu,tipo_error,NULL,0);

	pthread_mutex_lock(&mutex_uso_lista_cpu);
	aux_cpu->id_prog_exec = 0;
	aux_cpu->soc_prog = 0;
	list_add(lista_cpu, aux_cpu);
	sem_incre(&cant_cpu_libres);
	pthread_mutex_unlock(&mutex_uso_lista_cpu);
}

void imprimir_valor(int32_t soc_cpu, t_men_comun *men_imp_valor){
	t_men_comun *men_id_prog;
	men_id_prog = socket_recv_comun(soc_cpu);

	if (men_id_prog->tipo != ID_PROG)
		printf("ERROR: esperaba el id de un programa y recibi: %i\n",men_id_prog->tipo);

	pthread_mutex_lock(&mutex_uso_lista_cpu);
	t_cpu *aux_cpu = get_cpu(soc_cpu);
	socket_send_comun(aux_cpu->soc_prog, men_imp_valor);
	pthread_mutex_unlock(&mutex_uso_lista_cpu);

	txt_write_in_file(pcp_log,"IMPRIMIR_VALOR por cpu con socket");
	logear_int(pcp_log,soc_cpu);
	txt_write_in_file(pcp_log,"al programa con id ");
	logear_int(pcp_log,aux_cpu->id_prog_exec);
	txt_write_in_file(pcp_log,".Valor impreso: ");
	logear_int(pcp_log,atoi(men_imp_valor->dato));
	txt_write_in_file(pcp_log,"\n");

	enviar_men_comun_destruir(soc_cpu, R_IMPRIMIR,NULL,0);
}

void imprimir_texto(int32_t soc, t_men_comun *men_imp_texto){
	t_men_comun *men_id_prog;

	men_id_prog = socket_recv_comun(soc);

	if (men_id_prog->tipo != ID_PROG)
		printf("ERROR: esperaba el id de un programa y recibi: %i\n",men_id_prog->tipo);

	pthread_mutex_lock(&mutex_uso_lista_cpu);
	t_cpu *aux_cpu = get_cpu(soc);
	socket_send_comun(aux_cpu->soc_prog, men_imp_texto);
	pthread_mutex_unlock(&mutex_uso_lista_cpu);

	txt_write_in_file(pcp_log,"IMPRIMIR_TEXTO por cpu con socket n°");
	logear_int(pcp_log,soc);
	txt_write_in_file(pcp_log," al programa con id:");
	logear_int(pcp_log,aux_cpu->id_prog_exec);
	txt_write_in_file(pcp_log,"\n");

	enviar_men_comun_destruir(soc, R_IMPRIMIR,NULL,0);
}

void fin_ejecucion(int32_t tipo,int32_t soc_cpu, t_men_comun *men_cpu){
	destruir_men_comun(men_cpu);

	pthread_mutex_lock(&mutex_uso_lista_cpu);
	t_cpu *aux_cpu = get_cpu_soc_cpu_remove(soc_cpu);
	pthread_mutex_unlock(&mutex_uso_lista_cpu);

	t_pcb_otros *aux_pcb_otros = get_pcb_otros_exec(aux_cpu->id_prog_exec);

	txt_write_in_file(pcp_log,"Termino la ejecucion del programa:");
	logear_int(pcp_log,aux_pcb_otros->pcb->id);
	txt_write_in_file(pcp_log,"\n");

	if(tipo==SEGMEN_FAULT){
		txt_write_in_file(pcp_log,"Error por segmentation fault del programa:");
		logear_int(pcp_log,aux_pcb_otros->pcb->id);
	}

	aux_pcb_otros->tipo_fin_ejecucion = tipo;

	if (aux_pcb_otros == NULL)
			printf("ERROR: fin_ejecucion()\n");

	pasar_pcb_exit(aux_pcb_otros);

	aux_cpu->id_prog_exec = 0;

	pthread_mutex_lock(&mutex_uso_lista_cpu);
	aux_cpu->id_prog_exec = 0;
	aux_cpu->soc_prog = 0;
	list_add(lista_cpu, aux_cpu);
	sem_incre(&cant_cpu_libres);
	pthread_mutex_unlock(&mutex_uso_lista_cpu);
}

void pasar_pcbBlock_ready(int32_t id_pcb){
	int32_t i = 0;

	pthread_mutex_lock(&mutex_block);
	int32_t tamanio_cola_block = queue_size(colas->cola_block);

	for (i=0; i<tamanio_cola_block; i++){
		t_pcb_otros *pcb_aux;
		pcb_aux =queue_pop(colas->cola_block);

		if (pcb_aux->pcb->id==id_pcb){
			pthread_mutex_lock(&mutex_ready);
			queue_push(colas->cola_ready, pcb_aux);
			sem_incre(&cant_ready);
			pthread_mutex_unlock(&mutex_ready);
			pthread_mutex_unlock(&mutex_block);
			return;
		}else {
			queue_push(colas->cola_block,pcb_aux);
		}
	}
	printf("ERROR: No se ha encontrado el pcb del proc IDº%i en la cola block\n",id_pcb);
	pthread_mutex_unlock(&mutex_block);
}

void pasar_pcb_exit(t_pcb_otros *pcb){
	if (pcb == NULL)
		return;
	pthread_mutex_lock(&mutex_exit);
	queue_push(colas->cola_exit,pcb);
	sem_incre(&cant_exit);
	pthread_mutex_unlock(&mutex_exit);
	sem_incre(&cant_multiprog);
}

//Para buscar un pcb por su socket, xq no se sabe en que cola esta(cuando se cae la conexion), lo encuentra y lo pasa a exit
void mover_pcb_exit(int32_t soc_prog){
	t_pcb_otros *aux_pcb;

	pthread_mutex_lock(&mutex_new);
	pthread_mutex_lock(&mutex_ready);
	pthread_mutex_lock(&mutex_block);
	pthread_mutex_lock(&mutex_exec);

	aux_pcb = buscar_pcb(colas->cola_new, soc_prog);
	pasar_pcb_exit(aux_pcb);
	aux_pcb = buscar_pcb(colas->cola_ready, soc_prog);
	pasar_pcb_exit(aux_pcb);
	aux_pcb = buscar_pcb(colas->cola_block, soc_prog);
	pasar_pcb_exit(aux_pcb);
	aux_pcb = buscar_pcb(colas->cola_exec, soc_prog);
	pasar_pcb_exit(aux_pcb);

	pthread_mutex_unlock(&mutex_new);
	pthread_mutex_unlock(&mutex_ready);
	pthread_mutex_unlock(&mutex_block);
	pthread_mutex_unlock(&mutex_exec);
}

t_pcb_otros *buscar_pcb(t_queue *cola, int32_t soc_prog){
	int32_t i;
	t_pcb_otros *ret;

	for(i=0; i < queue_size(cola) ;i++){
		ret = queue_pop(cola);
		if (ret->n_socket == soc_prog)
			return ret;
		queue_push(cola, ret);
	}
	return NULL;
}

t_pcb_otros *actualizar_pcb_y_bloq(t_cpu *aux_cpu){
	t_pcb_otros *aux_pcb_otros=get_pcb_otros_exec(aux_cpu->id_prog_exec);
	t_pcb *pcb_recibido = socket_recv_quantum_pcb(aux_cpu->soc_cpu)->pcb;
	actualizar_pcb(aux_pcb_otros->pcb,pcb_recibido);

	pthread_mutex_lock(&mutex_uso_lista_cpu);
	aux_cpu->id_prog_exec = 0;
	aux_cpu->soc_prog = 0;
	list_add(lista_cpu, aux_cpu);
	sem_incre(&cant_cpu_libres);
	pthread_mutex_unlock(&mutex_uso_lista_cpu);

	pthread_mutex_lock(&mutex_block);
	queue_push(colas->cola_block,aux_pcb_otros);
	pthread_mutex_unlock(&mutex_block);

	return aux_pcb_otros;
}

t_pcb_otros *get_pcb_otros_exec(int32_t id_proc){
	t_pcb_otros *aux_pcb_otros;
	int32_t i;

	pthread_mutex_lock(&mutex_exec);
	int32_t tam = queue_size(colas->cola_exec);
	for(i=0 ;i < tam ;i++){
		aux_pcb_otros = queue_pop(colas->cola_exec);
		if (id_proc == aux_pcb_otros->pcb->id){
			pthread_mutex_unlock(&mutex_exec);
			return aux_pcb_otros;
		}
		queue_push(colas->cola_exec, aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_exec);
	printf("ERROR en get_pcb_otros_exec\n");
	return NULL;
}

void *manejador_exit(){
	t_pcb_otros *aux_pcb_otros;

	while(quit_sistema){
		sem_decre(&cant_exit);
		pthread_mutex_lock(&mutex_exit);
		aux_pcb_otros = queue_pop(colas->cola_exit);
		pthread_mutex_unlock(&mutex_exit);
		if (aux_pcb_otros == NULL){
			printf("ERROR en manejador_exit()\n");
			continue;
		}
		if (aux_pcb_otros->tipo_fin_ejecucion != -1)
			enviar_men_comun_destruir(aux_pcb_otros->n_socket, aux_pcb_otros->tipo_fin_ejecucion,NULL,0);
		umv_destrui_pcb(aux_pcb_otros->pcb->id);
		free(aux_pcb_otros->pcb);
		free(aux_pcb_otros);
	}
	return NULL;
}

void umv_destrui_pcb(int32_t id_pcb){
	enviar_umv_mem_seg_destruir(soc_umv, DESTR_SEGS, id_pcb, 0);
	//recibo respuesta para sincronizar
	t_men_comun *men_resp = socket_recv_comun(soc_umv);
	if (men_resp->tipo != SINCRO_OK)
		printf("ERROR se esperaba, de la UMV, un tipo de mensaje SINCRO_OK y se recibio:%i\n",men_resp->tipo);
	printf("Segmentos del programa:%i destruidos\n", id_pcb);
	destruir_men_comun(men_resp);
}

void *manejador_new_ready(){
	t_pcb_otros *aux_pcb_otros;
	while(quit_sistema){
		sem_decre(&cant_new);// Se fija que haya algun programa en la cola de New
		sem_decre(&cant_multiprog); // Se fija que no se supere el grado de multiprogramación
		pthread_mutex_lock(&mutex_new);
		pthread_mutex_lock(&mutex_ready);
		aux_pcb_otros = get_peso_min();
		if (aux_pcb_otros == NULL){
			printf("ERROR en manejador_new_ready()\n");
			pthread_mutex_unlock(&mutex_new);
			pthread_mutex_unlock(&mutex_ready);
			continue;
		}
		queue_push(colas->cola_ready, aux_pcb_otros);
		sem_incre(&cant_ready);
		pthread_mutex_unlock(&mutex_new);
		pthread_mutex_unlock(&mutex_ready);
	}
	return NULL;
}

t_pcb_otros *get_peso_min(){
	int32_t i, peso_min = -1;
	t_pcb_otros *aux;
	int32_t tam_cola = queue_size(colas->cola_new);

	for (i=0;i < tam_cola;i++){
		aux = queue_pop(colas->cola_new);
		if(peso_min < aux->peso)
			peso_min = aux->peso;
		queue_push(colas->cola_new,aux);
	}
	for (i=0;i < tam_cola;i++){
		aux = queue_pop(colas->cola_new);
		if (peso_min == aux->peso)
			return aux;
		queue_push(colas->cola_new,aux);
	}
	printf("ERROR: get_peso_min()\n");
	return NULL;
}

t_cpu *get_cpu_libre_remove(){
	bool _esta_libre(t_cpu *un_cpu){
		return un_cpu->id_prog_exec == 0;
	}
	return list_remove_by_condition(lista_cpu, (void*)_esta_libre);
}

void *manejador_ready_exec(){
	t_pcb_otros *aux_pcb_otros;
	t_cpu *aux_cpu;

	while(quit_sistema){
		sem_decre(&cant_ready);
		sem_decre(&cant_cpu_libres);

		pthread_mutex_lock(&mutex_ready);
		pthread_mutex_lock(&mutex_uso_lista_cpu);

		aux_cpu = get_cpu_libre_remove();

		aux_pcb_otros = queue_pop(colas->cola_ready);

		if(aux_pcb_otros == NULL){
			printf("ERROR manejador_ready_exec()\n");
			pthread_mutex_unlock(&mutex_ready);
			pthread_mutex_unlock(&mutex_uso_lista_cpu);
			continue;
		}

		enviar_cpu_pcb_destruir(aux_cpu->soc_cpu,aux_pcb_otros->pcb);

		aux_cpu->id_prog_exec = aux_pcb_otros->pcb->id;
		aux_cpu->soc_prog = aux_pcb_otros->n_socket;

		list_add(lista_cpu, aux_cpu);

		queue_push(colas->cola_exec, aux_pcb_otros);

		pthread_mutex_unlock(&mutex_ready);
		pthread_mutex_unlock(&mutex_uso_lista_cpu);
	}
	return NULL;
}

void recibir_resp_escrbir_seg(){//uso esto para q espere y no ponga en la cola de new un proceso con los segmentos vacios
	t_men_comun *men = socket_recv_comun(soc_umv);
	if(men->tipo != MEN_ALM_OK)
		printf("ERROR se esperaba un tipo de dato MEN_ALM_OK y se recibio:%i",men->tipo);
	destruir_men_comun(men);
}

t_pcb *crear_pcb_escribir_seg_UMV(t_men_comun *men_cod_prog ,t_resp_sol_mem *resp_sol){
	t_metadata_program *metadata_program = metadata_desde_literal(men_cod_prog->dato);

	// Escribe el segmento de codigo
	enviar_umv_mem_seg_destruir(soc_umv , ESCRIBIR_SEG, _CONTADOR_PROGRAMA, 0);
	enviar_men_comun_destruir(soc_umv , CODIGO_SCRIPT,men_cod_prog->dato,men_cod_prog->tam_dato);
	recibir_resp_escrbir_seg();

	// Escribe el segmento de indice de etiquetas
	enviar_umv_mem_seg_destruir(soc_umv , ESCRIBIR_SEG, _CONTADOR_PROGRAMA, 0);
	char *etis = metadata_program->etiquetas;
	enviar_men_comun_destruir(soc_umv ,IND_ETI_FUNC,etis,metadata_program->etiquetas_size);
	recibir_resp_escrbir_seg();

	// Escribe el segmento de indice de codigo
	enviar_umv_mem_seg_destruir(soc_umv , ESCRIBIR_SEG, _CONTADOR_PROGRAMA, 0);
	int32_t tam_ind_cod = metadata_program->instrucciones_size*8;
	enviar_men_comun_destruir(soc_umv ,IND_COD,(void *)metadata_program->instrucciones_serializado,tam_ind_cod);
	recibir_resp_escrbir_seg();

	t_pcb *pcb = malloc(sizeof(t_pcb));
	pcb->id = _CONTADOR_PROGRAMA;
	pcb->dir_seg_codigo = resp_sol->dir_seg_codigo;
	pcb->dir_seg_stack = resp_sol->dir_seg_stack;
	pcb->dir_cont_actual = resp_sol->dir_seg_stack;
	pcb->dir_indice_codigo = resp_sol->dir_indice_codigo;
	pcb->dir_indice_etiquetas = resp_sol->dir_indice_etiquetas;
	pcb->program_counter = metadata_program->instruccion_inicio;
	pcb->cant_var_cont_actual = 0;
	pcb->tam_indice_etiquetas = metadata_program->etiquetas_size;

	return pcb;
}

int32_t gestionar_resp_sol_mem(){
	int32_t ret = -1;
	t_men_comun *resp_men_cod = socket_recv_comun(soc_umv);
	if (resp_men_cod->tipo == MEM_OVERLOAD){
		destruir_men_comun(resp_men_cod);
		return ret;
	}
	if((resp_men_cod->tipo != RESP_MEM_SEG_COD)&&(resp_men_cod->tipo != RESP_MEM_IND_ETI)&&(resp_men_cod->tipo != RESP_MEM_IND_COD)&&(resp_men_cod->tipo != RESP_MEM_SEG_STACK))
		printf("ERROR: se esperaba una respuesta de pedido de memoria y obtube un %i\n",resp_men_cod->tipo);
	ret = atoi(resp_men_cod->dato);
	destruir_men_comun(resp_men_cod);
	return ret;
}

t_resp_sol_mem * solicitar_mem(t_men_comun *men_cod_prog){
	t_resp_sol_mem *resp_sol = malloc(sizeof(t_resp_sol_mem));
	int32_t tam = 0, resp;

	//pido mem para el codigo del script
	enviar_umv_mem_seg_destruir(soc_umv , PED_MEM_SEG_COD,_CONTADOR_PROGRAMA,men_cod_prog->tam_dato);
	resp = gestionar_resp_sol_mem();
	if(resp == -1)
		return NULL;
	resp_sol->dir_seg_codigo = resp;

	//pido mem para el indice de etiquetas y funciones
	t_metadata_program* metadata_program = metadata_desde_literal(men_cod_prog->dato);
	tam = (metadata_program->etiquetas_size);
	enviar_umv_mem_seg_destruir(soc_umv ,  PED_MEM_IND_ETI , _CONTADOR_PROGRAMA, tam);
	resp = gestionar_resp_sol_mem();
	if(resp == -1)
		return NULL;
	resp_sol->dir_indice_etiquetas= resp;

	//pido mem para el indice de codigo
	tam = (metadata_program->instrucciones_size*8);
	enviar_umv_mem_seg_destruir(soc_umv ,  PED_MEM_IND_COD , _CONTADOR_PROGRAMA, tam);
	resp = gestionar_resp_sol_mem();
	if(resp == -1)
		return NULL;
	resp_sol->dir_indice_codigo = resp;

	//pido mem para el stack
	enviar_umv_mem_seg_destruir(soc_umv , PED_MEM_SEG_STACK , _CONTADOR_PROGRAMA, _TAMANIO_STACK);

	resp = gestionar_resp_sol_mem();
	if(resp == -1)
		return NULL;
	resp_sol->dir_seg_stack = resp;

	return resp_sol;
}

int32_t calcular_peso(t_men_comun *men_cod_prog){
	t_metadata_program *metadata_program = metadata_desde_literal(men_cod_prog->dato);
	return (5 * (metadata_program->cantidad_de_etiquetas) + 3 * (metadata_program->cantidad_de_funciones) + (metadata_program->instrucciones_size));
}

void *manejador_IO(t_IO *io){
	int32_t i;
	t_IO_espera *proceso;

	while(quit_sistema){
		sem_decre(&(io->cont_cant_proc));
		proceso = queue_pop(io->procesos);
		for(i=0; i<proceso->unidades;i++)
			usleep(io->io_sleep*1000);
		pasar_pcbBlock_ready(proceso->id_prog);
	}
	return NULL;
}

void enviar_cpu_pcb_destruir(int32_t soc,t_pcb *pcb){
	t_men_quantum_pcb * men_pcb= crear_men_quantum_pcb(PCB_Y_QUANTUM,_QUANTUM_MAX,pcb);
	socket_send_quantum_pcb(soc,men_pcb);
	destruir_quantum_pcb(men_pcb);
}

void actualizar_pcb(t_pcb *pcb, t_pcb *pcb_actualizado){
	pcb->cant_var_cont_actual = pcb_actualizado->cant_var_cont_actual;
	pcb->dir_cont_actual = pcb_actualizado->dir_cont_actual;
	pcb->program_counter = pcb_actualizado->program_counter;
}

void imp_variables_compartidas(){
	void _imp_var_comp(char* key, int32_t *valor){
		printf("	ID:%s ,Valor:%i\n", key, *valor);
	}
	pthread_mutex_lock(&mutex_dicc_var);
	dictionary_iterator(dicc_var, (void *)_imp_var_comp);
	pthread_mutex_unlock(&mutex_dicc_var);
}

void imp_semaforos(){
	void _imp_sem(char* key, t_semaforo *un_sem){
		printf("	ID:%s ,Valor:%i\n", key, un_sem->valor);
	}
	pthread_mutex_lock(&mutex_dicc_sem);
	dictionary_iterator(dicc_sem, (void *)_imp_sem);
	pthread_mutex_unlock(&mutex_dicc_sem);
}

t_datos_config *levantar_config(){
	void _imp_disp(char *key, t_IO *io){
		printf("	ID:%s ,Sleep:%i\n",key,io->io_sleep);
	}
	int i;

	t_config *diccionario_config = config_create("./kernel/kernel_config");
	t_datos_config *ret = malloc(sizeof(t_datos_config));
	PUERTO_PROG = config_get_string_value( diccionario_config, "Puerto_prog");
	PUERTO_CPU = config_get_string_value( diccionario_config, "Puerto_CPU");
	ret->ip_umv = config_get_string_value( diccionario_config, "IP_UMV");
	ret->puerto_umv = config_get_string_value( diccionario_config, "Puerto_umv");
	char **id_ios = config_get_array_value( diccionario_config, "ID_HIO");
	char **io_sleeps = config_get_array_value( diccionario_config, "HIO");

	for (i=0; id_ios[i] != '\0'; i++){
		t_IO *new_IO = malloc(sizeof(t_IO));
		crear_cont(&(new_IO->cont_cant_proc) , 0);
		new_IO->io_sleep = atoi(io_sleeps[i]);
		new_IO->procesos = queue_create();
		dictionary_put(dicc_disp_io, id_ios[i],new_IO);
		pthread_create(&(new_IO->hilo), NULL, (void *)manejador_IO, (void*)new_IO);
	}

	free(id_ios);
	free(io_sleeps);

	char **aux_id_sem = config_get_array_value( diccionario_config, "SEMAFOROS");
	char **aux_valor = config_get_array_value( diccionario_config, "VALOR_SEMAFORO");

	for(i=0;aux_id_sem[i] != '\0'; i++){
		t_semaforo *new_sem = malloc(sizeof(t_semaforo));
		new_sem->valor = atoi(aux_valor[i]);
		new_sem->procesos = queue_create();
		dictionary_put(dicc_sem, aux_id_sem[i],new_sem);
	}

	free(aux_id_sem);
	free(aux_valor);

	ret->multiprogramacion = config_get_int_value( diccionario_config, "MULTIPROGRAMACION");
	_QUANTUM_MAX = config_get_int_value( diccionario_config, "QUANTUM");
	_RETARDO = config_get_int_value( diccionario_config, "RETARDO");
	_TAMANIO_STACK = config_get_int_value( diccionario_config, "TAMANIO_STACK");
	char **aux_var_glob = config_get_array_value( diccionario_config, "VARIABLES_GLOBALES");

	for (i=0; aux_var_glob[i] != '\0'; i++){
		int32_t *valor_ini = malloc(sizeof(int32_t));
		*valor_ini = 0;
		dictionary_put(dicc_var, aux_var_glob[i],valor_ini);
	}
	free(aux_var_glob);

	printf("------Archivo Config------------------------------\n");
	printf("Puerto Proc-Prog = %s\n", PUERTO_PROG);
	printf("Puerto CPU = %s\n", PUERTO_CPU);
	printf("UMV\n");
	printf("	IP     = %s\n", ret->ip_umv);
	printf("	Puerto = %s\n", ret->puerto_umv);
	printf("Entrada/Salida\n");

	dictionary_iterator(dicc_disp_io, (void *)_imp_disp);

	printf("\nSemaforos\n");

	imp_semaforos();

	printf("Varias\n");
	printf("	Multiprogramacion = %i\n", ret->multiprogramacion);
	printf("	Quantum = %i\n", _QUANTUM_MAX);
	printf("	Retardo =  %i\n", _RETARDO);
	printf("	Tamanio Stack = %i\n", _TAMANIO_STACK);
	printf("Variables Globales\n");
	imp_variables_compartidas();
	printf("--------------------------------------------------\n");

	return ret;
}

void handshake_cpu(int32_t soc){
	t_men_comun *men_hs = socket_recv_comun(soc);

	if(men_hs->tipo != HS_CPU)
		printf("ERROR HANDSHAKE CPU = %i\n",men_hs->tipo);
	destruir_men_comun(men_hs);

	enviar_men_comun_destruir(soc,HS_KERNEL,NULL,0);
}

void handshake_prog(int32_t soc){
	t_men_comun *men_hs = socket_recv_comun(soc);

	if(men_hs->tipo != HS_PROG){
		printf("ERROR se esperaba HS_PROG y se recibio %i\n",men_hs->tipo);
		txt_write_in_file(plp_log,"Error en el handshake con el programa \n");
	}
	destruir_men_comun(men_hs);

	enviar_men_comun_destruir(soc,HS_KERNEL,NULL,0);
}

void handshake_umv(char *ip_umv, char *puerto_umv){
	soc_umv = socket_crear_client(puerto_umv,ip_umv);

	enviar_men_comun_destruir(soc_umv,HS_KERNEL,NULL,0);

	t_men_comun *men_hs = socket_recv_comun(soc_umv);
	if(men_hs->tipo != HS_UMV){
		printf("ERROR HANDSHAKE");
		txt_write_in_file(plp_log,"Error en el handshake con la UMV \n");
	}
	printf("UMV conectada\n");
	txt_write_in_file(plp_log,"Conectado a la UMV \n");
	destruir_men_comun(men_hs);
}

void menu_imp(){
	char opcion;
	int new_quantum;

	while(quit_sistema){
		do {
			scanf("%c", &opcion);
		} while (opcion != 'c' && opcion != 'v' && opcion != 's' && opcion != 'Q');
		switch (opcion) {
		case 'c':
			imp_colas();
			break;
		case 'v':
			printf("Variables compartidas:\n");
			imp_variables_compartidas();
			break;
		case 's':
			printf("Semaforos:\n");
			imp_semaforos();
			break;
		case 'Q':
			scanf("%i",&new_quantum);
			_QUANTUM_MAX = new_quantum;
			break;
		}
	}
}

void imprimir_cola(t_queue *cola){
	int32_t i;
	t_pcb_otros *aux;

	if (queue_is_empty(cola)){
		printf("*************COLA VACIA********************");
	}else {
		for(i=0; i < queue_size(cola) ;i++){
			aux = queue_pop(cola);
			printf("%i/%i - ",aux->pcb->id,aux->peso);
			queue_push(cola, aux);
		}
	}
	printf("\n");
}

void imprimir_lista_cpu(){
	void _imp_cpu(t_cpu *un_cpu){
		printf("%i/%i/%i - ",un_cpu->soc_cpu, un_cpu->soc_prog, un_cpu->id_prog_exec);
	}
	if (list_size(lista_cpu) == 0)
		printf("*****LISTA VACIA*******************");
	else
		list_iterate(lista_cpu, (void*)_imp_cpu);
	printf("\n");
}

void imp_colas(){
		pthread_mutex_lock(&mutex_new);
		pthread_mutex_lock(&mutex_ready);
		pthread_mutex_lock(&mutex_block);
		pthread_mutex_lock(&mutex_exec);
		pthread_mutex_lock(&mutex_exit);
		pthread_mutex_lock(&mutex_uso_lista_cpu);

		printf("--COLA-NEW---------------------------------------------------------\n");
		printf("|PID/PESO= ");
		imprimir_cola(colas->cola_new);

		printf("--COLA-READY-------------------------------------------------------\n");
		printf("|PID/PESO= ");
		imprimir_cola(colas->cola_ready);

		printf("--COLA-BLOCK-------------------------------------------------------\n");
		printf("|PID/PESO= ");
		imprimir_cola(colas->cola_block);

		printf("--COLA-EXEC--------------------------------------------------------\n");
		printf("|PID/PESO= ");
		imprimir_cola(colas->cola_exec);

		printf("--COLA-EXIT--------------------------------------------------------\n");
		printf("|PID/PESO= ");
		imprimir_cola(colas->cola_exit);

		printf("--LISTA-CPU--------------------------------------------------------\n");
		printf("|S_CPU/S_PROG/PID= ");
		imprimir_lista_cpu();
		printf("-------------------------------------------------------------------\n");

		pthread_mutex_unlock(&mutex_new);
		pthread_mutex_unlock(&mutex_ready);
		pthread_mutex_unlock(&mutex_block);
		pthread_mutex_unlock(&mutex_exec);
		pthread_mutex_unlock(&mutex_exit);
		pthread_mutex_unlock(&mutex_uso_lista_cpu);
}

void crear_cont(sem_t *sem ,int32_t val_ini){
	if(sem_init(sem, 0, val_ini)== -1){
		perror("No se puede crear el semáforo");
		exit(1);
	}
}

void sem_decre(sem_t *sem){
	if (sem_wait(sem) == -1){
		perror("ERROR sem decrementar");
		exit(1);
	}
}

void sem_incre(sem_t *sem){
	if (sem_post(sem) == -1){
		perror("ERROR sem decrementar");
		exit(1);
	}
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

t_cpu *get_cpu_soc_cpu_remove(int32_t soc_cpu){
	bool _es_soc_cpu(t_cpu *un_cpu){
		return un_cpu->soc_cpu == soc_cpu;
	}
	t_cpu *aux_cpu = list_remove_by_condition(lista_cpu, (void*)_es_soc_cpu);
	if (aux_cpu == NULL)
		printf("ERROR: get_cpu_soc_cpu_remove()\n");
	return aux_cpu;
}
