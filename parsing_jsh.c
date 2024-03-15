#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "toolbox_jsh.h"
#include "parsing_jsh.h"


/* Prend en argument une string correspondant à une commande ou à une pipeline et renvoie une structure
Command associée, avec dans ses champs les structures Command associées à son input et aux substitutions
qu'elle utilise. */
Command* getCommand(char* command_line) {
    Command* pipeline[MAX_LENGTH_PIPELINE]; // Tableau servant à stocker temporairement les commandes de la pipeline.
    unsigned index = 0;
    char* firstCommand = first_command(command_line);
    bool error = false;
    do { // Pour chaque commande de la pipeline.
        // Création et initialisation de la structure Command.
        pipeline[index] = malloc(sizeof(Command));
        memset(pipeline[index], 0, sizeof(*pipeline[index]));
        // Remplissage du champ strComm de la structure commande.
        pipeline[index] -> strComm = malloc(MAX_NB_ARGS * 10);
        strcpy(pipeline[index] -> strComm, firstCommand);
        free(firstCommand);
        // Découpage des arguments et des redirections de la commande.
        if (parse_command(pipeline[index]) == -1 || parse_redirections(pipeline[index]) == -1) error = true;
        // Préparation prochaine itération.
        index++;
        firstCommand = first_command(command_line);
        if (firstCommand == NULL) break;
        if (is_only_spaces(firstCommand)) {
            error = true;
            fprintf(stderr,"Error: empty command inside pipeline.\n");
            break;
        }
        if (error) break;
    } while (firstCommand != NULL);
    // Liaison des commandes de la pipeline entre elles via leur champ input.
    for (unsigned i = 1; i < index; ++i) {
        pipeline[i] -> input = pipeline[i-1];
    }
    if (error) {
        free_command(pipeline[index-1]);
        return NULL;
    }
    return pipeline[index-1];
}

/* Retourne 1 ou 0 suivant si la ligne de commande passée en argument se termine par un symbole '&'
ou pas. Le cas échéant, le symbole est enlevé de la string. */
int parse_ampersand(char* command_line) {
    bool found = 0;
    for (unsigned i = strlen(command_line)-1; i >= 0; --i) {
        if (command_line[i] == '&') {
            command_line[i] = '\0';
            found = true;
            break;
        } else if (command_line[i] != ' ') break;
    }
    return found;
}

/* Alloue l'espace mémoire nécéssaire pour une structure commande, l'initialise avec des valeurs par
défaut, et renvoie un pointeur vers lui. */
// Command* create_command() {
//     Command* command = malloc(sizeof(Command));
//     command -> strComm = NULL;
//     command -> argsComm = NULL;
//     command -> nbArgs = 0;
//     command -> in_sub = NULL;
//     command -> in_redir = NULL;
//     command -> out_redir = NULL;
//     command -> err_redir = NULL;
//     command -> substitutions = NULL;
//     command -> nbSubstitutions = 0;
//     command -> input = NULL;
//     command -> background = false;
//     return command;
// }

// Renvoie la première commande de la ligne de commande (éventuellement une pipeline) passée en argument.
char* first_command(char* input) {
    unsigned len_input = strlen(input);
    if (len_input == 0) return NULL;
    unsigned len_command = len_input; // Par défaut, on considère que l'input est constituée d'une seule commande.
    char* strComm = malloc(MAX_NB_ARGS * 10);
    char* first_bar_adress = (char*) NULL;
    /* Recherche de la première occurence du caractère | qui ne soit pas dans un symbole de redirection ou
    à l'intérieur d'une substitution. */
    unsigned nb_parentheses_ouvrantes = 0;
    unsigned nb_parentheses_fermantes = 0;
    for (unsigned i = 0; i < len_command; ++i) {
        if ((input[i] == '|') && (nb_parentheses_fermantes == nb_parentheses_ouvrantes)
            && (i != 0) && (input[i-1] == ' ')) {
            first_bar_adress = input+i;
            break;
        }
        if (input[i] == '(') nb_parentheses_ouvrantes++;
        if (input[i] == ')') nb_parentheses_fermantes++;
    }
    if (first_bar_adress != NULL) {
        len_command = len_input - strlen(first_bar_adress);
        strncpy(strComm, input, len_command); // copie de la première commande dans strComm. 
        strComm[len_command] = '\0';
        memmove(input, input+len_command+1,strlen(first_bar_adress)); /* troncage de l'input
        au niveau de la fin de la première commande. */
    } else {
      strcpy(strComm, input);
      memset(input,0,len_input);
    }
    return strComm;
}

