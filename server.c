//compile and run in linux
//already tested on ubuntu desktop

//Collaborated by:	Hongfeng Wang	22289267	Haoran Zhang		22289211
#include "server.h"

int playResult[MAX_ROUND+1][MAX_PLAYER];
int dice[MAX_ROUND+1][2];
pid_t pid,readyTimer,rejectClient,playing[MAX_ROUND+1][MAX_PLAYER];
int status[MAX_ROUND+1][MAX_PLAYER];
int playerCount=0;
struct P player[MAX_PLAYER];

int main (int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr,"Usage: %s [port]\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);

    int server_fd, client_fd, err, opt_val;
    struct sockaddr_in server, client;
    char *buf;
    char symbol;
    buf = calloc(BUFFER_SIZE, sizeof(char));

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0){
        fprintf(stderr,"Could not create socket\n");
        exit(EXIT_FAILURE);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);
    
    err = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
    if(err<0) {
        fprintf(stderr,"Could not bind!\n");
        exit(EXIT_FAILURE);
    }
    err = listen(server_fd, 128);
    if (err < 0){
        fprintf(stderr,"Could not listen on socket\n");
        exit(EXIT_FAILURE);
    } 
    
    printf("Server is listening on %d\n", port);
    
    while(1) {
        //initialise game
        playerCount=0;
	fflush(stdin);
	
	printf("Want to start the game?(Y/N):");
	scanf(" %c",&symbol);		
	if(symbol!='y'&&symbol!='Y') break;
	
        //set socket to non-block
        int flags = fcntl(server_fd, F_GETFL, 0);
        fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
        
        printf("Accepting clients connection for 30 seconds!\n");
        readyTimer=fork();
	//child process as timer        
	if(readyTimer==0) {
            sleep(30);
            exit(1);
        }
        
        else {
            while(1) {
                socklen_t client_len = sizeof(client);
                client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);
                
                if(client_fd>0) {
                    flags = fcntl(client_fd, F_GETFL, 0);
                    fcntl(client_fd, F_SETFL, flags &~O_NONBLOCK);//set socket to non-block to receive INIT from clients
                    setup_player(playerCount,client_fd);
                }
                if(waitpid(readyTimer,NULL,WNOHANG)!=0) {
                    printf("START\n");
                    break;
                }
            }
        }
        //starting the game
        printf("There are %d players\n",playerCount);
        
        //less than 4 players, cancel the game
        if(playerCount<4) {
            for(int i=0;i<playerCount;i++) {
                sprintf(buf,"CANCEL");
                printf("send to %d: CANCEL\n",player[i].clientFD);
                send_message(player[i].clientFD,buf);
                bzero(buf,BUFFER_SIZE);
                close(player[i].clientFD);
            }
            continue;
        }
        
        //set to block
        flags = fcntl(server_fd, F_GETFL, 0);
        fcntl(server_fd, F_SETFL, flags &~O_NONBLOCK);
        
        printf("we can start the game\n");
        for(int i=0;i<playerCount;i++) {
            sprintf(buf,"START,%d,%d",playerCount,MAX_LIFE);
            printf("send to %d: START\n",player[i].clientFD);
            send_message(player[i].clientFD,buf);
            bzero(buf,BUFFER_SIZE);
        }
        
        //Start
        
        //roll dices
        rollDice();
        
	//set the socket receive timeout with 20 seconds, overtime would be treated as failure
        struct timeval timeout={20,0};
	for(int j=0;j<playerCount;j++) {
            setsockopt(player[j].clientFD,SOL_SOCKET,SO_RCVTIMEO,(struct timeval *)&timeout,sizeof(struct timeval));
	}
        
        //child process dealing with incoming client that to be rejected
        rejectClient=fork();
        if(rejectClient==0) {
            while(1) {
                socklen_t client_len = sizeof(client);
                client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);
                if(client_fd>0) {
                    recv(client_fd, buf, BUFFER_SIZE, 0);
                    bzero(buf,BUFFER_SIZE);
                    sprintf(buf,"REJECT");
                    send_message(client_fd,buf);
                    close(client_fd);
                }
            }
        }
        
        else {
            for(int i=1;i<=MAX_ROUND;i++) {
                printf("round %d\nDICE1\t%d\nDICE2\t%d\n",i,dice[i][0],dice[i][1]);
		
                for(int j=0;j<playerCount;j++) {
                    //do not deal with eliminated client(s)
                    if(player[j].inGame==false) continue;
                    //create child process to deal with the incoming message
                    playing[i][j]=fork();
                    if(playing[i][j]==0) {
                        bzero(buf,BUFFER_SIZE);
                        
                        int read=recv(player[j].clientFD,buf,BUFFER_SIZE, 0);
                        
                        if(read==0) exit(5);//off-line client
                        
                        if(read<0) {
                            if(errno==54) exit(5);//special case off-line
                            exit(4);//overtime
                        }
                        int result=parse_message(i,j,buf);
                        //1 for pass, 2 for fail, 3 for cheating, 0 for wrong input
                        exit(result);
                    }
                }
                //waiting all clients move
                for(int j=0;j<playerCount;j++) {
                    if(player[j].inGame==false) continue;
                    waitpid(playing[i][j],&status[i][j],0);
                }
                
		//dealing with identical messages arriving simultaneously
                bzero(buf,BUFFER_SIZE);
                int dumpMSG;
                for(int j=0;j<playerCount;j++) {
                    if(player[j].inGame==false) continue;
                    do {
                        dumpMSG=recv(player[j].clientFD,buf,BUFFER_SIZE, MSG_DONTWAIT);
                        if(dumpMSG==0) {//player exit when waiting for the result
                            printf("%d out\n",player[j].clientFD);
                            player[j].life[i]=0;
                            player[j].inGame=false;
                            close(player[j].clientFD);
                        }
                    }
                    while(dumpMSG>0);
                }
                
                //load the result to player struct
                for(int j=0;j<playerCount;j++) {
                     if(player[j].inGame==false) continue;
                    switch(WEXITSTATUS(status[i][j])) {
                        
                        case 1://success
                            player[j].life[i]=player[j].life[i-1];
                            break;
                        case 0://no recv
                            printf("client,%d:wrong message\n",player[j].clientFD);
                        case 4://overtime
                        case 2://fail
                            player[j].life[i]=player[j].life[i-1]-1;
                            break;
                        case 3://cheating
                            printf("%d cheating\n",player[j].clientFD);
                            player[j].life[i]=0;
                            teargame(j,false);
                            break;
                        case 5://off-line
                            printf("%d out\n",player[j].clientFD);
                            player[j].life[i]=0;
                            player[j].inGame=false;
                            close(player[j].clientFD);
                            break;
                        default:
                            break;
                    }
                }

		//sending message to clients based on the result
                int num=playerLeft(i);
                printf("alive:%d\n",num);//2
                
                for(int j=0;j<playerCount;j++) {
                    if(player[j].inGame==false) continue;
                    switch(num) {
                        case 0://no players left, players still in the game but live==0 will be the winner
                            teargame(j,true);
                            break;
                        case 1:// one player's life greater than '0', it is the only winner
                            if(player[j].life[i]>0) teargame(j,true);
                            else teargame(j,false);
                            break;
                        default://more than one player's life greater than '0'
                            if(i==MAX_ROUND) {//last round 
                                if(player[j].life[i]>0)
                                    teargame(j,true);
                                if(player[j].life[i]==0) {
                                    teargame(j,false);
                                }
                            }
                            else {
                                if(player[j].life[i]==0) {
                                    teargame(j,false);
                                }
                                else {
                                    bzero(buf,BUFFER_SIZE);
                                    if(WEXITSTATUS(status[i][j])==1) {//send PASS to winner
                                        sprintf(buf,"%d,PASS",player[j].playerId);
                                        send_message(player[j].clientFD,buf);
                                    }
                                    else if(WEXITSTATUS(status[i][j])%2==0) {//send FAIL to failer
                                        sprintf(buf,"%d,FAIL",player[j].playerId);
                                        send_message(player[j].clientFD,buf);
                                    }
                                }
                            }
                            break;
                    }
                }
                if(playerLeft(i)==0) break;//no player left,game over	
            }
        }
        printf("game over\n");
        kill(rejectClient,SIGKILL);//kill the child process that rejecting the incoming clients
    }
    free(buf);
    printf("Thanks for using! Exiting gracefully\n");
    close(server_fd);
}
