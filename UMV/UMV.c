#include "UMV.h"
int32_t quit_sistema = 1;
char *mem_prin;
int32_t retardo = 0;
char alg_actual;
t_list *list_seg;
t_config *ptrConfig;
pthread_mutex_t mutex_mem_prin = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_list_seg = PTHREAD_MUTEX_INITIALIZER;

int32_t main(){
	srand(time(0));
	//crear configuracion y solicitar memoria
	ptrConfig = config_create("./UMV/umv_config.txt");
	encabezado(config_get_int_value(ptrConfig,"tamanio"),config_get_string_value(ptrConfig,"modo"));
	mem_prin = malloc(config_get_int_value(ptrConfig,"tamanio"));
	list_seg = list_create();
	alg_actual = config_get_int_value(ptrConfig,"modo");

	pthread_t hilo_consola;
	pthread_t hilo_conecciones;
	crear_hilo(&hilo_conecciones,  admin_conecciones,  NULL);
	crear_hilo(&hilo_consola,  crearConsola,  NULL);

	pthread_join( hilo_consola, NULL);

	destruir_lista_segmento(list_seg);
	config_destroy(ptrConfig);
	free(mem_prin);
	return 0;
}

void crear_hilo(pthread_t *hilo,  void *_funcion (void *),  void *param){
	int resp = pthread_create( hilo, NULL, _funcion, param);
	if (resp != 0)
		perror("crear hilo");
}

void *admin_conecciones(){
	int32_t listen_soc = socket_crear_server(config_get_string_value(ptrConfig,"puerto"));
	int32_t new_soc;
	t_men_comun *men_hs;
	pthread_t hilo_conec_kernel;
	pthread_t hilo_conec_cpu;
	t_param_conec_kernel *param_kernel = malloc(sizeof(t_param_conec_kernel));
	t_param_conec_cpu *param_cpu = malloc(sizeof(t_param_conec_cpu));
	while(quit_sistema){
		new_soc = socket_accept(listen_soc);
		men_hs = socket_recv_comun(new_soc);
		if (men_hs->tipo == CONEC_CERRADA){
			printf("Socket nº%i desconectado",new_soc);
			continue;
		}
		if (men_hs->tipo >= HS_KERNEL){//si se conecta el kernel
			param_kernel->soc = new_soc;
			crear_hilo(&hilo_conec_kernel, admin_conec_kernel, param_kernel);
			continue;
		}
		if (men_hs->tipo >= HS_CPU){//si es una cpu nueva
			param_cpu->soc = new_soc;
			crear_hilo(&hilo_conec_cpu, admin_conec_cpu, param_cpu);
			continue;
		}
		printf("ERROR se esperaba recibir un tipo handshake y se recibio %i", men_hs->tipo);
		destruir_men_comun(men_hs);
	}
	free(param_kernel);
	free(param_cpu);
	socket_cerrar(listen_soc);
	return NULL;
}

void *admin_conec_kernel(t_param_conec_kernel *param){
	t_men_seg *men_seg;

	t_men_comun *men_hs= crear_men_comun(HS_UMV,NULL,0);
	socket_send_comun(param->soc,men_hs);
	destruir_men_comun(men_hs);

	while(quit_sistema){
		men_seg = socket_recv_seg(param->soc);
		sleep(retardo);
		if(men_seg->tipo == CONEC_CERRADA){
			printf("El Kernel cerro la coneccion\n");
			quit_sistema = 0;
			break;
		}
		if (men_seg->tipo == DESTR_SEGS){
			destruirSegmentos(men_seg->id_prog);
			continue;
		}
		if (men_seg->tipo == PED_MEM_SEG_COD){
			gestionar_ped_seg(men_seg, RESP_MEM_SEG_COD, param->soc);
			continue;
		}
		if (men_seg->tipo == PED_MEM_IND_ETI){
			gestionar_ped_seg(men_seg, RESP_MEM_IND_ETI, param->soc);
			continue;
		}
		if (men_seg->tipo == PED_MEM_IND_COD){
			gestionar_ped_seg(men_seg, RESP_MEM_IND_COD, param->soc);
			continue;
		}
		if (men_seg->tipo == PED_MEM_SEG_STACK){
			gestionar_ped_seg(men_seg, RESP_MEM_SEG_STACK, param->soc);
			continue;
		}
		if (men_seg->tipo == ESCRIBIR_SEG){
			gestionar_alm_seg(men_seg->id_prog,param->soc);
			continue;
		}
		printf("ERROR al recibir el tipo de dato %i\n", men_seg->tipo);
		destruir_men_seg(men_seg);
	}
	return NULL;
}

