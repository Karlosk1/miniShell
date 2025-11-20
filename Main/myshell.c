#include "myshell.h"
#define MAXCOM 1000 // max number of letters to be supported
#define MAXLIST 100 // max number of commands to be supported


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

int input(char* str){
    char* buf;

    //buf = readline("\n>>> ");
    if (strlen(buf) != 0) {
        add_history(buf);
        strcpy(str, buf);
        return 0;
    } else {
        return 1;
    }
}

int main(){
    char inputString[MAXCOM], *parsedArgs[MAXLIST];
    char* parsedArgsPiped;
    int flagger = 0; /*will flag zero if its void, 1 if its a simple command
    and 2 if its including a pipe*/
    init_shell();

    while (1) {
        // print shell line
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
    }
    return 0;
}