#include <stdio.h>
#include <signal.h>
#include <readline/readline.h>
#include <unistd.h>
#include <readline/history.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#ifdef __APPLE__
#include <libc.h>
#endif

#include "parser.h"
#include "myshell.h"

#define BUFFER_SIZE 1024

// --- ESTRUCTURA DE JOBS (ARRAY DINÁMICO) ---
typedef struct {
    int id;
    pid_t pgid;
    char* comando;
} tJob;

// Variables globales para el array dinámico
tJob* jobs_array = NULL;
int jobs_count = 0;
int jobs_capacity = 0;
int siguienteId = 1;

// --- DEFINICIONES DE TIPOS PARA COMANDOS ---
typedef int (*command_func_tline)(tline* linea);

typedef struct {
    char *name;
    command_func_tline func;
} command_entry;

// Declaraciones de funciones
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

// --- UTILIDADES ---

void clear() {
    printf("\033[H\033[J");
}

void manejador_sigint(int sig) {
    printf("\n");
    rl_on_new_line();
    rl_redisplay();
}

// Función para añadir jobs al array dinámico
void add_job(pid_t pgid, int id, const char *cmd) {
    // Si el array está lleno o no existe, redimensionamos
    if (jobs_count >= jobs_capacity) {
        int new_capacity = (jobs_capacity == 0) ? 4 : jobs_capacity * 2;
        tJob* temp = realloc(jobs_array, new_capacity * sizeof(tJob));

        if (temp == NULL) {
            perror("Error realloc job");
            return;
        }
        jobs_array = temp;
        jobs_capacity = new_capacity;
    }

    // Añadimos el nuevo job
    jobs_array[jobs_count].pgid = pgid;
    jobs_array[jobs_count].id = id;
    jobs_array[jobs_count].comando = strdup(cmd); // Copiamos la cadena
    jobs_count++;
}

// Función para borrar jobs del array dinámico
void remove_job(pid_t pgid) {
    int index = -1;

    // Buscar el índice
    for (int i = 0; i < jobs_count; i++) {
        if (jobs_array[i].pgid == pgid) {
            index = i;
            break;
        }
    }

    if (index != -1) {
        // Liberar la memoria del string del comando
        free(jobs_array[index].comando);

        // Desplazar los elementos siguientes hacia atrás para tapar el hueco
        for (int i = index; i < jobs_count - 1; i++) {
            jobs_array[i] = jobs_array[i + 1];
        }

        jobs_count--;
    }
}

// Obtener un job por su ID (para fg)
tJob* getJobXid(int id) {
    for (int i = 0; i < jobs_count; i++) {
        if (jobs_array[i].id == id) {
            return &jobs_array[i];
        }
    }
    return NULL;
}

int getNextId() {
    return siguienteId++;
}

void init_shell() {
    clear();
    signal(SIGINT, manejador_sigint);
    signal(SIGQUIT, SIG_IGN);

    char* username = getenv("USER");
    printf("\n\n\nUSER is: @%s\n", username);
    sleep(1);
    clear();
}

void printDir(){
    char currentDirectory[1024];
    if (getcwd(currentDirectory, sizeof(currentDirectory)) != NULL)
        printf("\nDir: %s", currentDirectory);
    else
        perror("getcwd");
}

tline* input() {
    char *str = readline(">>> ");

    if (!str) {
        printf("\nSaliendo...\n");
        // Limpieza de memoria del array antes de salir por Ctrl-D
        if(jobs_array) {
            for(int i=0; i<jobs_count; i++) free(jobs_array[i].comando);
            free(jobs_array);
        }
        exit(0);
    }

    if (strlen(str) > 0) {
        add_history(str);
    }

    // IMPORTANTE: No hacemos free(str) aquí porque tokenize podría
    // estar usando punteros a esta cadena internamente.
    tline* linea = tokenize(str);

    return linea;
}

// --- MANEJADORES DE COMANDOS ---

int manejador_cd(tline* linea) {
    tcommand comando = linea->commands[0];
    char* dir = (comando.argc > 1) ? comando.argv[1] : getenv("HOME");
    if (!dir) { printf("cd: Variable HOME no definida\n"); return 1; }
    if (chdir(dir) != 0) {
        perror("cd");
    }
    return 0;
}

int manejador_exit(tline* linea){
    printf("Saliendo de la miniShell \n");
    // Limpieza de memoria del array
    if(jobs_array) {
        for(int i=0; i<jobs_count; i++) free(jobs_array[i].comando);
        free(jobs_array);
    }
    exit(0);
}

int manejador_umask(tline* linea) {
    tcommand cmd = linea->commands[0];
    mode_t old_mask;
    if (cmd.argc == 1) {
        old_mask = umask(0);
        umask(old_mask);
        printf("%03o\n", old_mask);
        return 0;
    }
    if (cmd.argc == 2) {
        char *endptr;
        mode_t new_mask = (mode_t)strtol(cmd.argv[1], &endptr, 8);
        if (*endptr != '\0') { fprintf(stderr, "umask: formato octal inválido: %s\n", cmd.argv[1]); return 1; }
        umask(new_mask);
        return 0;
    }
    printf("Error: Uso umask [mode]\n");
    return 1;
}

