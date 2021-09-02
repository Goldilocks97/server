#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#ifndef INBUFSIZE
#define INBUFSIZE 1024
#endif

int value = 0;

typedef struct tag_sess_var {
	int s_fd;
	char buf[INBUFSIZE];
	int buf_used;
} sess_var;

typedef struct tag_server_var {
	int ls;
	int num_usrs;
	int cur_usrs;
	sess_var **sessions;

} server_var;

enum tag_act {
	increase,
	decrease,
	show,
	error
} act;

int is_number(const char *str)
{
	int i, num = 0;
	
	for (i = 0; str[i] != '\0'; i++) {
		if ((str[i] < '0') || (str[i] > '9'))
			return -1;
		num = (num * 10) + (str[i] - '0');
	}
	return num;
}

void sess_send_msg(const char* msg, int fd)
{
	write(fd, msg, strlen(msg));
}

int server_set(server_var *server, int port, int players)
{
	int sock, opt = 1, i;
	struct sockaddr_in addr;
	
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("socket");
		return -1;
	}
	
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
		perror("bind");
		return -1;
	}
	
	if (listen(sock, 5) != 0) {
		perror("listen");
		return -1;
	}
	server->ls = sock;
	server->num_usrs = players;
	server->cur_usrs = 0;
	server->sessions = malloc(sizeof(sess_var*) * players);
	for (i = 0; i < players; i++)
		(server->sessions)[i] = NULL;
	return 0;
}

int server_connect(server_var *server)
{
	int res, fd = server->cur_usrs;
	sess_var *tmp;

	res = accept(server->ls, NULL, NULL);
	if (res == -1) {
		perror("accept");
		return -1;
	}
	tmp = malloc(sizeof(sess_var));
	tmp->s_fd = res;
	tmp->buf_used = 0;
	(server->sessions)[fd] = tmp;
	server->cur_usrs++;
	return 0;
}

void session_close(int sock, server_var *server)
{
	int i, j = 0;
	sess_var  **new;
	
	shutdown(sock, 2);
	close(sock);
	for (i = 0; ((server->sessions)[i])->s_fd != sock; i++)
		;
	free((server->sessions)[i]);
	(server->sessions)[i] = NULL;
	server->cur_usrs--;
	server->num_usrs--;
	new = malloc(sizeof(sess_var*) * server->num_usrs);
	for (i = 0; i < server->num_usrs + 1; i++)
		if ((server->sessions)[i] != NULL) {
			new[j] = (server->sessions)[i];
			j++;
		}
	free(server->sessions);
	server->sessions = new;
}

void server_close(server_var *server)
{
	free(server->sessions);
	shutdown(server->ls, 2);
	close(server->ls);
}

void copy_str(char *str1, const char *str2, int n)
{
	int i;

	for (i = 0; i < n; i++)
		str1[i] = str2[i];
}

void server_send_msg(const char *msg, const server_var *server)
{
	int i, fd;

	for (i = 0; i < server->cur_usrs; i++) {
		fd  = ((server->sessions)[i])->s_fd;
		sess_send_msg(msg, fd);
	}
}

int find_ch(const char *buf, int size, char c)
{
	int i;

	for (i = 0; i <= size; i++)
		if (buf[i] == c)
			return i;
	return -1;
}

int get_size(int value)
{
	int size;

	for (size = 0; value != 0; size++)
		value = value / 10;
	return size;
}

char *make_massage(int value)
{
	char *massage;
	int size;
	
	size = get_size(value);
	massage = malloc(size + 20);
	sprintf(massage, "Current value is %d\n", value);
	return massage;
}

void exc_cmd(int command, const server_var *server)
{
	char *massage;

	switch (command) {
		case increase:
			value++;
			break;
		case decrease:
			value--;
			break;
		default:
			break;
	}
	massage = make_massage(value);
	server_send_msg(massage, server);
	free(massage);
}

int cmp_str(const char *str1, const char *str2)
{
	int i;
	
	for (i = 0; str1[i] != '\0'; i++)
		if (str1[i] != str2[i])
			return 0;
	return 1;
}

