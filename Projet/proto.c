/* ------------------------------------------------------------------------ */
/**
 *  \file       proto.c
 *  \brief      Programme réalisant un executable serveur d'enregistrement et un executable client
 *              permettant de gerer des parties temple_run multijoueurs 
 *				table de villes chargée à partir d'un fichier.
 *
 *  \author     Morcq Alexandre B2
 *
 *  \date       08/02/2022
 *
 *	\version    1.0
 * *
 *	\remark		fichier contenant nos fonctions, ...
 *
 */
#include <netdb.h>
#include "proto.h"
#include "cltSrv.h"
#include "basic_func.h"
#include "data.h"
#include "graphisme.h"

/* ------------------------------------------------------------------------ */
/*      FONCTION SERVEUR    & CLIENT                                                  */
/* ------------------------------------------------------------------------ */

void afficherPartie(unsigned nbPart)
{
    int i = 0;
    statPartie_t *partie;

    printf("Liste parties disponibles:\n");
#ifdef SERVER
    CHECK_T(sem_wait(&mutex) == 0, "erreur attente mutex");
#endif
    for (i = 0; i < nbPart; i++)
    {
        partie = &listePartie[i];

        printf("%3d:%s:%15s:%5d:%20s:%15s:%5d:%20s:%5d:%5d\n",
               partie->id,
               statutPartieTxt[partie->statut],
               partie->addrMaitre.ip,
               partie->addrMaitre.port,
               partie->addrMaitre.pseudo,
               partie->addrAdverse.ip,
               partie->addrAdverse.port,
               partie->addrAdverse.pseudo,
               partie->scoreMaitre,
               partie->scoreAdverse);
    }
#ifdef SERVER
    CHECK_T(sem_post(&mutex) == 0, "erreur post mutex");
#endif
}

/* ------------------------------------------------------------------------ */
/*      FONCTION SERVEUR                                                     */
/* ------------------------------------------------------------------------ */
#ifdef SERVER

///-à chaque req ,on associera &fct de traitement qui genere une réponse
//    void newpartieServ(short lg,buffer_t buff,struct sockaddr_in *clt,int sock){
//-fct generation des requétes
void createPartieRep(int sock, statPartie_t *partie)
{
    rep_t rep;
    rep.idRep = CREERPARTIE;
    partieTOstr(rep.msgRep, partie);
    rep.lgrep = strlen(rep.msgRep);

    // Envoie de la requete au serveur
    char repTxt[sizeof(rep_t)];
    repTOstr(&rep, repTxt);
    ecrireMsgTCP(sock, repTxt);
};
int newpartieServ(int sock, req_t *req, int *idPart)
{
    // Peut on encore accueillir des clients
    if (nbPartie == NBMAXCLIENT)
    {
        // On fait une partie vide en failed
        statPartie_t partie;
        partie.id = 0;
        partie.statut = FAILED;
        partie.addrMaitre.ip[0] = '\0';
        partie.addrMaitre.port = -1;
        partie.addrMaitre.pseudo[0] = '\0';
        partie.addrAdverse.ip[0] = '\0';
        partie.addrAdverse.port = -1;
        partie.addrAdverse.pseudo[0] = '\0';
        partie.scoreMaitre = 0;
        partie.scoreAdverse = 0;
        createPartieRep(sock, &partie);
        return NOT_OK;
    }

    adresse_t clientMaitre;

    DEBUG_S("Nouvelle partie avant strTOadresse");
    strTOadresse(&clientMaitre, req->msgReq);

    DEBUG_S("Nouvelle partie avant mutex");
    CHECK_T(sem_wait(&mutex) == 0, "erreur attente mutex");

    statPartie_t *nouvellePartie = &listePartie[nbPartie];
    nouvellePartie->id = nbPartie;
    *idPart = nbPartie;
    nouvellePartie->statut = WAITTINGADVERSE;
    strcpy(nouvellePartie->addrMaitre.ip, clientMaitre.ip);
    nouvellePartie->addrMaitre.port = clientMaitre.port;
    strcpy(nouvellePartie->addrMaitre.pseudo, clientMaitre.pseudo);
    nouvellePartie->addrAdverse.ip[0] = '\0';
    nouvellePartie->addrAdverse.port = 0;
    nouvellePartie->addrAdverse.pseudo[0] = '\0';
    nouvellePartie->scoreMaitre = 0;
    nouvellePartie->scoreAdverse = 0;

    DEBUG_S2("Nouvelle partie pour clientMaitre ip <%s> pseudo <%s>\n", nouvellePartie->addrMaitre.ip, nouvellePartie->addrMaitre.pseudo);
    nbPartie++;
    CHECK_T(sem_post(&mutex) == 0, "erreur post mutex");

    createPartieRep(sock, nouvellePartie);
    return OK;
}

