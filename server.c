#include "server.h"


int playResult[MAX_ROUND+1][MAX_PLAYER];
int dice[MAX_ROUND+1][2];
//int playerFD[MAX_PLAYER];
//bool gameStatus=false;
//int currentRound=1;
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

//    struct timeval
    

    err = listen(server_fd, 128);
    if (err < 0){
        fprintf(stderr,"Could not listen on socket\n");
        exit(EXIT_FAILURE);
    } 

    printf("Server is listening on %d\n", port);
    
    //set socket to non-block
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
    
    printf("Accepting clients connection for 30 seconds!\n");
    readyTimer=fork();
    if(readyTimer==0) {
        sleep(30);
        exit(1);
    }
    
    else {
        while(1) {
            
            socklen_t client_len = sizeof(client);
            client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);
            
            if(client_fd>0) {
                flags = fcntl(server_fd, F_GETFL, 0);
                fcntl(server_fd, F_SETFL, flags &~O_NONBLOCK);
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
            bzero(buf,BUFFER_SIZE);
            sprintf(buf,"CANCEL");
            printf("send to %d: CANCEL\n",i+100);
            send_message(player[i].clientFD,buf);
            close(player[i].clientFD);
        }
        close(server_fd);
    }
    
    //set to block
    flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags &~O_NONBLOCK);
    
    printf("we can start the game\n");
    for(int i=0;i<playerCount;i++) {
        bzero(buf,BUFFER_SIZE);
        sprintf(buf,"START,%d,%d",playerCount,MAX_LIFE);
        printf("send to %d: START\n",i+100);
        send_message(player[i].clientFD,buf);
        close(player[i].clientFD);
    }
    
    //started
    
    //roll dices
    rollDice();
    
    //set the recv timoeout for the socket
    struct timeval timeout = {30,0};
    setsockopt(server_fd,SOL_SOCKET,SO_RCVTIMEO,(char *)&timeout,sizeof(struct timeval));
    
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
            for(int j=0;player[j].inGame&&j<playerCount;j++) {
                
                playing[i][j]=fork();
                if(playing[i][j]==0) {
                    bzero(buf,BUFFER_SIZE);
                    if(recv(player[j].clientFD,buf,BUFFER_SIZE, 0)<0) {//overtime
                        exit(4);
                    }
                    int result=parse_message(i,j,buf);
                        exit(result);//1 for pass, 2 for fail, 3 for cheating
                }
            }
            //waiting each client moving
            for(int j=0;player[j].inGame&&j<playerCount;j++) {
                waitpid(playing[i][j],&status[i][j],0);
            }
            //load the result to player struct
            for(int j=0;player[j].inGame&&j<playerCount;j++) {
                
                switch(WEXITSTATUS(status[i][j])) {
                    case 1://success
                        player[j].life[i]=player[j].life[i-1];
                        break;
                    case 4://overtime
                    case 2://fail
                        player[j].life[i]=player[j].life[i-1]-1;
                        //if(player[j].life[i]==0) teargame(j,false);
                        break;
                    case 3://cheating
                        player[j].life[i]=0;
                        player[j].inGame=false;
                        teargame(j,false);
                        break;
                    default:
                        break;
                }
            }
            int num=playerLeft(i);
            
            
            for(int j=0;player[j].inGame&&j<playerCount;j++) {
                switch(num) {
                    case 0:
                        teargame(j,true);
                        break;
                    case 1:
                        if(player[j].life[i]>0) teargame(j,true);
                        else teargame(j,false);
                        break;
                    default:
                        if(i==MAX_ROUND) {
                            if(player[j].life[i]>0)
                                teargame(j,true);
                            if(player[j].life[i]==0) {
                                teargame(j,false);
                            }
                        }
                        else if(player[j].life[i]==0) {
                            teargame(j,false);
                        }
                        else if(WEXITSTATUS(status[i][j])==1) {
                            bzero(buf,BUFFER_SIZE);
                            sprintf(buf,"%d,PASS",player[j].playerId);
                            send_message(player[j].clientFD,buf);
                        }
                        else if(WEXITSTATUS(status[i][j])%2==0) {
                            bzero(buf,BUFFER_SIZE);
                            sprintf(buf,"%d,FAIL",player[j].playerId);
                            send_message(player[j].clientFD,buf);
                        }
                }
            }
            
            
        }
        
    }
    printf("game over\n");
    kill(rejectClient,SIGKILL);
    close(server_fd);
    /**
    while (true) {
        socklen_t client_len = sizeof(client);
        
        // Will block until a connection is made
        client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);

        if (client_fd < 0) {
            fprintf(stderr,"Could not establish new connection\n");
            exit(EXIT_FAILURE);
        }
     
        buf = calloc(BUFFER_SIZE, sizeof(char)); // Clear our buffer so we don't accidentally send/print garbage
        printf("hello\n");//ok
        int read = recv(client_fd, buf, BUFFER_SIZE, 0);    // Try to read from the incoming client
        //////////////////////////
        if(strstr(buf,"INIT")) {
            printf("%s\n",buf);
            buf[0]='\0';
            sprintf(buf,"WELCOME,%d",client_fd*100);
            send(client_fd, buf, strlen(buf), 0);
        }
        ///////////////////////////
        if(playerCount==0) {//first player connecting
            setup_player(playerCount,client_fd);
            player[playerCount-1]=fork();
            if(player[playerCount-1]<0) {
                printf("fork error!");
            }
            
            else if(player[playerCount-1]==0) {
                printf("This is child process with pid of %d\n",getpid());
                sleep(15);
                exit(1);
            }
//            else if(player[playerCount-1]>0){
//                int a=wait(NULL);
//                printf("catch :%d\n",a);
//            }
            
        }
        else if(playerCount<MAX_PLAYER) {
            setup_player(playerCount,client_fd);
//            player[playerCount-1]=fork();
//            if(player[playerCount-1]<0) {
//                printf("fork error!");
//            }
//            else if(player[playerCount-1]==0) {//wait for the last child process terminated
//                waitpid(player[playerCount-2]*(-1),NULL,0);
//            }
            
        }
        
        else {//cancel the player as players reach the MAX_PLAYER
            recv(client_fd, buf, BUFFER_SIZE, 0);
            sprintf(buf,"REJECT");
            send(client_fd, buf, strlen(buf), 0);
        }
        wait(NULL);
        if(playerCount==MAX_PLAYER) {//we can start the game
            printf("game start\n");
            rollDice();
            
            for(int i=0;i<MAX_PLAYER;i++) {
                buf[0]='\0';
                sprintf(buf,"START,%d,%d",playerFD[i]*100,MAX_LIFE);
                send(playerFD[i], buf, strlen(buf), 0);
            }
        }
        else {
            for(int i=0;i<playerCount;i++) {
                buf[0]='\0';
                sprintf(buf,"CANCEL");
                puts(buf);
                send(playerFD[i], buf, strlen(buf), 0);
            }
        }
        
        while (true) {  
            buf = calloc(BUFFER_SIZE, sizeof(char)); // Clear our buffer so we don't accidentally send/print garbage
            int read = recv(client_fd, buf, BUFFER_SIZE, 0);    // Try to read from the incoming client

            if (read < 0){
                fprintf(stderr,"Client read failed\n");
                exit(EXIT_FAILURE);
            }

            printf("%s\n", buf);

            buf[0] = '\0';
            sprintf(buf, "My politely respondance");

            err = send(client_fd, buf, strlen(buf), 0); // Try to send something back
            // printf("Client's message is: %s",buf);
             //sleep(5); //Wait 5 seconds

            free(buf);
        }

    }*/
}