void gestionar_alm_seg(int32_t id_proc, int32_t soc_kernel){
	t_men_comun *aux_men = socket_recv_comun(soc_kernel);

	pthread_mutex_lock(&mutex_list_seg);
	almacenar_segmento(aux_men, id_proc);
	pthread_mutex_unlock(&mutex_list_seg);

	destruir_men_comun(aux_men);
}

void almacenar_segmento(t_men_comun *aux_men, int32_t id_proc){
	t_seg *aux_seg;

	aux_seg = buscar_segmento(aux_men->tipo, id_proc);
	if (aux_men->tam_dato != aux_seg->tam_seg)
		printf("ERROR EL TAM DEL SEG ES ERRONEO\n");
	pthread_mutex_lock(&mutex_mem_prin);
	memcpy(&mem_prin[aux_seg->dir_fisica],aux_men->dato,aux_men->tam_dato);
	pthread_mutex_unlock(&mutex_mem_prin);
}

t_seg *buscar_segmento(int32_t tipo_seg,int32_t id_proc){
	bool _es_el_proc(t_seg *seg){
		return seg->id_proc == id_proc;
	}
	bool _es_tipo_seg(t_seg *seg){
		return seg->tipo_seg == tipo_seg;
	}
	t_seg *ret = malloc(sizeof(t_seg));//todo liberar
	t_list *list_aux = list_create();//todo destruir
	list_aux = list_filter(list_seg, (void*)_es_el_proc);
	ret = list_find(list_aux, (void*)_es_tipo_seg);
	if(ret == NULL)
		printf("ERROR no se a encontrado el tipo de segmento n°%i del proceso %i",tipo_seg,id_proc);
	//destruir_lista_segmento(list_aux);
	return ret;
}

void destruir_lista_segmento(t_list *list_seg){
	void destruir_t_seg(t_seg *seg){
		free(seg);
	}
	list_destroy_and_destroy_elements(list_seg, (void*)destruir_t_seg);
}

void gestionar_ped_seg(t_men_seg *men_seg,int32_t tipo_resp, int32_t soc_kernel){
	t_men_comun *aux_men;
	int resp_dir_mem = crearSegmento(men_seg);

	if (resp_dir_mem == -1){
		destruirSegmentos(men_seg->id_prog);
		aux_men= crear_men_comun(MEM_OVERLOAD,NULL,0);
		socket_send_comun(soc_kernel,aux_men);
	}else{
		aux_men = crear_men_comun(tipo_resp,string_itoa(resp_dir_mem),sizeof(string_itoa(resp_dir_mem)));
		socket_send_comun(soc_kernel,aux_men);
	}
	destruir_men_comun(aux_men);
}

void *admin_conec_cpu(t_param_conec_cpu *param){
	t_men_cpu_umv *men_bytes;
	int32_t proc_activo = 0;

	t_men_comun *men_hs= crear_men_comun(HS_UMV,NULL,0);
	socket_send_comun(param->soc,men_hs);
	destruir_men_comun(men_hs);

	while(quit_sistema){
		men_bytes = socket_recv_cpu_umv(param->soc);
		sleep(retardo);
		if( men_bytes->tipo == CAMBIO_PA){
			proc_activo = men_bytes->base;
			continue;
		}
		if( men_bytes->tipo == CONEC_CERRADA){
			printf("El CPU n° %i se ha desconectado\n",param->soc);
			break;//probar q se desconecta realmente todo
		}
		if( men_bytes->tipo == SOL_BYTES){
			gestionar_solicitud_bytes(param->soc, men_bytes, proc_activo);
			continue;
		}
		if( men_bytes->tipo == ALM_BYTES){
			gestionar_almacenamiento_bytes(param->soc, men_bytes, proc_activo);
			continue;
		}
		printf("ERROR al recibir el tipo de dato %i\n", men_bytes->tipo);
		destruir_men_cpu_umv(men_bytes);
	}
	socket_cerrar(param->soc);
	return NULL;
}

