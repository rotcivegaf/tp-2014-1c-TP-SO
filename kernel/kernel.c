#include "kernel.h"

int32_t soc_umv;
t_colas *colas;  //colas de los cinco estados de los procesos
t_queue *cola_cpu; //cola donde tengo los cpu conectados
pthread_mutex_t mutex_new = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_block = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_exec = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_exit = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_miltiprog = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_uso_cola_cpu= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ready_vacia= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_cpu_vacia= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_dispositivos_io= PTHREAD_MUTEX_INITIALIZER;
sem_t buff_multiprog, libre_multiprog, cont_exit;
int32_t quit_sistema = 1;
t_list *dispositivos_IO;
t_dictionary *diccionario_variables;
t_queue *cola_semaforos;
FILE *plp_log;
FILE *pcp_log;

int main(void){
	diccionario_variables = dictionary_create(); //todo ver donde destruir esto, porque da memory leak de 96 bytes!
	cola_semaforos = queue_create();
	t_datos_config *diccionario_config = levantar_config();

	plp_log = txt_open_for_append("./kernel/logs/plp.log");
	txt_write_in_file(plp_log,"---------------------Nueva ejecucion--------------------------------------------------------------------------------------------\n");

	pcp_log = txt_open_for_append("./kernel/logs/pcp.log");
	txt_write_in_file(pcp_log,"---------------------Nueva ejecucion--------------------------------------------------------------------------------------------\n");

	//Inicializacion semaforos
	crear_cont(&cont_exit , 0);
	crear_cont(&buff_multiprog , 0); //cantidad de procesos entre ready y exec
	crear_cont(&libre_multiprog , diccionario_config->multiprogramacion); //cantidad de procesos que todavia entran

	//Creacion de colas
	cola_cpu = queue_create();
	colas = malloc(sizeof(t_colas));
	colas->cola_new = queue_create();
	colas->cola_ready = queue_create();
	colas->cola_block = queue_create();
	colas->cola_exec = queue_create();
	colas->cola_exit = queue_create();

	//Lista dispositivos
	dispositivos_IO = list_create();

	//parametros plp pcp
	t_param_plp *param_plp = ini_pram_plp(diccionario_config);
	t_param_pcp *param_pcp = ini_pram_pcp(diccionario_config);

	//Creacion de hilos
	pthread_t hilo_imp_colas, hilo_plp, hilo_pcp;
	pthread_create(&hilo_imp_colas, NULL, imp_colas , NULL);
	pthread_create(&hilo_plp, NULL, plp, (void *)param_plp);
	pthread_create(&hilo_pcp, NULL, pcp, (void *)param_pcp);
	pthread_join(hilo_plp, NULL);
	pthread_join(hilo_pcp, NULL);

	//todo todos estos free y demas, es como si no estuvieran para el valgrind..
	free(param_plp->puerto_prog);
	free(param_plp->puerto_umv);
	free(param_plp->ip_umv);
	free(param_plp);
	free(param_pcp->puerto_cpu);
	free(param_pcp->semaforos);
	free(param_pcp->valor_semaforos);
	free(param_pcp->variables_globales);
	queue_destroy(param_pcp->cola_IO);
	queue_destroy(colas->cola_block);
	queue_destroy(colas->cola_exec);
	queue_destroy(colas->cola_exit);
	queue_destroy(colas->cola_new);
	queue_destroy(colas->cola_ready);
	queue_destroy(cola_cpu);
	list_destroy(dispositivos_IO);
	return EXIT_SUCCESS;
}

void *plp(t_param_plp *param_plp){

	//Creacion del hilo que maneja el pasaje de pcbs de new a ready
	pthread_t hilo_new_ready;
	t_param_new_ready *param_new_ready = malloc(sizeof(t_param_new_ready));
	param_new_ready->multiprogramacion = param_plp->max_multiprogramacion;
	pthread_create(&hilo_new_ready, NULL, manejador_new_ready, (void *)param_new_ready);

	int32_t contador_prog = 0;
	fd_set master;   // conjunto maestro de sockets
	fd_set read_fds; // conjunto temporal de sockets para select()
	int32_t fdmax;   // socket con el mayor valor

	//Conecta con la umv
	handshake_umv(param_plp->ip_umv, param_plp->puerto_umv);

	//Abre un server para escuchar las conexiones de los programas
	int32_t listener_prog = socket_crear_server(param_plp->puerto_prog);
	txt_write_in_file(plp_log,"Escuchando conexiones entrantes de programas \n");
    int32_t prog_new_fd;

	FD_ZERO(&master);    // borra los conjuntos maestro y temporal de sockets
	FD_ZERO(&read_fds);
	FD_SET(listener_prog, &master);

	fdmax = listener_prog; // por ahora es éste

	int32_t i;

	while(quit_sistema){

		read_fds = master; // Copia el conjunto maestro al temporal

		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			txt_write_in_file(plp_log,"Error en el select \n");
			perror("PLP-select");
			exit(1);
		}

		for(i = 3; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // Si hay datos entrantes en el socket i

				if (i == listener_prog) { //Si i es el socket que espera conexiones, es porque hay un nuevo prog

					prog_new_fd = socket_accept(listener_prog);
					handshake_prog(prog_new_fd);

					txt_write_in_file(plp_log,"Se acepto la conexion de un nuevo programa con socket n°:");
					logear_int(plp_log,prog_new_fd);
					txt_write_in_file(plp_log,"\n");

					FD_SET(prog_new_fd, &master);

					if (prog_new_fd > fdmax) // Actualiza el socket maximo
						fdmax = prog_new_fd;

				}else{

					// Sino, es el socket de un programa ya conectado que manda datos
					t_men_comun *men_cod_prog = socket_recv_comun(i);

					if (men_cod_prog->tipo == CONEC_CERRADA) {
						txt_write_in_file(plp_log,"Cerro la conexion el programa con socket n°:");
						logear_int(plp_log,i);
						txt_write_in_file(plp_log,"\n");
						printf("PLP-select: Prog desconectado n°socket %d\n", i);
						mover_pcb_exit(i);
						FD_CLR(i, &master); // Elimina al socket del conjunto maestro
						destruir_men_comun(men_cod_prog);
						break;
					}

					printf("PLP-select: nuevo prog con socket n°%i\n", i);
					txt_write_in_file(plp_log,"Nuevo programa con socket n°:");
					logear_int(plp_log,i);
					txt_write_in_file(plp_log,"\n");

					contador_prog++;

					//Pide mem para el prog
					t_resp_sol_mem *resp_sol = solicitar_mem(men_cod_prog, param_plp->tam_stack,contador_prog);

					if (resp_sol->memoria_insuficiente == MEM_OVERLOAD){
						txt_write_in_file(plp_log,"Memoria insuficiente para el programa con socket n°:");
						logear_int(plp_log,i);
						txt_write_in_file(plp_log,"\n");

						//Avisa al programa que no hay memoria
						char *aux_sring = string_itoa(contador_prog);
						enviar_men_comun_destruir(i,MEM_OVERLOAD,aux_sring,string_length(aux_sring));
						free(aux_sring);
						free(resp_sol);
					}else{

						//Crea el pcb y lo pone en new
						t_pcb_otros *pcb_otros = malloc(sizeof(t_pcb_otros));
						pcb_otros->n_socket = i;
						pcb_otros->pcb = crear_pcb_escribir_seg_UMV(men_cod_prog,resp_sol, contador_prog);
						pcb_otros->peso = calcular_peso(men_cod_prog);
						pcb_otros->tipo_fin_ejecucion = -1;
						pthread_mutex_lock(&mutex_new);
						queue_push(colas->cola_new,pcb_otros);
						pthread_mutex_unlock(&mutex_new);
						pthread_mutex_unlock(&mutex_miltiprog);

						txt_write_in_file(plp_log,"Se creo el pcb para el programa con socket n°:");
						logear_int(plp_log,i);
						txt_write_in_file(plp_log," , su id es:");
						logear_int(plp_log,pcb_otros->pcb->id);
						txt_write_in_file(plp_log,"\n");
					}
					destruir_men_comun(men_cod_prog);
				}
			}
		}
	sleep(param_plp->retardo);
	}
	return NULL;
}

