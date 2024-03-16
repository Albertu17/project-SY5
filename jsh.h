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
void launch_job_execution(char* command_line);
bool foreground_job_stopped(int status);
int execute_command(Command* command, int pipe_out[2]);
int* apply_redirections(Command* command, int pipe_in[2], int pipe_out[2]);
void restore_standard_streams(int standard_streams_copy[3]);
int callRightCommand(Command* command);
bool is_internal_command(char* command_name);
bool correct_nbArgs(char**, unsigned, unsigned);
char* getPrompt(char* prompt_buf);

// Variables globales
bool running;
int lastReturn;
char* current_folder;
char* previous_folder;
struct sigaction sa;
int nbJobs;
Job* l_jobs;
int refering_tty_fd;