/* Prend en argument une structure Command initialisée avec seulement une string de commande, et parse
ses arguments (strings) et ses substitutions (Command), en les mettant respectivement dans les champs
argsComm et substitutions de la structure Command. */
int parse_command(Command* command) {
    int returnValue = 0;
    char* substitutions_tmp[MAX_NB_SUBSTITUTIONS]; // Stocke temporairement les strings des substitutions.
    char* in_sub_tmp = NULL;
    char* cpy = malloc(MAX_NB_ARGS * 10); /* On opère le parsing sur une copie de la string
    de commande originelle */
    strcpy(cpy, command -> strComm);
    // char* tmp = malloc(50 * sizeof(char)); // Stocke temporairement les tokens.
    char* inside_parentheses = malloc(MAX_NB_ARGS * 10); // Stocke la commande qui constitue une substitution.
    strcpy(inside_parentheses, ""); // Initialisation du buffer (important pour utiliser strcat).
    // Initialisation tableau argsComm.
    command -> argsComm = malloc(MAX_NB_ARGS * sizeof(char*));
    for (int i = 0; i < MAX_NB_ARGS; ++i) {
        command -> argsComm[i] = NULL;
    }
    unsigned index = 0; // Nombre de tokens.
    command -> argsComm[index] = malloc(MAX_SIZE_ARG);
    char* tmp = strtok(cpy, " ");
    strcpy(command -> argsComm[0], tmp);
    index++;
    while (1) { // Boucle sur les mots de la commande.
        if (index == (MAX_NB_ARGS)-1) { // Erreur si la commande contient trop de mots.
            fprintf(stderr,"bash : %s: too many arguments\n", command -> argsComm[0]);
            returnValue = -1;
            break;
        }
        tmp = strtok(NULL, " ");
        if (tmp == NULL) break;
        command -> argsComm[index] = malloc(MAX_SIZE_ARG);
        if (strcmp(tmp, "<(") == 0) { // Si on est au début d'une substitution.
            unsigned nb_parentheses_ouvrantes = 1;
            unsigned nb_parentheses_fermantes = 0;
            while(1) {
                tmp = strtok(NULL, " ");
                if (tmp == NULL) {
                    fprintf(stderr,"Command %s: badly formed brackets.\n", command -> argsComm[0]);
                    returnValue = -1;
                    break;
                }
                if (strcmp(tmp, "<(") == 0) nb_parentheses_ouvrantes++;
                if (strcmp(tmp, ")") == 0) nb_parentheses_fermantes++;
                if (nb_parentheses_ouvrantes != nb_parentheses_fermantes) {
                    strcat(inside_parentheses, tmp);
                    strcat(inside_parentheses, " ");
                } else {
                    break;
                }
            }
            strcpy(command -> argsComm[index], "fifo");
            if (command -> argsComm[index-1] != NULL && is_redirection_symbol(command -> argsComm[index-1])) {
                in_sub_tmp = malloc(MAX_NB_ARGS * 10);
                strcpy(in_sub_tmp, inside_parentheses);
            } else {
                substitutions_tmp[command -> nbSubstitutions] = malloc(MAX_NB_ARGS * 10);
                strcpy(substitutions_tmp[command -> nbSubstitutions], inside_parentheses);
                command -> nbSubstitutions++;
            }
            memset(inside_parentheses,0,strlen(inside_parentheses));
        }
        else strcpy(command -> argsComm[index],tmp);
        ++index;
    }
    command -> nbArgs = index;
    // Libération buffers.
    free(cpy);
    free(inside_parentheses);
    /* On crée des commandes à partir des substitutions récupérées. On le fait maintenant, à la fin de la
    fonction, et pas lorsque la string de commande est découpée, pour ne pas interrompre strtok. */
    if (command -> nbSubstitutions > 0) {
        // Initialisation tableau substitutions.
        command -> substitutions = malloc(MAX_NB_SUBSTITUTIONS * sizeof(Command));
        for (unsigned i = 0; i < MAX_NB_SUBSTITUTIONS; ++i) {
            command -> substitutions[i] = NULL;
        }
        // Remplissage tableau substitutions.
        for (int i = 0; i < command -> nbSubstitutions; ++i) {
            command -> substitutions[i] = getCommand(substitutions_tmp[i]);
            free(substitutions_tmp[i]);
            if (command -> substitutions[i] == NULL) {returnValue = -1;break;}
        }
    }
    // Pour l'éventuelle substitution au fichier d'entrée.
    if (in_sub_tmp != NULL) {
        command -> in_sub = getCommand(in_sub_tmp);
        free(in_sub_tmp);
        if (command -> in_sub == NULL) returnValue = -1;
    }
    return returnValue;
}