int def_cmd(char *line)
{
	int size;

	size = strlen(line);
	switch (line[0]) {
		case 'i':
			if (size == 3)
				if (cmp_str(line, "inc"))
					return increase;
			break;
		case 'd':
			if (size == 3)
				if (cmp_str(line, "dec"))
					return decrease;
			break;
		case 's':
			if (size == 4)
				if (cmp_str(line, "show"))
					return show;
	}
	return error;
}

void skip_space(char **str)
{
	int i = 0, pos = 0, flag = 0;

	while ((*str)[pos] == ' ')
		pos++;
	while ((*str)[pos] != '\0') {
		if ((*str)[pos] == ' ') {
			flag = 1;
			break;
		}
		(*str)[i] = (*str)[pos];
		pos++;
		i++;
	}
	if (flag) {
		while ((*str)[pos] != '\0') {
			if ((*str)[pos] != ' ') {
				(*str)[0] = '\0';
				return;
			}
			pos++;
		}
	}
	(*str)[i] = '\0';
}

void is_ready(sess_var *session, const server_var *server)
{
	int pos, res;
	char *line;
	
	if ((pos = find_ch(session->buf, session->buf_used, '\n')) == -1)
		return;
	line = malloc(pos + 1);
	copy_str(line, session->buf, pos);
	line[pos] = '\0';
	memmove(session->buf, session->buf+pos, pos+1);
	session->buf_used -= (pos + 1);
	if (line[pos-1] == '\r')
		line[pos-1] = '\0';
	skip_space(&line); //make sure!!!
	if ((res = def_cmd(line)) == error) {
		free(line);
		sess_send_msg("Sorry, you can't do it.\n", session->s_fd);
		return;
	}
	free(line);
	exc_cmd(res, server);
}

int sess_handler(sess_var *session, const server_var *server)
{
	int res, used = session->buf_used;

	res = read(session->s_fd, session->buf + used, INBUFSIZE - used);
	if (res <= 0)
		return 0;
	session->buf_used += res;
	is_ready(session, server);
	if(session->buf_used >= INBUFSIZE) {
		sess_send_msg("Line too long! Good bye...\n", session->s_fd);
		return 0;
	}
	return 1;
}

void no_place(int ls)
{
	int res;

	res = accept(ls, NULL, NULL);
	if (res == -1)
		return;
	sess_send_msg("Game has started. Try again late...\n", res);
	shutdown(res, 2);
	close(res);
}

int server_start(server_var *server)
{
	fd_set readfds;
	int fd, max_d, i;
	
	for (;;) {
		max_d = server->ls;
		FD_ZERO(&readfds);
		FD_SET(server->ls, &readfds);
		for (i = 0; i < server->cur_usrs; i++) {
			if ((server->sessions)[i]) {
				fd = ((server->sessions)[i])->s_fd;
				FD_SET(fd, &readfds);
				if (fd > max_d)
					max_d = fd;
			}
		}
		if (select(max_d+1, &readfds, NULL, NULL, NULL) == -1) {
			perror("select");
			return 5;
		}
		if (FD_ISSET(server->ls, &readfds)) {
			if ((server->cur_usrs) < (server->num_usrs)) {
				if (server_connect(server) == -1)
					return 6;
			} else 
				no_place(server->ls);
		}
		for (i = 0; i < server->num_usrs; i++) {
			if ((server->sessions)[i]) {
				fd = ((server->sessions)[i])->s_fd;
				if (FD_ISSET(fd, &readfds)) {
					if (sess_handler((server->sessions)[i], server) == 0)
						session_close(fd, server);
				}
			}
		}
		if (server->cur_usrs == 0) {
			server_close(server);
			return 0;
		}
	}
}

int main(int argc, const char * const *argv)
{
	int port, players;
	server_var server;

	if (argc != 3) {
		fprintf(stderr, "Note: server <number_of_players> <port>.\n");
		return 1;
	}
	if ((players = is_number(argv[1])) == -1) {
		fprintf(stderr, "Invalid number of players.\n");
		return 2;
	}
	if ((port = is_number(argv[2])) == -1) {
		fprintf(stderr, "Invalid port number.\n");
		return 3;
	}
	if ((server_set(&server, port, players) != 0))
		return 4;
	
	return server_start(&server);
}
