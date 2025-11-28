#include <stdio.h>
#include <signal.h>
#include <readline/readline.h>
#include <unistd.h>
#include <readline/history.h>
#include <libc.h>
#include "parser.h"
#include "myshell.h"

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

typedef struct job {
    int id;
    pid_t pgid;
    char* comando;
    struct job* siguiente;
} tNodeJob;

typedef tNodeJob* tJobs;

tJobs listaJobs = NULL;
int siguienteId = 1;

// Comandos internos reciben tline*
typedef int (*command_func_tline)(tline* linea);

typedef struct {
    char *name;
    command_func_tline func;
} command_entry;

// Declaración de manejadores
int manejador_cd(tline* linea);
int manejador_exit(tline* linea);
int manejador_umask(tline* linea);
int manejador_jobs(tline* linea);
int manejador_fg(tline* linea);

// Diccionario de comandos internos
command_entry diccionariodeComandos[] = {
    {"cd", manejador_cd},
    {"exit", manejador_exit},
    {"umask", manejador_umask},
    {"jobs", manejador_jobs},
    {"fg", manejador_fg},
    {NULL, NULL}
};


void clear() {
    printf("\033[H\033[J");
    //\033[H Mueve el cursos a la esquina superior izquierda
    //\033[J Borra la pantalla desde el cursor hasta el final
}


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

tline* input() { //Str es donde se almacenara lo leido
    char *str = readline(">>> ");

    if (!str) { //Esto significa CtrlD
            printf("Saliendo...\n");
            exit(0);
    }

    if (strlen(str)>0) {
        add_history(str); //Se mantiene un registro de los inputs del usuario
    }

    tline* linea = tokenize(str);
    free(str);
    return linea; //Lo devolvemos tokenizado para poder trabajar con todo lo que usar tLine
}

void add_job(pid_t pgid,int id, const char *cmd) {
    tNodeJob* nodo = realloc(tNodeJob, (tJobs.size+1)*sizeof(tNodeJob)); //Hacer la funcion size
    if (nodo == NULL) {
        printf("Error en la reserva de memoria\n");
        return;
    }
    nodo->pgid =pgid;
    nodo->id = id;
    nodo->comando = strdup(cmd); //Reserva memoria
    nodo->siguiente = listaJobs;
    listaJobs = nodo;
}

