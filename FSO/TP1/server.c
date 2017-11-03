#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "mysocks.h"

#define STATUS		'S'
#define PRINT		'P'

#define MINARG		3
#define MAXB		64
#define BUFSZ		16
#define MAXQUEUE	10
#define MAXFILE		1024
#define TIMEGAP		50000



struct buf b;
pthread_mutex_t lock;
pthread_cond_t ready;
int printing;					// Informa se o servidor se encontra a imprimir qualquer ficheiro;
int queued;					// Quantidade de ficheiros na lista de espera;



struct buf {

	int buffer[BUFSZ];
	int writepos, readpos;
	pthread_mutex_t mutex;
	pthread_cond_t not_full;
	pthread_cond_t not_empty;
};

int init(struct buf *b) {

	b->writepos = 0; 
	b->readpos = 0;
	pthread_mutex_init(&b->mutex, NULL);
	pthread_cond_init(&b->not_full, NULL);
	pthread_cond_init(&b->not_empty, NULL);
	return 0;
}

void put(struct buf *b, int data) {
  
	pthread_mutex_lock(&b->mutex);
  	while ((b->writepos + 1) % BUFSZ == b->readpos)
   		pthread_cond_wait(&b->not_full, &b->mutex);

  	b->buffer[b->writepos] = data;
  	b->writepos++;
  	if (b->writepos >= BUFSZ) 
		b->writepos = 0;
  	
  	pthread_cond_signal(&b->not_empty);
  	pthread_mutex_unlock(&b->mutex);
}

int get(struct buf *b) {

	int data;

	pthread_mutex_lock(&b->mutex);
	while (b->writepos == b->readpos)
		pthread_cond_wait(&b->not_empty, &b->mutex);

	data = b->buffer[b->readpos];
	b->readpos++;
  	if (b->readpos >= BUFSZ) 
		b->readpos = 0;

  	pthread_cond_signal(&b->not_full);
  	pthread_mutex_unlock(&b->mutex);
  	return data;
}

void *print_file(void *chout) {
	
	char nrb;
	char print_buffer[MAXFILE];
	char *p = print_buffer;

	pthread_mutex_lock(&lock);
	while (printing)
		pthread_cond_wait(&ready, &lock);
	printing = 1;
	pthread_mutex_unlock(&lock);

	int chin = get(&b);

	do {

		nrb = read(chin, p, MAXFILE);
		while (*p != '\0') {
			write((int)(long) chout, p++, 1);
			usleep(TIMEGAP);
		}

	} while (nrb);

	close(chin);
	
	pthread_mutex_lock(&lock);
	printing = 0;
	queued--;
	pthread_mutex_unlock(&lock);

	pthread_cond_signal(&ready);
	return NULL;
}


int main(int argc, char *argv[]) {

	if (argc < MINARG) {
		printf("Usage: %s <pseudo terminal> <port>\n", argv[0]);
		exit(1);
	}

	int socket;					// Socket TCP do servidor;
	int port = atoi(argv[2]);			// Porta do servidor;
	int ch_client;					// Canal de I/O entre o servidor e o cliente;
	int ch_terminal;				// Canal de I/O entre o servidor e o terminal de escrita;
	int ch_file;					// Canal de I/O entre o servidor e o ficheiro especificado pelo cliente;
	char request[MAXB];				// Pedido recebido proveniente de um cliente;
	char reply[MAXB];				// Resposta do servidor perante o pedido;
	pthread_t printer;				// Thread para leitura e escrita de ficheiros;

	init(&b);
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&ready, NULL);
	printing = 0;
	queued = 0;

	if ((ch_terminal = open(argv[1], O_WRONLY)) < 0) {
		perror("Unable to open pseudo terminal");
		exit(2);
	}

	if ((socket = myServerSocket(port)) == -1) {
		perror("Unable to create TCP socket in the indicated port");
		exit(3);
	}

	while (1) {

		if ((ch_client = myAcceptServerSocket(socket)) == -1)
			perror("Unable to complete accept");

		if (myReadSocket(ch_client, request, MAXB) > 0) {

			if (request[0] == PRINT) {

				if ((ch_file = open(request+2, O_RDONLY)) == -1)
					sprintf(reply, "Unable to open the specified file.");
				else {
					put(&b, ch_file);
					queued++;
					sprintf(reply, "Print request has been queued;");
					pthread_create(&printer, NULL, print_file, (void *)(long) ch_terminal);
				}

			} else if (request[0] == STATUS)
				sprintf(reply, "Files waiting to be printed: %d;", queued);
			else
				sprintf(reply, "Unknown request;");

			myWriteSocket(ch_client, reply, MAXB);
		}
		myCloseSocket(ch_client);
	}
	return 0;
}
