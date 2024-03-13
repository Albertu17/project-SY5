#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>
#include "toolbox_jsh.h"
#include "parsing_jsh.h"
#include "jsh.h"

#define NORMAL "\033[00m"
#define BLEU "\033[01;34m"

int main(int argc, char** argv) {

    // Mise en place d'un masque.
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask,SIGINT);
    sigaddset(&sa.sa_mask,SIGTERM);
    sigaddset(&sa.sa_mask,SIGTTIN);
    sigaddset(&sa.sa_mask,SIGTTOU);
    sigaddset(&sa.sa_mask,SIGTSTP);
    pthread_sigmask(SIG_BLOCK,&sa.sa_mask,NULL);

    // Initialisation variables globales.
    previous_folder = pwd();
    current_folder = pwd();
    running = 1;
    lastReturn = 0;
    nbJobs = 0;
    l_jobs = malloc(sizeof(Job)*40); //maximum de 40 jobs simultanément.

    main_loop(); // récupère et traite les commandes entrées.

    // Libération des variables globales.
    free(previous_folder);
    free(current_folder);
    for (int i = 0; i < nbJobs; i++){
        free(l_jobs[i].command_name);
        free(l_jobs[i].state);
        free(l_jobs[i].ground);
    }
    for (int i = 0; i < nbJobs; i++) {
        kill(l_jobs[i].pid,SIGKILL); // À MODIFIER: arrêter les job via leur pgid
    }
    free(l_jobs);
    return lastReturn;
}

void main_loop() {
    // Initialisation buffers.
    char* command_line = (char*) NULL; /* Stocke la ligne de commande entrée par l'utilisateur (allocation
    espace mémoire faite par readline). */
    // Paramétrage readline.
    rl_outstream = stderr;
    using_history();
    // Boucle de récupération et de traitement des commandes.
    while (running) {
        // Libération de la mémoire allouée par readline.
        free(command_line);
        // On vérifie l'état des éventuels processus créés précedemment.
        // check_sons_state();
        // Récupération de la commande entrée et affichage du prompt.
        char* tmp = getPrompt();
        command_line = readline(tmp);
        free(tmp);
        // Tests commande non vide.
        if (command_line == NULL) {
            exit(lastReturn);
        } else if (is_only_spaces(command_line)) continue;
        // Traitement de la ligne de commande entrée.
        else {
            add_history(command_line); // Ajoute la ligne de commande entrée à l'historique.
            lastReturn = launch_job_execution(command_line);
        }
    }
    // Libération de la mémoire allouée par readline.
    free(command_line);
}

// Supervise le traitement d'un job.
int launch_job_execution(char* command_line) {
    // Appel à la création de la structure commande associée à la ligne de commande.
    char* command_line_cpy = malloc(sizeof(command_line));
    strcpy(command_line_cpy, command_line);
    bool* job_background = malloc(sizeof(bool));
    *job_background = 0;
    Command* command = getCommand(command_line, job_background);
    free(command_line_cpy);

    if (*job_background == 1) {
        if (nbJobs == 40) {
            fprintf(stderr,"Too many jobs running simultaneously\n");
            return 1;
        }
        char * ground = malloc(sizeof(char)*11);
        strcpy(ground,"Background");
        create_job(command_name,"Running",pid,ground);
        print_job(l_jobs[nbJobs-1]);
    }

    int tmp = execute_command(command, NULL);
        // À la fin de la pipeline.
    if (pipe_out == NULL && !command -> background) {//si il y a des jobs qui tournent alors on ne veut pas attendre les fils
        //waitForAllSons();
        while(wait(NULL) > 0); // On attend la fin de tous les processus fils.
        lastReturn = tmp; // On met à jour lastReturn.
    }

            if (pipe_out == NULL) { // Si on est arrivé à la fin de la pipeline.
            int status;
            if (!command -> background) {
                waitpid(pid,&status,0); /* On attend la fin du dernier fils et on récupère sa valeur de retour.
                On attend la fin du reste des fils lors du retour à execute_command(), après avoir retiré
                le dernier lecteur. */
            }
            else {
                pid_t state = waitpid(pid,&status,WNOHANG);
                if (nbJobs == 40) {
                    fprintf(stderr,"Too many jobs running simultaneously\n");
                    return 1;
                }
                else if (state != 0) {
                    if (WIFEXITED(status)) {//si le fils s'est terminé normalement c'est qu'en parametre il y avait une fonction instantané
                        fprintf(stderr,"[%d] %d\n",nbJobs,pid);
                    }
                    return 1;
                }
                else {
                    char * ground = malloc(sizeof(char)*11);
                    strcpy(ground,"Background");
                    create_job(command_name,"Running",pid,ground);
                    print_job(l_jobs[nbJobs-1]);
                    return 0;
                }
            }
            return WEXITSTATUS(status);//return the exit value of the son
        } else return 0; // Valeur de retour au milieu d'une pipeline: ne sera pas mise dans lastReturn.


    free(job_background);
}

