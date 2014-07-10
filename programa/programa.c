#include "programa.h"

int soc_kernel;
t_log *logger;

int main(int argc, char *argv[]){
	logger = log_create("programa.log",argv[1],true,LOG_LEVEL_INFO);
	log_info(logger,"Inicio de ejecicion");

	t_config *config=config_create(getenv("ANSISOP_CONFIG"));// comando para usar la variable de entorno   export ANSISOP_CONFIG=/home/victor/workspace/tp-2014-1c-hashtaggers/programa/programa_config

	printf("\n\n------------------------------Archivo Config----------------------------------------\n");
	char *ip_kernel=config_get_string_value(config, "IP_Kernel");
	char *puerto_kernel = config_get_string_value(config, "Puerto_Kernel");
	printf("	IP Kernel     = %s\n", ip_kernel);
	printf("	Puerto Kernel = %s\n", puerto_kernel);
	printf("------------------------------------------------------------------------------------\n\n");

	FILE *scriptAProcesar = fopen(argv[1],"r");
	int tamanioScript=tamanioArchivo(scriptAProcesar);
	char *script = malloc(tamanioScript);

	soc_kernel=socket_crear_client(puerto_kernel, ip_kernel);
	handshake_kernel(soc_kernel);

	int i=0;
	//Lee el script
	while(!feof(scriptAProcesar))
		script[i++]=fgetc(scriptAProcesar);
	fclose(scriptAProcesar);

	//Envio del script serializado
	t_men_comun *mensaje_cod_prog = crear_men_comun(CODIGO_SCRIPT,script,tamanioScript);
	socket_send_comun(soc_kernel,mensaje_cod_prog);
	log_info(logger,"Se envió el codigo del script al kernel");
	destruir_men_comun(mensaje_cod_prog);
	free(script);

	//Espero mensajes del kernel
	int fin_ejecucion = 0;
	t_men_comun *mensaje_recibido;
	while (fin_ejecucion != 1){
		mensaje_recibido = socket_recv_comun(soc_kernel);

		switch(mensaje_recibido->tipo){
		case IMPRIMIR_VALOR:
			for(i=0;i<mensaje_recibido->tam_dato;i++)
				printf("%c",mensaje_recibido->dato[i]);
			break;
		case IMPRIMIR_TEXTO:
			for(i=0;i<mensaje_recibido->tam_dato;i++)
				printf("%c",mensaje_recibido->dato[i]);
			printf("\n");
			break;
		case CPU_DESCONEC:
			printf("CPU DESCONECTADA\n");
			log_error(logger,"Se desconecto la CPU que estaba ejecutando el programa");
			fin_ejecucion = 1;
			break;
		case CONEC_CERRADA:
			printf("Se cayo la conexion con el kernel \n");
			log_error(logger,"Se cayó la conexion con el kernel ");
			fin_ejecucion = 1;
			break;
		case FIN_EJECUCION:
			printf("Ejecucion finalizada \n");
			log_info(logger,"Finalizo la ejecucion del script correctamente");
			fin_ejecucion = 1;
			break;
		case MEM_OVERLOAD:
			fin_ejecucion = 1;
			printf("Memory Overload\n");
			log_error(logger,"No hay memoria suficiente para ejecutar el programa");
			break;
		case VAR_INEX:
			fin_ejecucion = 1;
			printf("Acceso a variable global inexistente \n");
			log_error(logger,"Acceso a variable global inexistente");
			break;
		case SEM_INEX:
			fin_ejecucion = 1;
			printf("Acceso a semaforo inexistente \n");
			log_error(logger,"Acceso a semaforo inexistente");
			break;
		default:
			sleep(1);
			printf("%i\n",mensaje_recibido->tipo);
			printf("El tipo de dato recibido es erroneo\n");
			log_error(logger,"Se recibio un msj del kernel de tipo erroneo");
			fin_ejecucion = 1;
			break;
		}
		destruir_men_comun(mensaje_recibido);
	}

	close(soc_kernel);
	config_destroy(config);
	log_info(logger,"Fin de ejecucion");
	log_destroy(logger);
	return 0;
}

int tamanioArchivo(FILE *archivo){
	fseek(archivo, 0L, SEEK_END);
	int tamanio = ftell(archivo);
	fseek(archivo,0L, SEEK_SET);
	return tamanio;
}

void handshake_kernel(){
	t_men_comun *men_hs = crear_men_comun(HS_PROG,"",1);
	socket_send_comun(soc_kernel,men_hs);
	destruir_men_comun(men_hs);

	men_hs = socket_recv_comun(soc_kernel);

	if(men_hs->tipo != HS_KERNEL){		
		printf("ERROR se esperaba HS_KERNEL y se recibio %i\n",men_hs->tipo);
		log_error(logger,"No se establecio correctamente la conexion con el kernel");
	}else
		log_info(logger,"Se establecio la conexion con el kernel");
	destruir_men_comun(men_hs);
}
