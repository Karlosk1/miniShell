#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include "myshell.h"
#include <libc.h>
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


int manejador_cd(int argc, char *argv[]); //esto es la funcion que cambia de directorio pero cd no debne manejarse mediante proceso hijo porque el directorio es parte del contexto del proceso
int manejador_exit(int argc, char *argv[]);
int manejador_umask(int argc, char *argv[]);
int manejador_jobs(int argc, char *argv[]);
int manejador_fg(int argc, char *argv[]);


command_entry diccionariodeComandos[] = {
    {"cd", (command_func)manejador_cd},
    {"exit", (command_func)manejador_exit},
    {"umask", (command_func)manejador_umask},
    {"jobs", (command_func)manejador_jobs},
    {"fg", (command_func)manejador_fg},
    {NULL, NULL}
};

int main(int argc, char* argv[]) {
    //char linea[BUFFER_SIZE];
    char *inputStdin[MAX_ARGS]; //El argv[] de nuestra shell
    tline* args;
    do { //Mientras que no se haga Ctrl+D no se exitea.
        printf("msh>");
        fgets(inputStdin,sizeof(inputStdin),stdin);
        args = tokenize(inputStdin); //Devuelve el mandato tokenizado



    }while (1);

    /*init_shell();

    while (1) {
        printDir();

        if (input(inputString) != 0)
            continue;

        flagger = processString(inputString, parsedArgs, parsedArgsPiped);

        if (flagger == 1)
            execArgs(parsedArgs);

        if (flagger == 2)
            execArgsPiped(parsedArgs, parsedArgsPiped);
    }*/

    return 0;
}

/*void init_shell() {
    printf("\033[H\033[J");  // clear screen
    char *username = getenv("USER");
    printf("\n\n\nUSER is: @%s\n", username);
    sleep(1);
    printf("\033[H\033[J");
}

void printDir() {
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
}*/

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

int manejador_exit(int argc, char *argv[]);
int manejador_umask(int argc, char *argv[]);
int manejador_jobs(int argc, char *argv[]);
int manejador_fg(int argc, char *argv[]);


