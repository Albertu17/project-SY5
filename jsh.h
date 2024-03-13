#include <stdbool.h>
#include "jobs_jsh.h"

// Fonctions de commande
char* pwd();
int cd(char* pathname);
int exit_jsh(int val);
void print_lastReturn();
int external_command(Command* command, int pipe_out[2]);
int bg(int job_num);
int fg(int job_num);

// Fonctions auxiliaires
int main(int argc, char** argv);
void main_loop();
int launch_job_execution(char* command_line);
int execute_command(Command* command, int pipe_out[2]);
int apply_redirections(Command* command, int pipe_in[2], int pipe_out[2]);
int callRightCommand(Command* command);
bool correct_nbArgs(char**, unsigned, unsigned);
char* getPrompt();

// Variables globales
bool running;
int lastReturn;
char* current_folder;
char* previous_folder;
struct sigaction sa;
int nbJobs;
Job* l_jobs;