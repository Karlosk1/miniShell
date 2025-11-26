#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
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

int manejador_cd(tline* linea); //esto es la funcion que cambia de directorio pero cd no debne manejarse mediante proceso hijo porque el directorio es parte del contexto del proceso
int manejador_exit();
int manejador_umask(int argc, char *argv[]);
int manejador_jobs(); //Muestra la lista de jobs
int manejador_fg(tline* linea);
tNodeJob* getJobXid(int id); //Obtiene el job mediante su id
void add_job(pid_t pgid,int id, const char *cmd);
void remove_job(pid_t pgid);

command_entry diccionariodeComandos[] = {
    {"cd", (command_func)manejador_cd},
    {"exit", (command_func)manejador_exit},
    {"umask", (command_func)manejador_umask},
    {"jobs", (command_func)manejador_jobs},
    {"fg", (command_func)manejador_fg},
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
    char *str;
    str = readline(">>> ");

    if (strlen(str)>0) {
        add_history(str); //Se mantiene un registro de los inputs del usuario
    }

    if (!str) { //Esto significa CtrlD
        printf("Saliendo...\n");
        exit(0);
    }

    tline* linea = tokenize(str);
    free(str);
    return linea; //Lo devolvemos tokenizado para poder trabajar con todo lo que usar tLine
}


int manejador_cd(tline* linea) {
    char* dir;
    tcommand comando = linea->commands[0]; //Es un unico comando ya que este manejador solo ejecuta cd

    if (comando.argc > 1) {
        dir = comando.argv[1];   // el usuario especificó un directorio
    } else {
        dir = getenv("HOME");  // si no, usar HOME
    }

    if (!dir) {
        printf(linea->redirect_error, "cd: Variable HOME no definida\n");
        return 1;
    }

    if (chdir(dir) == 0) {
        printf("Cambiando a %s", dir);
    }else {
        printf("Error al cambiar a %s: No se encuntra o no existe tal directorio", dir);
    }

    return 0;
}

