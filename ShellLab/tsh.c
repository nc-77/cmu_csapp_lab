#include"tsh.h"
/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name and login ID here>
 */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */


struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */




/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    char buf[MAXLINE];
    strcpy(buf,cmdline);
    char *argv[MAXARGS];
    pid_t pid;
    int bg=parseline(buf,argv); /* run bg or fg */

    sigset_t mask_one;
    sigemptyset(&mask_one);
    sigaddset(&mask_one,SIGCHLD);

    if(argv[0]==NULL) 
        return;
    if(!builtin_cmd(argv)){ /*if builtin_cmd execute immediately,if not fork and execute*/
        sigprocmask(SIG_BLOCK,&mask_one,NULL);  /* 阻塞SIGCHLD防止竞争,确保正确addjob */
        
        if((pid=Fork())==0){    /* child */
            sigprocmask(SIG_UNBLOCK,&mask_one,NULL); /* 子进程继承父进程BOLCK,需要取消子进程阻塞 */
            setpgid(0,0); /* 为每个进程设立一个新的进程组 */
            if(execve(argv[0],argv,environ)<0){
                printf("%s: Commond not found\n",argv[0]);
                exit(0);
            }
        }
        /* parent wait for fg job to terminate */
        int state=(bg? BG:FG);
        
        addjob(jobs,pid,state,cmdline);
        
        /* 后台程序在printf后再解除CHLD阻塞防止在printf前被CHLD信号打断 */
        //sigprocmask(SIG_UNBLOCK,&mask_one,NULL);
        if(!bg) {
            sigprocmask(SIG_UNBLOCK,&mask_one,NULL);  /* 解除父进程CHLD阻塞 */
            waitfg(pid);
        }
        else{
            printf("[%d] (%d) %s",pid2jid(pid),pid,cmdline);
            sigprocmask(SIG_UNBLOCK,&mask_one,NULL);  /* 解除父进程CHLD阻塞 */
        }
    }
    
    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately. 
 *  quit jobs bg fg 
 */
int builtin_cmd(char **argv) 
{
    if(!strcmp(argv[0],"quit"))
        exit(0);
    else if(!strcmp(argv[0],"jobs")){
        listjobs(jobs);
        return 1;
    }
    else if(!strcmp(argv[0],"bg")||!strcmp(argv[0],"fg")){
        do_bgfg(argv);
        return 1;
    }
    return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    if(argv[1]==NULL){
        printf("%s command requires PID or %%jobid argument\n",argv[0]);
        return;
    }
    int id;
    struct job_t *job;
    /* 按&%d格式读取jid */
    if(sscanf(argv[1],"%%%d",&id)>0){
        job=getjobjid(jobs,id);
        if(job==NULL){
            printf("(%d): No such job\n",id);
            return;
        }
    }
    /* 按%d格式读取pid */
    else if(sscanf(argv[1],"%d",&id)>0){
        job=getjobpid(jobs,id);
        if(job==NULL){
            printf("%s: No such process\n",argv[1]);
            return;
        }
    }
    /* 格式错误 */
    else{
        printf("%s: argument must be a PID or %%jobid\n",argv[0]);
        return;
    }
    kill(-(job->pid),SIGCONT);
    if(!strcmp("bg",argv[0])){
        printf("[%d] (%d) %s",job->jid,job->pid,job->cmdline);
        job->state=BG;
    }
    else{   /*job转到前台运行并等待job运行完 */
        job->state=FG;
        
        waitfg(job->pid);
    }
    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    sigset_t mask_empty;
    sigemptyset(&mask_empty);
    while(fgpid(jobs)==pid){  /* 等待fg结束向父进程发送SIGCHLD信号 */
        sigsuspend(&mask_empty);
        
    }
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
/* 子进程终止时向父进程发送SIGCHLD信号  */
void sigchld_handler(int sig) 
{
    int olderrno=errno;
    pid_t pid;
    sigset_t mask_all,prev;
    sigfillset(&mask_all);
    int status;
    while((pid=waitpid(-1,&status,WNOHANG|WUNTRACED))>0){   /* WNOHANG 所有子进程终止后返回 | WUNTRACED 即使进程被ST也立即返回 */
        if(WIFEXITED(status)){                       /* 进程正常终止 */
            sigprocmask(SIG_BLOCK,&mask_all,&prev);  /* 访问全局变量jobs上阻塞 */
            deletejob(jobs,pid);
            sigprocmask(SIG_SETMASK,&prev,NULL);  /* 解除阻塞 */
        }
        else if(WIFSIGNALED(status)){           /* 进程由于SIGINT信号终止 */
            sigprocmask(SIG_BLOCK,&mask_all,&prev);  
            printf("Job [%d] (%d) terminated by signal %d\n",pid2jid(pid),pid,SIGINT); /* 这里printf非异步函数,其实不应该用在此处 */
            deletejob(jobs,pid);
            sigprocmask(SIG_SETMASK,&prev,NULL); 
        }
        else if(WIFSTOPPED(status)){           /* 进程由于SIGTSTP信号挂起 */
            struct job_t *job=getjobpid(jobs,pid);
            sigprocmask(SIG_BLOCK,&mask_all,&prev);  
            printf("Job [%d] (%d) stoped by signal %d\n",pid2jid(pid),pid,SIGTSTP); /* 这里printf非异步函数,其实不应该用在此处 */
            job->state=ST;
            sigprocmask(SIG_SETMASK,&prev,NULL); 
        }
    }
    errno=olderrno;
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    int olderrno=errno;
    pid_t pid=fgpid(jobs);
    if(pid){
        kill(-pid,SIGINT);
    }
    
    errno=olderrno;
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    int olderrno=errno;
    pid_t pid=fgpid(jobs);
    if(pid){
        struct job_t *job=getjobpid(jobs,pid);
        if(job->state==ST)  /* 已经停止了 */
            return;
        else 
            kill(-pid,SIGTSTP);
    }
    errno=olderrno;
    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

pid_t Fork()
{
    pid_t pid;
    if((pid=fork())<0)
        unix_error("Fork error");
    return pid;
}


