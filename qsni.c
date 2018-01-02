#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/limits.h>
#define NET_CLS_DIR "/sys/fs/cgroup/net_cls/"
#define IPTABLES_PROFILES_DIR "/etc/qsni.d/"

//exits if we are already inside a profile.
void ensure_outside_profile()
{
	FILE *fp = fopen("/proc/self/cgroup", "r");
	if(fp == NULL)
	{
		fprintf(stderr, "Failed to open cgroup file\n");
		exit(EXIT_FAILURE);
	}
	
	char *line = NULL;
	size_t n = 0;
	while(getline(&line, &n, fp) != -1)
	{
		char *id = line;
		char *tmp = strchr(id, ':');
		if(tmp == NULL)
		{
			fprintf(stderr, "Misformated cgroups file\n");
			exit(EXIT_FAILURE);
		}
		*tmp = 0;
		++tmp;
		char *controllers = tmp;
		tmp = strchr(controllers, ':');
		if(tmp == NULL)
		{
			fprintf(stderr, "Misformated cgroups file\n");
			exit(EXIT_FAILURE);
		}
		*tmp = 0;
		++tmp;
		
		char *assigned = tmp;
		
		if(strstr(controllers, "net_cls") != NULL)
		{
			if(assigned[0] == '/' && assigned[1] != '\n')
			{
				fprintf(stderr, "already assigned to a net class, thus you can't use this binary to change that\n");
				exit(EXIT_FAILURE);
			}
		}
		line = NULL;
		n = 0;
	}
	
	fclose(fp);
	
}

void init_profile(const char *profilepath)
{
	pid_t pid = fork();
	if(pid == 0)
	{
		if(clearenv() != 0)
		{
			perror("clearenv");
			exit(EXIT_FAILURE);
		}
		
		int ret = execl(profilepath, profilepath, (char *) NULL);
		if(ret == -1)
		{
			perror("execl of child");
			exit(EXIT_FAILURE);
		}
	}
	else if(pid > 0)
	{
		int status=1;
		pid_t w = waitpid(pid, &status, 0);
		if(w == -1)
		{
			perror("waitpid");
			exit(EXIT_FAILURE);
		}
		if(! WIFEXITED(status) || WEXITSTATUS(status) != 0)
		{
			
			fprintf(stderr, "profile setup script failed\n");
			exit(EXIT_FAILURE);
		}
	}
	if(pid == -1)
	{
		perror("fork");
		exit(EXIT_FAILURE);
	}
	
}
void drop(uid_t u, gid_t g)
{
	if(setgid(g) != 0)
	{
		perror("setgid");
		exit(EXIT_FAILURE);
	}
	if(setuid(u) != 0)
	{
		perror("setuid");
		exit(EXIT_FAILURE);
	}
}

void assign_to_profile(const char *profilename)
{
	char taskspath[PATH_MAX +1];
	snprintf(taskspath, sizeof(taskspath), "%s/%s/tasks", NET_CLS_DIR, profilename);
	
	pid_t mypid = getpid();
	FILE *fp = fopen(taskspath, "a");
	if(fp == NULL)
	{
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	fprintf(fp, "%ld", (long)mypid);
	fclose(fp);
}
int main(int argc, char *argv[])
{
	if(argc < 3)
	{
		fprintf(stderr, "Usage: %s profile command [arguments...]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	ensure_outside_profile();
	
	char *profilename = argv[1];

	char profilepath[PATH_MAX +1];
	snprintf(profilepath, sizeof(profilepath), "%s/%s", IPTABLES_PROFILES_DIR, profilename);
	int ret = access(profilepath, R_OK);
	if(ret != 0)
	{
		perror("check for profile path failed\n");
		exit(EXIT_FAILURE);
	}
	
	uid_t myuid = getuid();
	gid_t myguid = getgid();
	if(setuid(0) != 0)
	{
		perror("setuid");
		exit(EXIT_FAILURE);
	}
	init_profile(profilepath);
	assign_to_profile(profilename);

	drop(myuid, myguid);
	
	
	argv += 2;
	int result = execvp(argv[0], argv);
	if(result == -1)
	{
		perror("execv");
		exit(EXIT_FAILURE);

	}
	return 0;
	
	
	
}
