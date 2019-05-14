#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>

#define BUFFER_SIZE 1024
#define MAX_ROUND 10
#define MAX_PLAYER 50
#define MAX_LIFE 5

extern int playResult[MAX_ROUND+1][MAX_PLAYER];
extern int dice[MAX_ROUND+1][2];
//extern int playerFD[MAX_PLAYER];
//extern bool gameStatus;
//extern int currentRound;//X
extern pid_t pid,readyTimer,rejectClient,playing[MAX_ROUND+1][MAX_PLAYER];
extern int playerCount;
extern int status[MAX_ROUND+1][MAX_PLAYER];

////function declare
//void parse_message(char *msg);
//int send_message(int client_fd, char* msg);
//void rollDice();
//int findPlayer(int client_fd);
//void play_game_round(int client_fd,bool result);
//void setup_player(int count, int client_fd);

struct P
{
    int clientFD;
    int playerId;
    int life[MAX_ROUND+1];
    bool inGame;
};
extern struct P player[MAX_PLAYER];


/*
 void parse_message(char *msg):
 Dealing with the moving message from the client
 Input: msg: the msg reading from the client
 */
int parse_message(int round,int index, char *msg) {
    if(strstr(msg,"MOV")) {
        printf("Receive:");puts(msg);
        char * comma=",";
        int clientId=atoi(strtok(msg,comma));
        
        if(clientId!=player[index].clientFD+100) return 3;//cheating
        
        int d1 = dice[round][0], d2 = dice[round][1];
        
        printf("dices:%d\t%d\n",d1,d2);
        
        //get the option
        strtok(NULL,comma);//get the "MOV"
        char *option=strtok(NULL,comma);
        
        if(strstr(option,"EVEN")) {
            if(d1!=d2&&(d1+d2)%2==0)
                return 1;
            else
                return 2;
        }
        
        else if(strstr(option,"ODD")) {
            if(d1+d2>5&&(d1+d2)%2==1)
                return 1;
            else
                return 2;
        }
        
        else if(strstr(option,"DOUB")) {
            if(d1==d2)
                return 1;
            else
                return 2;
        }
    
        else if(strstr(option,"CON")) {
            int num=atoi(strtok(NULL,comma));
            if(d1==num||d2==num)
                return 1;
            else
                return 2;
        }
    }
    return 0;
}

/*
 int send_message(int client_fd, char* msg):
 Simply send the message to the client
 Input: Client_fd, Msg: message to send
 Return: 1 for Successful sending, 0 otherwise
 */
int send_message(int client_fd, char* msg) {
    return send(client_fd, msg, strlen(msg), 0);
}



/*
 void rollDice():
 Rolling the dices for all rounds
 */
void rollDice() {
    srand((unsigned) time(NULL));//time as random variable
    
    //filling the result array
    for (int i = 0; i <= MAX_ROUND; i++)
        for(int j = 0;j < 2; j++)
            dice[i][j]=rand() % 6 + 1;
}
///*
// int findPlayer(int client_fd):
// Looking for the index in clientFD array
// */
//int findPlayer(int client_fd) {
//    for(int i=0;i<MAX_PLAYER;i++) {
//        if(playerFD[i]==client_fd)
//            return i;
//    }
//    return -1;
//}
//
///*
// void play_game_round(int client_fd,bool result):
// Dealing with win or lose message sending
// Input: int client_fd, boolean result (win:true/lose:false)
// Return:    void
// */
//void play_game_round(int client_fd,bool result) {
//    int index=findPlayer(client_fd);
//    char * res = calloc(BUFFER_SIZE, sizeof(char));
//    if(result==true) {
//        playResult[currentRound][index]=playResult[currentRound-1][index];
//        sprintf(res,"%d,PASS",client_fd*100);
//        send_message(client_fd,res);
//        *res='\0';
//        printf("%d win\n",client_fd*100);
//    }
//    else {
//        playResult[currentRound][index]=playResult[currentRound-1][index]-1;
//        sprintf(res,"%d,FAIL",client_fd*100);
//        send_message(client_fd,res);
//        *res='\0';
//        printf("%d lost\n",client_fd*100);
//    }
//    currentRound++;
//}

/*
 void setup_player(int count, int client_fd):
    Initialise the global variables for the player
 Input:
 */
void setup_player(int count, int client_fd) {
    
    
    
    char *buf = calloc(BUFFER_SIZE, sizeof(char));
    int read = recv(client_fd, buf, BUFFER_SIZE, 0);    // Try to read from the incoming client
    if (read < 0){
        fprintf(stderr,"Client read failed\n");
        //close(server_fd);
        exit(EXIT_FAILURE);
    }
    if(strstr(buf,"INIT")) {
        //memset(buf, 0, sizeof(buf));
        //buf[0]='\0';
        player[count].clientFD=client_fd;
        player[count].playerId=100+client_fd;
        player[count].life[0]=MAX_LIFE;
        player[count].inGame=true;
        bzero(buf,BUFFER_SIZE);
        sprintf(buf,"WELCOME,%d",player[count].playerId);
        printf("SEND: %s",buf);
        send_message(client_fd,buf);

    }
    playerCount++;
}

void teargame(int index,bool result) {
    char *buf = calloc(BUFFER_SIZE, sizeof(char));
    bzero(buf,BUFFER_SIZE);
    
    if(result) {//send VIC
        sprintf(buf,"%d,VICT",player[index].playerId);
    }
    else {
        sprintf(buf,"%d,ELIM",player[index].playerId);
    }
    send_message(player[index].clientFD,buf);
    player[index].inGame=false;
    close(player[index].clientFD);
}

int playerLeft(int round) {
    int sum=0;
    for(int i=0;player[i].inGame&&i<playerCount;i++) {
        if(player[i].inGame&&player[i].life[round]>0) sum++;
    }
    return sum;
}

