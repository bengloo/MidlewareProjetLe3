#include <pthread.h>
#include "cltSrv.h"

statPartie_t listePartie[NBMAXCLIENT]; // Liste des parties en cours
unsigned nbPartie = 0;                 // nb de partie en cours

// mutex server enregistrement
sem_t mutex;
// mutex server partie
sem_t mutexpartie;

#ifdef CLIENT
// vis à vis du server
int sock = 0; // Numero Socket client du server  d'enregistrement

// vis à vis du client Maitre adverse
int sockPartie = 0;         // Numero Socket client du server  de partie
int socketEcoutePartie = 0; // Numero SocketPartie
int socketClientPartie[NBMAXCLIENT];
int nbClientPartie = 0;
int continuerPartie = 1; // inutiliser ?

char myPseudo[MAX_LEN];

#endif

#ifdef SERVER
int socketEcoute = 0; // Numero Socket serveur
int socketClient[NBMAXCLIENT];
int nbClient = 0;
int continuer = 1; // inutiliser ?
#endif

//#define CLIENT
#ifdef CLIENT
void afficherMenu()
{
    printf("Menu:\n \
		1) Se connecter au serveur\n\
		2) Lister les parties en cours\n\
		3) Creer une partie\n\
		4) Voir la partie d'un joueur\n\
		10) Lancer partie en solo\n\
		0) Quitter\n");
}

int main(/*int argc, char const *argv[]*/)
{
    int choix = 4;

    // On catch le SIGINT pour fermer la socket
    installDeroute(SIGINT, deroute);

    draw_ascii(empty_picture(' '));
    printf("veuillez saisir votre pseudo pour vous connecter:\n");
    scanf("%s", myPseudo);
    choix = 1;

    while (choix != 0)
    {
        afficherMenu();
        // draw_ascii(empty_picture(' '));
        scanf("%d", &choix);
        switch (choix)
        {
        case 1:
            connecterServeur();
            break;
        case 2:
            partieAdverse(sock, myPseudo);
            break;
        case 3:
            partieMaitre(sock, myPseudo);
            break;
        case 4:
            afficherStream(sock, myPseudo);
            break;
        case 10:
            partieSolo(sock, myPseudo);
            break;

        default:

            break;
        }
    }

    return 0;
}

void connecterServeur(void)
{
    req_t req;
    // envois creation party dgram
    DEBUG_S("Debut connecterServeur\n");
    if (sock != 0)
    {
        printf("Deja connecté au serveur\n");
        return;
    }

    sock = creerSocketClient(PORT_SERVER, ADDRSERVERENR);
    if (sock == 0)
        printf("Erreur connection serveur\n");
};

void connecterServeurPartie(adresse_t addrServerPartie)
{
    req_t req;
    // envois creation party dgram
    DEBUG_S("Debut connecterServeurPartie\n");
    if (sockPartie != 0)
    {
        printf("Deja connecté au serveurPartie\n");
        return;
    }
    DEBUG_S2("tentative de conexion à %d,%s",PORT_CLIENTMAITRE_PARTIE,addrServerPartie.ip);
    sockPartie = creerSocketClient(PORT_CLIENTMAITRE_PARTIE,addrServerPartie.ip);
    if (sockPartie == 0)
        printf("Erreur connection serveurPartie\n");
};

int serverPartie()
{

    // Creation d'une sockPartie en ecoute des autres clients
    socklen_t cltLen;
    struct sockaddr_in clt;

    pthread_t tid[NBMAXCLIENT];
    int idxThread[NBMAXCLIENT];
    double *status;

    // TODO adapté instalderoute pour passé en parametre la socket a fermé que ce soit inter client ou server
    /*
    // On catch le SIGINT pour fermer la socket
    installDeroute(SIGINT, deroute);
    */

    // On se met en ecoute sur le port  du Serveur de partie
    socketEcoutePartie = creerSocketEcoute(PORT_CLIENTMAITRE_PARTIE);
    DEBUG_S1("Serveur de partie socket <%d> en ecoute\n", socketEcoutePartie);

    // On prepar le mutex autorise (permet de refoulé les adverssaire voulant joindre une partie inexsitante ou dejas commencé)
    CHECK_T(sem_init(&mutexpartie, 0, 1) == 0, "erreur initialisation mutex");
    CHECK_T(sem_post(&mutexpartie) == 0, "erreur post mutex");

    while (1) // TODO tant que partie en cour ou attente
    {
        cltLen = sizeof(clt);
        CHECK(socketClientPartie[nbClientPartie] = accept(socketEcoutePartie, (struct sockaddr *)&clt, &cltLen), "Can't accept"); // accept de recevoir mess
        DEBUG_S1("Nouvelle connexion <%i>\n", socketClientPartie[nbClientPartie]);
        CHECK_T(pthread_create(&tid[nbClientPartie], NULL, (pf_t)lireReqClient, (void *)(&socketClientPartie[nbClientPartie])) == 0, "Erreur pthread_create()");

        nbClientPartie++;
    }
    return 0;
}

