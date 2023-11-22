// Codigo entrega Practica 2
// Santiago Mateos Fernandez

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>


int main(int argc, char *argv[]){

	//variables bucle, ficheros
	bool bucle = false;
	FILE *fp;
	int fr; //fichero redireccion
	int fr_stdout;
	int fr_stderr;
	int exec_redir;
	
	//variables extracion comandos
	char *buffer = NULL;
	char *buffer_mod;
	char delim[] = "&";
	char *com;
	char *part;
	size_t bufsize = 0;
	ssize_t count;
	
	//variables ejecucion comandos, conteo, flags...
	int cont;
	int nargs;
	int var = 0;
	pid_t proceso;
	char error_message[30] = "An error has occurred\n";
	int modo = 1; //modo interactivo
	bool redir = false; //flag para redireccion

	//variables gestion espacios y tabulaciones
	int i;
	int j;
	bool comd; //flag deteccion comando
	
	//variables chequeo fichero de redireccion
	FILE *file;
	bool ejec = true;
	
	
	if(argc == 2){
		//modo por lotes
		modo = 2;
		fp = fopen(argv[1], "r");
		if(fp == NULL){
			fprintf(stderr, "%s", error_message);
			exit(1);
		}
		
	}else if(argc > 2){
		//si hay mas de un argumento, error
		fprintf(stderr, "%s", error_message);
		exit(1);
	}

	while(!bucle){
		
		//lectura cadena / linea fichero
		do{
			if(modo == 1){ //Modo interactivo
			
				printf("Uvash> ");
				if((count = getline(&buffer, &bufsize, stdin)) < 0){
					exit(0);
				}
			
			}else{	//Modo por lotes
				// si count < 0, fichero se ha terminado y se cierra
				if((count = getline(&buffer, &bufsize, fp)) < 0){
					fclose(fp);
					exit(0);
				}
			}
			
			i = 0;
			j = 0;
			comd = false;
			
			//trataminto de tabulaciones y espacios al comienzo del comando
			do{
				if(buffer[i] == 32 || buffer[i] == 9){
					j++;
				}else{
					comd = true; //apunta al comando 
				}
			
				i++;
			
			}while(i < count && !comd);
			
			//nuevo puntero que apunta al comienzo del comando
			buffer_mod = buffer + j;
			
		//ignora el "comando" si su tamaño es menor de 2
		}while(strlen(buffer_mod) < 2);
		
		
		//soluciona el problema de un & al final
		if(buffer[strlen(buffer_mod)-2] == 38 && strlen(buffer_mod) != 2){
			buffer[strlen(buffer_mod)-2] = 0;
			
		}
		
		//si ponen ">" solo
		if(buffer[strlen(buffer_mod)-2] == 62){
			fprintf(stderr, "%s", error_message);
			exit(0);
		}
		
		//Tratamiento de comandos enlazados (&)
		part = strtok(buffer_mod, delim);
		
		while(part != NULL){
		
				cont = 1;
				
				//si la parte comienza por > o & -> error
				if(part[0] == 38 || part[0] == 62){
					fprintf(stderr, "%s", error_message);
					exit(0);
				}
		
				//bucle para contar el numero de argumentos
				for(int i = 1; i < strlen(part)-1; i++){
					if(part[i]== 32 && part[i+1] != 32){
						cont++;
					}
				}
		
				char *arg[cont+1]; //se debe crear aqui porque tamanio difiere en funcion de argumentos
				var = 0;
		
				//bucle para extraer comando + argumentos
				while( (com = strsep(&part, " ")) != NULL){

					//quitar el espacio al final del comando			
					if(com[strlen(com)-1] == 10){
						com[strlen(com)-1] = 0;
					}
			
					//exit sin argumentos
					if(strcmp(com,"exit")==0){
					
						if(cont == 1){
							free(buffer);
							exit(0);
						}else{
							fprintf(stderr, "%s", error_message);
							ejec = false; //evitamos la creacion del hijo
						}
					}

					//Tratamiento redireccion
					if(com[0] == 62){
						
						nargs = 1;
						
						//guardado salida estandar y salida error estandar
						fr_stdout = dup(1);
						fr_stderr = dup(2);
						
						//conteo nº args tras ">"
						for(int i = 1; i < strlen(part)-1; i++){
							if(part[i] == 32 && part[i+1] != 32){
								nargs++;
							}
						}
						
						if(nargs != 1){
							fprintf(stderr, "%s", error_message);
							exit(0);
						}
						
						
						if((com = strsep(&part, " ")) != NULL){
							
							//si solo hay espacio, error
							if(com[0] == 32){
								fprintf(stderr, "%s", error_message);
								exit(1);
							}
							
							//quita el \n al final del nombre del fichero
							if(com[strlen(com)-1] == 10){
								com[strlen(com)-1] = 0;
							}
							
							//quita el espacio al final del nombre del fichero
							if(com[strlen(com)-1] == 32){
								com[strlen(com)-1] = 0;
							}
							
							//comproabacion existencia fichero
							if(!(file = fopen(com, "r"))){
								
								//abre el fichero para redireccion o lo crea con todos los permisos
								fr = open(com, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0777);
								if(fr < 0){
									fprintf(stderr, "%s", error_message);
									exit(1);
								}
							
								redir = true;
								
								
							}else{	//si existe, ignora la redireccion y no ejecuta el comando
								fclose(file);
								ejec = false;
							}
							
						}else{	//si despues de ">" esta vacio -> error
							fprintf(stderr, "%s", error_message);
						}
						
					}else{
						//Solo se guardan argumentos que no sean nulos y antes de ">"
						if(com[strlen(com)-1] != 0){
							arg[var] = com;			
							var++;
						}
					}
				}
				
				//necesario anadir NULL al final de los args
				arg[var] = NULL;
				
				//comando se ejecuta si es salida estandar o redireccion a fichero nuevo
				if(ejec){
				
					//Comando cd fuera, no usa execvp, no necesita fork()
					if(strcmp(arg[0], "cd") == 0){
					
						//Error si cd tiene mas o menos de un argumento
						if(cont < 2 || cont > 2){
							fprintf(stderr, "%s", error_message);
						}else{
							if(chdir(arg[1]) != 0){
								fprintf(stderr, "%s", error_message);
							}
						}

					}else{
					
						//Proceso hijo para execvp
						proceso = fork();
		
						if(proceso < 0){
							fprintf(stderr, "%s", error_message);
						}
		
						if(proceso == 0){
						
							//redireccion de comandos a ficheros
							if(redir){
								
								dup2(fr, 1);	
							
								exec_redir = execvp(arg[0], arg);
								
								//comprobacion del comando
								if(exec_redir != 0){
									dup2(fr, 2);
									fprintf(stderr, "%s", error_message);
									dup2(fr_stderr, 2);
								}
							
								close(fr);
						
							}else{ //ejecucion normal, salida estandar
								if(execvp(arg[0], arg) != 0){
									fprintf(stderr, "%s", error_message);
								}
							}
			
							//fin del hijo
							exit(EXIT_SUCCESS);
				
				
						}else{
							
							//cierre de fichero en el padre
							//vuelta a salida estandar
							if(redir){
								dup2(fr_stdout, 1);
								redir = false;
								close(fr);
							}
						
							//Proceso padre espera proceso hijo
							wait(NULL);
						}	
				
					}
			
			}
			
			ejec = true;
			part = strtok(NULL, delim);
			
		}
		
	}
	
	//cierre fichero
	if(modo == 2){
		fclose(fp);
	}
	
	//liberacion memoria
	free(buffer);
	free(com);
	free(part);

	return(0);
}

