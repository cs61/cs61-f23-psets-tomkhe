#include "sh61.hh"
#include <cstring>
#include <cerrno>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>
#include <iostream>

// For the love of God
#undef exit
#define exit __DO_NOT_CALL_EXIT__READ_PROBLEM_SET_DESCRIPTION__


// struct command
//    Data structure describing a command. Add your own stuff.

struct command {
    std::vector<std::string> args;
    pid_t pid = -1;      // process ID running this command, -1 if none

    // linnked list structure
    command* prev_in_pipeline = nullptr; 
    command* next_in_pipeline = nullptr; 

    int read_fd = -1; 

    command();
    ~command();

    void run();
};

struct pipeline {
    command* child_command = nullptr; 
    pipeline* next_in_conditional = nullptr; 
    bool next_is_or = false; 
}; 

struct conditional {
    pipeline* child_pipeline = nullptr; 
    conditional* next_in_list = nullptr; 
    bool is_background = false; 
}; 

struct list {
    conditional* child_conditional = nullptr; 
}; 

// command::command()
//    This constructor function initializes a `command` structure. You may
//    add stuff to it as you grow the command structure.

command::command() {
}


// command::~command()
//    This destructor function is called to delete a command.

command::~command() {
}


// COMMAND EXECUTION

// command::run()
//    Creates a single child process running the command in `this`, and
//    sets `this->pid` to the pid of the child process.
//
//    If a child process cannot be created, this function should call
//    `_exit(EXIT_FAILURE)` (that is, `_exit(1)`) to exit the containing
//    shell or subshell. If this function returns to its caller,
//    `this->pid > 0` must always hold.
//
//    Note that this function must return to its caller *only* in the parent
//    process. The code that runs in the child process must `execvp` and/or
//    `_exit`.
//
//    PART 1: Fork a child process and run the command using `execvp`.
//       This will require creating a vector of `char*` arguments using
//       `this->args[N].c_str()`. Note that the last element of the vector
//       must be a `nullptr`.
//    PART 4: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PART 7: Handle redirections.

void command::run() {
    assert(this->pid == -1);
    assert(this->args.size() > 0);

    int pipefd[2]; 
    if (this->next_in_pipeline)
    {
        // set up pipe
        int r = pipe(pipefd); 
        assert(r == 0); 
    }
    
    pid_t p = fork(); 
    if (p == 0)
    {
        // in child process
        // create char* array as argument
        char* arr[this->args.size()+1]; 
        for (int i = 0; (size_t) i < this->args.size(); i++) arr[i] = (char*) this->args[i].c_str(); 
        arr[this->args.size()] = nullptr; 

        if (this->next_in_pipeline)
        {
            close(pipefd[0]); // close read end
            dup2(pipefd[1], STDOUT_FILENO); // make stdout point to write end of pipe
            close(pipefd[1]); 
        }

        if (this->prev_in_pipeline)
        {
            assert(read_fd != -1);
            dup2(this->read_fd, STDIN_FILENO); // make stdin point to read end of pipe
            close(this->read_fd); 
        }
        
        // run process
        int r = execvp(arr[0], arr); 
        if (r == -1) _exit(EXIT_FAILURE); 
        
        return; 
    }else if (p != 0)
    {
        // in parent process
        this->pid = p; 

        if (this->next_in_pipeline)
        {
            (this->next_in_pipeline)->read_fd = pipefd[0];      // preserve read end of pipe
            close(pipefd[1]);                                   // close write end of pipe
        }

        return; 
    }else _exit(EXIT_FAILURE); 

    fprintf(stderr, "command::run not done yet\n");
}


// run_list(c)
//    Run the command *list* starting at `c`. Initially this just calls
//    `c->run()` and `waitpid`; you’ll extend it to handle command lists,
//    conditionals, and pipelines.
//
//    It is possible, and not too ugly, to handle lists, conditionals,
//    *and* pipelines entirely within `run_list`, but many students choose
//    to introduce `run_conditional` and `run_pipeline` functions that
//    are called by `run_list`. It’s up to you.
//
//    PART 1: Start the single command `c` with `c->run()`,
//        and wait for it to finish using `waitpid`.
//    The remaining parts may require that you change `struct command`
//    (e.g., to track whether a command is in the background)
//    and write code in `command::run` (or in helper functions).
//    PART 2: Introduce a loop to run a list of commands, waiting for each
//       to finish before going on to the next.
//    PART 3: Change the loop to handle conditional chains.
//    PART 4: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PART 5: Change the loop to handle background conditional chains.
//       This may require adding another call to `fork()`!