void partieMaitre(int masock, char *myPseudo)
{

    if (createPartyReq(masock, myPseudo))
    {
        if (serverPartie())
        {
            // TODO reinitialisé les varaible globale de partie avant de revenir au menu.
        }
        else
        {
            // TODO update le statut de la partie au server d'enregistrement en erreure
        };
    };

    // retour au menu
};
void partieAdverse(int masock, char *myPseudo)
{
    if (getPartiesReq(masock))
    {
        int choix = -2;
        while (choix != -1)
        {
            choix=-2;
            afficherPartie();
            printf("\n\n-1:Revenir au menu principal\n selectioné une partie avec son indices\n");
            scanf("%d", &choix);
            if (choix >= 0 && choix < nbPartie)
            {
                connecterServeurPartie(listePartie[choix].addrMaitre);
                printf("ADVERSE:on c'est conecter au server  de partie sockPartie<%d>\n",sockPartie);
                if(sockPartie!=0){
                    time_t top;
                    int obstRecus[NBMAXOBSTACLES+1];
                    if(joinPartieReq(sockPartie, myPseudo,obstRecus,&top)){
                        printf("debut init partie\n");
                        int mon_score=0; 
                        int son_score=0; 
                        char **pic = empty_picture(' ');
                        partie(obstRecus,&mon_score,&son_score,pic,top);
                    }else{
                        printf("imposible de joindre cette partie\n");
                    }
                }
                getchar();
                //TODO lencé partie
            }
        }
    };

    // retour menu
};

#endif
// define SERVER
#ifdef SERVER

int main(/*int argc, char const *argv[]*/)
{
    socklen_t cltLen;
    struct sockaddr_in clt;

    pthread_t tid[NBMAXCLIENT];
    int idxThread[NBMAXCLIENT];
    double *status;

    // On catch le SIGINT pour fermer la socket
    installDeroute(SIGINT, deroute);

    // On se met en ecoute sur le port Serveur
    socketEcoute = creerSocketEcoute(PORT_SERVER);
    DEBUG_S1("Serveur socket <%d> en ecoute\n", socketEcoute);

    // Initialisatio des parties
    initstatPartie();

    // On prepar le mutex
    CHECK_T(sem_init(&mutex, 0, 1) == 0, "erreur initialisation mutex");
    CHECK_T(sem_post(&mutex) == 0, "erreur post mutex");

    while (1)
    {
        cltLen = sizeof(clt);
        CHECK(socketClient[nbClient] = accept(socketEcoute, (struct sockaddr *)&clt, &cltLen), "Can't accept"); // accept de recevoir mess
        DEBUG_S1("Nouvelle connexion <%i>\n", socketClient[nbClient]);
        CHECK_T(pthread_create(&tid[nbClient], NULL, (pf_t)lireReqServ,
                               (void *)(&socketClient[nbClient])) == 0,
                "Erreur pthread_create()");

        nbClient++;
    }
}
#endif

void installDeroute(int numSig, void (*pfct)(int))
{
    struct sigaction newAction;
    // renseigner l'action pour ignorer le crtl C
    newAction.sa_handler = pfct;
    CHECK_T(sigemptyset(&newAction.sa_mask) == 0, "--Erreur sigemptyset--");
    newAction.sa_flags = SA_RESTART; // permet de redemarrer les appel bloquants
                                     // on installe le gestionnaire de SIGINT
    CHECK_T(sigaction(numSig, &newAction, NULL) == 0, "--sigaction--");
}

void deroute(int numSig)
{
    switch (numSig)
    {
    case SIGINT:
        DEBUG_S1("\t[PID=%d]^C reçu, on ferme\n", getpid());
        terminerProcess();
        break;

    default:
        printf("Incredible\n");
    }
}

void terminerProcess(void)
{
#ifdef CLIENT
    DEBUG_S1("Client fermeture socket <%d>\n", sock);
    fermerSocket(sock); // Numero Socket serveur
    exit(0);
#endif

#ifdef SERVER
    DEBUG_S1("Serveur fermeture socket <%d>\n", socketEcoute);
    fermerSocket(socketEcoute); // Numero Socket serveur
    CHECK_T(sem_destroy(&mutex) == 0, "erreur destroy mutex");
    // On force la fermeture des socket
    DEBUG_S("Serveur fermeture des sockets clientes\n");
    for (int i = 0; i < nbClient; i++)
    {
        fermerSocket(socketClient[i]);
        DEBUG_S1("Serveur fermeture socket cliente <%d>\n", socketClient[i]);
    }
    exit(0);
#endif
}
