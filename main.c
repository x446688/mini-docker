/*
  This files defines the main logic of the mini-docker program, including but
  not limited to daemonization and getopt.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include "mini-container/create.h"
#include "mini-logger/logger.h"

static int stop = 0;
static int delay = 1;
static int counter = 0;
static pid_t child_pid = 0;
static pid_t container_pid = 0;
static char *command_file_name = NULL;
static char *file_name = NULL;
static char *pid_file_name = NULL;
static char *mount_point = NULL;
static int pid_fd = -1;
static char *app_name = NULL;

int container_start() {
    char *stack = 0;
    stack = malloc(CONTAINER_STACK_SIZE);
    container_cfg config = {0};

    logdoc(LOG_LVL_INFO, LOGFILE, "%s %s", file_name, command_file_name);
    config.hostname = "mini";
    config.uid = 0;
    config.mnt = mount_point;
    
    if (command_file_name == NULL) {    
        config.cmd = "/bin/sh";
        config.arg = "sh";
    } else {
        config.cmd = command_file_name;
        config.arg = file_name;
    }
    
    printf("%s\n", mount_point);
    int sockets[2] = {0};
    int exitcode = 0;
    char progname[] = "mini-docker";

    if (geteuid() != 0) {
        printf("mini-docker should be run as root\n");
    }

    config.hostname = "mini";

    printf("initializing socket pair...\n");
    if (socketpair(AF_LOCAL, SOCK_SEQPACKET, 0, sockets)) {
        printf("failed to initialialize socket pair: %m\n");
        exitcode = 1;
        return exitcode;
    }

    printf("setting socket flags...\n");
    if (fcntl(sockets[0], F_SETFD, FD_CLOEXEC)) {
        printf("failed to socket fcntl: %m\n");
        exitcode = 1;
        return exitcode;
    }
    config.fd = sockets[1];

    printf("initializing container...\n");
    if ((container_pid = container_init(&config, stack + CONTAINER_STACK_SIZE)) == -1) {
        printf("failed to container_init\n");
        exitcode = 1;
        return exitcode;
    }
    
    printf("container pid: %d\n", container_pid);
    
    printf("initializing cgroups...\n");
    if (cgroups_init(config.hostname, container_pid) == -1) {
        printf("failed to initialize cgroups\n");
        exitcode = 1;
        return exitcode;
    }

    printf("configuring user namespace...\n");
    if (user_namespace_prepare_mappings(container_pid, sockets[0])) {
        exitcode = 1;
        printf("failed to user_namespace_set_user: %s\nstopping container...\n", strerror(errno));
        return exitcode;
    }
    
    printf("waiting for container to exit...\n");
    exitcode |= container_wait(container_pid);
    printf("container exited\n");

    printf("freeing resources...\n");
    printf("freeing stack...\n");
    free(stack);
    printf("freeing sockets...\n");
    close(sockets[0]);
    close(sockets[1]);
    printf("freeing cgroups...\n");
    cgroups_free(config.hostname);
    
    return exitcode;
}

void handle_signal(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        fprintf(stderr, "Debug: stopping...\n");
        if (child_pid > 0) {
            logdoc(LOG_LVL_INFO, LOGFILE, "Sending SIGINT to child %d", child_pid);
            kill(child_pid, SIGINT);
            if (kill(child_pid, 0) == 0) {
                kill(child_pid, SIGKILL);
            }
        }
        
        if (container_pid > 0) {
            logdoc(LOG_LVL_INFO, LOGFILE, "Sending SIGINT to container %d", container_pid);
            kill(container_pid, SIGINT);
            if (kill(container_pid, 0) == 0) {
                kill(container_pid, SIGKILL);
            }
        }

        if (pid_fd != -1) {
            lockf(pid_fd, F_ULOCK, 0);
            close(pid_fd);
        }
        if (pid_file_name != NULL) {
            logdoc(LOG_LVL_INFO, LOGFILE, "DEBUG: unlinking %s\n", pid_file_name);
            unlink(pid_file_name);
        }
        stop = 1;
        exit(0);
    } else if (sig == SIGHUP) {
        fprintf(stderr, "Debug: reloading daemon config file ...\n");
    } else if (sig == SIGCHLD) {
        fprintf(stderr, "Debug: received SIGCHLD signal\n");
    }
}

static void daemonize()
{
    pid_t pid = 0;
    int fd;

    pid = fork();
    if (pid < 0) {
        logdoc(LOG_LVL_ERROR, LOGFILE, "Error forking: %s", strerror(errno));;
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, SIG_IGN);

    pid = fork();
    if (pid < 0) {
        logdoc(LOG_LVL_ERROR, LOGFILE, "Error forking: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);
    chdir("/");

    for (fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--) {
        close(fd);
    }

    stdin = fopen("/dev/null", "r");
    stdout = fopen("/dev/null", "w+");
    stderr = fopen("/dev/null", "w+");

    if (pid_file_name != NULL) {
        logdoc(LOG_LVL_INFO, LOGFILE, "%s\n", pid_file_name);
        char str[256];
        pid_fd = open(pid_file_name, O_RDWR|O_CREAT, 0640);
        if (pid_fd < 0) {
            logdoc(LOG_LVL_ERROR, LOGFILE, "Can't open lockfile");
            exit(EXIT_FAILURE);
        }
        if (lockf(pid_fd, F_TLOCK, 0) < 0) {
            logdoc(LOG_LVL_ERROR, LOGFILE, "Can't lock file");
            exit(EXIT_FAILURE);
        }
        sprintf(str, "%d\n", getpid());
        write(pid_fd, str, strlen(str));
        close(pid_fd);
    }
}

void print_help(void)
{
    printf("\n Usage: %s [OPTIONS]\n\n", app_name);
    printf("  Options:\n");
    printf("   -h --help                 Print this help\n");
    printf("   -m --mount                Mountpoint\n");
    printf("   -c --command filename     Execute the command\n");
    printf("   -l --log_file  filename   Write logs to the file\n");
    printf("   -d --daemon               Daemonize this application\n");
    printf("   -p --pid_file  filename   PID file used by daemonized app\n");
    printf("\n");
}

int main(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"command", required_argument, 0, 'c'},
        {"mount", required_argument, 0, 'm'},
        {"log_file", required_argument, 0, 'l'},
        {"help", no_argument, 0, 'h'},
        {"daemon", no_argument, 0, 'd'},
        {"pid_file", required_argument, 0, 'p'},
        {NULL, 0, 0, 0}
    };
    int value, option_index = 0;
    char *log_file_name = NULL;
    int start_daemonized = 0;

    app_name = argv[0];

    while ((value = getopt_long(argc, argv, "c:m:l:p:dh", long_options, &option_index)) != -1) {
        switch (value) {
            case 'c':
                command_file_name = strdup(optarg);
                file_name = strdup(basename(optarg));
                break;
            case 'm':
                mount_point = strdup(optarg);
                break;
            case 'l':
                log_file_name = strdup(optarg);
                break;
            case 'p':
                pid_file_name = strdup(optarg);
                break;
            case 'd':
                start_daemonized = 1;
                break;
            case 'h':
                print_help();
                break;
            case '?':
                print_help();
                break;
            default:
                break;
        }
    }

    if (start_daemonized == 1) {
        logdoc(LOG_LVL_WARNING, LOGFILE, "Starting daemonize");
        daemonize();
        signal(SIGCHLD, SIG_DFL);
        
        signal(SIGINT, handle_signal);
        signal(SIGTERM, handle_signal);
        signal(SIGHUP, handle_signal);
        
        while (stop == 0) {
            pid_t pid = fork();

            if (pid < 0) {
                logdoc(LOG_LVL_ERROR, LOGFILE, "Error forking: %s", strerror(errno));
                sleep(1);
                continue;
            }
            
            if (pid == 0) {
                char *cmd[] = { "mini-docker-init", mount_point, command_file_name, NULL };
                if (execve("/usr/bin/mini-docker-init", cmd, NULL) == -1) {
                    logdoc(LOG_LVL_ERROR, LOGFILE, "Failed to execve: %s", strerror(errno));
                    exit(EXIT_FAILURE);
                }
            } else {
                child_pid = pid;
                wait(NULL);
                logdoc(LOG_LVL_INFO, LOGFILE, "Calling container_start()");
                container_start();
            }
        }
    } else {
        signal(SIGINT, handle_signal);
        signal(SIGHUP, handle_signal);
        
        pid_t pid = fork();

        if (pid < 0) {
            logdoc(LOG_LVL_ERROR, LOGFILE, "Error forking: %s", strerror(errno));
            return EXIT_FAILURE;
        }
        
        if (pid == 0) {
            char *cmd[] = { "mini-docker-init", mount_point, command_file_name, NULL };
            if (execve("/usr/bin/mini-docker-init", cmd, NULL) == -1) {
                logdoc(LOG_LVL_ERROR, LOGFILE, "Failed to execve: %s", strerror(errno));
                exit(EXIT_FAILURE);
            }
        } else {
            child_pid = pid;
            wait(NULL);
            logdoc(LOG_LVL_INFO, LOGFILE, "Calling container_start()");
            int result = container_start();
            
            /* Free allocated memory */
            if (command_file_name != NULL) free(command_file_name);
            if (file_name != NULL) free(file_name);
            if (log_file_name != NULL) free(log_file_name);
            if (pid_file_name != NULL) free(pid_file_name);
            if (mount_point != NULL) free(mount_point);
            
            return result;
        }
    }

    if (command_file_name != NULL) free(command_file_name);
    if (file_name != NULL) free(file_name);
    if (log_file_name != NULL) free(log_file_name);
    if (pid_file_name != NULL) free(pid_file_name);
    if (mount_point != NULL) free(mount_point);

    return EXIT_SUCCESS;

}
