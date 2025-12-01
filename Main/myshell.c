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
#include <errno.h> // Necesario para gestionar interrupciones de señal

#ifdef __APPLE__
#include <libc.h>
#endif

#include "parser.h"
#include "myshell.h"

// --- ESTRUCTURA DE JOBS ---
typedef struct {
    int id;
    pid_t pgid;
    char* comando;
} tJob;

tJob* jobs_array = NULL;
int jobs_count = 0;
int jobs_capacity = 0;
int siguienteId = 1;

// --- TIPOS ---
typedef int (*command_func_tline)(tline* linea);

typedef struct {
    char *name;
    command_func_tline func;
} command_entry;

// Declaraciones
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

// FIX: Ctrl+C (Usamos un manejador simple, la magia está en sigaction)
void manejador_sigint(int sig) {
    printf("\n");
    rl_on_new_line();
    rl_redisplay();
}

// --- GESTIÓN DE JOBS ---

void add_job(pid_t pgid, int id, const char *cmd) {
    if (jobs_count >= jobs_capacity) {
        int new_capacity = (jobs_capacity == 0) ? 4 : jobs_capacity * 2;
        tJob* temp = realloc(jobs_array, new_capacity * sizeof(tJob));
        if (!temp) { perror("realloc"); return; }
        jobs_array = temp;
        jobs_capacity = new_capacity;
    }
    jobs_array[jobs_count].pgid = pgid;
    jobs_array[jobs_count].id = id;
    jobs_array[jobs_count].comando = strdup(cmd);
    jobs_count++;
}

void remove_job_by_index(int index) {
    if (index >= 0 && index < jobs_count) {
        free(jobs_array[index].comando);
        for (int i = index; i < jobs_count - 1; i++) {
            jobs_array[i] = jobs_array[i + 1];
        }
        jobs_count--;
    }
}

void remove_job(pid_t pgid) {
    for (int i = 0; i < jobs_count; i++) {
        if (jobs_array[i].pgid == pgid) {
            remove_job_by_index(i);
            return;
        }
    }
}

tJob* getJobXid(int id) {
    for (int i = 0; i < jobs_count; i++) {
        if (jobs_array[i].id == id) return &jobs_array[i];
    }
    return NULL;
}

int getNextId() { return siguienteId++; }

void check_finished_jobs() {
    pid_t pid;
    int status;
    // Comprueba si algún hijo ha terminado sin bloquear (WNOHANG)
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < jobs_count; i++) {
            if (jobs_array[i].pgid == pid) {
                // Mensaje de Done
                printf("[%d]+  Done\t\t%s\n", jobs_array[i].id, jobs_array[i].comando);
                remove_job_by_index(i);
                break;
            }
        }
    }
}

// --- SHELL CORE ---

void init_shell() {
    clear();

    // FIX Ctrl+C: Usamos sigaction para evitar SA_RESTART.
    // Esto permite que readline se interrumpa y el prompt vuelva inmediatamente
    // sin tener que pulsar Enter.
    struct sigaction sa;
    sa.sa_handler = manejador_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Importante: NO poner SA_RESTART
    sigaction(SIGINT, &sa, NULL);

    // Ignorar resto de señales conflictivas
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    char* username = getenv("USER");
    printf("\n\n\nUSER is: @%s\n", username);
    sleep(1);
    clear();
}

void printDir(){
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        printf("\nDir: %s", cwd);
    else
        perror("getcwd");
}

tline* input() {
    // Si readline es interrumpido por Ctrl+C, devuelve NULL pero errno es EINTR
    char *str = readline(">>> ");

    if (!str) {
        // Diferenciar Ctrl+D (EOF) de una interrupción
        if (errno == EINTR) {
             // Fue Ctrl+C, limpiamos errno y devolvemos línea vacía para regenerar prompt
            errno = 0;
            return NULL;
        }

        printf("\nSaliendo...\n");
        for(int i=0; i<jobs_count; i++) free(jobs_array[i].comando);
        free(jobs_array);
        exit(0);
    }

    if (strlen(str) > 0) add_history(str);
    return tokenize(str);
}