void run_list(list* ls) {
    conditional* curr_conditional = ls->child_conditional; 
    pipeline* curr_pipeline = curr_conditional->child_pipeline; 
    command* curr_command = curr_pipeline->child_command; 
    bool run = 1; 
    int status = 0; 

    while (curr_conditional)
    {
        assert(curr_command && curr_pipeline && curr_conditional); 
        
        if (run && curr_command->args.size() != 0)
        {
            // run current command
            curr_command->run(); 
            assert(curr_command->pid != -1); 
        }

        int pid = curr_command->pid; 
        curr_command = curr_command->next_in_pipeline; 
        if (curr_command == nullptr || curr_command->args.size() == 0)
        {
            // reached end of current pipeline
            // close all read ends
            command* com = curr_pipeline->child_command; 
            while (com)
            {
                if (com->read_fd != -1) close(com->read_fd); 
                com = com->next_in_pipeline; 
            }

            if (run)
            {
                // wait for last command to finish
                pid_t exited_pid = waitpid(pid, &status, 0); 
                if (exited_pid != 0 && exited_pid != pid) break; 
            }

            // look at conditional
            run = (curr_pipeline->next_is_or == (WIFEXITED(status) && WEXITSTATUS(status))); 
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) run = !curr_pipeline->next_is_or; 
            else run = curr_pipeline->next_is_or; 

            // try to go to the next pipeline
            curr_pipeline = curr_pipeline->next_in_conditional; 

            if (curr_pipeline == nullptr)
            {
                // go to next conditional
                curr_conditional = curr_conditional->next_in_list; 
                if (curr_conditional) curr_pipeline = curr_conditional->child_pipeline; 

                // reset conditional
                run = 1; 
                status = 0; 
            }

            if (curr_pipeline) curr_command = curr_pipeline->child_command; 
        }
    }

    if (curr_conditional == nullptr) return; 

    fprintf(stderr, "command::run not done yet\n"); 
}


// parse_line(s)
//    Parse the command list in `s` and return it. Returns `nullptr` if
//    `s` is empty (only spaces). You’ll extend it to handle more token
//    types.

list* parse_line(const char* s) {
    shell_parser parser(s);

    list* ls = new list; 
    conditional* prev_conditional = nullptr; 
    conditional* curr_conditional = new conditional; 
    pipeline* prev_pipeline = nullptr; 
    pipeline* curr_pipeline = new pipeline; 
    command* prev_command = nullptr; 
    command* curr_command = new command; 

    ls->child_conditional = curr_conditional; 
    curr_conditional->child_pipeline = curr_pipeline; 
    curr_pipeline->child_command = curr_command; 

    int con_n = 1, pip_n = 1, com_n = 1; 

    for (auto it = parser.begin(); it != parser.end(); ++it) {
        switch (it.type())
        {
            case TYPE_SEQUENCE: 
                // new conditional
                prev_conditional = curr_conditional; 
                curr_conditional = new conditional; 
                prev_conditional->next_in_list = curr_conditional; 
                con_n++; 

                // new pipeline
                prev_pipeline = nullptr; 
                curr_pipeline = new pipeline; 
                curr_conditional->child_pipeline = curr_pipeline; 
                pip_n++; 

                // new command
                prev_command = nullptr; 
                curr_command = new command; 
                curr_pipeline->child_command = curr_command; 
                com_n++; 
                break; 

            case TYPE_AND: case TYPE_OR: 
                // new pipeline
                prev_pipeline = curr_pipeline; 
                curr_pipeline = new pipeline; 
                prev_pipeline->next_in_conditional = curr_pipeline; 
                prev_pipeline->next_is_or = (it.type() == TYPE_OR); 
                pip_n++; 

                // new command
                prev_command = nullptr; 
                curr_command = new command; 
                curr_pipeline->child_command = curr_command; 
                com_n++; 
                break; 

            case TYPE_PIPE: 
                // new command
                prev_command = curr_command; 
                curr_command = new command; 
                curr_command->prev_in_pipeline = prev_command; 
                prev_command->next_in_pipeline = curr_command; 
                com_n++; 
                break; 

            default: 
                curr_command->args.push_back(it.str());  
        }
    }
    return ls; 
}

void cleanup(list* ls)
{
    conditional* curr_conditional = ls->child_conditional; 
    while (curr_conditional != nullptr)
    {
        pipeline* curr_pipeline = curr_conditional->child_pipeline; 
        while (curr_pipeline != nullptr)
        {
            command* curr_command = curr_pipeline->child_command; 
            while (curr_command != nullptr)
            {
                command* temp = curr_command->next_in_pipeline; 
                delete curr_command; 
                curr_command = temp; 
            }
            pipeline* temp = curr_pipeline->next_in_conditional; 
            delete curr_pipeline; 
            curr_pipeline = temp; 
        }
        conditional* temp = curr_conditional->next_in_list; 
        delete curr_conditional; 
        curr_conditional = temp; 
    }
    delete ls; 
}



int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    bool quiet = false;

    // Check for `-q` option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = true;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            return 1;
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    claim_foreground(0);
    set_signal_handler(SIGTTOU, SIG_IGN);

    char buf[BUFSIZ];
    int bufpos = 0;
    bool needprompt = true;

    while (!feof(command_file)) {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = false;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == nullptr) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file)) {
                    perror("sh61");
                }
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            if (list* ls = parse_line(buf)) {
                run_list(ls);
                cleanup(ls); 
            }
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        // Your code here!
    }

    return 0;
}