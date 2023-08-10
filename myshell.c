//Programa realizado por Daniel Requena Garrido y Raul Rodriguez Lopez Rey
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include <errno.h>

#define TOPE 1024


pid_t pidj[TOPE];//dos arrays para los procesos en bg, luego se usa en jobs
char nombresjobs[100][TOPE];

int idjobs=0;
int numjob;

void redireccion_entrada(tline *line);
void redireccion_salida(tline *line);
void redireccion_error(tline *line);
void handlerprocesofinalizado();
void unmandato(tline* line);
void dosmandatos(tline* line);
void sigchld_handler(int s);
void masdedosmandatos(tline *line);


char buffer[1024];
int main(void){

	signal(SIGINT,SIG_IGN);
	signal(SIGQUIT,SIG_IGN);
	numjob=0;
	char buf[1024];
	tline * line;
	int i;
	pid_t pid;
	int status;

	printf("msh > ");	
	while (fgets(buf, 1024, stdin)) {
		strcpy(buffer,buf);
		line = tokenize(buf);
		if ((line==NULL)|| line!=NULL && line->ncommands==0 ) {
			printf("msh > ");
			continue;
		}
		if (line->redirect_input != NULL) {
			printf("redirección de entrada: %s\n", line->redirect_input);
		}
		if (line->redirect_output != NULL) {
			printf("redirección de salida: %s\n", line->redirect_output);
		}
		if (line->redirect_error != NULL) {
			printf("redirección de error: %s\n", line->redirect_error);
		}
		if (line->background) {
			
		} 
		signal(SIGCHLD,handlerprocesofinalizado);

		if (line->ncommands==1){// comprobamos si el nº de comandos es 1

			if (strcmp("exit", line->commands[0].argv[0]) == 0)
			{
				exit(1);
			}
			else if(strcmp("jobs",line->commands[0].argv[0])==0){ //comprobamos si el comando es jobs
				if(numjob<TOPE){//si tiene mas del maximo dará error
					for(i=0;i<numjob;i++){//recorre los procesos en bg
						char state[12];
						strcpy(state,"Ejecutando");//pone los procesos en ejecutando por defecto
						char nombre[40];
						strcpy(nombre,(char*) nombresjobs[i]);//se copia el nombre del job indexado en una variable aparte
						char signo[2];
						char ampersand[2];
						strcpy(ampersand,"&");//se le da el valor por defecto
						if(i==numjob-1)
							strcpy(signo,"+");//si es el ultimo proceso de la lista pondra +
						if(i==numjob -2)
							strcpy(signo,"-");//si es el penultimo pondrá -
						if(waitpid(pidj[i],NULL,WNOHANG)!=0){//si los procesos se han terminado se cambia el estado a hecho
							strcpy(state,"Hecho");
							strcpy(ampersand,"");//se quita el ampersand
							pidj[i]=0;//se pone su pid a 0
							strcpy(nombresjobs[i],"");//Se le quita el nombre
							numjob=numjob -1;//Se corre el tope para sacarlo de la lista
						}
						printf("[%d]%s \t %s\t %s %s\n", i+1,signo,state,nombre,ampersand);//Se imprime  con formato
						
					}
				}
			}

			else if (strcmp("fg", line->commands[0].argv[0]) == 0){
			
				i=0;
				while((intptr_t)(line->commands[0].argv[1])!=(pidj[i]) && i<numjob){//para sacar la posicion en la que hay que empezar
				//a correr la lista y borrar el proceso de jobs
					i++;
				}
				if((intptr_t)(line->commands[0].argv[1])!=(pidj[i])){//se hace el casting de puntero a integer para poder compararlo con el pidj
				//del proceso que queremos poner en fg
					pid	=pidj[i];
					while(i<=numjob){
						pidj[i]=pidj[i+1];
						strcpy(nombresjobs[i],nombresjobs[i+1]);
						i++;
						
					}
					numjob--;
					waitpid(pid, &status,0);//como ahora esta en fg hay que esperar a que acabe
				}
				else{//si el proceso ya no se esta ejecutando en segundo plano, se indica
					printf("Ese job no existe o esta terminado");
				}
			}
			
				
			else if(strcmp(line->commands[0].argv[0],"cd")==0){//comprobamos si el comando es cd
				char directorio[TOPE];
				if(line->commands[0].argc>2){
					fprintf(stderr,"Error! Demasiados argumentos\n");
				}
				if(line->commands[0].argc==1){
					strcpy(directorio,getenv("HOME"));
					if(directorio==NULL)
						printf("No existe la variable $HOME\n");
				}
				else
					strcpy(directorio,line->commands[0].argv[1]);
				if(chdir(directorio)!=0){
					printf("Error al cambiar de directorio!\n");
				}
			}
				else{// cualquier otro comando
					unmandato(line);
				}
		}
		else if (line->ncommands==2){ //comprobamos si el nº de comandos es 2
			dosmandatos(line);
		}
		else{ // comprobamos si el nº de comando es mayor que 2
			masdedosmandatos(line);
		}
		printf("msh > ");	
	}
	return 0;
}
void unmandato(tline* line){
	pid_t aux;
	int status;
	int i=0;
	tcommand comando = line->commands[i];
	aux= fork();
		if(aux<0){
			fprintf(stderr,"fallo al crear el hijo\n %s \n",strerror(errno));
			exit(1);
		}
		if(aux==0){//proceso hijo, aqui ejecutamos el mandato
			//aqui debemos habilitar el Control C y las demas señales de interrupcion
			
			redireccion_entrada(line); // comprobamos si hay redirecciones de entrada
			redireccion_salida(line); // comprobamos si hay redirecciones de salida 
			redireccion_error(line); // comprobamos si hay redirecciones de error
			execvp(comando.argv[0],comando.argv);
			printf("%s: No se encuentra el mandato\n",line->commands[0].argv[0]);
			exit(1);
		}
		else{//proceso padre
			if(line->background!=1){//no esta en background
				signal(SIGINT,SIG_DFL);
				signal(SIGQUIT,SIG_DFL);
				waitpid(aux, NULL,0);//Se espera a que acabe el proceso hijo 
			}
			else{//esta en background y se añade al array de jobs
				//proceso de concatenacion para agregarlo al nombresjobs
			char argumentos1[200];
			char resultado2[200];
			char opcional1[200];
			
			char separador[5];
			strcpy(separador," ");
			strcpy(argumentos1,line->commands[0].argv[0]);
			strcpy(opcional1,argumentos1);
			for(int k=1;k<line->commands[0].argc;k++){
					strcat(opcional1,separador);
					strcat(opcional1,line->commands[0].argv[k]);	
				}
			strcpy(resultado2,opcional1);
			pidj[numjob]=aux;//Se agrega el pid
			strcpy(nombresjobs[numjob],resultado2);//Se agrega el nombre
			numjob++;//se incrementa en uno el numjob		
			}
	}
}

