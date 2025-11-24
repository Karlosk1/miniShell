#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include "myshell.h"
#include "parser.h"

#define BUFFER_SIZE 1024
#define MAX_ARGS 64

/*
 Funciones que tiene que tener nuestra shell
 - Ejecutar en foreground lineas con un solo mandato y 0 mas argumentos
 - Redireccion de stdin desde archivo y redireccion de salida a archivo
 - Ejecutar en foreground lineas con dos mandatos con sus respectivos argumentos haciendo pipe y posible redireccion de stdin desde archivo y redireccion de salida a archivo
 - Lo mismo con mas de dos mandatos
- Ser capaz de ejecutar el mandato cd (0,5 puntos). Mientras que la mayoría de los mandatos son programas
del sistema, cd es un mandato interno que debe ofrecer el propio intérprete de mandatos. El mandato cd debe
permitir el acceso a través de rutas absolutas y relativas, además de la posibilidad de acceder al directorio
especificado en la variable HOME si no recibe ningún argumento, escribiendo la ruta absoluta del nuevo
directorio actual de trabajo. Para el correcto cambio de directorio el mandato cd se debe ejecutar sin pipes.
- Ejecutar exit
- Ser capaz de ejecutar el mandato interno umask (1 punto). Este mandato controla la máscara con los permisos
de creación de nuevos ficheros. Cuando umask imprima los permisos por la salida estándar deberá hacerlo
en forma numérica. Bastará con admitir el parámetro mode únicamente en formato octal (con un 0
seguido de tres dígitos octales). Recuérdese que el mandato umask emplea la forma octal para especificar
los permisos que no se establecerán.
- Ser capaz de reconocer y ejecutar tanto en foreground como en background líneas con más de dos mandatos
con sus argumentos, enlazados con ‘|’, con redirección de entrada estándar desde archivo y redirección de
salida a archivo. Para su correcta demostración, se deben realizar los mandatos internos jobs y fg (2 puntos):
o jobs: Muestra la lista de procesos que se están ejecutando en segundo plano en la minishell (no es
necesario considerar aquellos procesos pausados con Ctrl-D). El formato de salida será similar al del
mandato jobs del sistema:
[1]+ Running find / -name hola | grep h &
- Reanudar la ejecucion de proceso en background identificado por el numero obtenido en jobs. SI no se le pasa ningun id sera el ultimo mandato en background
- Evitar que los mandatos lanzados en background y el minishell mueran al enviar desde el teclado la señal
SIGINT, mientras los procesos en foreground responderán ante ellas. El comportamiento del intérprete de
mandatos al pulsar Ctrl-C deberá ser similar al de bash.
 */

//Cada proceso se maneja dentro del main. Las funciones que debemos de implementar a parte son los manejadores de esos procesos
//Es decir si estamos en el proceso de cd, el proceso llama a una funcion EXTERNA que cambia el directorio y lo devuelve al proceso

/*void init_shell();

void printDir();

int input(char *str);*/


typedef int (*command_func)(int argc, char *argv[]); //definimos un tipo que es un puntero a funcion que tenga esos argumentos. Devuelve int
//
typedef struct { //Esto es basicamente un diccionario en c
    char *name;         // nombre del comando
    command_func func;  // puntero a la función que implementa el comando
} command_entry;

typedef struct job{
    int id; //id del job
    pid_t pgid; //id del grupo de proceso
    char* comando; //comando que contiene
    struct job* siguiente;
}tNodeJob;

typedef tNodeJob* tJobs;
tJobs listaJobs = NULL;
int siguienteId = 1;

int manejador_cd(int argc, char *argv[]); //esto es la funcion que cambia de directorio pero cd no debne manejarse mediante proceso hijo porque el directorio es parte del contexto del proceso
int manejador_exit(int argc, char *argv[]);
int manejador_umask(int argc, char *argv[]);
int manejador_jobs(int argc, char *argv[]); //Muestra la lista de jobs
int manejador_fg(int argc, char *argv[]);
tNodeJob* getJobXid(int id); //Obtiene el job mediante su id
void add_job(pid_t pgid, const char *cmd);
void remove_job(pid_t pgid);

command_entry diccionariodeComandos[] = {
    {"cd", (command_func)manejador_cd},
    {"exit", (command_func)manejador_exit},
    {"umask", (command_func)manejador_umask},
    {"jobs", (command_func)manejador_jobs},
    {"fg", (command_func)manejador_fg},
    {NULL, NULL}
};

typedef tNodeJob* tJobs;
tJobs listaJobs = NULL;
int siguienteId = 1;


void init_shell() {
    clear();
    char* username = getenv("USER");
    printf("\n\n\nUSER is: @%s\n", username);
    sleep(1);
    clear();
}


void printDir(){
    char currentDirectory[1024];
    getcwd(currentDirectory, sizeof(currentDirectory));
    printf("\nDir: %s", currentDirectory);
}

int input(char *str) {
    char *buf = readline("\n>>> ");

    if (!buf) return 1; // Ctrl+D or null input

    if (strlen(buf) > 0) {
        add_history(buf);
        strcpy(str, buf);
        free(buf);
        return 0;
    }

    free(buf);
    return 1;
}