void getParties(int sock)
{
    DEBUG_S("Debut getParties\n");
    rep_t rep;
    rep.idRep = LISTERPARTIE;

    CHECK_T(sem_wait(&mutex) == 0, "erreur attente mutex");
    listePartieTOStr(listePartie, nbPartie, rep.msgRep);
    CHECK_T(sem_post(&mutex) == 0, "erreur post mutex");
    rep.lgrep = strlen(rep.msgRep);
    DEBUG_S1("getParties msgRep=<%s>\n", rep.msgRep);

    // Envoie de la requete au serveur
    char repTxt[sizeof(rep_t)];
    repTOstr(&rep, repTxt);

    ecrireMsgTCP(sock, repTxt);
};

//1 fct de selection traitement selon requete
//void lireReqServ(req_t req, struct sockaddr_in *clt, int sock)
void lireReqServ(int *sock)
{
    DEBUG_S1("Serveur : New thread pour socket <%i>\n", *sock);

    buffer_t msgLu;
    req_t req;
    int lenLu = 1;
    unsigned idPartieEnCours = 0;
    unsigned partieEnCours = NOT_OK;

    // On attend les inputs du Client Maitre

    while (lenLu > 0)
    {
        afficherPartie(nbPartie);
        lenLu = lireMsgTCP(*sock, msgLu, sizeof(buffer_t));
        DEBUG_S1("Serveur : message reçu len <%d>\n", lenLu);
        if (lenLu > 0)
        {
            strTOreq(&req, msgLu);
            DEBUG_S3("Serveur : socket <%i> msg recu <%s> avec idReq <%d>\n", *sock, msgLu, req.idReq);

            switch (req.idReq)
            {
            case CREERPARTIE:
                DEBUG_S("Case CreerPartie");
                partieEnCours = newpartieServ(*sock, &req, &idPartieEnCours);
                break;
            case LISTERPARTIE:
                DEBUG_S("Case GetPartie");
                getParties(*sock);
                break;
            case STATUTPARTIE:
                DEBUG_S("Case STATUTPARTIE");
                updateStatutPartie(req.msgReq);
                //StrTOlistePartie(listePartie,req.msgReq);
                break;
            default:
                DEBUG_S("Case Default");
                break;
            }
        }
    }

    DEBUG_S1("Serveur : le client avec la socket <%d> a quitté la connexion\n", *sock);
    if (partieEnCours == OK)
        listePartie[idPartieEnCours].statut = CLOSED;

    afficherPartie(nbPartie);

    // Le Client a fermé on cloture la socket client
    fermerSocket(*sock);
}