void handlerprocesofinalizado(){//evita que se creen procesos zombie
	int wstate;
	int i,j;
	for(i=0;i<numjob;i++){
		waitpid(pidj[i],&wstate,WNOHANG);
		
	}
}

void redireccion_entrada(tline * line){
	int redirec;
	if(line->redirect_input!=NULL){ //redireccion de entrada
		//open(cadena de la redireccion,forma de apertura,permisos)
		redirec=open(line->redirect_input,O_RDONLY,S_IRUSR);
		if(redirec==-1){
			fprintf(stderr,"%s: Error. el fichero no existe\n",line->redirect_input);
			exit(1);
		}
		else{
			dup2(redirec,0);
			close(redirec);
		}
	}
}
void redireccion_salida(tline * line){
	int redirec;
	if(line->redirect_output!=NULL){ //redireccion de salida
		redirec=open(line->redirect_output,O_WRONLY | O_CREAT | O_TRUNC,S_IRUSR | S_IWUSR);
		if(redirec==-1){
			fprintf(stderr,"%s: Error. el fichero no existe\n",line->redirect_output);
			exit(1);
		}
		else{
			dup2(redirec,1);
			close(redirec);
		}
	}
}
void redireccion_error(tline * line){
	int redirec;
	if(line->redirect_error!=NULL){ //redireccion de error
		redirec=open(line->redirect_error,O_WRONLY | O_CREAT | O_TRUNC,S_IRUSR | S_IWUSR);
		if(redirec==-1){
			fprintf(stderr,"%s: Error. el fichero no existe\n",line->redirect_error);
			exit(1);
		}
		else{
			dup2(redirec,2);
			close(redirec);
		}
	}
}

