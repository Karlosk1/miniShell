#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h> // Necesario para gestionar interrupciones de señal
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "parser.h"

//resuelve warning de unresolved symbol
extern int rl_catch_signals;

//TAD jobs como array dinamico

typedef struct {
    int id;
    pid_t pgid;
    char* comando;
} tJob;

tJob* jobs_Array = NULL;
int contador_Jobs = 0;
int jobs_capacity = 0;
int siguienteId = 1;

//Creamos un diccionario para manejar los comandos internos

typedef int (*funcion_tLine)(tline* linea);

//Par nombre-funcion

typedef struct {
    char *nombre;
    funcion_tLine funcion;
} command_entry;

// Declaraciones de las funciones internas para que el diccionario funcione

int manejador_cd(tline* linea);
int manejador_exit(tline* linea);
int manejador_umask(tline* linea);
int manejador_jobs(tline* linea);
int manejador_fg(tline* linea);

command_entry diccionariodeComandos[] = {
    {"cd", manejador_cd},
    {"exit", manejador_exit},
    {"umask", manejador_umask},
    {"jobs", manejador_jobs},
    {"fg", manejador_fg},
    {NULL, NULL}
};

void limpiarEntrada() {
    printf("\033[H\033[J"); //033 es ESC en octal, codigo de escape en terminal [H lo pone la izquierda del todo el cursor. [J limpia desde el cursor hasta el final de la pantalla. como estamos a la izquierda del todo borra toda la pantalla
}

void manejador_CrtlC() {
    // Imprimir salto de línea antes del prompt
    write(STDOUT_FILENO, "\n", 1); //Escribe en el descriptor de archivo standar de salida un \n de 1 byte

    // Limpiar la línea actual
    rl_replace_line("", 0);
    rl_on_new_line();   // Informar a readline que la línea terminó
    rl_redisplay();     // Reimprime prompt inmediatamente
}

// Funciones relacionadas con la gestion de jobs

void add_job(pid_t pgid, int id, const char *cmd) {
    if (contador_Jobs >= jobs_capacity) {
        int new_capacity = (jobs_capacity == 0) ? 4 : jobs_capacity * 2;
        tJob* temp = realloc(jobs_Array, new_capacity * sizeof(tJob));
        if (!temp) {
            perror("realloc"); return;
        }
        jobs_Array = temp;
        jobs_capacity = new_capacity;
    }
    jobs_Array[contador_Jobs].pgid = pgid;
    jobs_Array[contador_Jobs].id = id;
    jobs_Array[contador_Jobs].comando = strdup(cmd);
    contador_Jobs++;
}

//Libera el indice y desde el indice hasta el final retrae todo el array una posicion

void removeJobxIndex(int index) {
    if (index >= 0 && index < contador_Jobs) {
        free(jobs_Array[index].comando);
        for (int i = index; i < contador_Jobs - 1; i++) {
            jobs_Array[i] = jobs_Array[i + 1];
        }
        contador_Jobs--;
    }
}

//Dado un pgid recorre el array buscando el pdgid y cuando lo encuentra borra el job

void removeJobxPgid(pid_t pgid) {
    for (int i = 0; i < contador_Jobs; i++) {
        if (jobs_Array[i].pgid == pgid) {
            removeJobxIndex(i);
            return;
        }
    }
}

tJob* getJobxId(int id) {

    for (int i = 0; i < contador_Jobs; i++) {
        if (jobs_Array[i].id == id) {
            return &jobs_Array[i]; //Devuelve la direccion de memoria. Es un puntero a tJob no una copia, para modificar el array
        }
    }
    return NULL;
}

int getSiguienteId() {
    return siguienteId++;
}

void comprobarJobsTerminados() {

    for (int i = 0; i < contador_Jobs; i++) {

        //Status recibe su valor despues de waitpid

        int estatus;

        pid_t resultado = waitpid(-jobs_Array[i].pgid, &estatus, WNOHANG); // Espera sin bloqueo por cualquier proceso del grupo pgid

        if (resultado > 0) {
            printf("[%d]+  Done\t\t%s\n", jobs_Array[i].id, jobs_Array[i].comando);
            removeJobxIndex(i);
            i--; //Se resta 1 posicion por cada job eliminado
        }
    }
}

// Gestion de la interfaz de shell

void iniciar_Shell() {
    limpiarEntrada();

    // readline no maneja todas las señales por sí mismo
    rl_catch_signals = 0;

    // se usa sigaction para manejar SIGINT en el control de CtrlC
    struct sigaction saction;
    saction.sa_handler = manejador_CrtlC;
    sigemptyset(&saction.sa_mask);
    saction.sa_flags = 0;
    sigaction(SIGINT, &saction, NULL);

    // Ignorar otras señales conflictivas
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    char* username = getenv("USER");
    printf("\n\n\nUSER is: @%s\n", username);
    sleep(1);
    limpiarEntrada();
}

// Gestion de la entrada