/* Découpe les éventuels symboles de redirection et les fichiers sur lequels rediriger, et l'éventuel
symbole '&', et remplit en conséquence les champs correspondant dans la structure Command passée en
argument. */
int parse_redirections(Command* command) {
    unsigned returnValue = 0;
    if (command -> argsComm == NULL) return -1;
    unsigned args_removed = 0;
    for (unsigned i = 0; i < command -> nbArgs; ++i) {
        if (command -> argsComm[i] == NULL) break;
        if (!strcmp(command -> argsComm[i], "&")) { /* '&' ne peut être qu'à la fin de la ligne de commande.
            et si c'est le cas, il a retiré plus tôt dans le parsing. */
            fprintf(stderr,"Command %s: misplaced '&' symbol.\n", command -> argsComm[0]);
            returnValue = -1;
            break;
        }
        int redirection_value = is_redirection_symbol(command -> argsComm[i]);
        if (redirection_value) {
            if (command -> argsComm[i+1] == NULL || is_redirection_symbol(command -> argsComm[i+1])) {
                fprintf(stderr,"Command %s: redirection symbol not followed by a file name.\n", command -> argsComm[0]);
                returnValue = -1;
                break;
            }
            switch (redirection_value) {
                case 1: // Cas d'un symbole de redirection d'entrée.
                    if (command -> in_redir != NULL) {
                        fprintf(stderr,"Command %s: too many input redirections.\n", command -> argsComm[0]);
                        return -1;
                    }
                    command -> in_redir = malloc(2 * sizeof(char*));
                    command -> in_redir[0] = malloc(5);
                    strcpy(command -> in_redir[0], command -> argsComm[i]);
                    command -> in_redir[1] = malloc(MAX_SIZE_ARG);
                    strcpy(command -> in_redir[1], command -> argsComm[i+1]);
                    break;
                case 2: // Cas d'un symbole de redirection de sortie.
                    if (command -> out_redir != NULL) {
                        fprintf(stderr,"Command %s: too many output redirections.\n", command -> argsComm[0]);
                        returnValue = -1;
                        break;
                    }
                    command -> out_redir = malloc(2 * sizeof(char*));
                    command -> out_redir[0] = malloc(5);
                    strcpy(command -> out_redir[0], command -> argsComm[i]);
                    command -> out_redir[1] = malloc(MAX_SIZE_ARG);
                    strcpy(command -> out_redir[1], command -> argsComm[i+1]);
                    break;
                case 3: // Cas d'un symbole de redirection de sortie erreur.
                    if (command -> err_redir != NULL) {
                        fprintf(stderr,"Command %s: too many error output redirections.\n", command -> argsComm[0]);
                        returnValue = -1;
                        break;
                    }
                    command -> err_redir = malloc(2 * sizeof(char*));
                    command -> err_redir[0] = malloc(5);
                    strcpy(command -> err_redir[0], command -> argsComm[i]);
                    command -> err_redir[1] = malloc(MAX_SIZE_ARG);
                    strcpy(command -> err_redir[1], command -> argsComm[i+1]);
                    break;
            }
            // Décalage du reste des arguments de deux cases vers la gauche.
            for (unsigned j = i+2; j < command -> nbArgs - args_removed; ++j) {
                strcpy(command -> argsComm[j-2], command -> argsComm[j]);
            }
            // Libération et mise à NULL des deux dernières cases d'arguments non-nulles.
            for (unsigned j = 0; j < 2; ++j) {
                free(command -> argsComm[command -> nbArgs - args_removed - 1]);
                command -> argsComm[command -> nbArgs - args_removed - 1] = NULL;
                args_removed++;
            }
            i--;
        }
    }
    command -> nbArgs -= args_removed;
    return returnValue;
}

