#include <stdbool.h>
#include "jobs_jsh.h"

// Fonctions de commande
void update_paths();
int cd(char* pathname);
int exit_jsh(char* val);
void print_lastReturn();

// Fonctions auxiliaires
int main(int argc, char** argv);
void main_loop();
void handle_job_execution(char* command_line);
bool foreground_job_stopped(int status);
int handle_command_execution(Command* command, int pipe_out[2]);
int* apply_redirections(Command* command, int pipe_in[2], int pipe_out[2]);
pid_t execute_command(Command* command, int pipe_out[2]);
void restore_standard_streams(int* standard_streams_copy);
int callRightCommand(Command* command);
bool is_internal_command(char* command_name);
bool correct_nbArgs(char**, unsigned, unsigned);
char* getPrompt(char* prompt_buf);

// Variables globales
bool running;
bool exit_tried;
int lastReturn;
char* current_folder_path;
char* previous_folder_path;
int path_buffers_size;
struct sigaction sa;
int nbJobs;
Job* l_jobs;
int refering_tty_fd;