/* Lance l'exécution de toutes les commandes situées à l'intérieur de la structure Command passée en
argument, gère le stockage de leur sortie, puis lance l'exécution de l'agument command. */
int execute_command(Command* command, int pipe_out[2]) {
    int* pipe_in = NULL;
    // Stockage de l'input sur un tube.
    if (command -> input != NULL) {
        pipe_in = malloc(8); // 8 octets nécessaires pour stocker 2 int.
        pipe(pipe_in);
        execute_command(command -> input, pipe_in);
    } else if (command -> in_sub != NULL) { // Si l'entrée est la sortie d'une substitution.
        pipe_in = malloc(8);
        pipe(pipe_in);
        execute_command(command -> in_sub, pipe_in);
    }
    // Stockage des substitutions sur des tubes anonymes.
    int* tubes[command -> nbSubstitutions];
    if (command -> nbSubstitutions != 0) {
        unsigned cpt = 0;
        // Pour toutes les éventuelles substitutions que la commande utilise.
        for (int i = 0; i < command -> nbArgs; ++i) {
            if (!strcmp(command -> argsComm[i], "fifo")) {
                int* pfd = malloc(8);
                tubes[cpt] = pfd;
                pipe(pfd);
                // Stockage sur le tube.
                execute_command(command -> substitutions[cpt], pfd);
                close(pfd[1]); // Fermeture de l'ouverture du tube en écriture.
                sprintf(command -> argsComm[i], "/dev/fd/%i", pfd[0]);
                cpt++;
            }
        }
    }

    int tmp = apply_redirections(command, pipe_in, pipe_out);

    // Libération de la mémoire allouée pour le tube stockant l'entrée.
    if (pipe_in != NULL) free(pipe_in);
    /* Fermeture de l'entrée en lecture et libération de la mémoire allouée pour
    les tubes stockant les sorties des substitutions */
    for (unsigned i = 0; i < command -> nbSubstitutions; ++i) {
        close(tubes[i][0]);
        free(tubes[i]);
    }
    // Libération de la mémoire allouée pour la commande.
    free_command(command);
    
    return tmp;
}

