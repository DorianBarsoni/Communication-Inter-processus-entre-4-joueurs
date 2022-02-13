#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#define PORT 8080

//Variables de communication
int sock;
struct sockaddr_in serv_adr;
char buf[300];
bool jouer = true;
bool fin_tour = true;
int tour = 1;
int min = 0;

void erreur(char* err);
void clearbuffer(char* buf);
void triTableau(int* tab, int longueur);
void sigMajSrv(int code)
{
	//printf("Reception signal...\n");
	clearbuffer(buf);
	int n = read(sock, buf, sizeof(buf));
	if(n < 0) erreur("Erreur : read() réception signal");
	printf("%s\n", buf);
	if(strcmp(buf, "fin") == 0)
	{
		exit(EXIT_FAILURE);
	}
	
	else if(strcmp(buf, "Fin du tour") == 0)
	{
		fin_tour = false;
	}
	
	//Envoie signal recu
	clearbuffer(buf);
	strcat(buf, "Recu");
	n = write(sock, buf, strlen(buf));
	if(n < 0) erreur("Erreur : write() réception signal");
	//printf("Envoie réponse %s\n", buf);
}


int main(int argc, char const *argv[])
{
	system("clear");
	
	//Signals
	signal(SIGUSR1, &sigMajSrv);
	
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) erreur("Erreur : socket()");
	
	//Initialisation de l'adresse du serveur
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_port = htons(PORT);
	
	if(inet_pton(AF_INET, "127.0.0.1", &serv_adr.sin_addr)<=0) erreur("Erreur : inet_pton()");
	
	//Connexion
	if (connect(sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) < 0) erreur("Erreur : connect()");
	
	//Récupération du pid du srv
	//printf("Recuperaation pid\n");
	char pid[10];
	int n = read(sock, pid, sizeof(pid));
	if(n < 0) erreur("Erreur : read");
	//printf("Client Recu : %s\n", pid);
	
	//On envoie notre pid
	//printf("Envoie pid\n");
	clearbuffer(buf);
	sprintf(buf, "%d", getpid());
	n = write(sock, buf, strlen(buf));
	if(n < 0) erreur("Erreur : write() pid");
	//printf("Pid envoye : %s\n", buf);
	
	//Attente d'autorisation d'envoie de réponse
	//printf("Attente autorisation envoie reponse\n");
	clearbuffer(buf);
	n = read(sock, buf, sizeof(buf));
	if(n < 0) erreur("Erreur : read() attente d'autorisation de reponse");
	
	//Réponse lancement de la partie ou non
	//printf("Envoie lancement partie ?\n");
	clearbuffer(buf);
	printf("Voulez vous lancer la partie ? (y/n)\n");
	scanf("%s", buf);
	n = write(sock, buf, strlen(buf));
	if(n < 0) erreur("Erreur client : write()");
	printf("En attente du lancement de la partie...\n\n");
	
	/*----------------*/
	/*LANCEMENT DU JEU*/
	/*----------------*/
	
	int num[3];
	while(jouer && tour<4)
	{
		for(int i=0; i<tour; i++)
		{
			//récupération du numéro
			//printf("Recuperation num\n");
			clearbuffer(buf);
			n = read(sock, buf, sizeof(buf));
			if(n < 0) erreur("Erreur : read() recup num\n");
			//printf("Lecture de %s\n", buf);
			num[i] = atoi(buf);
			//confirmation réception numéro
			n = write(sock, buf, strlen(buf));
			if(n < 0) erreur("Erreur : write() confirmation reception num\n");
			//printf("%d\n", num);
		}
		triTableau(num, tour);
		
		//On vide le buffer avant le scanf
		int c = 0;
		if(tour == 1)
		{
			while ((c = getchar()) != '\n' && c != EOF);
		}
		printf("Tour : %d\n", tour);
		
		for(int i=0; i<tour; i++)
		{
			//Détection d'envoie et envoie
			char carac;
			printf("Appuyez sur ENTRER pour envoyer votre plus petit nombre\n");
			printf("Mes numéros : ");
			for(int j=min; j<tour; j++) printf("%d ", num[j]);
			printf("\n");
			scanf("%c", &carac);
			clearbuffer(buf);
			sprintf(buf, "%d", num[min]);
			n = write(sock, buf, strlen(buf));
			if(n < 0) erreur("Erreur : write() envoie num");
			//printf("Envoie de %d\n", num[min]);
			min++;
		}
		
		printf("Vous n'avez plus de numéros, attente du tour suivant...\n");
		while(fin_tour)
		{
			//printf("Waiting maj\n");
		}
		
		printf("\n");
		min = 0;
		tour++;
		fin_tour = true;
	}
	
	printf("Terminé ! La partie est gagnée !\n");
	
	return EXIT_SUCCESS;
}

void erreur(char* err)
{
	perror(err);
	exit(EXIT_FAILURE);
}

void clearbuffer(char* buf)
{
	for(int i=0; i<300; i++) buf[i] = 0;
}

void triTableau(int* tab, int longueur)
{
	int verif = 0;
	
	while(verif != (longueur - 1))
	{
		verif = 0;
		for(int i=0; i<(longueur - 1); i++)
		{
			if(tab[i] <= tab[i+1]) verif++;
			else
			{
				int a = tab[i];
				tab[i] = tab[i+1];
				tab[i+1] = a;
			}
		}
	}
}
