/* A program to simulate the "dmesg" utility.
 *
 * Author: Murali Tejeshwar Janaswami
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/klog.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#define HANDLE_ERROR(msg) {	\
	perror(msg);		\
	exit(EXIT_FAILURE);	\
}

typedef void (*fptr)(const char *argv[]);

static __always_inline void without_prefixes(char *buf)
{
	char *data_start, *data_end;

	while (data_start = strchr(buf, '>')) {
		data_end = strchr(buf, '\n');
		printf("%.*s", (int)(++data_end - ++data_start), data_start);
		buf = data_end;
	}
}

static void read_all(const bool raw)
{
	void *ptr;
	char *buf;
	int buf_size, bytes_read;

	/* retrieve the total size of the kernel log buffer */
	buf_size = klogctl(10, NULL, 0);
	if (buf_size < 0)
		HANDLE_ERROR("klogctl");

	buf = ptr = malloc(buf_size);
	if (!ptr)
		HANDLE_ERROR("malloc");

	/* read the contents of the kernel log buffer */
	bytes_read = klogctl(3, buf, buf_size);
	if (bytes_read < 0) {
		free(ptr);
		HANDLE_ERROR("klogctl");
	}

	if (!bytes_read) {
		fprintf(stderr, "The kernel log buffer is currently empty\n");
		goto free_buf;
	}

	/* display the contents of the kernel log buffer */
	raw ? printf("%s", buf) : without_prefixes(buf);

free_buf:
	free(ptr);
}

static void clear(const char *argv[])
{
	if (klogctl(5, NULL, 0))
		HANDLE_ERROR("klogctl");
}

static void read_all_and_clear(const char *argv[])
{
	read_all(false);

	clear(NULL);
}

static void console_off(const char *argv[])
{
	if (klogctl(6, NULL, 0))
		HANDLE_ERROR("klogctl");
}

static void console_on(const char *argv[])
{
	if (klogctl(7, NULL, 0))
		HANDLE_ERROR("klogctl");
}

static void read_all_raw(const char *argv[])
{
	read_all(true);
}

static void read_from_file(const char *argv[])
{
	int fd;
	ssize_t bytes;
	unsigned char buf[4096];

	fd = open(argv[2], O_RDONLY);
	if (fd < 0)
		HANDLE_ERROR(argv[2]);

	while ((bytes = read(fd, buf, sizeof(buf))) > 0)
		if (write(STDOUT_FILENO, buf, bytes) < 0)
			goto err;

	if (bytes < 0)
err:
		HANDLE_ERROR(NULL);

	close(fd);
}

static void set_console_lvl(const char *argv[])
{
	int level = atoi(argv[2]);

	if (level < 1 || level > 8) {
		printf("Invalid level specified. Level must be in the range: [1, 8]\n");
		exit(EXIT_FAILURE);
	}

	if (klogctl(8, NULL, level))
		HANDLE_ERROR("klogctl");
}

static void handle(const char *argv[])
{
	int i;
	const char *options[] = {"-C", "-c", "-D", "-E", "-r", "-F", "-n"};
	const fptr fp[] = {clear, read_all_and_clear, console_off, console_on, read_all_raw, read_from_file, set_console_lvl};

	for (i = 0; i < (sizeof(options) / sizeof(options[0])); i++) {
		if (!strcmp(options[i], argv[1])) {
			fp[i](argv);
			return;
		}
	}

	printf("Invalid option specified\n");
}

int main(int argc, char *argv[])
{
	/* sanity check */
	if (argc > 3) {
		printf("USAGE: %s <option> <argument to the option>\n", program_invocation_name);
		exit(0);
	}

	argv[1] ? handle((const char **)argv) : read_all(false);

	return 0;
}