void gestionar_solicitud_bytes(int32_t soc_cpu,t_men_cpu_umv *men_bytes, int32_t proc_activo){
	t_men_comun *aux_men;
	pthread_mutex_lock(&mutex_list_seg);
	t_seg *aux_seg = buscar_segmento(IND_STACK,proc_activo);
	char  *bytes = malloc(men_bytes->tam);//todo liberar

	int32_t pos = aux_seg->dir_fisica + men_bytes->offset;
	if (aux_seg->tam_seg<(men_bytes->offset+men_bytes->tam)){
		pthread_mutex_unlock(&mutex_list_seg);
		aux_men = crear_men_comun(SEGMEN_FAULT,NULL,0);
		socket_send_comun(soc_cpu, aux_men);
	}else{
		pthread_mutex_lock(&mutex_mem_prin);
		memcpy(&bytes,&mem_prin[pos],men_bytes->tam);
		pthread_mutex_unlock(&mutex_mem_prin);
		aux_men = crear_men_comun(R_SOL_BYTES,bytes,men_bytes->tam);
		pthread_mutex_unlock(&mutex_list_seg);
		socket_send_comun(soc_cpu, aux_men);
	}
	destruir_men_comun(aux_men);
}

void gestionar_almacenamiento_bytes(int32_t soc_cpu, t_men_cpu_umv *men_bytes, int32_t proc_activo){
	t_men_comun *aux_men;
	pthread_mutex_lock(&mutex_list_seg);
	t_seg *aux_seg = buscar_segmento(IND_STACK,proc_activo);
	int32_t pos = aux_seg->dir_fisica + men_bytes->offset;

	if (aux_seg->tam_seg<(men_bytes->offset+men_bytes->tam)){
		pthread_mutex_unlock(&mutex_list_seg);
		aux_men = crear_men_comun(MEM_OVERLOAD,NULL,0);
		socket_send_comun(soc_cpu, aux_men);
	}else{
		pthread_mutex_lock(&mutex_mem_prin);
		memcpy(&mem_prin[pos],men_bytes->buffer,men_bytes->tam);
		pthread_mutex_unlock(&mutex_mem_prin);
		aux_men = crear_men_comun(R_ALM_BYTES, NULL , 0);
		pthread_mutex_unlock(&mutex_list_seg);
		socket_send_comun(soc_cpu, aux_men);
	}
	destruir_men_comun(aux_men);
}

void ordenar_lista_seg_por_dir_fisica(){
	bool _menor_dir_fisica(t_seg *seg1, t_seg *seg2){
		return seg1->dir_fisica < seg2->dir_fisica;
	}
	list_sort(list_seg, (void*)_menor_dir_fisica);
}

int32_t crearSegmento(t_men_seg *men_ped){
	int resp;
	pthread_mutex_lock(&mutex_list_seg);
	resp = buscar_espacio_mem_prin(men_ped->tam_seg);
	if (resp==-1){
		compactar();
		resp = buscar_espacio_mem_prin(men_ped->tam_seg);
		if (resp==-1){
			pthread_mutex_unlock(&mutex_list_seg);
			return -1;
		}
	}
	t_seg *aux_seg = malloc(sizeof(t_seg));//todo liberar
	switch(men_ped->tipo){
	case PED_MEM_SEG_COD:
		aux_seg->tipo_seg = CODIGO_SCRIPT;
		break;
	case PED_MEM_IND_ETI:
		aux_seg->tipo_seg = IND_ETI_FUNC;
		break;
	case PED_MEM_IND_COD:
		aux_seg->tipo_seg = IND_COD;
		break;
	case PED_MEM_SEG_STACK:
		aux_seg->tipo_seg = IND_STACK;
		break;
	default:
		aux_seg->tipo_seg = men_ped->tipo;
		break;
	}
	aux_seg->id_proc = men_ped->id_prog;
	aux_seg->tam_seg = men_ped->tam_seg;
	aux_seg->dir_fisica = resp;
	aux_seg->dir_logica = asignarMemoriaAleatoria(men_ped->tam_seg);
	list_add(list_seg, aux_seg);
	pthread_mutex_unlock(&mutex_list_seg);
	return aux_seg->dir_logica;
}

