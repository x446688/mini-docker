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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include "mini-container/create.h"

static int running = 0;
static int delay = 1;
static int counter = 0;
static char *conf_file_name = NULL;
static char *pid_file_name = NULL;
static char *mount_point = NULL;
static int pid_fd = -1;
static char *app_name = NULL;

static FILE *log_stream;

enum {
  // ARGTABLE_ARG_MAX is the maximum number of arguments
  ARGTABLE_ARG_MAX = 20
};

/* global arg_xxx structs */
struct arg_lit *help, *version;
struct arg_int *uid;
struct arg_str *mnt;
struct arg_str *cmd;
struct arg_str *arg;
struct arg_lit *vrb;
struct arg_end *end;

int container_start(char *mount) {
  // used for container stack
  char *stack = 0;
  stack = malloc(CONTAINER_STACK_SIZE);
  // used for container config
  container_cfg config = {0};

  config.hostname = "mini";
  config.uid = 0;
  config.mnt = mount;
  config.cmd = "/usr/bin/busybox";
  config.arg = "sh";
  
  printf("%s\n", mount);
  // used for container pid
  int container_pid = 0;
  // socket pair
  int sockets[2] = {0};

  int exitcode = 0;
  char progname[] = "mini-docker";

  // Check if running as root
  if (geteuid() != 0) {
    printf("mini-docker should be run as root\n");
  }

  config.hostname = "mini";

  // Initialize a socket pair to communicate with the container
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
  if ((container_pid = container_init(&config,stack+CONTAINER_STACK_SIZE)) ==
      -1) {
    printf("failed to container_init\n");
    exitcode = 1;
    return exitcode;
  }
  printf("container pid: %d\n",container_pid);
  // Prepare cgroups for the process 
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
  // Clear resources (cgroups, stack, sockets)

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
				conf_file_name,
				app_name);
		} else {
			syslog(LOG_INFO, "Configuration of %s read from file %s",
				app_name,
				conf_file_name);
		}
	}

	fclose(conf_file);

	return ret;
}


int test_conf_file(char *_conf_file_name)
{
	FILE *conf_file = NULL;
	int ret = -1;

	conf_file = fopen(_conf_file_name, "r");

	if (conf_file == NULL) {
		fprintf(stderr, "Can't read config file %s\n",
			_conf_file_name);
		return EXIT_FAILURE;
	}

	ret = fscanf(conf_file, "%d", &delay);

	if (ret <= 0) {
		fprintf(stderr, "Wrong config file %s\n",
			_conf_file_name);
	}

	fclose(conf_file);

	if (ret > 0)
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

void handle_signal(int sig)
{
	if (sig == SIGINT) {
		fprintf(log_stream, "Debug: stopping daemon ...\n");
		/* Unlock and close lockfile */
		if (pid_fd != -1) {
			lockf(pid_fd, F_ULOCK, 0);
			close(pid_fd);
		}
		/* Try to delete lockfile */
		if (pid_file_name != NULL) {
			unlink(pid_file_name);
		}
		running = 0;
		/* Reset signal handling to default behavior */
		signal(SIGINT, SIG_DFL);
	} else if (sig == SIGHUP) {
		fprintf(log_stream, "Debug: reloading daemon config file ...\n");
		read_conf_file(1);
	} else if (sig == SIGCHLD) {
		fprintf(log_stream, "Debug: received SIGCHLD signal\n");
	}
}

static void daemonize()
{
	pid_t pid = 0;
	int fd;

	/* Fork off the parent process */
	pid = fork();

	/* An error occurred */
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}

	/* Success: Let the parent terminate */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* On success: The child process becomes session leader */
	if (setsid() < 0) {
		exit(EXIT_FAILURE);
	}

	/* Ignore signal sent from child to parent process */
	signal(SIGCHLD, SIG_IGN);

	/* Fork off for the second time*/
	pid = fork();

	/* An error occurred */
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}

	/* Success: Let the parent terminate */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* Set new file permissions */
	umask(0);

	/* Change the working directory to the root directory */
	/* or another appropriated directory */
	chdir("/");

	/* Close all open file descriptors */
	for (fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--) {
		close(fd);
	}

	/* Reopen stdin (fd = 0), stdout (fd = 1), stderr (fd = 2) */
	stdin = fopen("/dev/null", "r");
	stdout = fopen("/dev/null", "w+");
	stderr = fopen("/dev/null", "w+");

	/* Try to write PID of daemon to lockfile */
	if (pid_file_name != NULL)
	{
        printf("%s\n", pid_file_name);
		char str[256];
		pid_fd = open(pid_file_name, O_RDWR|O_CREAT, 0640);
		if (pid_fd < 0) {
			/* Can't open lockfile */
			exit(EXIT_FAILURE);
		}
		if (lockf(pid_fd, F_TLOCK, 0) < 0) {
			/* Can't lock file */
			exit(EXIT_FAILURE);
		}
		/* Get current PID */
		sprintf(str, "%d\n", getpid());
		/* Write PID to lockfile */
		write(pid_fd, str, strlen(str));
	}
}

