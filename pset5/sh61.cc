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

    int status;
    bool is_background = false;

    command* next = nullptr;
    command* prev = nullptr;

    command();
    ~command();

    bool redirect_out = false;
    bool redirect_in = false;
    bool redirect_err = false;

    int read_fd = -1;

    // Connecting operator
    int op = TYPE_SEQUENCE;

    std::string out;
    std::string in;
    std::string err;
    std::string file;

    pid_t run();
};

command* chain_in_background(command* c) {
    while (c->next && c->op != TYPE_SEQUENCE && c->op != TYPE_BACKGROUND) {
        c = c->next;
    }
    return c->next;
}

void change_fd(int file, int curr) {
    if (file < 0) {
        fprintf(stderr, "No such file or directory\n");
        _exit(1);
    }
    else {
        dup2(file, curr);
        close(file);
    }
}


// command::command()
//    This constructor function initializes a `command` structure. You may
//    add stuff to it as you grow the command structure.

command::command() {
}


// command::~command()
//    This destructor function is called to delete a command.

command::~command() {
    if (this->next) {
        delete(this->next);
    }
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

pid_t command::run() {
    assert(this->pid == -1);
    assert(this->args.size() > 0);
    // Your code here!

    int N = this->args.size();
    char* argv[N + 1];
    for (int i = 0; i < N; i++) {
        argv[i] = (char *) this->args[i].c_str();
    }
    argv[N] = nullptr;

    this->pid = fork();

    if (this->pid == 0) {
        // cd check
        if (strcmp(argv[0], "cd") == 0) {
            if (chdir(argv[1]) < 0) {
                _exit(1);
            }
            else {
                _exit(0);
            }
            return this->pid;
        }
        // pipe check
        if (this->read_fd != -1) {
            dup2(this->read_fd, 0);
            close(this->read_fd);
        }

        //redirection check
        if (this->redirect_in) {
            int i_file = open(this->in.c_str(), O_RDONLY);
            change_fd(i_file, STDIN_FILENO);
        }
        if (this->redirect_out) {
            int o_file = open(this->out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            change_fd(o_file, STDOUT_FILENO);
        }
        if (this->redirect_err) {
            int e_file = open(this->err.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            change_fd(e_file, STDERR_FILENO);
        }
        if (execvp(argv[0], argv) < 0) {
            _exit(0);
        }
    }
    else {
        if (strcmp(argv[0], "cd") == 0) {
            //avoiding unused variable warning
            assert(chdir(argv[1]) != -10);
        }
    }

    return this->pid;
}

void run_pipe(command* c, int status) {
    int pipefd[2];
    assert(pipe(pipefd) >= 0);
    if (c->next) {
        c->next->read_fd = pipefd[0];
    }

    if (fork() == 0) {
        dup2(pipefd[1], 1);
        close(pipefd[1]);
        close(pipefd[0]);
        c->pid = c->run();
        _exit(0);
    }
    else {
        waitpid(0, &status, WNOHANG);
        close(pipefd[1]);
    } 
}

void run_helper(command* c, command* end, bool background = false) {
    bool cond_true = true;
    int wstatus = c->status;
    while (c && c != end) {
        if (cond_true) {
            if (background) {
                c->is_background = true;
            }
            if (c->op == TYPE_PIPE) {
                run_pipe(c, wstatus);
                c = c->next; 
                continue;
            }
            
            c->pid = c->run();
            if (c->op != TYPE_BACKGROUND) {
                waitpid(c->pid, &wstatus, 0);
            }
            if (c->op == TYPE_AND) {
                cond_true = (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0);
            }
            else if (c->op == TYPE_OR) {
                cond_true = (!(WIFEXITED(wstatus)) || (WEXITSTATUS(wstatus) != 0));
            }
        }
        else {
            cond_true = true;
            if (c->op == TYPE_AND) {
                cond_true = (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0);
            }
            else if (c->op == TYPE_OR) {
                cond_true = (!(WIFEXITED(wstatus)) || (WEXITSTATUS(wstatus) != 0));            
            }   
        }
        c = c->next;
    }
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

void run_list(command* c) {
    command* last = chain_in_background(c);
    if (last) {
        if (last->prev->op == TYPE_BACKGROUND) {
            if (fork() == 0) {
                run_helper(c, last, true);
                _exit(0);
            }
            else {
                run_list(last);
            }
        }
        else {
            run_helper(c, nullptr);
        }
    }
    else {
        run_helper(c, nullptr);
    }
}


// parse_line(s)
//    Parse the command list in `s` and return it. Returns `nullptr` if
//    `s` is empty (only spaces). You’ll extend it to handle more token
//    types.

command* parse_line(const char* s) {
    shell_parser parser(s);
    // Your code here!

    // Build the command
    // The handout code treats every token as a normal command word.
    // You'll add code to handle operators.
    command* chead = nullptr;    // first command in list
    command* clast = nullptr;    // last command in list
    command* ccur = nullptr;     // current command being built
    for (auto it = parser.begin(); it != parser.end(); ++it) {
        switch (it.type()) {
        case TYPE_NORMAL:
            // Add a new argument to the current command.
            // Might require creating a new command.
            if (!ccur) {
                ccur = new command;
                ccur->op = it.type();
                if (clast) {
                    clast->next = ccur;
                    ccur->prev = clast;
                } else {
                    chead = ccur;
                }
            }
            ccur->args.push_back(it.str());
            break;
        case TYPE_SEQUENCE:
        case TYPE_BACKGROUND:
        case TYPE_PIPE:
        case TYPE_AND:
        case TYPE_OR:
            // These operators terminate the current command.
            assert(ccur);
            clast = ccur;
            clast->op = it.type();
            ccur = nullptr;
            break;
        case TYPE_REDIRECT_OP:
            clast = ccur;
            clast->op = it.type();
            // Input
            if (it.str().compare("<") == 0) {
                clast->redirect_in = true;
                ++it;
                clast->in = it.str();
            }
            // Output
            if (it.str().compare(">") == 0) {
                clast->redirect_out = true;
                ++it;
                clast->out = it.str();
            }
            // Error
            if (it.str().compare("2>") == 0) {
                clast->redirect_err = true;
                ++it;
                clast->err = it.str();
            }
            break;
        }
    }
    return chead;
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
            if (command* c = parse_line(buf)) {
                run_list(c);
                delete c;
            }
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        while (waitpid(-1, NULL, WNOHANG) > 0) {
            continue;
        }
    }

    return 0;
}
