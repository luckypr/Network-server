#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sqlite3.h>

//portnumber
#define portnum 12345

//nicer printing
#define BORDER  printf("----------------------------------------\n");

//block size for transfering files
#define block	2048

//define states
#define logged 	1
#define not_logged 0

/*
 * Server default addres is 127.0.0.1 
 * You can change it by passing address of server via argument (argv[1]) 
 * argv[0] is program name!!!
 */

#define local "127.0.0.1"

/*
 *	Supported commands:
 * 
 *	1-LOG-used to login
 *	2-ALL-list all files on server
 * 	3-USR-list all users on server
 *	4-MSG for sending message to other user
 *	5-BOX for reading personal messages from other users
 *	6-PUT for uploading file on server
 *	7-GET for downloading file from server
 *	8-OUT for log out and exit
 *	9-MAN for help on usage and commands
 */

//send login request
int req_log(int);

//send request for all files on server
void req_all(int );

//send request for all users on server
void req_usr(int );

//send message to some user
int req_msg(int);

//read your messages
void req_box(int );

//download file from server
int req_get(int);

//upload file to server
int req_put(int);

//log out
int req_out(int );

//It can be only a-z or A-Z
int validate(char *text);

//display help on using commands
void man();

//display main menu
void disp_menu();

//username will be stored here,useful for other functions
char username[16];

//store here server address	
char 	*srv_addr;

