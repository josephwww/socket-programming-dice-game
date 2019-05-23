//compile and run in linux
//already tested on ubuntu desktop

//Collaborated by:	Hongfeng Wang	22289267	Haoran Zhang		22289211
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
#include <errno.h>

#define BUFFER_SIZE 1024
#define MAX_ROUND 20
#define MAX_PLAYER 20
#define MAX_LIFE 5

extern int playResult[MAX_ROUND+1][MAX_PLAYER];
extern int dice[MAX_ROUND+1][2];
extern pid_t pid,readyTimer,rejectClient,playing[MAX_ROUND+1][MAX_PLAYER];
extern int playerCount;
extern int status[MAX_ROUND+1][MAX_PLAYER];


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
        
        //get the option
        strtok(NULL,comma);//get the "MOV"
        char *option=strtok(NULL,comma);
        
        if(strstr(option,"EVEN")) {
	    if(strtok(NULL,comma))
		return 0;
            if(d1!=d2&&(d1+d2)%2==0)
                return 1;
            else
                return 2;
        }
        
        else if(strstr(option,"ODD")) {
            if(strtok(NULL,comma))
		return 0;
	    if(d1+d2>5&&(d1+d2)%2==1)
                return 1;
            else
                return 2;
        }
        
        else if(strstr(option,"DOUB")) {
	    if(strtok(NULL,comma))
		return 0;
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
    return send(client_fd, msg, strlen(msg),0);
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

/*
  void setup_player(int count, int client_fd);
  setup the player
*/
void setup_player(int count, int client_fd) {
    char *buf = calloc(BUFFER_SIZE, sizeof(char));
    int read = recv(client_fd, buf, BUFFER_SIZE, 0);    // Try to read from the incoming client
    if (read < 0){
        fprintf(stderr,"Client read failed\n");
        exit(EXIT_FAILURE);
    }
    if(strstr(buf,"INIT")) {
        player[count].clientFD=client_fd;
        player[count].playerId=100+client_fd;
        player[count].life[0]=MAX_LIFE;
        player[count].inGame=true;
        bzero(buf,BUFFER_SIZE);
        sprintf(buf,"WELCOME,%d",player[count].playerId);
        printf("SEND: %s\n",buf);
        send_message(client_fd,buf);
    }
    playerCount++;
}

//end the game and send coerresponding msg
void teargame(int index,bool result) {
    char *buf = calloc(BUFFER_SIZE, sizeof(char));
    bzero(buf,BUFFER_SIZE);
    
    if(result) {//send VIC
        sprintf(buf,"%d,VICT",player[index].playerId);
	printf("Winner:%d\n",player[index].playerId);
    }
    else {
        sprintf(buf,"%d,ELIM",player[index].playerId);
	printf("Eliminated:%d\n",player[index].playerId);
    }
    send_message(player[index].clientFD,buf);
    player[index].inGame=false;
    close(player[index].clientFD);
}

/*
  int playerLeft(int round);
  count the player who lives>0 AND still in game
*/
int playerLeft(int round) {
    int sum=0;
    for(int i=0;i<playerCount;i++) {
        if(player[i].inGame&&player[i].life[round]>0) sum++;
    }
    return sum;
}

