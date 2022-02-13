#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>

#define MAXJOUEURS 4
#define PORT 8080

void erreur(char* err);
void clearbuffer(char* buf);
int* tirage(int* tableau, int tour);
int selection(int* Nb, int size, int tour);
void convertListToString(int nb, char* buf);
void triTableau(int* tab, int longueur);
void socketNonBloquant(int* sock);
void socketBloquant(int* sock);

int main(int argc, char const *argv[])
{	
	system("clear");
	
	//Variables utilisées pour communiquer
	int srvsocket, newsocket, clilen;
	struct sockaddr_in serv_adr, cli_adr;
	char buf[300];
	
	//Variables pour connexion avec les clients
	int sock[4], thrlen[4];
	struct sockaddr_in thr_adr[4];
	char pid[4][10];
	int i = 0;
	int nbjoueurs = 0;
	
	//Création du socket
	srvsocket = socket(AF_INET, SOCK_STREAM, 0);
	if(srvsocket < 0) erreur("Erreur : socket()");
	//Ligne pour pouvoir réutiliser le même port rapidement
	if (setsockopt(srvsocket, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) erreur("setsockopt(SO_REUSEADDR) failed");
	
	//Initialisation de l'adresse
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = INADDR_ANY;
	serv_adr.sin_port = htons(PORT);
	
	//On raccroche le socket au serveur
	if(bind(srvsocket, (struct sockaddr *) &serv_adr, sizeof(serv_adr)) <0) erreur("Erreur : bind()");
	
	//On ouvre l'écoute du socket
	if (listen(srvsocket, 1) < 0) erreur("Erreur : listen()");
    
    printf("Séquence de connexion :\n");
    while(nbjoueurs != MAXJOUEURS)
    {
		//On établie la connexion
		thrlen[i] = sizeof(thr_adr[i]);
		sock[i] = accept(srvsocket, (struct sockaddr *) &thr_adr[i], &thrlen[i]);
		if(sock[i] < 0) erreur("Erreur : accept()");
		
		//On lui envoie notre pid
		//printf("Envoie pid\n");
		clearbuffer(buf);
		sprintf(buf, "%d", getpid());
		int n = write(sock[i], buf, strlen(buf));
		if(n < 0) erreur("Erreur : write() pid");
		
		//On reçoit son pid
		//printf("Reception pid\n");
		n = read(sock[i], pid[i], sizeof(pid[i]));
		if(n < 0) erreur("Erreur : read() pid");
		//printf("Pid recu : %s\n", pid[i]);
		
		//write pour autoriser le client a envoyer la réponse suivante
		//printf("Autorisation envoie réponse\n");
		clearbuffer(buf);
		sprintf(buf, "%s", "autorisation");
		n = write(sock[i], buf, strlen(buf));
		if(n < 0) erreur("Erreur : write() autorisation réponse");

		//On incrémente le nombre de joueurs et on l'affiche
		nbjoueurs++;
		printf("%d/%d\n", nbjoueurs, MAXJOUEURS);
		
		//Attente de la réponse du client (lancer la partie ou non)
		//printf("Lancement partie ?\n");
		clearbuffer(buf);
		n = read(sock[i], buf, sizeof(buf));
		if(n < 0) erreur("Erreur : read() lancement partie");
		i++;
		
		//Si on a atteint le nombre max de joueurs
		if(nbjoueurs == MAXJOUEURS)
		{	
			/*----------------*/
			/*LANCEMENT DU JEU*/
			/*----------------*/
			
			srand((unsigned)time(NULL));
			
			int tour = 0, num_rcv = 0, num;
			int tableau[100] = {0};
			bool jouer = true;
			//printf("Lancement du jeu\n");
			
			printf("Lancement du jeu : \n\n");
			while(jouer && tour < 4)
			{
				//On incrémente tour
				tour++;
				
				//On fait le tirage des nombres
				tirage(tableau, tour);
				
				//On les distribue aux clients
				for(int ite=0; ite<4; ite++)
				{
					for(int ite2=(tour*ite); ite2<((tour*ite)+tour); ite2++)
					{
						clearbuffer(buf);
						convertListToString(tableau[ite2], buf);
						n = write(sock[ite], buf, strlen(buf));
						if(n < 0) erreur("Erreur : write() numeros");
						n = read(sock[ite], buf, sizeof(buf));
						if(n < 0) erreur("Erreur : read() numero recu");
						
					}
				}
				
				//On passe les reads en non bloquant
				socketNonBloquant(sock);
			
				printf("Tour %d :\n", tour);
				
				//On trie le tableau par ordre croissant
				triTableau(tableau, tour*4);
				printf("Les nombres : ");
				for(int i=0; i<4*tour; i++) printf("%d ", tableau[i]);
				printf("\n");
				
				//On passe les reads en non bloquant
				socketNonBloquant(sock);
				
				//On attend qu'un client envoie son numéro
				num_rcv = 0;
				
				while(num_rcv !=(4*tour) && jouer)
				{
					for(int i=0; i<4; i++)
					{
						//Dès qu'on reçoit un numéro
						clearbuffer(buf);
						n = read(sock[i], buf, sizeof(buf));
						switch(n)
						{
							case(-1) :
								break;
							case(0) :
								break;
							default :
								printf("Recu %s\n", buf);
								//On regarde si il correspond au plus petit des nombres tirés
								num = atoi(buf);
								//Si oui
								if(num == tableau[num_rcv])
								{
									printf("%d est le plus petit\n", num);
									//On incrément num_rcv
									num_rcv++;
									//On envoie la mise à jour aux joueurs
									strcat(buf, " joué\n");
									//On repasse les sockets en bloquant
									socketBloquant(sock);
									for(int k=0; k<4; k++)
									{
										kill(atoi(pid[k]), SIGUSR1);
										n = write(sock[k], buf, strlen(buf));
										if(n < 0) erreur("Erreur : write() Signal nombre plus petit");
									}
									for(int k=0; k<4; k++)
									{
										n = read(sock[k], buf, strlen(buf));
										//printf("Réponse : %s\n", buf);
										if(n < 0) erreur("Erreur : read() Signal nombre plus petit");
									}
								}
								//Sinon
								else
								{
									printf("%d n'est le plus petit. Partie perdue...\n", num);
									//On passe jouer à false
									jouer = false;
									//On envoie la mise à jour aux joueurs
									strcat(buf, " Partie perdue...\n");
									//On repasse les sockets en bloquant
									socketBloquant(sock);
									for(int k=0; k<4; k++)
									{
										//printf("%d kill\n", k);
										kill(atoi(pid[k]), SIGUSR1);
										n = write(sock[k], buf, strlen(buf));
										if(n < 0) erreur("Erreur : write() Signal partie perdue");
									}
									for(int k=0; k<4; k++)
									{
										n = read(sock[k], buf, strlen(buf));
										//printf("Réponse : %s\n", buf);
										if(n < 0) erreur("Erreur : read() Signal nombre plus petit");
									}
									
									//On indique que le jeu est terminé
									clearbuffer(buf);
									strcat(buf, "fin");
									for(int k=0; k<4; k++)
									{
										//printf("%d kill\n", k);
										kill(atoi(pid[k]), SIGUSR1);
										n = write(sock[k], buf, strlen(buf));
										if(n < 0) erreur("Erreur : write() Signal partie perdue");
									}
									for(int k=0; k<4; k++)
									{
										n = read(sock[k], buf, strlen(buf));
										//printf("Réponse : %s\n", buf);
										if(n < 0) erreur("Erreur : read() Signal nombre plus petit");
									}
								}
								socketNonBloquant(sock);
								break;
						}
					}
				}
				
				clearbuffer(buf);
				strcat(buf, "Fin du tour");
				//On envoie la mise à jour aux joueurs
				for(int k=0; k<4; k++)
				{
					//printf("%d kill2\n", k);
					kill(atoi(pid[k]), SIGUSR1);
					//printf("%d Longueur envoyée : %d\n", k, strlen(buf));
					n = write(sock[k], buf, strlen(buf));
					if(n < 0) erreur("Erreur : write() tour suivant");
				}
				socketBloquant(sock);
				for(int k=0; k<4; k++)
				{
					n = read(sock[k], buf, strlen(buf));
					//printf("Réponse : %s\n", buf);
					if(n < 0) erreur("Erreur : read() Signal nombre plus petit");
				}
			}
		}
		//Sinon
		else
		{
			//Si on a reçu y on lance un bot
			if(strcmp(buf, "y") == 0)
			{
				//printf("Lancement bot\n");
				if(fork() == 0)
				{
					execl("./bot", NULL);
				}
			}
		}
	}
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

int* tirage(int* tableau, int tour)
{
	int size = 100;
    int Nb[size];

    for(int i=0; i<size; i++)
    {
        Nb[i] = i+1;
    }
    
    for(int i=0; i<4*tour; i++)
    {
		tableau[i] = selection(Nb, size, tour);
		size--;
	}
    
    return tableau;
}

int selection(int* Nb, int size, int tour)
{
	int indice = rand()%size;
    int res = Nb[indice];

    for(int i=indice; i<size-1; i++)
    {
        Nb[i] = Nb[i+1];
    }

    return res;
}

void convertListToString(int nb, char* buf)
{
	char s[10];
	sprintf(s, "%d", nb);
	strcat(buf, s);
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

void socketNonBloquant(int* sock)
{
	for(int i=0; i<4; i++)
	{
		int flags = fcntl(sock[i], F_GETFL, 0);
		fcntl(sock[i], F_SETFL, flags | O_NONBLOCK);
	}
}

void socketBloquant(int* sock)
{
	for(int i=0; i<4; i++)
	{
		int flags = fcntl(sock[i], F_GETFL, 0);
		flags = flags & ~O_NONBLOCK;
		fcntl(sock[i], F_SETFL, flags);
	}
}