int manejador_cd(int argc, char *argv[]) {
    char *dir = (argc > 1) ? argv[1] : getenv("HOME");
    if (!dir) {
        fprintf(stderr, "cd: Variable HOME no definida\n");
        return 1;
    }
    if (chdir(dir) != 0) {
        perror("cd");
        return 1;
    }
    return 0;
}

int manejador_exit(int argc, char *argv[]) {
    printf("Saliendo de la miniShell \n");
    exit(0);
}

int manejador_umask(int argc, char *argv[]) {
    mode_t old_mask;

    if (argc == 1) {
        old_mask = umask(0);  // Obtener valor actual
        umask(old_mask);      // Restaurar

        printf("0%03o\n", old_mask);
        return 0;
    }

    if (argc == 2) {
        char *endptr;
        mode_t new_mask = strtol(argv[1], &endptr, 8);

        if (*endptr != '\0') {
            fprintf(stderr, "umask: formato octal inválido: %s\n", argv[1]);
            return 1;
        }

        umask(new_mask);
        return 0;
    }

    fprintf(stderr, "Uso: umask [mode]\n");
    return 1;
}


int manejador_jobs(int argc, char *argv[]){
    tNodeJob* actual = listaJobs;
    while (actual != NULL) {
        printf("[%d] Corriendo %s\n",actual->id, actual->comando);
        actual = actual->siguiente;
    }
}

int manejador_fg(int argc, char *argv[]) {
    int id;

    if (argc > 1) {
        id = atoi(argv[1]);
    } else {
        if (listaJobs == NULL) {
            printf("fg: no hay trabajos\n");
            return 1;
        }
        id = listaJobs->id; //coge el id del ultimo job
    }

    tNodeJob *cabeza = getJobXid(id);
    if (cabeza == NULL) {
        printf("fg: job %d no encontrado\n", id);
        return 1;
    }

    // Dar el terminal al job
    tcsetpgrp(STDIN_FILENO, cabeza->pgid); //Las señales que mandes al terminal las recibirá este job

    printf("%s\n", cabeza->comando); //Imprime el commando que irá a fg

    // Reanudar ejecución si está parado
    kill(-cabeza->pgid, SIGCONT); //Envia la señal de reanudación a todo el grupo de procesos(-)

    // Esperar hasta que termine
    waitpid(-cabeza->pgid, NULL, WUNTRACED); //Espera el cambio(termine) en alguno del grupo. Cambiar el stado null a un int status y mirarlo

    // Devolver terminal a la shell
    tcsetpgrp(STDIN_FILENO, getpgrp()); //Devuelve el control del terminal a shell y devuelve el pgid del proceso actual, que es la shell

    remove_job(cabeza->pgid); //Elimina el job de la lista interna. Ya ha terminado. Hay que comprobar que todos los procesos del grupo han terminado
    return 0;
}

tNodeJob* getJobXid(int id) {
    tNodeJob* nodo = listaJobs;
    while (nodo!= NULL) {
        if (nodo->id == id) {
            return nodo;
        }
        nodo = nodo->siguiente;
    }
    return NULL;
}

void add_job(pid_t pgid, const char *cmd) {
    tNodeJob* nodo = (tNodeJob*) malloc(sizeof(tNodeJob));
    nodo->pgid =pgid;
    strcpy(nodo->comando, cmd);
    nodo->siguiente = listaJobs;
    listaJobs = nodo;
}

void remove_job(pid_t pgid) {
    // Búsqueda
    int encontrado = 0;
    tNodeJob * pAnt = NULL;
    tNodeJob * pAct = listaJobs;
    while (!encontrado && pAct != NULL) {
        if (pAct->pgid == pgid) {
            encontrado = 1;
        } else {
            pAnt = pAct;
            pAct = pAct->siguiente;
        }
    }

    // Borrado
    if (encontrado) {
        if (pAnt == NULL) { // if (pAct == *l) { Eliminar 1º
            listaJobs = (listaJobs)->siguiente;
        } else { // Eliminar cualquiera salvo el 1º
            pAnt->siguiente = pAct->siguiente;
        }
        free(pAct);
    }
}
int processString(char* str, char** parsed, char** parsedpipe){
    char* strpiped[2];
    int piped = 0;

    piped = parsePipe(str, strpiped);

    if (piped) {
        parseSpace(strpiped[0], parsed);
        parseSpace(strpiped[1], parsedpipe);
    } else {
        parseSpace(str, parsed);
    }

    if (ownCmdHandler(parsed)){
        return 0;
    } else {
        return 1 + piped;
    }
}



int main(int argc, char* argv[]) {
    char inputString[BUFFER_SIZE]; //El argv[] de nuestra shell
    char* parsedArgs[MAX_ARGS];
    char* parsedArgsPiped;
    tline* args;
    int flagger=0; //Servirá para identificar si es void, comando o piped
    
    init_shell();
    
    do {// print shell line
        printDir();

        if (input(*inputString)==0){
            continue;
        }
        
        flagger = processString(inputString, parsedArgs, parsedArgsPiped);

        // execute
        if (flagger == 1){
            execArgs(parsedArgs);
        }

        if (flagger == 2){
            execArgsPiped(parsedArgs, parsedArgsPiped);
        }

    }while (1);

    return 0;
}
