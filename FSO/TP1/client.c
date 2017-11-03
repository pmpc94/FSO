#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mysocks.h"

#define PRINT		"print"
#define STATUS		"status"
#define QUIT		"quit"

#define GET_STATUS	'S'
#define SEND_PRINT	'P'

#define MINARG		3
#define MAXARG		10
#define LINESZ		20
#define ARGVMAX		10
#define MAXB		64


int makeargv(char *line, char *argv[]) {

	int ntokens;

	if (line == NULL || argv == NULL)
		return -1;

	ntokens = 0;
	argv[ntokens] = strtok(line, " \t\n");

	while ((argv[ntokens] != NULL) && (ntokens < ARGVMAX)) {
		ntokens++;
		argv[ntokens] = strtok(NULL, " \t\n");
	}

	argv[ntokens] = NULL;
	return ntokens;
}


int main(int argc, char *argv[]) {

	if (argc < MINARG) {
		printf("Usage: %s <host> <port>\n", argv[0]);
		exit(1);
	}

	char input[LINESZ];					// Input do utilizador;
	char *cmd[MAXARG];					// Comandos inseridos pelo utilizador (processados pela funcao "makeargv");
	char request[MAXB];					// Pedido a enviar para o servidor;
	char reply[MAXB];					// Resposta do servidor perante o pedido enviado;
	char *host = argv[1];					// Host do servidor;
	int port = atoi(argv[2]);				// Porta em que o servidor se encontra;
	int ch_server;						// Canal de I/O para o servidor;

	printf("> ");
	fflush(stdout);

	while (fgets(input, LINESZ, stdin) != NULL) {

		if (makeargv(input, cmd) > 0) {
			
			if (strcmp(cmd[0], QUIT) == 0)
				exit(0);
			else if (strcmp(cmd[0], STATUS) == 0)
				request[0] = GET_STATUS;
			else if (strcmp(cmd[0], PRINT) == 0)
				sprintf(request, "%c %s", SEND_PRINT, cmd[1]);
			else
				sprintf(request, "%s", cmd[0]);

			if ((ch_server = myConnectSocket(host, port)) == -1) {
				perror("Unable to connect to server");
				exit(2);
			}

			if (myWriteSocket(ch_server, request, MAXB) == -1) {
				perror("Unable to send request to server");
			}

			int nrb = myReadSocket(ch_server, reply, MAXB);
			if (nrb == 0)
				perror("Server closed the connection");
			else if (nrb == -1)
				perror("Unable to get reply from server");

			printf("%s\n", reply);
			myCloseSocket(ch_server);
		}
		printf("> ");
  		fflush(stdout);
	}
	return 0;
}
