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
void print_jobs(pid_t job, bool isJob, bool tHyphen);
void removeJob(Job* l_jobs, int nbJobs, int job_num);
bool inspectAllSons(pid_t pid, int sig,bool print,bool hasStopped);
int killJob (char* sig, char* pid);
void check_sons_state();
void waitForAllSons(pid_t pid);