void updateStatutPartie(char *txt)
{
    statPartie_t statePartie;
    printf("update statpartie :%s\n", txt);
    strTOpartie(&statePartie, txt);
    for (int i = 0; i < nbPartie; i++)
    {
        if (listePartie[i].id == statePartie.id)
        {
            printf("Update Partie id:%d\nstatut:%d\nport:%d\nip:%s\npseudo:%s\nport:%d\nip:%s\npseudo:%s\nscore1:%d\nscore2:%d\n",
                   statePartie.id,
                   statePartie.statut,
                   statePartie.addrMaitre.port,
                   statePartie.addrMaitre.ip,
                   statePartie.addrMaitre.pseudo,
                   statePartie.addrAdverse.port,
                   statePartie.addrAdverse.ip,
                   statePartie.addrAdverse.pseudo,
                   statePartie.scoreMaitre,
                   statePartie.scoreAdverse);
            CHECK_T(sem_wait(&mutex) == 0, "erreur attente mutex");
            if (statePartie.statut != -1)
                listePartie[statePartie.id].statut = statePartie.statut;

            if (statePartie.addrMaitre.port != -1)
                listePartie[statePartie.id].addrMaitre.port = statePartie.addrMaitre.port;
            if (strcmp(statePartie.addrMaitre.ip, "SAME"))
                strcpy(listePartie[statePartie.id].addrMaitre.ip, statePartie.addrMaitre.ip);
            if (strcmp(statePartie.addrMaitre.pseudo, "SAME"))
                strcpy(listePartie[statePartie.id].addrMaitre.pseudo, statePartie.addrMaitre.pseudo);

            if (statePartie.addrAdverse.port != -1)
                listePartie[statePartie.id].addrAdverse.port = statePartie.addrAdverse.port;
            if (strcmp(statePartie.addrAdverse.ip, "SAME"))
                strcpy(listePartie[statePartie.id].addrAdverse.ip, statePartie.addrAdverse.ip);
            if (strcmp(statePartie.addrAdverse.pseudo, "SAME"))
                strcpy(listePartie[statePartie.id].addrAdverse.pseudo, statePartie.addrAdverse.pseudo);

            if (statePartie.scoreMaitre != -1)
                listePartie[statePartie.id].scoreMaitre = statePartie.scoreMaitre;
            if (statePartie.scoreAdverse != -1)
                listePartie[statePartie.id].scoreAdverse = statePartie.scoreAdverse;
            CHECK_T(sem_post(&mutex) == 0, "erreur poste mutex");
        }
    }
}
// initialisation de la structure statPatie_t
void initstatPartie(void)
{
    nbPartie = 0;
}

#endif
/* ------------------------------------------------------------------------ */
/*      FONCTION CLIENT                                                     */
/* ------------------------------------------------------------------------ */
#ifdef CLIENT
//-fct generation des requétes
int createPartyReq(int sock, char *pseudo)
{
    // Verification on est connecte
    if (sock == 0)
    {
        printf("Connectez vous avant svp\n");
        return 0;
    }
    // On recupere notre adresse IP
    char IPbuffer[MAX_LEN];
    getMyIp(IPbuffer);
    DEBUG_S2("Creation partie avec IP <%s> et port <%d>\n", IPbuffer, portClientMaitre);

    // On prepare la requete pour le serveur
    req_t req;
    req.idReq = CREERPARTIE;
    adresse_t monAdr;
    strcpy(monAdr.ip, IPbuffer);
    strcpy(monAdr.pseudo, pseudo);
    monAdr.port = portClientMaitre;
    adresseTOstr(&monAdr, req.msgReq);
    req.lgreq = strlen(req.msgReq);

    // Envoie de la requete au serveur
    char reqTxt[sizeof(req_t)];
    reqTOstr(&req, reqTxt);
    ecrireMsgTCP(sock, reqTxt);

    // Attente de la reponse du serveur
    buffer_t msgLu;
    int lenLu = 1;
    rep_t rep;
    statPartie_t partie;

    lenLu = lireMsgTCP(sock, msgLu, sizeof(buffer_t));
    DEBUG_S2("Client : message reçu len <%d> <%s>\n", lenLu, msgLu);
    if (lenLu <= 0)
    {
        printf("Erreur de message\n");
        return NOT_OK;
    }
    strTOrep(&rep, msgLu);

    if (rep.idRep != CREERPARTIE)
    {
        printf("Mauvaise reponse reçu : impossible\n");
        return NOT_OK;
    }
    strTOpartie(&partie, rep.msgRep);
    DEBUG_S4("Client : socket <%i> msg recu <%s> idPartie <%d> statut <%d>\n", sock, msgLu, partie.id, partie.statut);
    if (partie.statut == FAILED)
    {
        printf("Echec de creation de partir sur le serveur\n");
        return NOT_OK;
    }
    printf("Creation de partie sur le serveur réussie\n");
    idPartie = partie.id;
    return OK;
}