// --- COMANDOS INTERNOS ---

int manejador_cd(tline* linea) {
    tcommand cmd = linea->commands[0];
    char* dir;

    if (cmd.argc > 1) {
        dir = cmd.argv[1];
    } else {
        dir = getenv("HOME");
        if (!dir) {
            fprintf(stderr, "cd: variable HOME no definida\n");
            return 1;
        }
    }

    if (chdir(dir) != 0) {
        perror("cd");
        return 1;
    }
    return 0;
}

int manejador_exit(tline* linea) {
    printf("Saliendo de la miniShell...\n");
    for(int i=0; i<jobs_count; i++) free(jobs_array[i].comando);
    free(jobs_array);
    exit(0);
}

int manejador_umask(tline* linea) {
    tcommand cmd = linea->commands[0];

    // Solo mostrar
    if (cmd.argc == 1) {
        mode_t current = umask(0);
        umask(current); // Restauramos
        printf("%03o\n", current);
    }
    // Cambiar máscara
    else if (cmd.argc == 2) {
        char *endptr;
        // strtol base 8 gestiona "077" y "77" igual
        long val = strtol(cmd.argv[1], &endptr, 8);

        if (*endptr != '\0' || val < 0 || val > 0777) {
            fprintf(stderr, "umask: número octal inválido: %s\n", cmd.argv[1]);
            return 1;
        }

        mode_t new_mask = (mode_t)val;
        umask(new_mask);

        // FIX Umask: Imprimir confirmación para el usuario
        printf("Mascara cambiada a: %03o\n", new_mask);
    }
    else {
        printf("Uso: umask [octal]\n");
    }
    return 0;
}

int manejador_jobs(tline* linea) {
    for (int i = 0; i < jobs_count; i++)
        printf("[%d]+ Running\t%s\n", jobs_array[i].id, jobs_array[i].comando);
    return 0;
}

int manejador_fg(tline* linea) {
    tcommand cmd = linea->commands[0];
    int id = -1;

    // Obtener ID (argumento o el último)
    if (cmd.argc > 1) {
        id = atoi(cmd.argv[1]);
    } else {
        if (jobs_count > 0) id = jobs_array[jobs_count - 1].id;
    }

    if (id == -1) { printf("fg: no hay trabajos\n"); return 1; }

    tJob *job = getJobXid(id);
    if (!job) { printf("fg: trabajo %d no encontrado\n", id); return 1; }

    printf("%s\n", job->comando);
    pid_t pgid = job->pgid;

    // Control de terminal
    tcsetpgrp(STDIN_FILENO, pgid);

    // Continuar si estaba parado
    kill(-pgid, SIGCONT);

    // Esperar
    waitpid(-pgid, NULL, WUNTRACED);

    // Recuperar terminal
    tcsetpgrp(STDIN_FILENO, getpgrp());

    // Sacar de la lista
    remove_job(pgid);
    return 0;
}

// FIX: Función más robusta para detectar comandos internos
int ownCmdHandler(tline* linea) {
    // Si hay pipes, NO ejecutamos comandos internos (salvo requerimiento específico)
    if (linea->ncommands != 1) return 0;

    // Verificamos tanto filename como argv[0] por seguridad
    char* cmdName = linea->commands[0].filename;
    if (!cmdName && linea->commands[0].argc > 0) {
        cmdName = linea->commands[0].argv[0];
    }

    if (!cmdName) return 0;

    for (int i = 0; diccionariodeComandos[i].name != NULL; i++) {
        if (strcmp(cmdName, diccionariodeComandos[i].name) == 0) {
            diccionariodeComandos[i].func(linea);
            return 1; // Manejado
        }
    }
    return 0; // No manejado (será externo)
}

// --- EJECUCION ---