void dosmandatos(tline *line){
	char argumentos[40];
	pid_t aux1,aux2;
	int status;
	int p[2];
	pipe(p);

	aux1= fork(); // 1 HIJO: ejecuta el primer mandato
	if(aux1<0){
		fprintf(stderr,"fallo al crear el hijo\n %s \n",strerror(errno));
		exit(1);
	}
	if (aux1==0){

		close(p[0]); // cerramos la salida del pipe
		dup2(p[1],1); // redirigimos la salida estandar del proceso a la entrada del pipe
		close(p[1]); // cerramos la entrada del pipe

		redireccion_entrada(line); // comprobamos si hay redireccion de entrada
		redireccion_error(line); // comprobamos si hay redireccion de error
		execvp(line->commands[0].argv[0],line->commands[0].argv);
		printf("%s: No se encuentra el mandato\n",line->commands[0].argv[0]);
		exit(1);
	}
	aux2= fork(); // 2 HIJO: ejecuta el segundo mandato
	if(aux2<0){
		fprintf(stderr,"fallo al crear el hijo\n %s \n",strerror(errno));
		exit(1);
	}
	if (aux2==0){

		close(p[1]); // cerramos la entrada del pipe
		dup2(p[0],0); // redirigimos la salida del pipe a la entrada estandar del proceso
		close(p[0]); // cerramos la salida del pipe

		redireccion_salida(line); // comprobamos si hay redireccion de salida
		redireccion_error(line); // comprobamos si hay redireccion de eror
		execvp(line->commands[1].argv[0],line->commands[1].argv);
		printf("%s: No se encuentra el mandato\n",line->commands[1].argv[0]);
		exit(1);
	}
	else{ // PADRE/*El texto comentado en este else era para concatenar strings y que saliera el nombre completo(2 comandos y |) en el jobs , pero daba bugs , salia todo menos una letra o saltaba el `core generado` y decidimos poner el nombre
    //Del primer mandato
	
		char argumentos1[40];
		char argumentos2[40];
		char resultado1[40];
		char resultado2[40];
		
		for(int i=0; i<2; i++){
			close(p[0]); //cerramos la entrada y salida del pipe ya que el padre no va a interacturar con ellas
			close(p[1]);

			if(line->background!=1){//no esta en background
				signal(SIGINT,SIG_DFL);//aqui si debe de ejecutarse con normalidad la interrupcion y quit
				signal(SIGQUIT,SIG_DFL);
				wait(&status);
			}
			else{
			}	
		}
		if(line->background==1){//concatenacion de argumentos y pipes para meterlo al jobs
			char argumentos1[100];
			char resultado2[100];
			char resultado1[100];
			char opcional1[100];
			char opcional2[100];
			char separador[5];
			char pip[5];
			strcpy(pip," | ");
			strcpy(separador," ");
			for(int j=0; j<line->ncommands; j++){
				if (j==0){
					strcpy(argumentos1,line->commands[j].argv[0]);
					strcpy(opcional1,argumentos1);
					for(int k=1;k<line->commands[j].argc;k++){
						strcat(opcional1,separador);
						strcat(opcional1,line->commands[j].argv[k]);	
					}
					strcpy(argumentos1,opcional1);
				}
				else{
					strcat(argumentos1,pip);				
					strcat(argumentos1,line->commands[j].argv[0]);	
					strcpy(opcional1,argumentos1);
					for(int k=1;k<line->commands[j].argc;k++){
						strcat(opcional1,separador);
						strcat(opcional1,line->commands[j].argv[k]);
					}
					strcpy(argumentos1,opcional1);
				}
				strcpy(resultado2,argumentos1);
			}
			pidj[numjob]=aux2;//Se agrega el pid
			strcpy(nombresjobs[numjob],resultado2);//Se agrega el nombre completo
			numjob++;//se incrementa en uno el numjob
			}
	}
}
void masdedosmandatos(tline *line){
	pid_t pid;
	int status;

	int p[2]; // pipe principal
	int p_aux[2]; // pipe auxiliar que utilizaremos cuando un proceso necesite interactuar con 2 pipes(mandatos intermedios)

	pipe(p);
	for(int i=0; i<line->ncommands; i++){
		if(i>0 && i<line->ncommands-1) // comprobamos si es un mandato intermedio
			pipe(p_aux);
		pid = fork();
		if(pid<0){ // ERROR
			fprintf(stderr,"fallo al crear el hijo\n %s \n",strerror(errno));
			exit(1);
		}
		if(pid==0){ // HIJO
			if(i==0){ // comprobamos si es el 1 mandato
				close(p[0]); // cerramos la salida del pipe
				dup2(p[1],1); // redirigimos la salida estandar del proceso a la entrada del pipe
				close(p[1]); // cerramos la entrada del pipe

				redireccion_entrada(line); // comprobamos si hay redireccion de entrada
				redireccion_error(line); // comprobamos si hay redireccion de error
				execvp(line->commands[i].argv[0],line->commands[i].argv);
				printf("%s: No se encuentra el mandato\n",line->commands[i].argv[0]);
				exit(1);
			}
			else if(i==line->ncommands-1){ // comprobamos si es el ultimo mandato

				close(p[1]); // cerramos la entrada del pipe
				dup2(p[0],0); // redirigimos la salida del pipe a la entrada estandar del proceso
				close(p[0]); // cerramos la salida del pipe

				redireccion_salida(line); // comprobamos si hay redireccion de salida
				redireccion_error(line); // comprobamos si hay redireccion de error
				execvp(line->commands[i].argv[0],line->commands[i].argv);
				printf("%s: No se encuentra el mandato\n",line->commands[i].argv[0]);
				exit(1);
			}
			else{ // mandatos intermedios
				close(p[1]); // cerramos la entrada del pipe
				dup2(p[0],0); // redirigimos la salida del pipe a la entrada estandar del proceso
				close(p[0]); // cerramos la salida del pipe

				close(p_aux[0]); // cerramos la salida del pipe auxiliar
				dup2(p_aux[1],1); // redirigimos 
				close(p_aux[1]); // cerramos la entrada del pipe auxiliar

				execvp(line->commands[i].argv[0],line->commands[i].argv);
				printf("%s: No se encuentra el mandato\n",line->commands[i].argv[0]);
				exit(1);
			}
		}
		else{ // PADRE
			if(i!=0){ // comprobamos si es el 1 mandato
				close(p[0]); //cerramos la entrada y salida del pipe ya que 
				close(p[1]);
			}
			if(i>0 && i<line->ncommands-1){
				p[0]=p_aux[0]; // copiamos la salida del pipe auxiliar al pipe "principal"
				p[1]=p_aux[1]; // copiamos la entrada del pipe auxiliar al pipe "principal"
			}
			if(line->background!=1)//no esta en background
				signal(SIGINT,SIG_DFL);
				signal(SIGQUIT,SIG_DFL);
				wait(&status);
		}
		
	}
	if(line->background==1){//inicio de concatenacion del nombre concreto
			char argumentos1[100];
			char resultado2[100];
			char resultado1[100];
			char opcional1[100];
			char opcional2[100];
			char separador[5];
			char pip[5];
			strcpy(pip," | ");
			strcpy(separador," ");
			for(int j=0; j<line->ncommands; j++){
				if (j==0){
					strcpy(argumentos1,line->commands[j].argv[0]);
					strcpy(opcional1,argumentos1);
					for(int k=1;k<line->commands[j].argc;k++){
						strcat(opcional1,separador);
						strcat(opcional1,line->commands[j].argv[k]);	
					}
					strcpy(argumentos1,opcional1);
				}
				else{
					strcat(argumentos1,pip);				
					strcat(argumentos1,line->commands[j].argv[0]);	
					strcpy(opcional1,argumentos1);
					for(int k=1;k<line->commands[j].argc;k++){
						strcat(opcional1,separador);
						strcat(opcional1,line->commands[j].argv[k]);
					}
					strcpy(argumentos1,opcional1);
				}
				strcpy(resultado2,argumentos1);
			}
			pidj[numjob]=pid;//se agrega el pid
			strcpy(nombresjobs[numjob],resultado2);//se agrega el nombre
			numjob++;//se incrementa en uno el numjob

			}
	// cerramos todas las entradas y salidas de los pipes
	close(p[0]);
	close(p[1]);
	close(p_aux[0]);
	close(p_aux[1]);
}