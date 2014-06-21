#include "UMV.h"
int quit_sistema = 1;

int main(){

	extern void *ptrMemoria;

	srand(time(0));
	//crear configuracion y solicitar memoria
	ptrConfig = config_create("umv_config.txt");
	encabezado(config_get_int_value(ptrConfig,"tamanio"),config_get_string_value(ptrConfig,"modo"));
	ptrMemoria = malloc(config_get_int_value(ptrConfig,"tamanio"));
	tablaProgramas = dictionary_create();


	pthread_t hilo_consola;
	pthread_t hilo_conecciones;


	pthread_create( &hilo_conecciones, NULL, admin_conecciones,NULL);
	pthread_create( &hilo_consola, NULL, crearConsola, NULL);

	pthread_join( hilo_conecciones, NULL);
	pthread_join( hilo_consola, NULL);


	return 0;
}

void *crearConsola (){
	char **arrayComando;
	int a;
	char entrada[100];

	printf("UMV >> ");
		gets(entrada);
		arrayComando =  string_split(entrada," ");
		a = clasificarComando(arrayComando[0]);

		while (a != 6){
			switch (a){
				case 1:
					operacion(atoi(arrayComando[1]),atoi(arrayComando[2]),atoi(arrayComando[3]),atoi(arrayComando[4]));
					break;
				case 2:
					retardo(atoi(arrayComando[1]));
					break;
				case 3:
					algoritmo(arrayComando[1]);
					break;
				case 4:
					compactar();
					break;
				case 5:
					dump();
					break;
				default:
					printf("Comando desconocido");
					break;
				}

		printf("\nUMV >> ");
		gets(entrada);
		arrayComando =  string_split(entrada," ");
		a = clasificarComando(arrayComando[0]);
		}
	return NULL;
}

int clasificarComando (char *comando){
	int a;
	if (!strcmp(comando,"operacion")) a=1;
	if (!strcmp(comando,"retardo")) a=2;
	if (!strcmp(comando,"algoritmo")) a=3;
	if (!strcmp(comando,"compactacion")) a=4;
	if (!strcmp(comando,"dump")) a=5;
	if (!strcmp(comando,"exit"))
		{
		a=6;
		free(ptrMemoria);
		}
	printf("%d",a);
	return a;
}


void *admin_conecciones(){
	int listen_soc = socket_crear_server(config_get_string_value(ptrConfig,"puerto"));
	int new_soc;
	t_men_comun *men;

	while(quit_sistema){
		new_soc = socket_accept(listen_soc);
		men = socket_recv_comun(new_soc);
		if (men->tipo == CONEC_CERRADA){
			printf("Socket nº%i desconectado",new_soc);
			continue;
		}
		if (men->tipo >= HS_KERNEL_UMV){//si se conecta el kernel
			//todo crear hilo para administrar el kernel
			//acordarse de responder el handshake
			continue;
		}
		if (men->tipo >= HS_CPU_UMV){//si es una cpu nueva
			//todo crear hilo para administrar la cpu nueva
			//acordarse de responder el handshake
			continue;
		}
		printf("ERROR se esperaba recibir un tipo handshake y se recibio %i", men->tipo);
	}
	socket_cerrar(listen_soc);
	return NULL;
}

void operacion(int proceso, int base, int offset, int tamanio){
	printf("operacion");
}
void retardo(int milisegundos){
	printf("retardo");
}
//modifica el modo de operacion
void algoritmo(char *modo){
	if (!strcmp(modo,"WorstFit") || !strcmp(modo,"FirstFit"))
		modoOperacion=modo[0];
	else
		printf("Modo incorrecto");
}

bool compararListaAuxiliar(ListAuxiliar *nodo1, ListAuxiliar *nodo2){
	return ((nodo1->ptrInicio) < (nodo2->ptrInicio));
}

void compactar(){
	ListAuxiliar *p1 = malloc(sizeof(ListAuxiliar));
	ListAuxiliar *p2 = malloc(sizeof(ListAuxiliar));
	int x;
	TabMen *actualizar;
	void *aux1;
	void *aux2;
	/*recorrer tabla de programas y crear una lista auxiliar ordenada, preguntar si hay espacio
	entre dos nodos, si hay se desplaza la memoria y se actualiza la tabla de programas*/
	recorrerTablaSegmentos();
	list_sort(listaAuxiliar, (void*)compararListaAuxiliar);
	for (x=0;x<list_size(listaAuxiliar)-1;x++){
		p1 = list_get(listaAuxiliar,x);
		p2 = list_get(listaAuxiliar,x+1);

		if (p2->ptrInicio != (p1->ptrFin) +1)
		{
			int aux = (p1->ptrFin)+1;
			aux1 = (&ptrMemoria + aux);
			aux = p2->ptrInicio;
			aux2 = (&ptrMemoria + aux);
			memcpy(aux1, aux2,(p2->ptrFin - p2->ptrInicio));
			actualizar = p2->ptrATabla;
			actualizar->memFisica = p1->ptrFin+1;
			p2->ptrFin = p1->ptrFin+1 + (p2->ptrFin - p2->ptrInicio);
			p2->ptrInicio = p1->ptrFin+1;

		}
	}
	list_destroy(listaAuxiliar);
}

void dump(){
	printf("dump");
}