int getPartiesReq(int sock)
{
    DEBUG_S("getPartiesReq debut");
    // Verification on est connecte
    if (sock == 0)
    {
        printf("Connectez vous avant svp\n");
        return 0;
    }

    // On prepare la requete pour le serveur
    req_t req;
    req.idReq = LISTERPARTIE;
    req.lgreq = 0;

    // Envoie de la requete au serveur
    char reqTxt[sizeof(req_t)];
    reqTOstr(&req, reqTxt);

    ecrireMsgTCP(sock, reqTxt);

    // Attente de la reponse du serveur
    buffer_t msgLu;
    int lenLu = 1;
    rep_t rep;

    lenLu = lireMsgTCP(sock, msgLu, sizeof(buffer_t));
    DEBUG_S2("Client : message reçu len <%d> <%s>\n", lenLu, msgLu);
    if (lenLu <= 0)
    {
        printf("Erreur de message\n");
        return 0;
    }
    strTOrep(&rep, msgLu);
    if (rep.idRep != LISTERPARTIE)
    {
        printf("Mauvaise reponse reçu : impossible\n");
        return 0;
    }
    return StrTOlistePartie(listePartie, rep.msgRep);
};

// Fonction joinPartieReq utilisée par l'adversaire
int joinPartieReq(int masock, char *pseudo, partieGraphique_t *partie, time_t *top)
{

    // On recupere notre adresse IP
    char IPbuffer[MAX_LEN];
    getMyIp(IPbuffer);

    // On prepare la requete pour le client Maitre
    req_t req;
    req.idReq = JOIN;
    adresse_t monAdr;
    strcpy(monAdr.ip, IPbuffer);
    strcpy(monAdr.pseudo, pseudo);
    monAdr.port = 0; // le client distant n'a pas de port d'écoute
    adresseTOstr(&monAdr, req.msgReq);
    req.lgreq = strlen(req.msgReq);

    DEBUG_S1("Demande de JOIN prete <%s>\n", req.msgReq);

    // Envoie de la requete JOIN au client maitre
    char reqTxt[sizeof(req_t)];
    reqTOstr(&req, reqTxt);
    ecrireMsgTCP(masock, reqTxt);

    // Le CLient Maitre repond avec les DATA de la partie
    char repTxt[sizeof(rep_t)];
    int lenLu = lireMsgTCP(masock, repTxt, MAX_LEN);
    if (lenLu <= 0)
    {
        printf("Erreur de message\n");
        return NOT_OK;
    }
    printf("DATA init partie recues:<%d> <%s>\n", lenLu, repTxt);
    rep_t rep;
    strTOrep(&rep, repTxt);
    if (rep.idRep = STARTPARTIE)
    {
        StringinitTOParti(top, partie, rep.msgRep);
        /* 		
		char tmp[MAX_LEN];
		initPartiTOString(tmp, *top, partie);
		
		DEBUG_S1("RECU : <%s>\n", rep.msgRep);
		DEBUG_S1("GENE : <%s>\n", tmp);
        return 0;
		
 */
    }
    else
    {
        printf("Reponse du client maitre incoherente\n");
        return 0;
    }

    // On envoie le Ok à l'adversaire
    rep.idRep = STATUT;
    statutReq_t statut;
    statut.statut = OK;
    statutReqTOstr(&statut, rep.msgRep);
    rep.lgrep = strlen(rep.msgRep);
    repTOstr(&rep, repTxt);
    ecrireMsgTCP(masock, repTxt);

    // Pret a jouer
    DEBUG_S("Pret pour lancer la partie");

    char **pic = empty_picture(' ');
    //updateStatutPlayerReq(masock,&mon_score,&son_score);
    jouerPartie(partie, &mon_score, &son_score, pic, *top, masock);
    //partie(obstRecus, &mon_score, &son_score, pic, top);

    return 1;
};

// Utilise par client maitre
void initPartie(int masock, adresse_t *adversaire)
{

    //init variable globale servant au req //TODO les rendre global pour qui soivent accesible à la rep de streaming et la rep de statpartie de l'adversaire
    DEBUG_S("debut initPartie\n");
    char **pic = empty_picture(' ');
    //generation des obsacle et top depart
    //srand((unsigned int)time);

    partieGraphique_t partie;
    initPartieGraphisme(&partie);
    time_t now = time(NULL);
    DEBUG_S("Partie initialisee avec succes\n");

    //caste data
    joinPartieRep(masock, &partie, now + DELAY_START);
    printf("Pret pour lancer la partie");
    jouerPartie(&partie, &mon_score, &son_score, pic, now + DELAY_START, masock);
};