int apply_redirections(Command* command, int pipe_in[2], int pipe_out[2]) {
    int cpy_stdin, cpy_stdout, cpy_stderr;
    int fd_in = -1, fd_out = -1, fd_err = -1;

    // Redirection entrée.
    if (pipe_in != NULL) { // Si l'entrée est sur un tube.
        if (command -> in_redir != NULL && strcmp(command -> in_redir[1], "fifo")) {
            fprintf(stderr, "command %s: redirection entrée impossible", command -> argsComm[0]);
            return 1;
        } else {
            cpy_stdin = dup(0);
            close(pipe_in[1]); // On va lire sur le tube, pas besoin de l'entrée en écriture.
            fd_in = pipe_in[0];
            dup2(fd_in, 0);
            close(fd_in);
        }
    } else if (command -> in_redir != NULL) { // Si l'entrée est sur un fichier.
        cpy_stdin = dup(0);
        if (!strcmp(command -> in_redir[0], "<")) {
            fd_in = open(command -> in_redir[1], O_RDONLY, 0666);
        }
        dup2(fd_in, 0);
        close(fd_in);
    }

    // Redirection sortie.
    if (pipe_out != NULL) { // Si la sortie est un tube.
        if (command -> out_redir != NULL) {
            fprintf(stderr, "command %s: redirection sortie impossible", command -> argsComm[0]);
            return 1;
        } 
        // La redirection est faite dans external_command().
    } else if (command -> out_redir != NULL) { // Si la sortie est un fichier.
        cpy_stdout = dup(1);
        if (!strcmp(command -> out_redir[0], ">")) {
            fd_out = open(command -> out_redir[1], O_WRONLY|O_APPEND|O_CREAT|O_EXCL, 0666);
            if (fd_out == -1) {
                fprintf(stderr,"bash : %s: file already exist.\n", command -> argsComm[0]);
                return 1;
            }
        } else if (!strcmp(command -> out_redir[0], ">|")) {
            fd_out = open(command -> out_redir[1], O_WRONLY|O_CREAT|O_TRUNC, 0666);
        } else if (!strcmp(command -> out_redir[0], ">>")) {
            fd_out = open(command -> out_redir[1], O_WRONLY|O_APPEND|O_CREAT, 0666);
        }
        dup2(fd_out, 1);
        close(fd_out);
    }

    // Redirection sortie erreur.
    if (command -> err_redir != NULL) { // Si la sortie erreur est un fichier.
        cpy_stderr = dup(2);
        if (!strcmp(command -> err_redir[0], "2>")) {
            fd_err = open(command -> err_redir[1], O_WRONLY|O_APPEND|O_CREAT|O_EXCL, 0666);
            if (fd_err == -1) {
                fprintf(stderr,"bash : %s: file already exists.\n", command -> argsComm[0]);
                return 1;
            }
        } else if (!strcmp(command -> err_redir[0], "2>|")) {
            fd_err = open(command -> err_redir[1], O_WRONLY|O_CREAT|O_TRUNC, 0666);
        } else if (!strcmp(command -> err_redir[0], "2>>")) {
            fd_err = open(command -> err_redir[1], O_WRONLY|O_APPEND|O_CREAT, 0666);
        }
        dup2(fd_err, 2);
        close(fd_err);
    }

    // Appel à l'exécution de la commande.
    int tmp;
    if (!strcmp(command -> argsComm[0],"cd") || !strcmp(command -> argsComm[0],"exit")) {
        tmp = callRightCommand(command); // cd et exit doivent être exécutées sur le processus jsh.
    } else tmp = external_command(command, pipe_out);

    // Remise en état.
    if (fd_in != -1) dup2(cpy_stdin, 0);
    if (fd_out != -1) dup2(cpy_stdout, 1);
    if (fd_err != -1) dup2(cpy_stderr, 2);

    return tmp;
}


/*
This function returns the error of execvp and is exuting the command "command_name" with the arguments "arguments".
*/
int external_command(Command* command, int pipe_out[2]) {
    pid_t pid = fork();

    if (pid == 0) { // processus enfant
        pthread_sigmask(SIG_UNBLOCK,&sa.sa_mask,NULL); // On retire le masquage des signaux.
        int tmp = 0;
        if (nbJobs < 40) {
            if (pipe_out != NULL) { // Redirection de la sortie de la commande exécutée si c'est attendu.
                close(pipe_out[0]);
                int fd_out = pipe_out[1];
                dup2(fd_out, 1);
                close(fd_out);
            }
            // On exécute aussi ici les commandes internes qui affichent quelque chose.
            if (!strcmp(command -> argsComm[0], "pwd") || !strcmp(command -> argsComm[0], "?")
                || !strcmp(command -> argsComm[0], "kill") || !strcmp(command -> argsComm[0], "jobs")
                || !strcmp(command -> argsComm[0], "bg") || !strcmp(command -> argsComm[0], "fg")) {
                tmp = callRightCommand(command);
            } else {
                tmp = execvp(command -> argsComm[0], command -> argsComm);
                fprintf(stderr,"%s\n", strerror(errno)); // Ne s'exécute qu'en cas d'erreur dans l'exécution de execvp.
            }
        }
        exit(tmp);
    }
    else { // processus parent
        return pid;
    }
}