void remove_job(pid_t pgid) {
    tNodeJob *pAnt = NULL;
    tNodeJob *pAct = listaJobs;

    while (pAct != NULL) {
        if (pAct->pgid == pgid) break;
        pAnt = pAct;
        pAct = pAct->siguiente;
    }

    if (pAct != NULL) {
        if (pAnt == NULL)
            listaJobs = pAct->siguiente;
        else
            pAnt->siguiente = pAct->siguiente;

        free(pAct->comando);  // liberar string
        free(pAct);           // liberar nodo
    }
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

int getNextId() {
    return siguienteId++;
}


int manejador_cd(tline* linea) {
    tcommand comando = linea->commands[0];
    char* dir = (comando.argc > 1) ? comando.argv[1] : getenv("HOME");
    if (!dir) { printf("cd: Variable HOME no definida\n"); return 1; }
    if (chdir(dir) == 0) printf("Cambiando a %s\n", dir);
    else printf("Error al cambiar a %s: No existe\n", dir);
    return 0;
}

int manejador_exit(tline* linea){
    printf("Saliendo de la miniShell \n");
    exit(0);
    return 0;
}

int manejador_umask(tline* linea) {
    tcommand cmd = linea->commands[0];
    mode_t old_mask;
    if (cmd.argc == 1) {
        old_mask = umask(0);
        umask(old_mask);
        printf("0%03o\n", old_mask);
        return 0;
    }
    if (cmd.argc == 2) {
        char *endptr;
        mode_t new_mask = strtol(cmd.argv[1], &endptr, 8);
        if (*endptr != '\0') { fprintf(stderr, "umask: formato octal inválido: %s\n", cmd.argv[1]); return 1; }
        umask(new_mask);
        return 0;
    }
    printf("Error: Uso umask [mode]\n");
    return 1;
}

int manejador_jobs(tline* linea){
    tNodeJob* actual = listaJobs;
    while (actual != NULL) {
        printf("[%d] Corriendo %s\n",actual->id, actual->comando);
        actual = actual->siguiente;
    }
    return 0;
}

int manejador_fg(tline* linea) {
    tcommand comando = linea->commands[0];
    int id = (comando.argc > 1) ? atoi(comando.argv[1]) : (listaJobs ? listaJobs->id : -1);
    if (id == -1) { printf("fg: no hay trabajos\n"); return 1; }

    tNodeJob *job = getJobXid(id);
    if (!job) { printf("fg: job %d no encontrado\n", id); return 1; }

    tcsetpgrp(STDIN_FILENO, job->pgid);
    printf("%s\n", job->comando);
    kill(-job->pgid, SIGCONT);
    waitpid(-job->pgid, NULL, WUNTRACED);
    tcsetpgrp(STDIN_FILENO, getpgrp());
    remove_job(job->pgid);
    return 0;
}

int ownCmdHandler(tline* linea) {
    if (linea->ncommands == 0 || !linea->commands[0].filename) return 0;
    tcommand cmd = linea->commands[0];

    for (int i = 0; diccionariodeComandos[i].name != NULL; i++) {
        if (strcmp(cmd.filename, diccionariodeComandos[i].name) == 0)
            return diccionariodeComandos[i].func(linea);
    }
    return 0;
}

void execArgs(tline* linea) {
    if (!linea || linea->ncommands == 0) return;

    tcommand cmd = linea->commands[0];
    int bg = linea->background;
    int id = getNextId();
    pid_t pid = fork();

    if (pid == 0) {
        if (linea->redirect_input) freopen(linea->redirect_input, "r", stdin);
        if (linea->redirect_output) freopen(linea->redirect_output, "w", stdout);
        if (linea->redirect_error) freopen(linea->redirect_error, "w", stderr);
        execvp(cmd.argv[0], cmd.argv);
        perror("execvp"); exit(EXIT_FAILURE);
    } else if (pid > 0) {
        if (!bg) waitpid(pid, NULL, 0);
        else { printf("[%d] %d\t%s &\n", id, pid, cmd.filename); add_job(pid, id, cmd.filename); }
    } else perror("fork");
}

void execArgsPiped(tline* linea) {
    if (!linea || linea->ncommands < 2) return;

    tcommand cmd1 = linea->commands[0];
    tcommand cmd2 = linea->commands[1];
    int bg = linea->background;
    int id = getNextId();
    int pipefd[2];

    if (pipe(pipefd) < 0) { perror("pipe"); return; }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        if (linea->redirect_input) freopen(linea->redirect_input, "r", stdin);
        execvp(cmd1.argv[0], cmd1.argv);
        perror("execvp cmd1"); exit(EXIT_FAILURE);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        if (linea->redirect_output) freopen(linea->redirect_output, "w", stdout);
        if (linea->redirect_error) freopen(linea->redirect_error, "w", stderr);
        execvp(cmd2.argv[0], cmd2.argv);
        perror("execvp cmd2"); exit(EXIT_FAILURE);
    }

    close(pipefd[0]); close(pipefd[1]);
    if (!bg) { waitpid(pid1, NULL, 0); waitpid(pid2, NULL, 0); }
    else {
        char job_cmd[1024];
        snprintf(job_cmd, sizeof(job_cmd), "%s | %s", cmd1.filename, cmd2.filename);
        printf("[%d] %d\t%s &\n", id, pid1, job_cmd);
        add_job(pid1, id, job_cmd);
    }
}

void free_line(tline* linea) {
    if (!linea) return;
    if (linea->commands) free(linea->commands);
    free(linea);
}

int main() {
    tline* entrada;
    init_shell();

    while (1) {
        printDir();
        entrada = input();
        if (!entrada) continue;

        if (entrada->commands && entrada->commands->filename &&
            strcmp(entrada->commands->filename, "exit") == 0) break;

        if (ownCmdHandler(entrada)) { free_line(entrada); continue; }

        if (entrada->ncommands == 1) execArgs(entrada);
        else if (entrada->ncommands >= 2) execArgsPiped(entrada);

        free_line(entrada);
    }
    return 0;
}