int32_t asignarMemoriaAleatoria(int32_t tamanio){//todo puede q se pisen pero es 1 en 256 a la 4
	int32_t ret = (rand());
	return ret;
}

int32_t buscar_espacio_mem_prin(int32_t tam_a_reservar){
	int32_t ind_mem=0,j;
	int32_t tam_seg = 0;
	int32_t mem_total = config_get_int_value(ptrConfig,"tamanio");
	t_seg *aux_seg = malloc(sizeof(t_seg));//todo liberar

	ordenar_lista_seg_por_dir_fisica();

	if (alg_actual == FIRST_FIT){
		for (j=0; j < list_size(list_seg);j++){
			aux_seg = list_get(list_seg, j);
			for(tam_seg=0; ind_mem<aux_seg->dir_fisica;ind_mem++,tam_seg++)
				if (tam_seg == tam_a_reservar)
					return ind_mem-tam_seg;
			ind_mem = ind_mem + aux_seg->tam_seg;
		}
		tam_seg = mem_total-ind_mem;
		if (tam_a_reservar <= tam_seg)
			return ind_mem;
		return -1;
	}else{
		int32_t pos_seg_max = -1;
		int32_t tam_seg_max = -1;
		for (j=0; j < list_size(list_seg);j++){
			aux_seg = list_get(list_seg, j);
			for(tam_seg=0; ind_mem<aux_seg->dir_fisica;ind_mem++,tam_seg++)
				if (tam_seg > tam_seg_max){
					tam_seg_max = tam_seg;
					pos_seg_max = ind_mem-tam_seg;
				}
			ind_mem = ind_mem + aux_seg->tam_seg;
		}
		tam_seg = mem_total-ind_mem;
		if (tam_seg > tam_seg_max){
			tam_seg_max = tam_seg;
			pos_seg_max = ind_mem;
		}
		if (tam_a_reservar <= tam_seg_max){
			return pos_seg_max;
		}else{
			return -1;
		}
	}
}

void compactar(){
	int i,j;
	int ind_mem=0, aux_dir_fisica;
	t_seg *aux_seg = malloc(sizeof(t_seg));//todo liberar

	ordenar_lista_seg_por_dir_fisica();
	for(i=0;i < list_size(list_seg);i++){
		aux_seg = list_get(list_seg, i);
		if(ind_mem != aux_seg->dir_fisica){
			aux_dir_fisica = ind_mem;
			for (j=0;j<aux_seg->tam_seg;j++)
				mem_prin[ind_mem]= mem_prin[aux_seg->dir_fisica+j];
			aux_seg->dir_fisica = aux_dir_fisica;
			ind_mem = ind_mem+aux_seg->tam_seg;
			list_replace(list_seg, i, aux_seg);
		}else{
			ind_mem = ind_mem+aux_seg->tam_seg;
		}
	}
}

void destruirSegmentos(int id_prog){
	bool es_id_prog(t_seg *seg){
		return seg->id_proc == id_prog;
	}
	void destruir_t_seg(t_seg *seg){
		free(seg);
	}
	pthread_mutex_lock(&mutex_list_seg);
	if (id_prog == 0){//si es 0 destruye todos los segmentos
		list_clean_and_destroy_elements(list_seg, (void*)destruir_t_seg);
	}else{
		while(list_any_satisfy(list_seg, (void*)es_id_prog))
			list_remove_and_destroy_by_condition(list_seg, (void*)es_id_prog, (void*)destruir_t_seg);
	}
	pthread_mutex_unlock(&mutex_list_seg);
}