/* Prend en argument une structure Command correspondant à une commande interne, vérifie que le
nombre d'arguments est correct, et appel la fonction qui correspond à l'exécution de cette commande. */
int callRightCommand(Command* command) {
    // Commande cd
    if (strcmp(command -> argsComm[0], "cd") == 0) {
        if (correct_nbArgs(command -> argsComm, 1, 2)) {
            if (command -> argsComm[1] == NULL || strcmp(command -> argsComm[1],"$HOME") == 0) {
                char* home = getenv("HOME");
                return cd(home);
            }
            else if (strcmp(command -> argsComm[1],"-") == 0) return cd(previous_folder);
            else return cd(command -> argsComm[1]);
        } else return 1;
    }
    // Commande pwd
    else if (strcmp(command -> argsComm[0], "pwd") == 0) {
        if (correct_nbArgs(command -> argsComm, 1, 1)) {
            char* path = pwd();
            if (path == NULL) return 1;
            else {
                printf("%s\n",path);
                free(path);
                return 0;
            }
        } else return 1;
    }
    // Commande ?
    else if (strcmp(command -> argsComm[0],"?") == 0) {
        if (correct_nbArgs(command -> argsComm, 1, 1)) {
            print_lastReturn();
            return 0;
        } else return 1;
    }
    // Commande jobs
    else if (strcmp(command -> argsComm[0],"jobs") == 0) {
        if (correct_nbArgs(command -> argsComm, 1, 3)) {
            if (command->argsComm[2] != NULL) {
                if (command->argsComm[2][0] != '%') return 1;
                if (command->argsComm[1][0] != '-') return 1;
                pid_t pidToFind = convert_str_to_int(command->argsComm[1]);
                print_jobs(-pidToFind,true,true);
            }
            else if (command->argsComm[1] != NULL) {
                if (command->argsComm[1][0] != '%' && command->argsComm[1][0] != '-') return 1;
                else if (command->argsComm[1][0] == '%') {
                    pid_t pidToFind = convert_str_to_int(command->argsComm[1]);
                    print_jobs(-pidToFind,true,false);
                }
                else {
                    print_jobs(0,false,true);
                }
            }
            else print_jobs(0,false,false);
            return 0;
        } else return 1;
    }
    // Commande kill
    else if (strcmp(command -> argsComm[0],"kill") == 0) {
        if (correct_nbArgs(command -> argsComm, 2, 3)) {
            int tmp = killJob(command -> argsComm[1],command -> argsComm[2]);
            if (tmp == -1) {
                perror(NULL);
            }
            return tmp;
        } else return 1;
    }
    // Commandes bg et fg
    else if (strcmp(command -> argsComm[0],"bg") == 0 || strcmp(command -> argsComm[0],"fg") == 0 ) {
        if (correct_nbArgs(command -> argsComm, 2, 3)) {
            char * s = command -> argsComm[1];
            char * secondlast = &s[strlen(s)-2];
            char * last = &s[strlen(s)-1];
            char * final = malloc(sizeof(char)*2);
            if (!strcmp(secondlast,"%")) {
                strcpy(&final[0],secondlast);
                strcpy(&final[1],last);
            }
            else strcpy(&final[0], last);
            int result;
            if(strcmp(command -> argsComm[0],"bg") == 0) result = bg(convert_str_to_int(final)-1);
            else result = fg(convert_str_to_int(final)-1);
            free(final);
            return result;
            
        }
        else return 1;
    }
    // Commande exit
    else if (strcmp(command -> argsComm[0], "exit") == 0) {
        if (correct_nbArgs(command -> argsComm, 1, 2)) {
            if (command -> argsComm[1] == NULL) {
                return exit_jsh(lastReturn);
            }
            else {
                int int_args = convert_str_to_int(command -> argsComm[1]);
                if (int_args == INT_MIN) {//we check the second argument doesn't contain some chars
                    fprintf(stderr,"Exit takes a normal integer as argument\n");
                    return 1;
                }
                else {
                    return exit_jsh(int_args);
                }
            }
        } else return 1;
    }  
    else return 1;
}


/* Retourne true si le nombre d'arguments de la commande passée en argument est correct, 
affiche un message d'erreur et retoure false sinon. */
bool correct_nbArgs(char** argsComm, unsigned min_nbArgs, unsigned max_nbArgs) {
    bool correct_nb = true;
    if (argsComm[min_nbArgs-1] == NULL) {
        fprintf(stderr,"bash : %s: too few arguments\n", argsComm[0]);
        correct_nb = false;
    } else if (argsComm[max_nbArgs] != NULL) {
        fprintf(stderr,"bash : %s: too many arguments\n", argsComm[0]);
        correct_nb = false;
    }
    return correct_nb;
}

char* pwd() {
    unsigned size = 30;
    char* buf = malloc(size * sizeof(char));
    checkAlloc(buf);
    while (getcwd(buf,size) == NULL) { // Tant que getwd produit une erreur.
        if (errno == ERANGE) { /* Si la taille de la string représentant le chemin est plus grande que
        size, on augmente size et on réalloue. */
            size *= 2;
            buf = realloc(buf, size * sizeof(char));
            checkAlloc(buf);
        }
        else { /* Si l'erreur dans getwd n'est pas dûe à la taille du buffer passé en argument, 
        on affiche une erreur. */
            fprintf(stderr,"ERROR IN pwd");
            free(buf);
            return NULL;
        }
    }
    return buf;
}

