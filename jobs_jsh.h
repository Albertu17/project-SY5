#include <stdbool.h>

// Structures
struct Job {//maximum de 40 jobs simultanément
    int job_num;
    int pgid;
    char* state;
    char* command_line;
    bool background;
};
typedef struct Job Job;

// Fonctions
Job create_job(char* command_name, int job_num, bool background);
void print_job(Job job);
// void print_jobs(pid_t job, bool isJob, bool tHyphen);
void print_jobs(Job* l_jobs, int nbJobs);
void remove_job(Job* l_jobs, int job_num);
int check_jobs_state(Job* l_jobs, int nbJobs);
void change_job_state(Job job, char* state);
bool inspectAllSons(pid_t pid, int sig,bool print,bool hasStopped);
int killJob (char* sig, char* pid);
void check_sons_state();
void waitForAllSons(pid_t pid);
int bg(int job_num);
int fg(int job_num, int refering_tty_fd);