void *pcp(t_param_pcp *param_pcp){
	int32_t i,j, fdmax, cpu_new_fd,tam,encontre=0;
	fd_set master, read_fds;
	t_cpu *aux_cpu;
	t_pcb_otros *aux_pcb_otros;
	t_men_comun *men_id_prog;
	t_men_comun *men_grab_valor;
	t_semaforo *semaforo;
	int32_t *valor;
	char *string_valor;

	// Crea el hilo que pasa los pcb de ready a exec
	pthread_t hilo_ready_exec;
	t_param_ready_exec *param_ready_exec = malloc(sizeof(t_param_ready_exec));
	param_ready_exec->quantum = param_pcp->quantum;
	pthread_create(&hilo_ready_exec, NULL, manejador_ready_exec, (void *)param_ready_exec);

	// Crea el hilo que maneja las llegadas de pcbs a exit
	pthread_t hilo_exit;
	pthread_create(&hilo_exit, NULL, manejador_exit, NULL);

	int32_t listener_cpu = socket_crear_server(param_pcp->puerto_cpu);
	txt_write_in_file(pcp_log,"Escuchando conexiones entrantes de cpus \n");

	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(listener_cpu, &master);
	fdmax = listener_cpu;

	while(quit_sistema){

		read_fds = master;

		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			txt_write_in_file(pcp_log,"Error en el select \n");
			perror("PCP-select");
			exit(1);
		}

		for(i = 3; i <= fdmax; i++) {

			if (FD_ISSET(i, &read_fds)) {

				if (i == listener_cpu) { // Si los datos son en el srv que escucha cpus(se conecta una nueva cpu)
					cpu_new_fd = socket_accept(listener_cpu);
					handshake_cpu(cpu_new_fd);

					txt_write_in_file(pcp_log,"Se acepto la conexion de una nueva CPU con socket n°:");
					logear_int(plp_log,cpu_new_fd);
					txt_write_in_file(pcp_log,"\n");

					t_cpu *cpu = malloc(sizeof(t_cpu));

					// Arma estructura con el n° de socket del cpu y el id del prog que esta ejecutando
					cpu->soc_cpu = cpu_new_fd;
					cpu->id_prog_exec = 0;

					// Pone la estructura en la cola de cpus (cola con cpus libres)
					pthread_mutex_lock(&mutex_uso_cola_cpu);
					queue_push(cola_cpu, cpu);
					pthread_mutex_unlock(&mutex_uso_cola_cpu);
					pthread_mutex_unlock(&mutex_cola_cpu_vacia);
					printf("PCP-select: CPU conectada n°socket %i\n", cpu_new_fd);
					FD_SET(cpu_new_fd, &master);

					if (cpu_new_fd > fdmax)
						fdmax = cpu_new_fd;

				}else{

					// Sino, es un cpu mandando datos
					t_men_comun *men_cpu = socket_recv_comun(i);
					switch(men_cpu->tipo){
					case CONEC_CERRADA:
						txt_write_in_file(pcp_log,"Cerro la conexion el cpu con socket n°:");
						logear_int(pcp_log,i);
						txt_write_in_file(pcp_log,"\n");
						printf("PCP-select: CPU desconectada n°socket %i\n", i);
						// Agarra el cpu a partir de su n° de socket
						aux_cpu = get_cpu(i);
						if (aux_cpu->id_prog_exec != 0){
							// Saca el pcb que corresponde de la cola de ejec a partir del id
							aux_pcb_otros = get_pcb_otros_exec(aux_cpu->id_prog_exec);
							aux_pcb_otros->tipo_fin_ejecucion= CPU_DESCONEC;
							// Pone el pcb en exit
							pasar_pcb_exit(aux_pcb_otros);
						}
						free(aux_cpu);
						socket_cerrar(i);
						FD_CLR(i, &master);
						break;
					case FIN_QUANTUM:
						aux_cpu = get_cpu(i);
						if (aux_cpu->id_prog_exec != 0){
							// Actualizacion pcb
							aux_pcb_otros=get_pcb_otros_exec(aux_cpu->id_prog_exec);
							t_men_quantum_pcb *men_quenatum_pcb = socket_recv_quantum_pcb(i);
							t_pcb *pcb_recibido = men_quenatum_pcb->pcb;
							actualizar_pcb(aux_pcb_otros->pcb,pcb_recibido);
							// Setea en la estructura del cpu como que no esta ejecutando nada
							aux_cpu->id_prog_exec = 0;
							// Pone el pcb en ready
							pthread_mutex_lock(&mutex_ready);
							queue_push(colas->cola_ready,aux_pcb_otros);
							pthread_mutex_unlock(&mutex_ready);
							pthread_mutex_unlock(&mutex_ready_vacia);
							// Pone el cpu devuelta en la cola de cpus
							pthread_mutex_lock(&mutex_uso_cola_cpu);
							queue_push(cola_cpu,aux_cpu);
							pthread_mutex_unlock(&mutex_uso_cola_cpu);
							pthread_mutex_unlock(&mutex_cola_cpu_vacia);

							txt_write_in_file(pcp_log,"Termino un quantum del programa:");
							logear_int(pcp_log,aux_pcb_otros->pcb->id);
							txt_write_in_file(pcp_log,"\n");
						}else
							printf("ERROR ningun cpu esta procesando el proceso con socket nº%i\n",i);
						break;
					case FIN_EJECUCION:
						aux_cpu = get_cpu(i);
						aux_pcb_otros = get_pcb_otros_exec(atoi(men_cpu->dato));
						aux_pcb_otros->tipo_fin_ejecucion = FIN_EJECUCION;
						pasar_pcb_exit(aux_pcb_otros);

						aux_cpu->id_prog_exec = 0;

						pthread_mutex_lock(&mutex_uso_cola_cpu);
						queue_push(cola_cpu,aux_cpu);
						pthread_mutex_unlock(&mutex_uso_cola_cpu);
						pthread_mutex_unlock(&mutex_cola_cpu_vacia);

						txt_write_in_file(pcp_log,"Termino la ejecucion del programa:");
						logear_int(pcp_log,aux_pcb_otros->pcb->id);
						txt_write_in_file(pcp_log,"\n");
						break;
					case SEGMEN_FAULT:
						aux_cpu = get_cpu(i);
						aux_pcb_otros = get_pcb_otros_exec(aux_cpu->id_prog_exec);
						aux_pcb_otros->tipo_fin_ejecucion = SEGMEN_FAULT;

						pasar_pcb_exit(aux_pcb_otros);

						aux_cpu->id_prog_exec = 0;

						pthread_mutex_lock(&mutex_uso_cola_cpu);
						queue_push(cola_cpu,aux_cpu);
						pthread_mutex_unlock(&mutex_uso_cola_cpu);
						pthread_mutex_unlock(&mutex_cola_cpu_vacia);

						txt_write_in_file(pcp_log,"Error por segmentation fault del programa:");
						logear_int(pcp_log,aux_pcb_otros->pcb->id);
						txt_write_in_file(pcp_log,"\n");
						break;
					case IMPRIMIR_TEXTO:
					case IMPRIMIR_VALOR:
						men_id_prog = socket_recv_comun(i);

						if (men_id_prog->tipo != ID_PROG)
							printf("ERROR: esperaba el id de un programa y recibi: %i\n",men_id_prog->tipo);
						int32_t aux_i = atoi(men_id_prog->dato);
						aux_pcb_otros = get_pcb_otros_exec_sin_quitarlo(aux_i);

						socket_send_comun(aux_pcb_otros->n_socket, men_cpu);
						destruir_men_comun(men_id_prog);

						txt_write_in_file(pcp_log,"IMPRIMIR por cpu con socket n°");
						logear_int(pcp_log,i);
						txt_write_in_file(pcp_log,"\n");
						enviar_men_comun_destruir(i, R_IMPRIMIR,NULL,0);
						break;
					case OBTENER_VALOR:
						if (dictionary_has_key(diccionario_variables, men_cpu->dato)){
							valor = dictionary_get(diccionario_variables,men_cpu->dato);
							string_valor = string_itoa(*valor);
							enviar_men_comun_destruir(i,R_OBTENER_VALOR,string_valor,string_length(string_valor));
							txt_write_in_file(pcp_log,"OBTENER VALOR por cpu con socket n°:");
							logear_int(pcp_log,i);
							txt_write_in_file(pcp_log,"\n");
						}else{
							llamada_erronea(i, VAR_INEX);
							txt_write_in_file(pcp_log,"Error en OBTENER VALOR (var inexistente) por cpu con socket n°:");
							logear_int(pcp_log,i);
							txt_write_in_file(pcp_log,"\n");
						}
						break;
					case GRABAR_VALOR:
						if (dictionary_has_key(diccionario_variables, men_cpu->dato)){
							enviar_men_comun_destruir(i,R_GRABAR_VALOR,NULL,0);
							men_grab_valor = socket_recv_comun(i);
							if((men_grab_valor->tipo) != VALOR_ASIGNADO)
								printf("ERROR: esperaba recibir VALOR_ASIGNADO y recibi: %i",men_grab_valor->tipo);

							dictionary_remove(diccionario_variables,men_cpu->dato);//todo free???
							int32_t *aux_i = malloc(sizeof(int32_t));
							*aux_i= atoi(men_grab_valor->dato);
							dictionary_put(diccionario_variables,men_cpu->dato,aux_i);

							destruir_men_comun(men_grab_valor);
							txt_write_in_file(pcp_log,"GRABAR VALOR por cpu con socket n°:");
							logear_int(pcp_log,i);
							txt_write_in_file(pcp_log,"\n");
						}else{
							llamada_erronea(i, VAR_INEX);
							txt_write_in_file(pcp_log,"Error en GRABAR VALOR (var inexistente) por cpu con socket n°:");
							logear_int(pcp_log,i);
							txt_write_in_file(pcp_log,"\n");
						}
						break;
					case WAIT:
						tam = queue_size(cola_semaforos);
						encontre=0;
						for(j=0;j<tam;j++){
							semaforo = queue_pop(cola_semaforos);
							if(semaforo->id_sem == men_cpu->dato){
								j = tam;
								encontre=1;
							}else{
								queue_push(cola_semaforos,semaforo);
							}
						}
						if(encontre==1){

							if(semaforo->valor > 0){
								(semaforo->valor)--;
								men_cpu->tipo=SEM_OK;
								socket_send_comun(i,men_cpu);
							}else{
								(semaforo->valor)--;
								aux_cpu = get_cpu(i);

								men_cpu->tipo = SEM_BLOQUEADO;
								socket_send_comun(i,men_cpu);

								aux_pcb_otros = actualizar_pcb_y_bloq(aux_cpu);

								queue_push(semaforo->procesos,aux_pcb_otros);

								queue_push(cola_semaforos,semaforo);
							}

						txt_write_in_file(pcp_log,"WAIT por cpu con socket n°:");
						logear_int(pcp_log,i);
						txt_write_in_file(pcp_log,"\n");
						}else{
							llamada_erronea(i, SEM_INEX);

							txt_write_in_file(pcp_log,"Error en WAIT (semaforo inexistente) por cpu con socket n°:");
							logear_int(pcp_log,i);
							txt_write_in_file(pcp_log,"\n");
						}
						break;
					case SIGNAL:
						tam = queue_size(cola_semaforos);
						encontre=0;

						for(j=0;j<tam;j++){

							semaforo = queue_pop(cola_semaforos);

							if(semaforo->id_sem == men_cpu->dato){
								j = tam;
								encontre=1;
							}else{
								queue_push(cola_semaforos,semaforo);
							}
						}

						if(encontre==1){
							(semaforo->valor)++;
							socket_send_comun(i,men_cpu);

							if(queue_size(semaforo->procesos)>0){
								aux_pcb_otros = queue_pop(semaforo->procesos);
								pasar_pcbBlock_exit(aux_pcb_otros->pcb->id);
							}
							queue_push(cola_semaforos,semaforo);

							txt_write_in_file(pcp_log,"SIGNAL por cpu con socket n°:");
							logear_int(pcp_log,i);
							txt_write_in_file(pcp_log,"\n");

						}else{
							llamada_erronea(i, SEM_INEX);

							txt_write_in_file(pcp_log,"Error en SIGNAL (semaforo inexistente) por cpu con socket n°:");
							logear_int(pcp_log,i);
							txt_write_in_file(pcp_log,"\n");

						}
						break;
					case IO_ID:
						enviar_IO(i, atoi(men_cpu->dato));
						break;
					default:
						printf("ERROR se esperaba al recibir el tipo de dato:%i\n", men_cpu->tipo);
						break;
					}
					destruir_men_comun(men_cpu);
				}
			}
		}
	sleep(param_pcp->retardo);
	}
	return NULL;
}