void execArgs(tline* linea) {
    tcommand cmd = linea->commands[0];
    int bg = linea->background;
    int id = getNextId();
    pid_t pid = fork();

    if (pid == 0) { // Hijo
        // Restaurar señales a default
        signal(SIGINT, SIG_DFL); signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL); signal(SIGTTOU, SIG_DFL); signal(SIGTTIN, SIG_DFL);

        setpgid(0, 0);

        if (linea->redirect_input) freopen(linea->redirect_input, "r", stdin);
        if (linea->redirect_output) freopen(linea->redirect_output, "w", stdout);
        if (linea->redirect_error) freopen(linea->redirect_error, "w", stderr);

        execvp(cmd.argv[0], cmd.argv);
        // Usar stderr para que el error no se pierda en pipes
        fprintf(stderr, "miniShell: %s: orden no encontrada\n", cmd.argv[0]);
        exit(1);
    } else if (pid > 0) { // Padre
        setpgid(pid, pid);
        if (!bg) {
            tcsetpgrp(STDIN_FILENO, pid);
            waitpid(pid, NULL, 0);
            tcsetpgrp(STDIN_FILENO, getpgrp());
        } else {
            printf("[%d] %d\t%s &\n", id, pid, cmd.filename);
            add_job(pid, id, cmd.filename);
        }
    } else perror("fork");
}

void execArgsPiped(tline* linea) {
    int n = linea->ncommands;
    int bg = linea->background;
    int id = getNextId();
    pid_t group_pid = 0;
    int pipes[n - 1][2];
    pid_t pids[n];

    for (int i = 0; i < n - 1; i++) pipe(pipes[i]);

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (i == 0) group_pid = pid;
        pids[i] = pid;

        if (pid == 0) {
            setpgid(0, group_pid);
            // Restaurar señales
            signal(SIGINT, SIG_DFL); signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL); signal(SIGTTOU, SIG_DFL); signal(SIGTTIN, SIG_DFL);

            if (i == 0 && linea->redirect_input) freopen(linea->redirect_input, "r", stdin);
            else if (i > 0) dup2(pipes[i-1][0], STDIN_FILENO);

            if (i == n-1) {
                if (linea->redirect_output) freopen(linea->redirect_output, "w", stdout);
                if (linea->redirect_error) freopen(linea->redirect_error, "w", stderr);
            } else dup2(pipes[i][1], STDOUT_FILENO);

            for (int k = 0; k < n - 1; k++) { close(pipes[k][0]); close(pipes[k][1]); }

            execvp(linea->commands[i].argv[0], linea->commands[i].argv);
            perror("execvp"); exit(1);
        }
        setpgid(pid, group_pid);
    }

    for (int i = 0; i < n - 1; i++) { close(pipes[i][0]); close(pipes[i][1]); }

    if (!bg) {
        tcsetpgrp(STDIN_FILENO, group_pid);
        for (int i = 0; i < n; i++) waitpid(pids[i], NULL, 0);
        tcsetpgrp(STDIN_FILENO, getpgrp());
    } else {
        char job_cmd[1024] = "";
        for (int i = 0; i < n; i++) {
            strcat(job_cmd, linea->commands[i].filename);
            if (i < n - 1) strcat(job_cmd, " | ");
        }
        printf("[%d] %d\t%s &\n", id, group_pid, job_cmd);
        add_job(group_pid, id, job_cmd);
    }
}

int main() {
    init_shell();
    tline* entrada;

    while (1) {
        check_finished_jobs();
        printDir();

        entrada = input();
        if (!entrada) continue; // Si fue Ctrl+C o línea vacía

        // 1. Intentar comando interno
        if (ownCmdHandler(entrada)) {
            // Ya se ejecutó internamente, no hacer nada más
        }
        // 2. Comando externo
        else if (entrada->ncommands == 1) {
            execArgs(entrada);
        }
        else if (entrada->ncommands >= 2) {
            execArgsPiped(entrada);
        }

        // No hacemos free(entrada) manualmente
    }
    return 0;
}