int manejador_exit() {
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


int manejador_jobs(){
    tNodeJob* actual = listaJobs;
    while (actual != NULL) {
        printf("[%d] Corriendo %s\n",actual->id, actual->comando);
        actual = actual->siguiente;
    }
}

int manejador_fg(tline* linea) {
    int id;
    tcommand comando = linea->commands[0]; //Cogemos el primer comando aunque haya varios ya que es el que determina el fg
    if (comando.argc > 1) {
        id = atoi(comando.argv[1]);
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

void add_job(pid_t pgid,int id, const char *cmd) {
    tNodeJob* nodo = (tNodeJob*) malloc(sizeof(tNodeJob));
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

/*int parsePipe(char* str, char** strpiped) { //Recibe la cadena entera un un array de punteros donde pondra las dos mitades separadas por |
    for (int i = 0; i < 2; i++) { //Extrae maximo dos tokens, antes y despues de |
        strpiped[i] = strsep(&str, "|"); //strsep busca | devuelve lo anterior y modifica para apuntar al resto
        if (strpiped[i] == NULL) //Si no quedan tokens
            break;
    }
    return (strpiped[1] != NULL); //Devuelve 1 si hay parte derecha del pipe
}

void parseSpace(char* str, char** parsed) { //Recibe el string y rellena parsed con punteros a tokens terminando con NULL
    int i = 0;

    while ((parsed[i] = strsep(&str, " ")) != NULL) { //En cada iteracion parsed[i] apunta al proximo token
        if (strlen(parsed[i]) == 0) continue; //Si el token es cadena vacia omite incrementar i
        i++;
    }

    parsed[i] = NULL;
}*/

int ownCmdHandler(tline* linea) { //Deteceta y ejecuta comandos internos
    if (linea->ncommands == 0) return 0;

    tcommand comando = linea->commands[0];

    if (comando.filename == NULL) {
        return 0; //Si no hay comando devuelve 0, no hay comando
    }

    for (int i = 0; diccionariodeComandos[i].name != NULL; i++) { //Recorre diccionario
        if (strcmp(comando.filename, diccionariodeComandos[i].name) == 0) {
            //Compara para encontrarlo
            return diccionariodeComandos[i].func(comando.argc, comando.argv) == 1; //1 si exito
        }
    }
    return 0; // no es comando interno
}

int getNextId() {
    return siguienteId++;
}

void execArgs(tline* linea) { //Cuando solo se ejecuta un unico comando externo
    if (!linea || linea->ncommands == 0) return;

    tcommand comando = linea->commands[0]; //Se coge el primer comando porque esta diseñado unicamente para ejecutar 1 comando
    int bg = linea->background; //Coges el booleano de bg
    int id = getNextId(); // ID único para el job

    pid_t pid = fork(); //Proceso hijo

    if (pid == 0) {
        // === HIJO ===

        // Redirección de entrada
        if (linea->redirect_input) { //Si existe algo en esa variable
            freopen(linea->redirect_input, "r", stdin); //El hijo redirige la entrada estandar desde ahi
        }

        // Redirección de salida
        if (linea->redirect_output) {
            freopen(linea->redirect_output, "w", stdout); //Igual
        }

        // Redirección de errores
        if (linea->redirect_error) {
            freopen(linea->redirect_error, "w", stderr); //Igual
        }

        execvp(comando.argv[0], comando.argv); // Ejecuta comando. Si cierto nunca vuelve. El proceso hijo deja de ser la shell y pasa a ser el comando
        printf("execvp");              // Si falla, mostramos error
        exit(EXIT_FAILURE);
    }
    else if (pid > 0) {
        // === PADRE ===

        if (!bg) { //Foreground
            waitpid(pid, NULL, 0); //Espera a que el hijo cambie de estado
        } else { //No se espera(background)
            printf("[%d] %d\t%s &\n", id, pid, comando.filename);
            add_job(pid, id, comando.filename); // Guardamos el job
        }
    }
    else {
        printf("fork");
    }
}


/*int processString(char* str, char** parsed, char** parsedpipe){
    char* strpiped[2];//Buffer local para las dos partes si hay |
    int piped = 0; //Flag para saber si hay pipe

    piped = parsePipe(str, strpiped); //LLama a parsePipe, modifica str

    if (piped) { //Si hay pipe
        parseSpace(strpiped[0], parsed); //Tokeniza por la izquierda
        parseSpace(strpiped[1], parsedpipe); //Tokeniza por la derecha
    } else {
        parseSpace(str, parsed); //Si no hay pipe
    }

    if (ownCmdHandler(parsed, strlen(parsed))){
        return 0;
    } else {
        return 1 + piped; //Si no hay pipe 1+0, sino 1+1
    }
}*/

void execArgsPiped(tline* linea) {
    if (!linea || linea->ncommands < 2) return;

    tcommand cmd1 = linea->commands[0]; // Comando izquierdo del pipe
    tcommand cmd2 = linea->commands[1]; // Comando derecho del pipe
    int bg = linea->background;
    int id = getNextId(); // ID único si es background

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        return;
    }

    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork");
        return;
    }

    if (pid1 == 0) {
        // HIJO 1: lado izquierdo
        dup2(pipefd[1], STDOUT_FILENO); // salida al pipe
        close(pipefd[0]);
        close(pipefd[1]);

        if (linea->redirect_input) {
            freopen(linea->redirect_input, "r", stdin);
        }

        execvp(cmd1.argv[0], cmd1.argv);
        perror("execvp cmd1");
        exit(EXIT_FAILURE);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork");
        return;
    }

    if (pid2 == 0) {
        // HIJO 2: lado derecho
        dup2(pipefd[0], STDIN_FILENO); // entrada desde el pipe
        close(pipefd[0]);
        close(pipefd[1]);

        if (linea->redirect_output) {
            freopen(linea->redirect_output, "w", stdout);
        }

        if (linea->redirect_error) {
            freopen(linea->redirect_error, "w", stderr);
        }

        execvp(cmd2.argv[0], cmd2.argv);
        perror("execvp cmd2");
        exit(EXIT_FAILURE);
    }

    // PADRE
    close(pipefd[0]);
    close(pipefd[1]);

    if (!bg) {
        // Foreground -> esperar a que terminen ambos
        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);
    } else {
        // Background -> registrar job solo con el primer pid y el comando completo
        printf("[%d] %d\t%s | %s &\n", id, pid1, cmd1.filename, cmd2.filename);
        char job_cmd[1024];
        snprintf(job_cmd, sizeof(job_cmd), "%s | %s", cmd1.filename, cmd2.filename);
        add_job(pid1, id, job_cmd); // Solo necesitamos un ID de job
    }
}

void free_line(tline* linea) {
    if (!linea) return;

    for (int i = 0; i < linea->ncommands; i++) {
        tcommand cmd = linea->commands[i];

        // Liberar cada string de argv
        for (int j = 0; j < cmd.argc; j++) {
            free(cmd.argv[j]);
        }
        free(cmd.argv);

        // filename apunta a argv[0], así que no hay que liberarlo por separado
    }

    free(linea->commands);
    free(linea);
}


int main() {
    tline* entrada;

    init_shell(); // Limpia pantalla y saluda

    while (1) {
        printDir();          // Muestra directorio actual
        entrada = input();   // Obtiene línea tokenizada

        if (!entrada) continue; // Nada que hacer

        // Comprobamos si es comando interno
        if (ownCmdHandler(entrada)) {
            free_line(entrada); // Liberamos memoria del tline
            continue;
        }

        // Ejecutamos comando externo
        if (entrada->ncommands == 1) {
            // Un solo comando -> execArgs
            execArgs(entrada);
        } else if (entrada->ncommands >= 2) {
            // Pipe -> execArgsPiped
            execArgsPiped(entrada);
        }

        free_line(entrada); // Liberar memoria de tline después de ejecutar
    }

    return 0;
}