//consola
void* crearConsola(){
	char opcion;
	do {
		menuPrincipal();
		do {
			scanf("%c", &opcion);
		} while (opcion<'1' || opcion>'6');

		switch (opcion) {
			case '1':
				operacion();
				break;
			case '2':
				cambiarRetardo();
				break;
			case '3':
				cambiarAlgoritmo();
				break;
			case '4':
				pthread_mutex_lock(&mutex_list_seg);
				compactar();
				pthread_mutex_unlock(&mutex_list_seg);
				break;
			case '5':
				imprimirDump();
				break;
			}
	}while (opcion != '6');
	return NULL;
}

void menuPrincipal(){
	printf ("--------------------------------\n"
			"Elija una opcion:\n"
			"	1.Operacion\n"
			"	2.Retardo\n"
			"	3.Cambiar Algoritmo\n"
			"	4.Compactacion\n"
			"	5.Dump\n"
			"	6.Salir\n");
}

void operacion(){
	char opcion;
	printf ("--------------------------------\n"
			"Que operacion desea realizar:\n"
			"	1.Crear Segmento\n"
			"	2.Destruir Segmento\n"
			"	3.Solicitar Memoria\n"
			"	4.Escribir Memoria\n");
	do {
		scanf("%c", &opcion);
	} while (opcion<'1' || opcion>'4');

	switch (opcion) {
	case '1':
	case '2':
		operacion_segmentos(opcion);
		break;
	case '3':
	case '4':
		operacion_memoria(opcion);
		break;
	}
}

void operacion_segmentos(char opcion){//todo hay algo q no me cierra
	int32_t tipo_seg, tam, id_proc;
	t_men_seg *men_seg;

	if (opcion == '1'){
		printf("ID del proceso:");
		scanf("%i",&id_proc);
		printf("Tipo de segmento:");
		scanf("%i",&tipo_seg);
		printf("Tamaño:");
		scanf("%i",&tam);
		men_seg = crear_men_seg(tipo_seg, id_proc, tam);
		crearSegmento(men_seg);
		destruir_men_seg(men_seg);
	}else{
		printf ("--------------------------------\n"
				"Ingrese 0 si desea destruir todos\n"
				"ID proceso: \n");
		scanf("%i",&id_proc);
		destruirSegmentos(id_proc);
	}
}

void operacion_memoria(char opcion){//todo hay algo q no me cierra
	int32_t base, offset, tam, id_proc,i, pos;
	char *buffer;

	printf("ID del proceso:");
	scanf("%i",&id_proc);
	printf("Base:");
	scanf("%i",&base);
	printf("Offset:");
	scanf("%i",&offset);

	if (opcion == '3'){
		printf("Tamaño:");
		scanf("%i",&tam);

		pos = base + offset;
		if ((offset+tam+base)>config_get_int_value(ptrConfig,"tamanio")){
			printf("MEMORY OVERLOAD\n");
		}else{
			buffer = malloc(tam);//todo liberar
			pthread_mutex_lock(&mutex_mem_prin);
			memcpy(&mem_prin[pos],buffer,tam);
			pthread_mutex_unlock(&mutex_mem_prin);
			for (i=0;i<tam;i++)
				printf("%i",buffer[i]);
		}
	}else{
		printf("Tamaño a escribir:");
		scanf("%i",&tam);
		buffer = malloc(tam);//todo liberar
		printf("Buffer:");
		scanf("%s",buffer);
		pos = base + offset;
		if ((offset+tam+base)>config_get_int_value(ptrConfig,"tamanio")){
			printf("MEMORY OVERLOAD\n");
		}else{
			pthread_mutex_lock(&mutex_mem_prin);
			memcpy(&mem_prin[pos],buffer,tam);
			pthread_mutex_unlock(&mutex_mem_prin);
		}
	}
}

void cambiarAlgoritmo(){
	if(alg_actual == WORST_FIT){
		alg_actual = FIRST_FIT;
		printf("Algoritmo cambiado a = First-Fit\n");
	}else{
		alg_actual = WORST_FIT;
		printf("Algoritmo cambiado a = Worst-Fit\n");
	}
}

void cambiarRetardo(){
	int32_t t;

	printf ("--------------------------------\n");
	printf("Ingrese el tiempo:");
	if (scanf("%i",&t)!= EOF && t>0){
		retardo = t;
		printf("Nuevo retardo: %i\n", t);
	}else{
		printf("Tiempo no válido.\n");
	}

}

