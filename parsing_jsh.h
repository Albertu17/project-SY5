#include <stddef.h>
#define MAX_NB_ARGS 64 // Nombre d'arguments max d'une commande (une substitution compte pour 1).
#define MAX_SIZE_ARG 128 // Taille maximum pour un argument (token) d'une commande.
#define MAX_NB_SUBSTITUTIONS 32 // Nombre de substitutions max pour une commande.
#define MAX_LENGTH_PIPELINE 32 // Nombre de commandes max dans une pipeline.

// Structure
struct Command {
    char* strComm; // La string de commande.
    char** argsComm; /* Les différents arguments de la commande (avec la string "fifo" aux
    emplacements des futures éventuels tubes, et sans les éventuelles redirections et symbole '&'). */
    unsigned nbArgs; // Le nombre d'arguments de la commande.
    struct Command* in_sub; // L'éventuelle substitution à un fichier d'entrée.
    char** in_redir; // Symbole de redirection de l'entrée et nom du fichier sur lequel rediriger.
    char** out_redir; // Symbole de redirection de la sortie et nom du fichier sur lequel rediriger.
    char** err_redir; // Symbole de redirection de la sortie erreur et nom du fichier sur lequel rediriger.
    struct Command** substitutions; // Les éventuelles substitutions (sans compter celle au fichier d'entrée).
    unsigned nbSubstitutions; // Le nombre de substitutions qu'utilise la commande (sans compter celle au fichier d'entrée).
    struct Command* input; // La commande précédente (dans le contexte d'une pipeline).
    // bool background; // booléen indiquant si la commande doit être lancée en arrière-plan ou non.
};
typedef struct Command Command;

// Fonctions
Command* getCommand(char* command_line);
int parse_ampersand(char* command_line);
Command* create_command();
char* first_command(char* input);
int parse_command(Command* command);
int parse_redirections(Command* command);
int is_redirection_symbol(char* string);
void print_command(Command* command);
void free_command(Command* command);