tline* input() {
    char *str = readline("msh> ");

    if (str == NULL) {
        if (errno == EINTR) {
            errno = 0;
            return NULL;      // vuelve al bucle principal y se reimprime
        }

        // Ctrl+D
        printf("\nSaliendo...\n");
        for(int i = 0; i < contador_Jobs; i++) free(jobs_Array[i].comando);
        free(jobs_Array);
        exit(0);
    }

    if (strlen(str) > 0) add_history(str);

    tline *linea = tokenize(str);
    free(str);
    return linea;
}

// Manejadores de comandos internos

int manejador_cd(tline* linea) {
    tcommand cmd = linea->commands[0];
    char* dir;

    if (cmd.argc > 1) dir = cmd.argv[1];
    else dir = getenv("HOME");

    if (dir == NULL) {
        fprintf(stderr, "cd: variable HOME no definida\n");
        return 1;
    }

    if (chdir(dir) != 0) {
        perror("cd");
        return 1;
    }

    // Obtener directorio absoluto actual
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Directorio cambiado a: %s\n\n", cwd);
    } else {
        perror("getcwd");
        printf("Directorio cambiado.\n\n");
    }

    return 0;
}

int manejador_exit(tline* linea) {
    printf("Saliendo de la miniShell...\n");
    for(int i=0; i<contador_Jobs; i++) {
        free(jobs_Array[i].comando);
    }

    //Libera el array de jobs cuando sale
    free(jobs_Array);
    exit(0);
}

int manejador_umask(tline* linea) {
    tcommand cmd = linea->commands[0];
    mode_t mascara_vieja = umask(0);
    umask(mascara_vieja);

    if (cmd.argc == 1) {
        printf("%03o\n", mascara_vieja);
    }
    else if (cmd.argc == 2) {

        //Puntero al final del numero
        char *endptr;

        //strtol devuelve long
        long val = strtol(cmd.argv[1], &endptr, 8);

        if (*endptr != '\0' || val < 0 || val > 0777) {
            fprintf(stderr, "umask: número octal inválido: %s\n", cmd.argv[1]);
            return 1;
        }

        mode_t mascara_nueva = (mode_t)val;
        umask(mascara_nueva);
        printf("%03o\n", mascara_nueva);
    }
    return 0;
}

//Se declara linea aunque no se use para que no de fallo en el diccionario

int manejador_jobs(tline* linea) {

    //Recorre el array de jobs e imprime el id y el comando de cada job que esta corriendo
    for (int i = 0; i < contador_Jobs; i++) {
        printf("[%d]+ Running\t%s\n", jobs_Array[i].id, jobs_Array[i].comando);
    }
    return 0;
}

int manejador_fg(tline* linea) {
    tcommand cmd = linea->commands[0];
    int id = -1;

    // Obtener ID

    //Si hay mas de un job corriendo en bg
    if (cmd.argc > 1) {
        id = atoi(cmd.argv[1]);
    } else {
        if (contador_Jobs > 0) id = jobs_Array[contador_Jobs - 1].id;
    }

    if (id == -1) {
        printf("fg: no hay trabajos\n");
        return 1;
    }

    tJob *job = getJobxId(id);
    if (!job) {
        printf("fg: trabajo %d no encontrado\n", id);
        return 1;
    }

    printf("%s\n", job->comando);
    pid_t pgid = job->pgid;

    // Control de terminal
    tcsetpgrp(STDIN_FILENO, pgid);

    // Continuar si estaba parado
    kill(-pgid, SIGCONT);

    int estatus;
    waitpid(-pgid, &estatus, WUNTRACED);

    tcsetpgrp(STDIN_FILENO, getpgrp());

    if (WIFEXITED(estatus) || WIFSIGNALED(estatus)) {
        removeJobxPgid(pgid);
    }
    return 0;
}

// Manejador de las ejecuciones de funciones internas

int manejador_internas(tline* linea) {
    // Si hay pipes, no se ejecuta funciones internas
    if (linea->ncommands != 1) {
        return 0;
    }

    // Verificamos filename y argv[0]
    char* nombre_Comando = linea->commands[0].argv[0]; // Siempre argv[0]

    if (!nombre_Comando) {
        return 0;
    }

    for (int i = 0; diccionariodeComandos[i].nombre != NULL; i++) {
        if (strcmp(nombre_Comando, diccionariodeComandos[i].nombre) == 0) {
            diccionariodeComandos[i].funcion(linea);
            return 1; // Manejado
        }
    }
    return 0; // No manejado (será externo)
}

// Ejecucion