void print_help(void)
{
	printf("\n Usage: %s [OPTIONS]\n\n", app_name);
	printf("  Options:\n");
	printf("   -h --help                 Print this help\n");
	printf("   -m --mount                Mountpoint\n");
	printf("   -c --conf_file filename   Read configuration from the file\n");
	printf("   -t --test_conf filename   Test configuration file\n");
	printf("   -l --log_file  filename   Write logs to the file\n");
	printf("   -d --daemon               Daemonize this application\n");
	printf("   -p --pid_file  filename   PID file used by daemonized app\n");
	printf("\n");
}

/* Main function */
int main(int argc, char *argv[])
{

	static struct option long_options[] = {
		{"conf_file", required_argument, 0, 'c'},
		{"mount", required_argument, 0, 'm'},
		{"test_conf", required_argument, 0, 't'},
		{"log_file", required_argument, 0, 'l'},
		{"help", no_argument, 0, 'h'},
		{"daemon", no_argument, 0, 'd'},
		{"pid_file", required_argument, 0, 'p'},
		{NULL, 0, 0, 0}
	};
	int value, option_index = 0, ret;
	char *log_file_name = NULL;
	int start_daemonized = 0;

	app_name = argv[0];

	/* Try to process all command line arguments */
	while ((value = getopt_long(argc, argv, "c:m:l:t:p:dh", long_options, &option_index)) != -1) {
		switch (value) {
			case 'c':
				conf_file_name = strdup(optarg);
				break;
			case 'm':
				// pid_t pid = 0;

				// pid = fork();

				// if (pid < 0) {
				// 	printf("%s\n", strerror(errno));;
				// }
				// else if (pid == 0){
				mount_point = strdup(optarg);
				// 	system("ls");
				// 	char *cmd[] = { "init.sh", mount_point, NULL };
				// 	if (execve("mini-container/init.sh", cmd, NULL))
				// 		printf("%s\n", strerror(errno));
				// 	printf("HELLO");
				// }
				// else {
				// 	mount_point = strdup(optarg);
				break;
				// }
			case 'l':
				log_file_name = strdup(optarg);
				break;
			case 'p':
				pid_file_name = strdup(optarg);
				break;
			case 't':
				return test_conf_file(optarg);
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

	// /* When daemonizing is requested at command line. */
	// if (start_daemonized == 1) {
	// 	/* It is also possible to use glibc function deamon()
	// 	 * at this point, but it is useful to customize your daemon. */
	// 	daemonize();
	// }
	// /* Daemon will handle two signals */
	// signal(SIGINT, handle_signal);
	// signal(SIGHUP, handle_signal);

	pid_t pid = 0;

	pid = fork();

	if (pid < 0) {
		printf("%s\n", strerror(errno));
	}
	else if (pid == 0){

		wait(NULL);

		system("ls");
		char *cmd[] = { "init.sh", mount_point, NULL };
		if (execve("mini-container/init.sh", cmd, NULL))
			printf("%s\n", strerror(errno));
		printf("HELLO");
	}
	else {

		wait(NULL);

	}

	container_start(mount_point);
	/* Try to open log file to this daemon */
	// if (log_file_name != NULL) {
	// 	log_stream = fopen(log_file_name, "a+");
	// 	if (log_stream == NULL) {
	// 		syslog(LOG_ERR, "Can not open log file: %s, error: %s",
	// 			log_file_name, strerror(errno));
	// 		log_stream = stdout;
	// 	}
	// } else {
	// 	log_stream = stdout;
	// }

	// /* This global variable can be changed in function handling signal */
	// running = 1;

	// /* Never ending loop of server */
	// while (running == 1) {
	// 	/* Debug print */
	// 	ret = fprintf(log_stream, "Debug: %d\n", counter++);
	// 	if (ret < 0) {
	// 		syslog(LOG_ERR, "Can not write to log stream: %s, error: %s",
	// 			(log_stream == stdout) ? "stdout" : log_file_name, strerror(errno));
	// 		break;
	// 	}
	// 	ret = fflush(log_stream);
	// 	if (ret != 0) {
	// 		syslog(LOG_ERR, "Can not fflush() log stream: %s, error: %s",
	// 			(log_stream == stdout) ? "stdout" : log_file_name, strerror(errno));
	// 		break;
	// 	}

	// 	/* TODO: dome something useful here */

	// 	/* Real server should use select() or poll() for waiting at
	// 	 * asynchronous event. Note: sleep() is interrupted, when
	// 	 * signal is received. */
	// 	sleep(delay);
	// }

	/* Free allocated memory */
	if (conf_file_name != NULL) free(conf_file_name);
	if (log_file_name != NULL) free(log_file_name);
	if (pid_file_name != NULL) free(pid_file_name);
	if (mount_point != NULL) free(mount_point);

	return EXIT_SUCCESS;
}