int cd (char* pathname) {
    char* tmp = pwd();
    int returnValue = chdir(pathname);
    if (returnValue == -1) {
        switch (errno) {
            case (ENOENT) : {
                char* home = getenv("HOME");
                cd(home);
                returnValue = chdir(pathname);//we returned to the root and try again
                if (returnValue == -1) {
                    if (errno == ENOENT) {
                        cd(tmp);//if this doesn't work we return where we were
                        fprintf(stderr,"cd : non-existent folder\n");break;
                    }
                    else {
                        cd(tmp);//if this doesn't work we return where we were
                    }
                }
                else {
                    strcpy(previous_folder,tmp);
                    char* tmp2 = pwd();
                    strcpy(current_folder,tmp2);
                    free(tmp2);
                    break;
                }
            }
            case (EACCES) : fprintf(stderr,"cd : Access restricted\n");break;
            case (ENAMETOOLONG) : fprintf(stderr,"cd : Folder name too long\n");break;
            case (ENOTDIR) : fprintf(stderr,"cd : An element is not a dir\n");break;
            case (ENOMEM) : fprintf(stderr,"cd : Not enough memory for the core\n");break;
            default : fprintf(stderr,"Unknown error !\n");break;
        }
        returnValue = 1;
        free(tmp);
    }
    else {
        strcpy(previous_folder,tmp);
        free(tmp);
        char* tmp2 = pwd();
        strcpy(current_folder,tmp2);
        free(tmp2);
    }
    return returnValue;
}


void print_lastReturn() {
    printf("%d\n", lastReturn);
}


int exit_jsh(int val) {
    int returnValue;
    if (nbJobs > 0) {
        fprintf(stderr,"exit\n");
        fprintf(stderr,"There are stopped jobs.\n");
        returnValue = 1;
    }
    else {
        returnValue = val;
        running = 0;
    }
    return returnValue;
}

// Relance à l'arrière-plan le job dont le numéro est passé en argument.
int bg(int job_num) {
    if (job_num > nbJobs) {
        fprintf(stderr, "Could not run bg, process not found.\n");
        return 1;
    }
    Job *job = & l_jobs[job_num];
    if(!strcmp(job->ground,"Background")){
        fprintf(stderr, "Process already running in Background.\n");
        return 1;
    }
    strcpy(job->ground,"Background");
    strcat(job->command_name, " &");
    inspectAllSons(job -> pid, SIGCONT, false, false);
    kill(job -> pid, SIGCONT);
    print_job(l_jobs[job_num]);
    return 0;
}

// Relance à l'avant-plan le job dont le numéro est passé en argument.
int fg(int job_num) {
    if (job_num > nbJobs) {
        fprintf(stderr, "Could not run bg, process not found.\n");
        return 1;
    }
    Job *job = & l_jobs[job_num];
    if(!strcmp(job->ground,"Foreground")) {
        fprintf(stderr, "Process already running in Foreground.\n");
        return 1;
    }
    strcpy(job->ground,"Foreground");
    inspectAllSons(job -> pid, SIGCONT, false, false);
    kill(job -> pid, SIGCONT);
    waitForAllSons(job -> pid);
    waitpid(job -> pid, NULL, 0);
    print_job(l_jobs[job_num]);
    return 0;
}


// Retourne le prompt à afficher.
char* getPrompt() {
    char* prompt = malloc(sizeof(char)* 50);
    int l_nbJobs = length_base10(nbJobs);
    if (strlen(current_folder) == 1) {
        sprintf(prompt, BLEU"[%d]" NORMAL "c$ ", nbJobs);
    }
    else if (strlen(current_folder) <= (26-l_nbJobs)) {
        sprintf(prompt, BLEU"[%d]" NORMAL "%s$ ", nbJobs, current_folder);
    }
    else{
        char* path = malloc(sizeof(char)*(27));
        strncpy(path, (current_folder + (strlen(current_folder) - (23 - l_nbJobs))), (25 - l_nbJobs));
        sprintf(prompt, BLEU"[%d]" NORMAL "...%s$ ", nbJobs, path);
        free(path);
    }
    return prompt;
}