void execArgs(tline* linea) {
    tcommand cmd = linea->commands[0];
    int bg = linea->background;
    int id = getSiguienteId(); // Reservamos un ID antes del fork para que padre e hijo lo conozcan
    pid_t pid = fork();

    if (pid == 0) { // Hijo
        // Restaurar señales a default
        signal(SIGINT, SIG_DFL); signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL); signal(SIGTTOU, SIG_DFL); signal(SIGTTIN, SIG_DFL);

        setpgid(0, 0);

        // Comprobacion de redirecciones

        if (linea->redirect_input && !freopen(linea->redirect_input, "r", stdin)) {
            fprintf(stderr, "%s: Error. %s\n", linea->redirect_input, strerror(errno));
            exit(1);
        }
        if (linea->redirect_output && !freopen(linea->redirect_output, "w", stdout)) {
            fprintf(stderr, "%s: Error. %s\n", linea->redirect_output, strerror(errno));
            exit(1);
        }
        if (linea->redirect_error && !freopen(linea->redirect_error, "w", stderr)) {
            fprintf(stderr, "%s: Error. %s\n", linea->redirect_error, strerror(errno));
            exit(1);
        }

        execvp(cmd.argv[0], cmd.argv);
        // Usar stderr para que el error no se pierda en pipes
        fprintf(stderr, "%s: no se encuentra\n", cmd.argv[0]);
        exit(1);
    }
    if (pid > 0) { // Padre
        setpgid(pid, pid);
        if (!bg) {
            tcsetpgrp(STDIN_FILENO, pid);
            waitpid(pid, NULL, 0);
            tcsetpgrp(STDIN_FILENO, getpgrp());
            printf("\n");
        } else {
            printf("[%d] %d\t%s &\n", id, pid, cmd.filename);
            add_job(pid, id, cmd.filename);
        }
    } else {
        perror("fork");
    }
}

void execArgsPiped(tline* linea) {
    int n = linea->ncommands;
    int bg = linea->background;
    int id = getSiguienteId();
    pid_t group_pid = 0;
    int pipes[n - 1][2];
    pid_t pids[n];

    // Crear N-1 pipes para conectar cada comando con el siguiente
    for (int i = 0; i < n - 1; i++) {
        pipe(pipes[i]);
    }

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (i == 0) {
            group_pid = pid;
        }
        pids[i] = pid;
        
        if (pid == 0) {
            // Todos los procesos en la pipeline comparten el mismo PGID
            setpgid(0, group_pid); 
            // Restaurar señales
            signal(SIGINT, SIG_DFL); signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL); signal(SIGTTOU, SIG_DFL); signal(SIGTTIN, SIG_DFL);


            // Gestionar redirecciones y flujo entre procesos
            if (i == 0 && linea->redirect_input && !freopen(linea->redirect_input, "r", stdin)) {
                fprintf(stderr, "%s: Error. %s\n", linea->redirect_input, strerror(errno));
                exit(1);
            }
            if (i > 0) {
                //dup2 duplica un descriptor de archivo y lo ridirige al especificado
                dup2(pipes[i-1][0], STDIN_FILENO);
            }

            if (i == n-1) {
                if (linea->redirect_output && !freopen(linea->redirect_output, "w", stdout)) {
                    fprintf(stderr, "%s: Error. %s\n", linea->redirect_output, strerror(errno));
                    exit(1);
                }
                if (linea->redirect_error && !freopen(linea->redirect_error, "w", stderr)) {
                    fprintf(stderr, "%s: Error. %s\n", linea->redirect_error, strerror(errno));
                    exit(1);
                }
            }else {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // Cierre de pipes
            for (int k = 0; k < n - 1; k++) {
                close(pipes[k][0]); close(pipes[k][1]);
            }

            execvp(linea->commands[i].argv[0], linea->commands[i].argv);
            perror("execvp"); exit(1);
        }
        setpgid(pid, group_pid);
    }

    // Cierre de pipes
    for (int i = 0; i < n - 1; i++) {
        close(pipes[i][0]); close(pipes[i][1]);
    }

    if (!bg) {
        tcsetpgrp(STDIN_FILENO, group_pid);
        for (int i = 0; i < n; i++) {
            waitpid(pids[i], NULL, 0);
        }
        tcsetpgrp(STDIN_FILENO, getpgrp());
        printf("\n");
    } else {
        char job_cmd[1024] = "";
        for (int i = 0; i < n; i++) {
            strcat(job_cmd, linea->commands[i].filename);
            if (i < n - 1) {
                strcat(job_cmd, " | ");
            }
        }
        printf("[%d] %d\t%s &\n", id, group_pid, job_cmd);
        add_job(group_pid, id, job_cmd);
    }
}

int main() {
    // Inicialización shell
    iniciar_Shell();

    //tline* entrada;

    while (1) {
        tline* entrada = input();

        // línea vacía o Ctrl+C
        if (!entrada) {
            continue;
        }

        // Ejecutar comandos internos o externos
        if (manejador_internas(entrada)) {
            continue;
        }

        if (entrada->ncommands == 1) {
            execArgs(entrada);
            printf("\n"); // salto de línea entre comandos
        }
        else if (entrada->ncommands >= 2) {
            execArgsPiped(entrada);
            printf("\n"); // salto de línea entre comandos
        }
    }
}