void encabezado(long byte, char *modo){
	printf("---------------** UMV: Unidad de Memoria Virtual **---------------\n");
	printf("Puerto: %s\n", config_get_string_value(ptrConfig,"puerto"));
	printf("IP: %d\n", config_get_int_value(ptrConfig,"IP"));
	printf("Operando en modo: %s\n", modo);
	printf("Espacio reservado: %ld bytes\n", byte);
	printf("------------------------------------------------------------------------\n\n");
}


void crearSegmento(char *id_Prog, int tamanio){
	void *lista;
	TabMen *nodoTab = malloc(sizeof(TabMen));
	/*pregunta si en la tabla de programas existe el id de prog,
	 si no existe se agrega el key al diccionario y si existe
	 se agrega un nodo a la lista de segmentos, controlando que
	 la memoria logica no se pise dentro del mismo programa */
	nodoTab->longitud = tamanio;
	nodoTab->memFisica = asignarMemoria(tamanio);

	if (dictionary_has_key(tablaProgramas, id_Prog))
		{
		lista = dictionary_get(tablaProgramas,id_Prog);
			do {
			memEstaOk = 1;
			nodoTab->memLogica = asignarMemoriaAleatoria(tamanio);
			//list_iterate(lista,controlarMemPisada(nodoTab->memLogica));
			}while (!memEstaOk);
		list_add(lista,nodoTab);
		}
	else
	{
		nodoTab->memLogica = asignarMemoriaAleatoria(tamanio);
		lista = list_create();
		list_add(lista,nodoTab);
		dictionary_put(tablaProgramas, id_Prog, lista);
	}

}
// revisar
void controlarMemPisada(TabMen *nodo, int numMemoria){
	if (((nodo->memLogica) < numMemoria) && ((nodo->memLogica + nodo->longitud) > numMemoria)){
		memEstaOk = 0;
	}

}

int asignarMemoria(int tamanio){
	ListAuxiliar *p1 = malloc(sizeof(ListAuxiliar));
	ListAuxiliar *p2 = malloc(sizeof(ListAuxiliar));
	int x;
	int espacioDisponible;
	int hayEspacio;
	int seCompacto;
	struct peorEspacio {
		int espacio;
		int offset;
	};

	struct peorEspacio pE;

	seCompacto =0;
	hayEspacio = 0;
	/*se recorre la tabla de segmentos y se crea una lista auxiliar
	 ordenada segun memoria fisica. Se comparan dos nodos, si hay
	 espacio entre ellos, se pregunta si ese espacio es suficiente, si
	 lo es se asigna segun el algoritmo en uso. Si no hay espacio se
	 compacta y se vuelve a buscar lugar. */
	recorrerTablaSegmentos();
	list_sort(listaAuxiliar, (void *)compararListaAuxiliar);

	etiqueta_1:
	for (x=0;x<list_size(listaAuxiliar)-1;x++){
		p1 = list_get(listaAuxiliar,x);
		p2 = list_get(listaAuxiliar,x+1);

		if (p2->ptrInicio != (p1->ptrFin) +1){
			espacioDisponible = (p2->ptrInicio - p1->ptrFin -1);
			if (espacioDisponible >= tamanio)
			{
				hayEspacio=1;
				switch (modoOperacion){
					case'F':
						{
							return (p1->ptrFin+1);
							break;
						}
					case 'W':
						{
						if (espacioDisponible > pE.espacio){
							pE.espacio = espacioDisponible;
							pE.offset = p1->ptrFin+1;
							}
						break;
						}
				}
			}
				if (hayEspacio) return pE.offset;
				if (!hayEspacio && !seCompacto){
					seCompacto = 1;
					compactar();
					goto etiqueta_1;
				}
				if (!hayEspacio && seCompacto) return -1;
		}
	}
	return 0;
}

int asignarMemoriaAleatoria(int tamanio){
	int numero;
	numero = (rand());
	return numero;
}

void recorrerTablaSegmentos(){
	listaAuxiliar= list_create();
	dictionary_iterator(tablaProgramas,recorrerLista);
	insertarNodosBarrera();
}

void recorrerLista(char* clave, void *ptrLista){
	list_iterate(ptrLista,(void *)completarListaAuxiliar);
}

void completarListaAuxiliar(TabMen *nodo){
	ListAuxiliar *nodoAux = malloc(sizeof(ListAuxiliar));

	nodoAux->ptrInicio = nodo->memFisica;
	nodoAux->ptrFin = nodo->memFisica + nodo->longitud;
	nodoAux->ptrATabla = nodo;

	list_add(listaAuxiliar, nodoAux);
}

void insertarNodosBarrera (){
	ListAuxiliar *nodoAux = malloc(sizeof(ListAuxiliar));
	nodoAux->ptrInicio = -1;
	nodoAux->ptrFin = -1;
	list_add_in_index(listaAuxiliar,0,nodoAux);
	nodoAux->ptrInicio=config_get_int_value(ptrConfig,"tamanio");
	nodoAux->ptrFin = config_get_int_value(ptrConfig,"tamanio");
	list_add(listaAuxiliar,nodoAux);
}

void destruirSegmentos(char *id_Prog){
	void *lista;
	lista = dictionary_get(tablaProgramas, id_Prog);
	list_destroy_and_destroy_elements(lista, (void *)eliminarElemento);
}
void eliminarElemento(void *elemento){
	free (elemento);
}
// error en iterator, en todas las funciones con orden superior