void imprimirDump(){
	char opcion;
	printf ("--------------------------------\n"
			"Elija lo que desea imprimir:\n"
			"	1.Estructuras de memoria\n"
			"	2.Memoria principal\n"
			"	3.Contenido memoria principal\n");
	do {
		scanf("%c", &opcion);
	} while (opcion<'1' || opcion>'3');

	switch (opcion) {
	case '1':
		imp_estructura_mem();
		break;
	case '2':
		imp_mem_prin();
		break;
	case '3':
		imp_cont_mem_prin();
		break;
	}
}

void imp_estructura_mem(){
	int id_prog;

	printf ("--------------------------------\n"
			"Ingrese 0 si desea imprimir todos\n"
			"ID proceso: ");
	scanf("%i",&id_prog);
	int32_t resp = imp_tablas_segmentos(id_prog);
	if (resp == -1)
		printf("El proceso no existe\n");
}

int32_t imp_tablas_segmentos(int32_t id_prog){
	bool _es_del_proc(t_seg *seg){
		return seg->id_proc == id_prog;
	}
	void imp_seg(t_seg *seg){
		printf(" %i   |",seg->tipo_seg);
		printf(" %i  |",seg->id_proc);
		printf(" %i  |",seg->dir_fisica);
		printf(" %i  |",seg->tam_seg);
		printf(" %i  |\n",seg->dir_logica);
	}
	pthread_mutex_lock(&mutex_list_seg);
	if (id_prog == 0){
		printf("TIPO | ID  |DIR_F| TAM |DIR_L|\n");
		list_iterate(list_seg, (void *)imp_seg);
		pthread_mutex_unlock(&mutex_list_seg);
		return 1;
	}
	//si no existe el proceso retorna -1
	if (list_find(list_seg, (void*)_es_del_proc) ==NULL)
		return -1;
	//crea una lista con los segmentos del proceso y la imprime
	printf("TIPO | ID  |DIR_F| TAM |DIR_L|\n");
	list_iterate(list_filter(list_seg, (void*)_es_del_proc), (void *)imp_seg);
	pthread_mutex_unlock(&mutex_list_seg);

	return 1;
}

void imp_mem_prin(){
	int32_t ind_mem=0,j,i;
	t_seg *aux_seg = malloc(sizeof(t_seg));//todo liberar
	pthread_mutex_lock(&mutex_list_seg);
	ordenar_lista_seg_por_dir_fisica();

	//imprime la memoria tal cual esta, en los espacios no usados imprime un ■
	for (j=0; j < list_size(list_seg);j++){
		aux_seg = list_get(list_seg, j);
		for(;ind_mem < aux_seg->dir_fisica;ind_mem++){
			printf("■");
		}
		for(i=0;i < aux_seg->tam_seg;i++,ind_mem++)
			printf("%i",aux_seg->id_proc);
	}
	for(i=0;i < config_get_int_value(ptrConfig,"tamanio");i++,ind_mem++)
		printf("■");
	printf("\n");
	pthread_mutex_unlock(&mutex_list_seg);
}

void imp_cont_mem_prin(){
	int32_t offset, tam, i;

	printf ("Offset: ");
	scanf("%i",&offset);
	printf ("Cantidad de bytes: ");
	scanf("%i",&tam);
	if (tam + offset > config_get_int_value(ptrConfig,"tamanio")){
		printf ("Segmentation Fault\n");
	}else{
		pthread_mutex_lock(&mutex_mem_prin);
		for(i=offset;i<(offset+tam);i++)
			printf("%c",mem_prin[i]);
		printf("\n");
		pthread_mutex_unlock(&mutex_mem_prin);
	}
}

void encabezado(long byte, char *modo){
	printf("---------------** UMV: Unidad de Memoria Virtual **---------------\n");
	printf("Puerto: %s\n", config_get_string_value(ptrConfig,"puerto"));
	printf("Operando en modo: %s\n", modo);
	printf("Espacio reservado: %ld bytes\n", byte);
	printf("------------------------------------------------------------------\n\n");
}