/* Suivant si la string passée en argument est un symbole de redirection d'entrée, de sortie, de sortie
erreur ou n'est pas un symbole de redirection, la fonction renvoie respectivement la valeur 1,2,3 ou 0.*/
int is_redirection_symbol(char* string) {
    if (!strcmp(string, "<")) return 1;
    else if (!strcmp(string, ">") || !strcmp(string, ">|") || !strcmp(string, ">>")) return 2;
    else if (!strcmp(string, "2>") || !strcmp(string, "2>|") || !strcmp(string, "2>>")) return 3;
    else return 0;
}

/* Affiche une commande, son input (commande qui la précède dans une pipeline), et les substitutions qu'elle
utilise */
void print_command(Command* command) {
    // Affichage string de commande.
    printf("strComm: %s\n", command -> strComm);
    // Affichage arguments de la commande.
    printf("nbArgs: %i\n", command -> nbArgs);
    printf("argsComm: ");
    for (int i = 0; i < command -> nbArgs-1; ++i) {
        printf("%s,", command -> argsComm[i]);
    }
    printf("%s\n",command -> argsComm[command -> nbArgs-1]);
    // Affichage entrée, sortie et sortie erreur standard de la commande.
    if (command -> in_redir != NULL) printf("Input: %s %s\n", command -> in_redir[0], command -> in_redir[1]);
    if (command -> out_redir != NULL) printf("Output: %s %s\n", command -> out_redir[0], command -> out_redir[1]);
    if (command -> err_redir != NULL) printf("Error output: %s %s\n", command -> err_redir[0], command -> err_redir[1]);
    // Affichage input de la commande.
    printf("Input: %s\n", command -> input == NULL ? "None" : command -> input -> strComm);
    // Affichage substitutions qu'utilise la commande.
    printf("Substitutions: %i\n", command -> nbSubstitutions > 0);
    // for (int i = 0; i < command -> nbSubstitutions-1; ++i) {
    //     printf("(%i) %s\n", i+1, command -> substitutions[i] -> strComm);
    // }
}

// Libère toute la mémoire allouée pour une structure Command.
void free_command(Command* command) {
    // Libération string de commande.
    free(command -> strComm);
    // Libération arguments de la commande.
    for (int i = 0; i < command -> nbArgs; ++i) {
        free(command -> argsComm[i]);
    }
    free(command -> argsComm);
    // Libération redirection entrée de la commande.
    if (command -> in_redir != NULL) {
        if (command -> in_redir[0] != NULL) free(command -> in_redir[0]);
        if (command -> in_redir[1] != NULL) free(command -> in_redir[1]);
        free(command -> in_redir);
    }
    // Libération redirection sortie de la commande.
    if (command -> out_redir != NULL) {
        if (command -> out_redir[0] != NULL) free(command -> out_redir[0]);
        if (command -> out_redir[1] != NULL) free(command -> out_redir[1]);
        free(command -> out_redir);
    }
    // Libération redirection sortie erreur de la commande.
    if (command -> err_redir != NULL) {
        if (command -> err_redir[0] != NULL) free(command -> err_redir[0]);
        if (command -> err_redir[1] != NULL) free(command -> err_redir[1]);
        free(command -> err_redir);
    }
    // Libération espace alloué pour stocker les pointeurs vers les substitutions (les substitutions en
    // elle-mêmes ainsi que l'input sont normalement libérées à la fin de leur exécution).
    free(command -> substitutions);
    // Libération espace alloué pour la commande.
    free(command);
}