int main(int argc,char *argv[])
{
	int 			sockfd,confd,rv,c;
	int 			state=not_logged;
	struct 			sockaddr_in servaddr;
	char 			buf[512];
	char 			cmd[5];
	
	//is there argv[1]? use it as ip addr of server	
	if(argc==2){
		srv_addr=(char*)malloc(strlen(argv[1]));	
		strncpy(srv_addr,argv[1],strlen(argv[1]));
	}else{
		//no argv[1]? use local
		srv_addr=(char*)malloc(sizeof(local));
		strncpy(srv_addr,local,sizeof(local));
	}

	sockfd=socket(AF_INET,SOCK_STREAM,0);
	if(sockfd < -1){
		perror("socket() :");
		free(srv_addr);
		exit(0);
	}
	
	//empty structure
	memset(&servaddr,0,sizeof(servaddr));
	//prepare structure
	servaddr.sin_family=AF_INET;
	servaddr.sin_port=htons(portnum);
	servaddr.sin_addr.s_addr=inet_addr(srv_addr);
	
	//connect to server
	rv=connect(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
	if(rv<0){
		perror("connect() :");
		free(srv_addr);
		exit(0);
	}

	printf("\nConnected to %s\n\n\n",srv_addr);

	system("clear");
	//infinitive loop
	//if you send OUT it will terminate
	while(1){
		disp_menu();
		//get user input
		fgets(cmd,5,stdin);

		//get rid off \n -> see man fgets
		cmd[3]='\0';	
		
		if(!(strncmp(cmd,"LOG",3)) && state==not_logged){
			//log in
			rv=req_log(sockfd);
			if(rv==-1)
				printf("\n\n[-]Try again with different nickname\n\n");
			else
				state=logged;
		}else if(!strncmp(cmd,"ALL",3) && state==logged){
			//list all users
			req_all(sockfd);	
		}else if( !strncmp(cmd,"USR",3) && state==logged){
			req_usr(sockfd);
		}else if(!strncmp(cmd,"MSG",3) && state==logged){
			//message someone
			rv=req_msg(sockfd);
			if(rv==-1)
				printf("[-]Failed to send message!\n\n");
			else
				printf("[+]Message sent\n\n");
		}else if(!strncmp(cmd,"BOX",3) && state==logged){
			//check your inbox
			req_box(sockfd);
		}else if(!strncmp(cmd,"PUT",3) && state==logged){
			//upload something
			rv=req_put(sockfd);
			if(rv==-1)
				printf("[-]PUT failed!\n\n");
			else
				printf("[+]Succesflly uploaded file\n\n");
		}else if(!strncmp(cmd,"GET",3) && state==logged){
			//download something
			rv=req_get(sockfd);
			if(rv==-1)
				printf("[-]GET failed\n\n");
			else
				printf("[+]Succesfully downloaded file\n\n");
		}else if(!strncmp(cmd,"OUT",3)){
			//log out
			rv=req_out(sockfd);
			if(rv==1){
				close(sockfd);
				return 1;
			}
			else
				printf("\n\n[-]Log out failed!\n\n");
			break;
		}else if(!strncmp(cmd,"MAN",3)){
			//display help on commands
			man();
		}else
			printf("UNSUPORTED COMMAND!\n\n");
		
		memset(cmd,0x00,sizeof(cmd));
			
	}

	free(srv_addr);
	close(sockfd);
	

	return 0;
}
int req_log(int sock)
{
	char 	uname[16];
	char 	out_buf[20];
	char 	in_buf[128];
	int 	rv;	
	//read username
	printf("Enter your username: ");
	fgets(uname,16,stdin);
	uname[strlen(uname)-1]='\0';
	printf("[+]Username is : %s\n",uname);	
	//minimum allowed len is 4
	if(strlen(uname)<4){
		printf("[-]Username must have at least 4 characters!\n");
		return -1;
	}
	//check is valid only a-z A-Z 
	rv=validate(uname);
	if(rv==-1){
		printf("[-]Username is not valid!\n");
		return -1;
	}
	printf("[+]Username is valid!\n");
	
	//LOG command send 
	sprintf(out_buf,"LOG %s",uname);
	int t=send(sock,out_buf,strlen(out_buf),0);
	//check answer
	recv(sock,in_buf,sizeof(in_buf),0);
	if(! strncmp(in_buf,"true",4)){
		strncpy(username,uname,strlen(uname));
		printf("[%s] -> Log in successfully\n\n",srv_addr);
		return 1;
	}else
		return -1;
}
void req_usr(int sock)
{
	//list all users 
	char 	in_buf[64];
	char	out_buf[64];
	int	pos;
	//format request
	strncpy(out_buf,"USR",sizeof(out_buf));
	//send request
	send(sock,out_buf,strlen(out_buf),0);
	BORDER;
	printf("All users: \n\n");
	while(1)
	{
		memset(in_buf,0x00,sizeof(in_buf));
		recv(sock,in_buf,sizeof(in_buf),0);
		//check if this is last packet -> end is with !done!
		pos=strlen(in_buf)-6;
		if(!strncmp( &in_buf[pos],"!done!",6)){
			in_buf[pos]='\0';
			if(strlen(in_buf)==6)
				return;
			printf("%s\n",in_buf);
			BORDER;
			return;
		}
		printf("%s",in_buf);
	}

}
void req_all(int sock)
{
	//list all files
	int count=0;
	char out_buf[256];
	char in_buf[256];
	//send request
	strncpy(out_buf,"ALL",sizeof(out_buf));
	send(sock,out_buf,strlen(out_buf),0);
	//wait in loop for answer
	printf("Format is: \n");
	printf("filename\t\t\t[owner]\n");
	BORDER;
	while(1){
		memset(in_buf,0x00,sizeof(in_buf));
		recv(sock,in_buf,sizeof(in_buf),0);
		if(!strncmp( &in_buf[strlen(in_buf)-6], "!done!",6))
		{
			//print last packet
			in_buf[strlen(in_buf)-6]='\0';
			printf("%s\n",in_buf);
			BORDER;
			return;
		}
		printf("%s",in_buf);
	}

	return;

}
int req_msg(int sock)
{
	//send message to someone
	char 	to[16], msg[256];
	char 	out_buf[1024],in_buf[128];
	int 	rv;	
	BORDER;
	printf("Enter username to send message: ");
	fgets(to,16,stdin);
	//get rid of \n
	to[strlen(to)-1]='\0';
	//validate username
	rv=validate(to);
	if(rv==-1){
		printf("Username is not valid!\n\n");
		return -1;
	}
	printf("Now enter message for %s\n",to);
	printf("msg: ");
	fgets(msg,256,stdin);
	msg[strlen(msg)-1]='\0';	
	printf("Message to send is %s\n",msg);
	sprintf(out_buf,"MSG %s %s %s",username,to,msg);
	send(sock,out_buf,strlen(out_buf),0);
	memset(in_buf,0,sizeof(in_buf));
	recv(sock,in_buf,sizeof(in_buf),0);
	if(!strncmp(in_buf,"true",4))
		return 1;
	else
		return -1;
	
}
void req_box(int sock)
{
	//read your inbox
	char 	out_buf[128];
	char	in_buf[256];
	int 	pos;
	BORDER;
	//send request
	sprintf(out_buf,"BOX %s", username);
	send(sock,out_buf,strlen(out_buf),0);
	while(1){
		memset(in_buf,0,sizeof(in_buf));
		recv(sock,in_buf,sizeof(in_buf),0);
		pos=strlen(in_buf)-6;
		if(!strncmp( &in_buf[pos], "!done!",6)){

			//is packet just !done!
			if(pos==0)
				return;	
			//no more data,print last message
			in_buf[pos]='\0';
			printf("%s",in_buf);
			return;
		}
		printf("%s",in_buf);

	}
	return;
}
int req_put(int sock)
{
	//put file on server
	char 	out_buf[block],in_buf[64];
	char 	file_name[32];
	long 	rb=0;
	struct	stat st;
	long	file_size,nSent=0,total=0;
	int	sendFd=-1;
	BORDER;
	//get name of file
	printf("Enter name of file to upload on server: ");
	fgets(file_name,32,stdin);
	//trim of \n
	file_name[strlen(file_name)-1]='\0';
	printf("[*]Uploading: %s\n",file_name);
	
	//try to open file
	sendFd=open(file_name,O_RDONLY);
	if(sendFd< 1){
		perror("[-]PUT failed: open file(): ");
		return -1;
	}
	//get file size
	stat(file_name,&st);	
	file_size=st.st_size;
	printf("File size is: %ld\n",file_size);
	//format request	
	memset(out_buf,0x00,sizeof(out_buf));
	sprintf(out_buf,"PUT %s %s %ld",file_name,username,file_size);
	//send request
	send(sock,out_buf,strlen(out_buf),0);
	//get answer	true or false
	memset(in_buf,0x00,sizeof(in_buf));
	recv(sock,in_buf,sizeof(in_buf),0);
	if(!strncmp(in_buf,"true",4)){
		//we can start sending data
		while( file_size>total ){
			memset(out_buf,0x00,block);
			nSent=read(sendFd,out_buf,block);
			total+=nSent;
			nSent=send(sock,out_buf,nSent,0);
		}
		close(sendFd);
		return 1;
	}else{
		close(sendFd);
		return -1;
	}
}
int req_get(int sock)
{
	//download file from server
	char 	filename[32];
	char 	in_buf[block];
	char 	out_buf[128];
	int	fd=-1;
	long	filesize,nread=0,total=0;
	BORDER;
	//get filename	
	printf("Enter name of file to download: ");
	fgets(filename,32,stdin);
	//get rid off \n
	filename[strlen(filename)-1]='\0';
	printf("[+]File name is: %s\n",filename);
	//create file 
	fd=open(filename,O_WRONLY | O_CREAT, S_IRWXU | S_IRGRP | S_IROTH);
	if(fd <= 0 ){
		perror("open():: ");
		return -1;
	}
	//format request	
	memset(out_buf,0x00,sizeof(out_buf));
	sprintf(out_buf,"GET %s",filename);
	//send request	
	send(sock,out_buf,strlen(out_buf),0);
	printf("[+]Request sent...\n");
	//read file size. False if no file on server
	memset(in_buf,0,block);
	recv(sock,in_buf,block,0);
	if(!strncmp(in_buf,"false",5)){
		return -1;
		close(fd);
	}
	else
		filesize=atoi(in_buf);
	printf("[+]File size is: %ld\n",filesize);
	send(sock,".",1,0);
	//we can start reading now
	memset(in_buf,0x00,block);
	while(total < filesize){
		nread=recv(sock,in_buf,block,0);
		total+=nread;
		nread=write(fd,in_buf,nread);
		memset(in_buf,0x00,block);	
	}
	close(fd);
	printf("Done!\n");
	return 1;
}
int req_out(int sock)
{
	//log out
	char 	out_buf[128];
	BORDER;
	strncpy(out_buf,"OUT",sizeof(out_buf));
	send(sock,out_buf,strlen(out_buf),0);
	printf("[%s] -> Closed connection!\n",srv_addr);
	return 1;

}

int validate(char *text)
{
	int i=0;
	int len=strlen(text);
	do{
		if( ( (text[i]>=65) && (text[i]<=90) ) || ( (text[i]>=97) && (text[i]<=122) )  )
			;	
		else
			return -1;
		i++;	
	}while(i<len);
	return 1;
}

void disp_menu()
{
	printf("Enter command:\n\n");
	printf("-) LOG \n");
	printf("-) USR \n");
	printf("-) ALL \n");
	printf("-) MSG \n");
	printf("-) BOX \n");
	printf("-) PUT \n");
	printf("-) GET \n");
	printf("-) OUT \n");
	printf("-) MAN \n\n");
	printf("> ");
}

void man()
{
	printf("\n************************************************************\n\n");
	printf("Server ussage instructions...\n\n");
	printf("LOG-provide interface for loggin on server\n");
	printf("\tFormat-> LOG <Press Enter>\n");
	printf("\tNow enter your username\n");
	printf("\tYou need to supply valid username[a-zA-z]\n");
	printf("\tUsername at least 4 characters len\n\n");
	printf("USR-list all users on server[active and offline]\n\n");
	printf("ALL-list all files on server\n\n");
	printf("MSG-for sending messages...\n");
	printf("\tFormat-> MSG <Enter>\n");
	printf("\tUsername:[whom to send]\n");
	printf("\tMessage:[Message to send]\n");
	printf("\tNOTE: Username must exists in database!!!\n\n");
	printf("BOX-for listing your inbox\n\n");
	printf("PUT-for uploading file on server\n");
	printf("\tFormat-> PUT [name_of_file]\n");
	printf("GET-for downloading file from server\n");
	printf("\tFormat GET [file_name_on_server]\n");
	printf("MAN-short instructions on available commands\n\n");
	printf("OUT-log out from server and close connection\n\n");
	printf("************************************************************\n\n");
}