int manejador_jobs(tline* linea){
    for (int i = 0; i < jobs_count; i++) {
        printf("[%d]+ Running\t%s\n", jobs_array[i].id, jobs_array[i].comando);
    }
    return 0;
}

int manejador_fg(tline* linea) {
    tcommand comando = linea->commands[0];

    // Si no pasan argumentos, cogemos el último job (LIFO)
    int id = -1;
    if (comando.argc > 1) {
        id = atoi(comando.argv[1]);
    } else {
        if (jobs_count > 0) {
            id = jobs_array[jobs_count - 1].id;
        }
    }

    if (id == -1) { printf("fg: no hay trabajos actuales\n"); return 1; }

    tJob *job = getJobXid(id);
    if (!job) { printf("fg: job %d no encontrado\n", id); return 1; }

    pid_t pgid = job->pgid;
    printf("%s\n", job->comando);

    tcsetpgrp(STDIN_FILENO, pgid);
    kill(-pgid, SIGCONT);
    waitpid(-pgid, NULL, WUNTRACED);
    tcsetpgrp(STDIN_FILENO, getpgrp());

    remove_job(pgid);
    return 0;
}

// Devuelve 1 si ejecutó un comando interno, 0 si no encontró ninguno
int ownCmdHandler(tline* linea) {
    if (linea->ncommands == 0 || !linea->commands[0].filename) return 0;
    tcommand cmd = linea->commands[0];

    for (int i = 0; diccionariodeComandos[i].name != NULL; i++) {
        if (strcmp(cmd.filename, diccionariodeComandos[i].name) == 0) {
            // Ejecutamos la función asociada
            diccionariodeComandos[i].func(linea);
            return 1; // IMPORTANTE: Retornamos 1 indicando que FUE manejado
        }
    }
    return 0; // No era un comando interno
}

// --- EJECUCIÓN ---

void execArgs(tline* linea) {
    if (!linea || linea->ncommands == 0) return;

    tcommand cmd = linea->commands[0];
    int bg = linea->background;
    int id = getNextId();
    pid_t pid = fork();

    if (pid == 0) {
        // PROCESO HIJO
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        if (linea->redirect_input) freopen(linea->redirect_input, "r", stdin);
        if (linea->redirect_output) freopen(linea->redirect_output, "w", stdout);
        if (linea->redirect_error) freopen(linea->redirect_error, "w", stderr);

        execvp(cmd.argv[0], cmd.argv);
        fprintf(stderr, "%s: no se encontró la orden\n", cmd.argv[0]);
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // PROCESO PADRE
        if (!bg) {
            waitpid(pid, NULL, 0);
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
    int id = getNextId();
    pid_t pids[n];
    int pipes[n - 1][2];

    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) { perror("pipe"); return; }
    }

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return; }

        pids[i] = pid;

        if (pid == 0) { // Hijo
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);

            if (i == 0) {
                if (linea->redirect_input) freopen(linea->redirect_input, "r", stdin);
            } else {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }

            if (i == n - 1) {
                if (linea->redirect_output) freopen(linea->redirect_output, "w", stdout);
                if (linea->redirect_error) freopen(linea->redirect_error, "w", stderr);
            } else {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            for (int k = 0; k < n - 1; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }

            execvp(linea->commands[i].argv[0], linea->commands[i].argv);
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < n - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    if (!bg) {
        for (int i = 0; i < n; i++) waitpid(pids[i], NULL, 0);
    } else {
        char job_cmd[1024] = "";
        for (int i = 0; i < n; i++) {
            strcat(job_cmd, linea->commands[i].filename);
            if (i < n - 1) strcat(job_cmd, " | ");
        }
        printf("[%d] %d\t%s &\n", id, pids[0], job_cmd);
        add_job(pids[0], id, job_cmd);
    }
}

int main() {
    tline* entrada;
    init_shell();

    while (1) {
        printDir();
        entrada = input();

        if (!entrada) continue;

        // 1. Intentamos ejecutar comando interno
        if (ownCmdHandler(entrada)) {
            // Si devuelve 1, ya se ejecutó. NO liberamos entrada porque causa error.
            continue;
        }

        // 2. Si no es interno, ejecutamos externo
        if (entrada->ncommands == 1) {
            execArgs(entrada);
        } else if (entrada->ncommands >= 2) {
            execArgsPiped(entrada);
        }

        // FIX: Eliminado liberar_tline(entrada) o free(entrada).
        // La librería parser.h que usas parece retornar un puntero estático
        // o gestionado internamente que no admite free().
    }
    return 0;
}