// Fonction joinPartieRep utilisée par le client maitre
void joinPartieRep(int masock, partieGraphique_t *partie, time_t temps)
{
    DEBUG_S("debut joinPartieRep\n");
    rep_t rep;
    rep.idRep = STARTPARTIE;
    DEBUG_S("initPartiTOString: on transforme partie en texte\n");
    initPartiTOString(rep.msgRep, temps, partie);
    DEBUG_S1("initPartiTOString: resultat <%s>\n", rep.msgRep);
    rep.lgrep = strlen(rep.msgRep);

    // Envoie de la partie au client adverse
    char repTxt[sizeof(rep_t)];
    repTOstr(&rep, repTxt);
    printf("Envoi de la partie à l'adversaire:%s\n", repTxt);
    ecrireMsgTCP(masock, repTxt);

    // Verification que le client adverse a bien reçu
    statutReq_t statut;
    char msgLu[MAX_LEN];
    DEBUG_S("Client : Attente du OK d l'adversaire\n");
    int lenLu = lireMsgTCP(masock, msgLu, sizeof(buffer_t));
    DEBUG_S2("Client : message reçu len <%d> <%s>\n", lenLu, msgLu);
    if (lenLu <= 0)
    {
        printf("Erreur de message\n");
        return;
    }
    strTOrep(&rep, msgLu);
    if (rep.idRep != STATUT)
    {
        printf("Mauvaise reponse reçu : impossible\n");
        return;
    }
    strTOstatutReq(&statut, rep.msgRep);
    DEBUG_S4("Client : socket <%i> msg recu <%s> avec idReq <%d> et statut <%d>\n", sock, msgLu, rep.idRep, statut.statut);
    if (statut.statut == OK)
        printf("Le client adverse a bien reçu la partie\n");
    else
        printf("Echec envoi partie à client adverse\n");
};

// permet de jouer seul
void partieSolo(int sock, char *myPseudo)
{
    //init variable globale servant au req
    int mon_score = 0;
    int son_score = 0;
    char **pic = empty_picture(' ');
    //generation des obsacle et top depart
    //srand(time);
    //int *obstaclesInitiaux = init_obstacles(NBMAXOBSTACLES);
    partieGraphique_t partieGraphique;
    initPartieGraphisme(&partieGraphique);
    time_t now = time(NULL);
    //on lence la partie
    system("./scriptZoom.sh -m");
    //draw_ascii_score(empty_picture('?'),mon_score,son_score);
    jouerPartie(&partieGraphique, &mon_score, &son_score, pic, now + DELAY_START, 0);
    system("./scriptZoom.sh -p");
    //printf("mon score:%d son score:%d\n", mon_score,son_score);
};

void updateScoreReq(int sock, int score)
{
    // On prepare la requete pour le serveur
    req_t req;
    req.idReq = UPDATESCORE;
    scoreTOstr(req.msgReq, score);
    req.lgreq = strlen(req.msgReq);

    // Envoie de la requete au serveur
    char reqTxt[sizeof(req_t)];
    reqTOstr(&req, reqTxt);
    ecrireMsgTCP(sock, reqTxt);
};

int readScoreReq(int sock)
{
    int lenLu = 1;
    rep_t rep;
    int score = 0;
    buffer_t msgLu;

    lenLu = lireMsgTCP(sock, msgLu, sizeof(buffer_t));
    if (lenLu <= 0)
    {
        printf("Erreur de message\n");
        return -1;
    }
    strTOrep(&rep, msgLu);

    if (rep.idRep != UPDATESCORE)
    {
        printf("Erreur de message\n");
        return -1;
    }

    // On prepare la requete pour le serveur
    strTOscore(rep.msgRep, &score);
    return score;
};

void updateStatutPartieReq(int masock, statPartie_t *statutpartie)
{
    DEBUG_S("Debut getParties\n");
    req_t req;
    req.idReq = STATUTPARTIE;
    partieTOstr(req.msgReq, statutpartie);
    req.lgreq = strlen(req.msgReq);
    DEBUG_S1("updateStatutPartieReq msgReq=<%s>\n", req.msgReq);

    // Envoie de la requete au serveur
    char reqTxt[sizeof(req_t)];
    reqTOstr(&req, reqTxt);

    ecrireMsgTCP(sock, reqTxt);
}
void stream(){};                                 // publier ma partie
void afficherStream(int sock, char *myPseudo){}; // voir une partie

#endif
