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
static char *conf_file_name = NULL;
static char *file_name = NULL;
static char *pid_file_name = NULL;
static char *mount_point = NULL;
static int pid_fd = -1;
static char *app_name = NULL;

int container_start() {
    char *stack = 0;
    stack = malloc(CONTAINER_STACK_SIZE);
    container_cfg config = {0};

    logdoc(LOG_LVL_INFO, LOGFILE, "%s %s", file_name, conf_file_name);
    config.hostname = "mini";
    config.uid = 0;
    config.mnt = mount_point;
    
    if (conf_file_name == NULL) {    
        config.cmd = "/usr/bin/busybox";
        config.arg = "sh";
    } else {
        config.cmd = conf_file_name;
        config.arg = file_name;
    }
    
    printf("%s\n", mount_point);
    int container_pid = 0;
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

int read_conf_file(int reload)
{
    FILE *conf_file = NULL;
    int ret = -1;

    if (conf_file_name == NULL) return 0;

    conf_file = fopen(conf_file_name, "r");
    if (conf_file == NULL) {
        syslog(LOG_ERR, "Can not open config file: %s, error: %s",
                conf_file_name, strerror(errno));
        return -1;
    }

    ret = fscanf(conf_file, "%d", &delay);
    if (ret > 0) {
        if (reload == 1) {
            syslog(LOG_INFO, "Reloaded configuration file %s of %s",
                conf_file_name, app_name);
        } else {
            syslog(LOG_INFO, "Configuration of %s read from file %s",
                app_name, conf_file_name);
        }
    }

    fclose(conf_file);
    return ret;
}

void handle_signal(int sig)
{
    if (sig == SIGINT) {
        fprintf(stderr, "Debug: stopping daemon ...\n");
        if (pid_fd != -1) {
            lockf(pid_fd, F_ULOCK, 0);
            close(pid_fd);
        }
        if (pid_file_name != NULL) {
            logdoc(LOG_LVL_INFO, LOGFILE, "DEBUG: unlinking %s\n", pid_file_name);
            unlink(pid_file_name);
        }
        stop = 1;
        signal(SIGINT, SIG_DFL);
    } else if (sig == SIGHUP) {
        fprintf(stderr, "Debug: reloading daemon config file ...\n");
        read_conf_file(1);
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
                conf_file_name = strdup(optarg);
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
        signal(SIGHUP, handle_signal);
        
        while (stop == 0) {
            pid_t pid = fork();

            if (pid < 0) {
                logdoc(LOG_LVL_ERROR, LOGFILE, "Error forking: %s", strerror(errno));
                sleep(1);
                continue;
            }
            
            if (pid == 0) {
                char *cmd[] = { "mini-docker-init", mount_point, conf_file_name, NULL };
                if (execve("/usr/bin/mini-docker-init", cmd, NULL) == -1) {
                    logdoc(LOG_LVL_ERROR, LOGFILE, "Failed to execve: %s", strerror(errno));
                    exit(EXIT_FAILURE);
                }
            } else {
                wait(NULL);
                logdoc(LOG_LVL_INFO, LOGFILE, "Calling container_start()");
                container_start();
            }
        }
    } else {
        signal(SIGINT, SIG_DFL);
        signal(SIGHUP, SIG_DFL);
        
        pid_t pid = fork();

        if (pid < 0) {
            logdoc(LOG_LVL_ERROR, LOGFILE, "Error forking: %s", strerror(errno));
            return EXIT_FAILURE;
        }
        
        if (pid == 0) {
            char *cmd[] = { "mini-docker-init", mount_point, conf_file_name, NULL };
            if (execve("/usr/bin/mini-docker-init", cmd, NULL) == -1) {
                logdoc(LOG_LVL_ERROR, LOGFILE, "Failed to execve: %s", strerror(errno));
                exit(EXIT_FAILURE);
            }
        } else {
            wait(NULL);
            logdoc(LOG_LVL_INFO, LOGFILE, "Calling container_start()");
            int result = container_start();
            
            /* Free allocated memory */
            if (conf_file_name != NULL) free(conf_file_name);
            if (file_name != NULL) free(file_name);
            if (log_file_name != NULL) free(log_file_name);
            if (pid_file_name != NULL) free(pid_file_name);
            if (mount_point != NULL) free(mount_point);
            
            return result;
        }
    }
    
    /* Free allocated memory for daemon mode */
    if (conf_file_name != NULL) free(conf_file_name);
    if (file_name != NULL) free(file_name);
    if (log_file_name != NULL) free(log_file_name);
    if (pid_file_name != NULL) free(pid_file_name);
    if (mount_point != NULL) free(mount_point);

    return EXIT_SUCCESS;
}