t_pcb_otros *get_pcb_otros_exec_sin_quitarlo(int32_t id_proc){

	t_pcb_otros *aux_pcb_otros;
	int32_t i;

	pthread_mutex_lock(&mutex_exec);
	int32_t tam = queue_size(colas->cola_exec);

	for(i=0 ;i < tam ;i++){

		aux_pcb_otros = queue_pop(colas->cola_exec);

		if (id_proc == aux_pcb_otros->pcb->id){

			queue_push(colas->cola_exec, aux_pcb_otros);
			aux_pcb_otros = queue_peek(colas->cola_exec);
			pthread_mutex_unlock(&mutex_exec);
			return aux_pcb_otros;
		}
		queue_push(colas->cola_exec, aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_exec);
	printf("ERROR EN get_pcb_otros_exec\n");
	return NULL;
}

void pasar_pcbBlock_exit(int32_t id_pcb){
	int32_t i = 0;
	pthread_mutex_lock(&mutex_block);
	int32_t tamanio_cola_block = queue_size(colas->cola_block);

	for (i=0; i<tamanio_cola_block; i++){
		t_pcb_otros *pcb_aux;
		pcb_aux =queue_pop(colas->cola_block);

		if (pcb_aux->pcb->id==id_pcb){
			pthread_mutex_lock(&mutex_ready);
			queue_push(colas->cola_ready, pcb_aux);
			pthread_mutex_unlock(&mutex_ready);
			pthread_mutex_unlock(&mutex_ready_vacia);
			i=tamanio_cola_block;
		}
		else {
			queue_push(colas->cola_block,pcb_aux);
		}
	}
	pthread_mutex_unlock(&mutex_block);
}

void pasar_pcb_exit(t_pcb_otros *pcb){
	pthread_mutex_lock(&mutex_exit);
	queue_push(colas->cola_exit,pcb);
	sem_incre(&cont_exit);
	pthread_mutex_unlock(&mutex_exit);

	sem_decre(&buff_multiprog);
	sem_incre(&libre_multiprog);
}

//Para buscar un pcb por su socket, xq no se sabe en que cola esta(cuando se cae la conexion), lo encuentra y lo pasa a exit
int32_t mover_pcb_exit(int32_t soc_prog){

	int32_t i;
	t_pcb_otros *aux;

	pthread_mutex_lock(&mutex_new);

	for(i=0; i < queue_size(colas->cola_new) ;i++){
		aux = queue_pop(colas->cola_new);

		if (aux->n_socket == soc_prog){
			pasar_pcb_exit(aux);
			pthread_mutex_unlock(&mutex_new);
			return 0;
		}
		queue_push(colas->cola_new, aux);
	}
	pthread_mutex_unlock(&mutex_new);

	pthread_mutex_lock(&mutex_ready);

	for(i=0; i < queue_size(colas->cola_ready) ;i++){
		aux = queue_pop(colas->cola_ready);

		if (aux->n_socket == soc_prog){
			pasar_pcb_exit(aux);
			pthread_mutex_unlock(&mutex_ready);
			return 0;
		}
		queue_push(colas->cola_ready, aux);
	}
	pthread_mutex_unlock(&mutex_ready);

	pthread_mutex_lock(&mutex_block);

	for(i=0; i < queue_size(colas->cola_block) ;i++){
		aux = queue_pop(colas->cola_block);

		if (aux->n_socket == soc_prog){
			pasar_pcb_exit(aux);
			pthread_mutex_unlock(&mutex_block);
			return 0;
		}
		queue_push(colas->cola_block, aux);
	}
	pthread_mutex_unlock(&mutex_block);

	pthread_mutex_lock(&mutex_exec);

	for(i=0; i < queue_size(colas->cola_exec) ;i++){
		aux = queue_pop(colas->cola_exec);

		if (aux->n_socket == soc_prog){
			pasar_pcb_exit(aux);
			pthread_mutex_unlock(&mutex_exec);
			return 0;
		}
		queue_push(colas->cola_exec, aux);
	}
	pthread_mutex_unlock(&mutex_exec);
	return -1;
}

void llamada_erronea(int32_t soc_cpu,int32_t tipo_error){
	t_cpu *aux_cpu = get_cpu(soc_cpu);
	t_pcb_otros *aux_pcb= get_pcb_otros_exec(aux_cpu->id_prog_exec);

	aux_cpu->id_prog_exec = 0;

	aux_pcb->tipo_fin_ejecucion=tipo_error;
	pasar_pcb_exit(aux_pcb);
	enviar_men_comun_destruir(soc_cpu,tipo_error,NULL,0);

	pthread_mutex_lock(&mutex_uso_cola_cpu);
	queue_push(cola_cpu,aux_cpu);
	pthread_mutex_unlock(&mutex_uso_cola_cpu);
	pthread_mutex_unlock(&mutex_cola_cpu_vacia);
}

t_pcb_otros *actualizar_pcb_y_bloq(t_cpu *cpu){
	t_pcb_otros *aux_pcb_otros=get_pcb_otros_exec(cpu->id_prog_exec);
	t_pcb *pcb_recibido = socket_recv_quantum_pcb(cpu->soc_cpu)->pcb;
	actualizar_pcb(aux_pcb_otros->pcb,pcb_recibido);

	cpu->id_prog_exec = 0;

	pthread_mutex_lock(&mutex_uso_cola_cpu);
	queue_push(cola_cpu,cpu);
	pthread_mutex_unlock(&mutex_uso_cola_cpu);
	pthread_mutex_unlock(&mutex_cola_cpu_vacia);

	pthread_mutex_lock(&mutex_block);
	queue_push(colas->cola_block,aux_pcb_otros);
	pthread_mutex_unlock(&mutex_block);

	return aux_pcb_otros;
}

t_cpu *get_cpu(int32_t soc_cpu){

	int32_t i;
	t_cpu *cpu;
	pthread_mutex_lock(&mutex_uso_cola_cpu);
	int32_t tam = queue_size(cola_cpu);

	for(i=0 ;i < tam ;i++){
		cpu = queue_pop(cola_cpu);

		if (cpu->soc_cpu == soc_cpu){
			pthread_mutex_unlock(&mutex_uso_cola_cpu);
			return cpu;
		}
		queue_push(cola_cpu, cpu);
	}
	pthread_mutex_unlock(&mutex_uso_cola_cpu);
	printf("ERROR EN GET_CPU\n");
	return NULL;
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
	printf("ERROR EN get_pcb_otros_exec\n");
	return NULL;
}

void enviar_IO(int32_t soc_cpu, int32_t id_IO){

	t_men_comun *men;
	t_IO *aux_IO;
	t_cpu *aux_cpu;
	aux_cpu = get_cpu(soc_cpu);
	int32_t i;

	//Busca el dispositivo de IO en la lista y lo guarda en aux_IO
	pthread_mutex_lock(&mutex_dispositivos_io);

	for (i=0; i < list_size(dispositivos_IO); i++){
		aux_IO = list_remove(dispositivos_IO,i);

		if(atoi(aux_IO->id_hio)==id_IO){
			i=list_size(dispositivos_IO);
		}
		else{
			list_add(dispositivos_IO,aux_IO);
		}
	}
	pthread_mutex_unlock(&mutex_dispositivos_io);

	men = socket_recv_comun(soc_cpu);

	if (men->tipo != IO_CANT_UNIDADES)
		printf("ERROR: La CPU mando el tipo %i y se esperaba %i\n",men->tipo,IO_CANT_UNIDADES);

	int32_t cant_unidades = atoi(men->dato);

	//Crea la estructura que tiene el id del programa y la cantidad de unidades que quiere usar para ponerlo en la cola del dispositivo
	t_IO_espera *espera = malloc(sizeof(t_IO_espera));
	espera->id_prog = (aux_cpu->id_prog_exec);
	espera->unidades = cant_unidades;
	queue_push(aux_IO->procesos,espera);
	pthread_mutex_unlock(&(aux_IO->mutex_dispositivo));

	actualizar_pcb_y_bloq(aux_cpu);

	destruir_men_comun(men);
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
	return NULL;
}

void *manejador_exit(){

	t_pcb_otros *aux_pcb_otros;
	int32_t soc_prog;

	while(quit_sistema){
		pthread_mutex_lock(&mutex_exit);

		if (queue_is_empty(colas->cola_exit)){
			pthread_mutex_unlock(&mutex_exit);
			sem_decre(&cont_exit);
		}else{

			aux_pcb_otros = queue_pop(colas->cola_exit);
			pthread_mutex_unlock(&mutex_exit);

			//if (aux_pcb_otros->tipo_fin_ejecucion == CPU_DESCONEC ||aux_pcb_otros->tipo_fin_ejecucion == FIN_EJECUCION )

			enviar_men_comun_destruir(soc_prog, aux_pcb_otros->tipo_fin_ejecucion,NULL,0);
			soc_prog = aux_pcb_otros->n_socket;
			socket_cerrar(soc_prog);
			umv_destrui_pcb(aux_pcb_otros->pcb->id);
			free(aux_pcb_otros->pcb);
			free(aux_pcb_otros);
			sem_decre(&cont_exit);
		}
	}
	return NULL;
}

void *manejador_new_ready(t_param_new_ready *param){

	while(quit_sistema){
		pthread_mutex_lock(&mutex_new);

		if (queue_is_empty(colas->cola_new)){
			pthread_mutex_unlock(&mutex_new);
			pthread_mutex_lock(&mutex_miltiprog);

		}else{

			//pthread_mutex_unlock(&mutex_new);
			sem_decre(&libre_multiprog);

			//pthread_mutex_lock(&mutex_new);
			pthread_mutex_lock(&mutex_ready);
			queue_push(colas->cola_ready, get_peso_min());
			pthread_mutex_unlock(&mutex_new);
			pthread_mutex_unlock(&mutex_ready);
			pthread_mutex_unlock(&mutex_ready_vacia);
			sem_incre(&buff_multiprog);
		}
	}
	free(param);
	return NULL;
}

void *manejador_ready_exec(t_param_ready_exec *param){

	t_pcb_otros *aux_pcb_otros;
	t_cpu *cpu;
	int32_t res;

	while(quit_sistema){

		pthread_mutex_lock(&mutex_ready);

		if (queue_is_empty(colas->cola_ready)){
			pthread_mutex_unlock(&mutex_ready);
			pthread_mutex_lock(&mutex_ready_vacia);

		}else{

			pthread_mutex_lock(&mutex_uso_cola_cpu);
			cpu = get_cpu_libre(&res);

			if(res == 0){
				pthread_mutex_unlock(&mutex_ready);
				pthread_mutex_unlock(&mutex_uso_cola_cpu);
				pthread_mutex_lock(&mutex_cola_cpu_vacia);

			}else{
				aux_pcb_otros = queue_pop(colas->cola_ready);
				pthread_mutex_unlock(&mutex_ready);

				enviar_cpu_pcb_destruir(cpu->soc_cpu,aux_pcb_otros->pcb,param->quantum);
				cpu->id_prog_exec = aux_pcb_otros->pcb->id;
				queue_push(cola_cpu, cpu);
				pthread_mutex_unlock(&mutex_uso_cola_cpu);
				pthread_mutex_lock(&mutex_exec);
				queue_push(colas->cola_exec, aux_pcb_otros);
				pthread_mutex_unlock(&mutex_exec);
			}
		}
	}
	return NULL;
}

t_cpu *get_cpu_libre(int32_t *res){

	int32_t i;
	t_cpu *cpu;

	int32_t tam = queue_size(cola_cpu);

	for(i=0 ;i < tam ;i++){
		cpu = queue_pop(cola_cpu);

		if (cpu->id_prog_exec == 0){
			*res = 1;
			return cpu;
		}
		queue_push(cola_cpu, cpu);
	}
	*res = 0;
	return cpu;
}

void umv_destrui_pcb(int32_t id_pcb){
	enviar_umv_mem_seg_destruir(soc_umv, DESTR_SEGS, id_pcb, 0);
	//recibo respuesta para sincronizar
	t_men_comun *men_resp = socket_recv_comun(soc_umv);
	if (men_resp->tipo != SINCRO_OK)
		printf("ERROR se esperaba, de la UMV, un tipo de mensaje SINCRO_OK y se recibio:%i\n",men_resp->tipo);
	destruir_men_comun(men_resp);
}

void recibir_resp_escrbir_seg(){//uso esto para q espere y no ponga en la cola de new un proceso con los segmentos vacios
	t_men_comun *men = socket_recv_comun(soc_umv);
	if(men->tipo != MEN_ALM_OK)
		printf("ERROR se esperaba un tipo de dato MEN_ALM_OK y se recibio:%i",men->tipo);
	destruir_men_comun(men);
}

t_pcb *crear_pcb_escribir_seg_UMV(t_men_comun *men_cod_prog ,t_resp_sol_mem *resp_sol ,int32_t contador_id_programa){
	t_metadata_program *metadata_program = metadata_desde_literal(men_cod_prog->dato);
	char *etis = metadata_program->etiquetas;

	// Escribe el segmento de codigo
	enviar_umv_mem_seg_destruir(soc_umv, ESCRIBIR_SEG, contador_id_programa, 0);
	enviar_men_comun_destruir(soc_umv, CODIGO_SCRIPT,men_cod_prog->dato,men_cod_prog->tam_dato);
	recibir_resp_escrbir_seg();

	// Escribe el segmento de indice de etiquetas
	enviar_umv_mem_seg_destruir(soc_umv, ESCRIBIR_SEG, contador_id_programa, 0);
	enviar_men_comun_destruir(soc_umv, IND_ETI_FUNC,etis,metadata_program->etiquetas_size);
	recibir_resp_escrbir_seg();

	// Escribe el segmento de indice de codigo
	enviar_umv_mem_seg_destruir(soc_umv, ESCRIBIR_SEG, contador_id_programa, 0);
	int32_t tam_ind_cod = metadata_program->instrucciones_size*8;
	enviar_men_comun_destruir(soc_umv,IND_COD , (void *)metadata_program->instrucciones_serializado,tam_ind_cod );
	recibir_resp_escrbir_seg();

	t_pcb *pcb = malloc(sizeof(t_pcb));
	pcb->id = contador_id_programa;
	pcb->dir_primer_byte_umv_segmento_codigo = resp_sol->dir_primer_byte_umv_segmento_codigo;
	pcb->dir_primer_byte_umv_segmento_stack = resp_sol->dir_primer_byte_umv_segmento_stack;
	pcb->dir_primer_byte_umv_contexto_actual = resp_sol->dir_primer_byte_umv_segmento_stack;
	pcb->dir_primer_byte_umv_indice_codigo = resp_sol->dir_primer_byte_umv_indice_codigo;
	pcb->dir_primer_byte_umv_indice_etiquetas = resp_sol->dir_primer_byte_umv_indice_etiquetas;
	pcb->program_counter = metadata_program->instruccion_inicio;
	pcb->cant_var_contexto_actual = 0;
	pcb->tam_indice_etiquetas = metadata_program->etiquetas_size;

	return pcb;
}

int32_t recibir_umv_dir_mem(int32_t tipo_men){
	t_men_comun *resp_umv = socket_recv_comun(soc_umv);
	if (resp_umv->tipo == tipo_men){
		int32_t ret = atoi(resp_umv->dato);
		destruir_men_comun(resp_umv);
		return ret;
	}
	if (resp_umv->tipo == MEM_OVERLOAD){
		destruir_men_comun(resp_umv);
		return -2;
	}
	printf("ERROR: se esperaba %i y obtube un %i\n",tipo_men,resp_umv->tipo);
	return -1;
}

t_resp_sol_mem * solicitar_mem(t_men_comun *men_cod_prog, int32_t tam_stack, int32_t id_prog){
	t_resp_sol_mem *resp_sol = malloc(sizeof(t_resp_sol_mem));
	resp_sol->memoria_insuficiente = 0;
	int32_t tam = 0;
	int32_t dir_mem_segmento_codigo,dir_mem_indice_etiquetas,dir_mem_indice_codigo,dir_mem_segmento_stack;

	//pido mem para el codigo del script
	enviar_umv_mem_seg_destruir(soc_umv,PED_MEM_SEG_COD,id_prog,men_cod_prog->tam_dato);
	dir_mem_segmento_codigo = recibir_umv_dir_mem(RESP_MEM_SEG_COD);

	if (dir_mem_segmento_codigo == -2){//si es mem overload
		resp_sol->memoria_insuficiente = MEM_OVERLOAD;
		return resp_sol;
	}
	//pido mem para el indice de etiquetas y funciones
	t_metadata_program* metadata_program = metadata_desde_literal(men_cod_prog->dato);
	enviar_umv_mem_seg_destruir(soc_umv, PED_MEM_IND_ETI , id_prog, metadata_program->etiquetas_size);
	dir_mem_indice_etiquetas = recibir_umv_dir_mem(RESP_MEM_IND_ETI);
	if (dir_mem_indice_etiquetas == -2){//si es mem overload
		resp_sol->memoria_insuficiente = MEM_OVERLOAD;
		return resp_sol;
	}
	//pido mem para el indice de codigo
	tam = (metadata_program->instrucciones_size*8);
	enviar_umv_mem_seg_destruir(soc_umv, PED_MEM_IND_COD , id_prog, tam);
	dir_mem_indice_codigo = recibir_umv_dir_mem(RESP_MEM_IND_COD);

	if (dir_mem_indice_etiquetas == -2){//si es mem overload
		resp_sol->memoria_insuficiente = MEM_OVERLOAD;
		return resp_sol;
	}
	//pido mem para el stack
	enviar_umv_mem_seg_destruir(soc_umv, PED_MEM_SEG_STACK , id_prog, tam_stack);
	dir_mem_segmento_stack = recibir_umv_dir_mem(RESP_MEM_SEG_STACK);

	if (dir_mem_indice_etiquetas == -2){//si es mem overload
		resp_sol->memoria_insuficiente = MEM_OVERLOAD;
		return resp_sol;
	}
	resp_sol->dir_primer_byte_umv_segmento_codigo = dir_mem_segmento_codigo;
	resp_sol->dir_primer_byte_umv_indice_etiquetas = dir_mem_indice_etiquetas;
	resp_sol->dir_primer_byte_umv_indice_codigo = dir_mem_indice_codigo;
	resp_sol->dir_primer_byte_umv_segmento_stack = dir_mem_segmento_stack;

	return resp_sol;
}

int32_t calcular_peso(t_men_comun *men_cod_prog){
	t_metadata_program *metadata_program = metadata_desde_literal(men_cod_prog->dato);
	return (5 * (metadata_program->cantidad_de_etiquetas) + 3 * (metadata_program->cantidad_de_funciones) + (metadata_program->instrucciones_size));
}

void handshake_umv(char *ip_umv, char *puerto_umv){ //sincronizar msjs con la umv

	//envio a la UMV
	soc_umv = socket_crear_client(puerto_umv,ip_umv);

	enviar_men_comun_destruir(soc_umv,HS_KERNEL,NULL,0);
	//espero coneccion de la UMV
	t_men_comun *mensaje_inicial = socket_recv_comun(soc_umv);

	if(mensaje_inicial->tipo == HS_UMV){
		printf("UMV conectada\n");
		txt_write_in_file(plp_log,"Conectado a la UMV \n");
	}else{
		printf("ERROR HANDSHAKE");
		txt_write_in_file(plp_log,"Error en el handshake con la UMV \n");
	}
	destruir_men_comun(mensaje_inicial);
}

void manejador_IO(t_IO *io){

	while(quit_sistema){

		if(queue_size(io->procesos)==0)
			pthread_mutex_lock(&(io->mutex_dispositivo));
		else{

			t_IO_espera *proceso = queue_pop(io->procesos);
			int32_t i;

			for(i=0; i<proceso->unidades;i++)
				sleep(io->hio_sleep);

			pasar_pcbBlock_exit(proceso->id_prog);

			pthread_mutex_unlock(&(io->mutex_dispositivo));
		}
	}
}

void enviar_cpu_pcb_destruir(int32_t soc,t_pcb *pcb,int32_t quantum){
	t_men_quantum_pcb * men_pcb= crear_men_quantum_pcb(PCB_Y_QUANTUM,quantum,pcb);
	socket_send_quantum_pcb(soc,men_pcb);
	destruir_quantum_pcb(men_pcb);
}

void actualizar_pcb(t_pcb *pcb, t_pcb *pcb_actualizado){
	pcb->cant_var_contexto_actual = pcb_actualizado->cant_var_contexto_actual;
	pcb->program_counter = pcb_actualizado->program_counter;
}

t_datos_config *levantar_config(){

	int i;
	t_IO *aux_IO;
	char **id_hios = malloc(sizeof(char));
	char **hio_sleeps = malloc(sizeof(char));
	t_config *diccionario_config = config_create("./kernel/kernel_config");
	t_datos_config *ret = malloc(sizeof(t_datos_config));
	ret->puerto_prog = config_get_string_value( diccionario_config, "Puerto_prog");
	ret->puerto_cpu = config_get_string_value( diccionario_config, "Puerto_CPU");
	ret->ip_umv = config_get_string_value( diccionario_config, "IP_UMV");
	ret->puerto_umv = config_get_string_value( diccionario_config, "Puerto_umv");
	ret->cola_IO = queue_create();
	id_hios = config_get_array_value( diccionario_config, "ID_HIO");
	hio_sleeps = config_get_array_value( diccionario_config, "HIO");


	for (i=0; id_hios[i] != '\0'; i++){

		t_IO *new_IO = malloc(sizeof(t_IO));
		new_IO->id_hio = id_hios[i];
		new_IO->hio_sleep = atoi(hio_sleeps[i]);

		new_IO->procesos = queue_create();
		queue_push(ret->cola_IO, new_IO);
		pthread_create(&(new_IO->hilo), NULL, (void*)manejador_IO, (void*)new_IO);
	}

	ret->semaforos = config_get_array_value( diccionario_config, "SEMAFOROS");
	ret->valor_semaforos = config_get_array_value( diccionario_config, "VALOR_SEMAFORO");

	for(i=0;ret->semaforos[i] != '\0'; i++){

		t_semaforo *new_sem = malloc(sizeof(t_semaforo));
		new_sem->id_sem = ret->semaforos[i];
		new_sem->valor = atoi(ret->valor_semaforos[i]);

		new_sem->procesos = queue_create();
		queue_push(cola_semaforos,new_sem);
	}

	ret->multiprogramacion = config_get_int_value( diccionario_config, "MULTIPROGRAMACION");
	ret->quantum = config_get_int_value( diccionario_config, "QUANTUM");
	ret->retardo = config_get_int_value( diccionario_config, "RETARDO");
	ret->tam_stack = config_get_int_value( diccionario_config, "TAMANIO_STACK");
	ret->variables_globales = config_get_array_value( diccionario_config, "VARIABLES_GLOBALES");

	for (i=0; ret->variables_globales[i] != '\0'; i++){
		int32_t *aux_i=malloc(sizeof(int32_t));
		dictionary_put(diccionario_variables,ret->variables_globales[i],aux_i);
	}

	printf("\n\n------------------------------Archivo Config----------------------------------------\n");
	printf("Puerto Proc-Prog = %s\n", ret->puerto_prog);
	printf("Puerto CPU = %s\n", ret->puerto_cpu);
	printf("UMV\n");
	printf("	IP     = %s\n", ret->ip_umv);
	printf("	Puerto = %s\n", ret->puerto_umv);
	printf("Entrada/Salida\n");
	printf("	ID    = ");

	for (i=0; i < queue_size(ret->cola_IO); i++){
		aux_IO = queue_pop(ret->cola_IO);
		printf("%s ", aux_IO->id_hio);
		queue_push(ret->cola_IO, aux_IO);
	}

	printf("\n	Sleep = ");

	for (i=0; i < queue_size(ret->cola_IO); i++){
		aux_IO = queue_pop(ret->cola_IO);
		printf("%i ", aux_IO->hio_sleep);
		queue_push(ret->cola_IO, aux_IO);
	}

	printf("\nSemaforos\n");

	for (i=0; ret->semaforos[i] != '\0'; i++){
		printf("	ID    = %s\n", ret->semaforos[i]);
		printf("	Valor = %s\n", ret->valor_semaforos[i]);
	}

	printf("Varias\n");
	printf("	Multiprogramacion = %i\n", ret->multiprogramacion);
	printf("	Quantum = %i\n", ret->quantum);
	printf("	Retardo =  %i\n", ret->retardo);
	printf("	Tamanio Stack = %i\n", ret->tam_stack);
	printf("Variables Globales\n");

	for (i=0; ret->variables_globales[i] != '\0'; i++){
		printf("	ID    = %s\n", ret->variables_globales[i]);
	}

	printf("------------------------------------------------------------------------------------\n\n");
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

void *imp_colas (){

	int32_t i;
	t_pcb_otros *aux;

	while(quit_sistema){

		sleep(5);

		printf("--COLA-NEW---------------------------------------------------------\n");
		printf("|PID/PESO= ");

		pthread_mutex_lock(&mutex_new);

		if (queue_is_empty(colas->cola_new)){
			printf("*************COLA VACIA********************");
		}else {

			for(i=0; i < queue_size(colas->cola_new) ;i++){
				aux = queue_pop(colas->cola_new);
				printf("%i/%i - ",aux->pcb->id,aux->peso);
				queue_push(colas->cola_new, aux);
			}
		}

		pthread_mutex_unlock(&mutex_new);

		printf("\n");
		printf("--COLA-READY-------------------------------------------------------\n");
		printf("|  PID   = ");

		pthread_mutex_lock(&mutex_ready);

		if (queue_is_empty(colas->cola_ready)){
			printf("*************COLA VACIA********************");
		}else {

			for(i=0; i < queue_size(colas->cola_ready) ;i++){
				aux = queue_pop(colas->cola_ready);
				printf(" %i  - ",aux->pcb->id);
				queue_push(colas->cola_ready, aux);
			}
		}

		pthread_mutex_unlock(&mutex_ready);

		printf("\n");
		printf("--COLA-BLOCK-------------------------------------------------------\n");
		printf("|  PID   = ");

		pthread_mutex_lock(&mutex_block);

		if (queue_is_empty(colas->cola_block)){
			printf("*************COLA VACIA********************");
		}else {

			for(i=0; i < queue_size(colas->cola_block) ;i++){
				aux = queue_pop(colas->cola_block);
				printf(" %i  - ",aux->pcb->id);
				queue_push(colas->cola_block, aux);
			}
		}

		pthread_mutex_unlock(&mutex_block);

		printf("\n");
		printf("--COLA-EXEC--------------------------------------------------------\n");
		printf("|  PID   = ");

		pthread_mutex_lock(&mutex_exec);

		if (queue_is_empty(colas->cola_exec)){
			printf("*************COLA VACIA********************");
		}else {

			for(i=0; i < queue_size(colas->cola_exec) ;i++){
				aux = queue_pop(colas->cola_exec);
				printf(" %i  - ",aux->pcb->id);
				queue_push(colas->cola_exec, aux);
			}
		}

		pthread_mutex_unlock(&mutex_exec);

		printf("\n");
		printf("--COLA-EXIT--------------------------------------------------------\n");
		printf("|  PID   = ");

		pthread_mutex_lock(&mutex_exit);

		if (queue_is_empty(colas->cola_exit)){
			printf("*************COLA VACIA********************");
		}else {

			for(i=0; i < queue_size(colas->cola_exit) ;i++){
				aux = queue_pop(colas->cola_exit);
				printf(" %i  - ",aux->pcb->id);
				queue_push(colas->cola_exit, aux);
			}
		}

		pthread_mutex_unlock(&mutex_exit);

		printf("\n");
		printf("-------------------------------------------------------------------\n");

	}

	return NULL;
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

t_param_plp *ini_pram_plp(t_datos_config *diccionario_config){

	t_param_plp *aux = malloc(sizeof(t_param_plp));

	aux->puerto_prog = diccionario_config->puerto_prog;
	aux->ip_umv = diccionario_config->ip_umv;
	aux->puerto_umv = diccionario_config->puerto_umv;
	aux->retardo = diccionario_config->retardo;
	aux->max_multiprogramacion = diccionario_config->multiprogramacion;
	aux->tam_stack = diccionario_config->tam_stack;

	return aux;
}

t_param_pcp *ini_pram_pcp(t_datos_config *diccionario_config){

	t_param_pcp *aux = malloc(sizeof(t_param_pcp));

	aux->cola_IO = diccionario_config->cola_IO;
	aux->max_multiprogramacion = diccionario_config->multiprogramacion;
	aux->puerto_cpu = diccionario_config->puerto_cpu;
	aux->quantum = diccionario_config->quantum;
	aux->retardo = diccionario_config->retardo;
	aux->semaforos = diccionario_config->semaforos;
	aux->valor_semaforos = diccionario_config->valor_semaforos;
	aux->variables_globales = diccionario_config->variables_globales;

	return aux;
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

