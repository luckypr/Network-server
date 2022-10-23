#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/times.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sqlite3.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>

//port number for connecting
#define portnum 	 12345

//block size used for file transfering
#define block		 2048       

//how many clients can connect to server in same time
#define n_clients 	 5

//for select() max value for File Descriptor
#define max_select_fd 1024

/*
 * number of file descriptors in set= num of clients + 1(for server)
 */
#define max	      n_clients+1 		


// create and prepare server 
int create_tcp_server();

//examine request and make decision what next to do
void answer(int* ,char *,sqlite3 *);

//perform loggin
void on_log(int* ,char *,sqlite3 *);

//list all files on server
void on_all(int* ,sqlite3 *);

//list all users in db
void on_usr(int *, sqlite3 *);

//send message to user
void on_msg(int *,char *,sqlite3 *);

//read your messages
void on_box(int *,char *, sqlite3 * );

//download file from server
void on_get(int *,char *);

//put file on server
void on_put(int *, char *,sqlite3 *);

//log out close conn
void on_out(int *);


int
main(int argc,char *argv[])
{
	int 		srvfd,res,i,j,maxfd;
	int 		clientfd;
	int 		clilen;
    	char   		buf[1024];
	int		socketsfd[max];
	int 		counter=0;
	int 		rc;
	struct 		sockaddr_in cli_addr;
	socklen_t 	cli_addr_size;
	sqlite3		*db;
	//initialize set of file descriptors-used for select()
	fd_set 		readfds;

	system("clear");
	
	// create server 
	srvfd=create_tcp_server();
		
	if(srvfd==-1){
		printf("[-]Failed to create server!\n");
		return 0;
	}
	printf("[+]Server is listening for connections\n\n");
	
	//open db
	if(sqlite3_open("srvdb.db",&db) < 0)
	{
		printf("[-]Failed to open database!\n");
		close(srvfd);
		exit(1);
	}
	printf("[+]Database opened!\n\n");
	//create directory for files
	rc=mkdir("documents",S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	// initialize all file descriptors in set to -1 
	for(i=0;i<max;i++)
		socketsfd[i]=-1;

	//first file descriptor is server fd	
	socketsfd[0]=srvfd;
	
	
	clilen=sizeof(cli_addr);
	cli_addr_size=sizeof(clilen);

	for(;;){
		/*
		 *	! ! !  NOTE ! ! !
		 * Using select in loops
		 * Reinitialize file descriptor set before every select call
		 * First step is to empty set
		 * Then we add in set file descriptors from arrray that are > -1
		 * Last step is select call
		 * Check man 2 select
		*/

		FD_ZERO(&readfds);
		for(i=0;i<max;i++){
			if(socketsfd[i]>-1)
				FD_SET(socketsfd[i],&readfds);
			
		}
		//process is blocked and waiting for action on some of file descriptors in set
		rc=select(max_select_fd,&readfds,NULL,NULL,NULL);
		if(rc<0){
		    perror("select(): ");
		    close(srvfd);
		    return 0;
	    	}
		for(i=0;i<max;i++){
			/*
			 * This loop is activated when event occurs on one or more fd 
			 * Loop through every fd  and check if
			 * fd socketsfd[0] is server fd
			 * Other are clients
			 * Respond on request
			 */
			if(FD_ISSET(socketsfd[i],&readfds) /*&& (socketsfd[i]>0)*/ ){
				if(socketsfd[i]==srvfd){
					//accept new connection 
					clientfd=accept(srvfd,(struct sockaddr *)&cli_addr,(socklen_t *)&clilen);
					if(clientfd<0){
						perror("accept(): ");
						return 1;
					}
					//find first available slot in array
					for(j=0;j<max;j++){
						if(socketsfd[j]==-1){
							//set slot to client file desc
							socketsfd[j]=clientfd;
							break;
						}
					}
					printf("[+]New Client added!\n");
				}else{
					//client wants something from server
					memset(buf,0x00,sizeof(buf));
					recv(socketsfd[i],buf,sizeof(buf),0);
					answer(&socketsfd[i],buf,db);	
				}
			}
		}

	}

	close(srvfd);
	return 0;

}

int create_tcp_server()
{
	int serverfd,res;
	struct sockaddr_in serv_addr;
	serverfd=socket(AF_INET,SOCK_STREAM,0);
	if(serverfd<0){
		perror("socket(): ");
		return -1;
	}

	/* setup structure */
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_port=htons(portnum);
	serv_addr.sin_addr.s_addr=INADDR_ANY;


	/* bind */

	res=bind(serverfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr));
	if(res<0){
		perror("bind(): ");
		close(serverfd);
		return -1;
	}

	/* listen */
	listen(serverfd,n_clients);
	time_t t=time(NULL);
	printf("[+]Server created\t\t%s\n",ctime(&t));
	return serverfd;

}
void answer(int *sock, char *cmd,sqlite3 *db)
{
	/*
	 * Based on user command this function activate one of available functions for handling request
	 */

	if(!strncmp(cmd,"LOG",3))
		on_log(sock,cmd+4,db);
	else if(!strncmp(cmd,"ALL",3))
		on_all(sock,db);
	else if(!strncmp(cmd,"USR",3))
		on_usr(sock,db);
	else if(!strncmp(cmd,"MSG",3))
		on_msg(sock, cmd+4,db);
	else if(!strncmp(cmd,"BOX",3))
		on_box(sock,cmd+4,db);
	else if(!strncmp(cmd,"PUT",3))
		on_put(sock,cmd+4,db);
	else if(!strncmp(cmd,"GET",3))
		on_get(sock,cmd+4);
	else
		on_out(sock);

}
void on_log(int *sock,char *uname,sqlite3 *db)
{
	//client wants to log in
	int 		rv;
	char		query[256];
	char 		out_buf[24];
	sqlite3_stmt	*stmt;
	
	sprintf(query,"INSERT OR IGNORE INTO users(name) VALUES('%s');commit;",uname);	
	//perform query
	sqlite3_prepare_v2(db,query,strlen(query),&stmt,NULL);
	rv=sqlite3_step(stmt);
	if(rv != SQLITE_DONE) {
		printf("[-]LOG failed [%s]!\n",sqlite3_errmsg(db));
		strncpy(out_buf,"false",sizeof(out_buf));
		send(*sock,out_buf,strlen(out_buf),0);
		return;
	}
	sqlite3_finalize(stmt);

	//tell client all went good	
	strncpy(out_buf,"true",sizeof(out_buf));
	send(*sock,out_buf,strlen(out_buf),0);

	//get time info
	time_t t=time(NULL);
	printf("[+]%s LOG IN at %s \n",uname,ctime(&t));
}
void  on_all(int *sock,sqlite3 *db)
{
	//list all files 
	char		out_buf[128];
	char		*query="select filename,owner from files;commit";
	sqlite3_stmt	*stmt;
	printf("[*]Request ALL!\n");
	sqlite3_prepare_v2(db,query,strlen(query),&stmt,NULL);
	while(sqlite3_step(stmt)!=SQLITE_DONE){
		memset(out_buf,0x00,sizeof(out_buf));
		sprintf(out_buf,"%-20s\t\t[%s]\n",(char*)sqlite3_column_text(stmt,0),(char*)sqlite3_column_text(stmt,1));
		send(*sock,out_buf,strlen(out_buf),0);
	}
	//signal done
	strncpy(out_buf,"!done!",sizeof(out_buf));
	send(*sock,out_buf,strlen(out_buf),0);
	printf("[+]Done ALL\n\n");
	sqlite3_finalize(stmt);
}
void  on_usr(int *sock,sqlite3 *db)
{
	//list all users on server
	char 		out_buf[64];
	char 		*query="select name from users;commit";
	sqlite3_stmt 	*stmt;
	printf("[*]Request USR!\n");
	sqlite3_prepare_v2(db,query,strlen(query),&stmt,NULL);
	while(sqlite3_step(stmt)!=SQLITE_DONE){
		memset(out_buf,0x00,sizeof(out_buf));
		sprintf(out_buf,"%s\n",(char*)sqlite3_column_text(stmt,0));
		send(*sock,out_buf,strlen(out_buf),0);
	}
	memset(out_buf,0x00,sizeof(out_buf));
	strncpy(out_buf,"!done!",sizeof(out_buf));
	send(*sock,out_buf,strlen(out_buf),0);
	sqlite3_finalize(stmt);
	printf("[+]Done USR!\n\n");
}
void on_msg(int *sock,char *arg,sqlite3 *db)
{	
	//send message to another client
	int		rv;
	char		from[16],to[16],msg[128];
	char 		out_buf[128];
	char		query[512];
	sqlite3_stmt	*stmt;
	sscanf(arg,"%s %s %[^\n]s",from,to,msg);
	printf("[*]MSG[%s]->[%s]\n",from,to);
	//perpare query
	sprintf(query,"INSERT INTO messages(sender,receiver,msg) values('%s','%s','%s');commit;",from,to,msg);
	sqlite3_prepare_v2(db,query,strlen(query),&stmt,NULL);
	rv=sqlite3_step(stmt);
	if(rv != SQLITE_DONE) {
		printf("[-]MSG failed!\n\n");
		return;
	}
	sqlite3_finalize(stmt);
	//signal client all good
	strncpy(out_buf,"true",sizeof(out_buf));
	send(*sock,out_buf,strlen(out_buf),0);
	printf("[+]Done MSG!\n\n");
}
void on_box(int *sock,char *name,sqlite3 *db)
{
	//client is reading his messages,then delete them
	int 		rv;
	int 		count=0;
	char 		query[128];
	char 		out_buf[256];
	sqlite3_stmt 	*stmt;
	printf("[*]Request BOX!\n");
	sprintf(query,"select sender,msg from messages where receiver='%s' ;commit;",name);
	sqlite3_prepare_v2(db,query,strlen(query),&stmt,NULL);
	while(sqlite3_step(stmt)!=SQLITE_DONE){
		memset(out_buf,0x00,sizeof(out_buf));
		sprintf(out_buf,"[%s]=> %s\n",(char*)sqlite3_column_text(stmt,0),(char*)sqlite3_column_text(stmt,1));
		send(*sock,out_buf,strlen(out_buf),0);
		count++;
	}
	if(count==0){
		memset(out_buf,0x00,sizeof(out_buf));
		strncpy(out_buf,"No messages for you!\n",sizeof(out_buf));
		send(*sock,out_buf,strlen(out_buf),0);
	}
	memset(out_buf,0x00,sizeof(out_buf));
	strncpy(out_buf,"!done!",sizeof(out_buf));
	
	send(*sock,out_buf,strlen(out_buf),0);

	//empty db
	char delete[128];
	sprintf(delete,"delete from messages where receiver='%s';",name);
	sqlite3_prepare_v2(db,delete,strlen(delete),&stmt,0);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	printf("[+]Done BOX!\n\n");
	return;

}
void on_put(int *sock,char *arg, sqlite3 *db)
{
	char 	user[16];
	char	path[64];
	long	filesize=0,nread=0,totalRead=0;
	int	copyFd=-1,rv;	
	char	in_buf[block],out_buf[64];
	char	log_msg[128];
	char	query[512];
	char	filename[32];
	sqlite3_stmt *stmt;
	//get full path
	printf("[*]Request PUT\n");
	sscanf(arg,"%s %s %ld",filename,user,&filesize);
	memset(path,0x00,sizeof(path));
	sprintf(path,"documents/%s",filename);
	printf("Full path to file is %s \n",path);
	printf("File size is: %ld\n",filesize);
	//create copy file
	copyFd=open(path,O_CREAT | O_WRONLY, S_IRWXU | S_IRGRP | S_IROTH);
	if(copyFd <= 0 ){
		//client wont start sending data
		strncpy(out_buf,"false",sizeof(out_buf));
		send(*sock,out_buf,strlen(out_buf),0);
	}else{
		//client will start sending data
		strncpy(out_buf,"true",sizeof(out_buf));
		send(*sock,out_buf,strlen(out_buf),0);
		
		while( totalRead < filesize ){
			memset(in_buf,0x00,block);
			nread=recv(*sock,in_buf,block,0);
			totalRead+=nread;
			nread=write(copyFd,in_buf,nread);
		}
		//Update database
		sprintf(query,"INSERT OR IGNORE INTO files(filename,owner) VALUES('%s','%s');commit;",filename,user);	
		sqlite3_prepare_v2(db,query,strlen(query),&stmt,NULL);
		rv=sqlite3_step(stmt);
		if(rv != SQLITE_DONE)
			printf("[-]Database update failed!\n\n");
		
		sqlite3_finalize(stmt);
		close(copyFd);
		printf("[+]New file uploaded on server!\\n");
	}
	printf("[+]Done PUT!\n\n");
}
void on_get(int *sock,char *filename)
{
	char	in_buf[128],out_buf[block];
	long	filesize,nsent=0,total=0;
	struct	stat st;
	int	fd=-1;
	char	path[64];

	printf("[*]Request GET\n");
	//open file
	sprintf(path,"documents/%s",filename);
	fd=open(path, O_RDONLY);
	if(fd==-1){
		printf("[-]Failed to open %s\n",path);
		strncpy(out_buf,"false",block);
		send(*sock,out_buf,strlen(out_buf),0);
		return;
	}else{
		
		stat(path,&st);
		filesize=st.st_size;
		sprintf(out_buf,"%ld",filesize);
		printf("[+]%s opened on %s size: %ld\n",filename,path,filesize);
		send(*sock,out_buf,strlen(out_buf),0);	
	}
	//receive some trash
	recv(*sock,in_buf,sizeof(in_buf),0);
	while(total < filesize){
		memset(out_buf,0x00,block);
		nsent=read(fd,out_buf,block);
		nsent=send(*sock,out_buf,nsent,0);
		total+=nsent;

	}
	printf("[+]Done GET!\n\n");
}
void on_out(int *sock)
{
	close(*sock);
	*sock=-1;
}
