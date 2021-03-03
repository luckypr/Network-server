#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define portnum 12345									//PORT NUMBER FOR CONNECTING


#define yes 1
#define not 0


void man(int sock);										//function for explaining main functions of server
void do_it(int sock);									//function for working with client
void complain(int sock);								//function for printing error when not logged
void help(int sock);									//function for explaining MSG command

static void eror(const char *e)							
{
  perror(e);
  exit(1);
}

int main()
{
  int r;												//r is used for storing returning values of functions && checking returning values
  int sockfd;											//socket descriptor of server					
  int newsock;											//socket descriptor of client
  struct sockaddr_in serv, cli;							
  sockfd=socket(AF_INET, SOCK_STREAM, 0);				//creating server socket
  if(sockfd==-1)					
    eror("Socket() err");
  serv.sin_family=AF_INET;
  serv.sin_port=htons(portnum);
  serv.sin_addr.s_addr=INADDR_ANY;

  //bind
  r=bind(sockfd, (struct sockaddr *)&serv,sizeof(serv));
  if(r==-1)
    eror("Bind() err");
  listen(sockfd,5);

  //Accept
  int clilen=sizeof(cli);
  newsock=accept(sockfd,(struct sockaddr *)&cli, &clilen);
  do_it(newsock);

  close(sockfd);
  close(newsock);
  
  return 0;
}

void do_it(int sock)
{
  FILE *fp;
  FILE *fm;
  FILE *fi;
  FILE *fs;
  fi=fopen("primaoci.txt","a+");
  fm=fopen("poruke.txt","a+");
  fp=fopen("korisnici.txt","a+");
  fs=fopen("poslanici.txt","a+");
  char buf[500], readf[500],msg[500];
  char user[25],name[25],sent[50];
  char nl[2];
  int len=0;
  int exsist, state=not;
  nl[0]='\n';
  nl[1]='\0';
  strcpy(buf,"Dobrodosli na server!\n");
  send(sock,buf,strlen(buf),0);
  memset(buf,'\0',strlen(buf));
  strcpy(buf,"Kako bi ste se ulogovali, ukucajte LOG i korisnicko ime, koje mora imati bar tri karaktera\n\n");
  send(sock,buf,strlen(buf),0);
  memset(buf,'\0',strlen(buf));
  while(1)
    {
      recv(sock,buf,500,0);
      if(!strncmp("LOG",buf,3) && state==not)
	{
	  rewind(fp);
	  exsist=not;
      	  sscanf(buf + 4 , "%s", user);
	  if(strlen(user)<3)
	    {
	      memset(buf,'\0',strlen(buf));
	      strcpy(buf,"Vas nik mora imati bar tri karaktera !!!\n");
	      send(sock,buf,strlen(buf),0);
	      memset(buf,'\0',strlen(buf));
	      continue;
	    }
	  memset(readf,'\0',strlen(readf));
	  while(fscanf(fp,"%s",readf)!=EOF)
	    {
	      if(!strcmp(user,readf))
		{
		  printf("Nasao sam\n\n");
		  exsist=yes;
		  break;
		}
	    }
	  if(exsist==not)
	    {
	      fprintf(fp,"%s\n",user);
	    }
	  printf("Korisnik je: %s\n",user);
	  memset(buf,'\0',strlen(buf));
	  strcpy(buf,"Ulogovani ste\n");
	  send(sock,buf,strlen(buf),0);
	  memset(buf,'\0',strlen(buf));
	  state=yes;
	  len=strlen(user);
	}
      else if(!strncmp("LIST",buf,4))
	{
	  if(state==not)
	    {
	      complain(sock);
	      continue;
	    }
	  rewind(fp);
	  printf("Korisnik %s, gleda listu registrovanih korisnika \n\n",user);
	  memset(readf,'\0',strlen(readf));
	  while(fscanf(fp,"%s",readf)!=EOF)
	    {
	      send(sock,readf,strlen(readf),0);
	      send(sock,nl,strlen(nl),0);
	      memset(readf,'\0',strlen(readf));
	    }
	  rewind(fp);
	  memset(buf,'\0',strlen(readf));
	}
      else if(!strncmp("MSG",buf,3))
	{
	  if(state==not)
	    {
	      complain(sock);
	      continue;
	    }
	  int t=not;
	  memset(readf,'\0',strlen(readf));
	  sscanf(buf+4, "%s", readf);
	  while(fscanf(fp,"%s",name)!=EOF)
	    {
	      if(!strcmp(name,readf)){ t=yes; break;  }
	    }
	  if(t==not)
	    {
	      rewind(fp);
	      memset(buf,'\0',strlen(buf));
	      strcpy(buf,"Komanda nije uspela, korisnik ne postoji u bazi!!! Probajte ponovo! \n\n");
	      send(sock,buf,strlen(buf),0);
	      memset(buf,'\0',strlen(buf));
	      continue;
	    }
	  memset(sent,'\0',strlen(sent));
	  printf("Poruka je za : %s\n", readf);
	  readf[strlen(readf)]='\0';
	  fprintf(fi,"%s\n",readf);
	  memset(buf,'\0',strlen(buf));
	  strcpy(buf,"Ukucajte poruku: ");
	  send(sock,buf,strlen(buf),0);
	  memset(buf,'\0',strlen(buf));
	  recv(sock,buf,500,0);
	  fprintf(fm,"%s",buf);
	  fprintf(fs,"%s\n",user);
	  memset(buf,'\0',strlen(buf));
	  strcpy(buf,"Poruka je poslata uspesno! \n\n");
	  send(sock,buf,strlen(buf),0);
	  memset(buf,'\0',strlen(buf));
	  nl[0]='\n';
	  nl[1]='\0';
	}
      else if(!strncmp("INBOX",buf,5))
	{
	  if(state==not)
	    {
	      complain(sock);
	      continue;
	    }
	  rewind(fi);
	  rewind(fm);
	  rewind(fs);
	  memset(readf,'\0',strlen(readf));
	  memset(buf,'\0',strlen(buf));
	  memset(msg,'\0',strlen(msg));
	  while(1)
	    {
	      if(fscanf(fi,"%s",readf)!=EOF)
		{
		  fgets(msg,sizeof(msg),fm);
		  fscanf(fs,"%s",buf);
		  if(!strcmp(user,readf))
		    {
		      printf("Korisnik pregleda prijemno sanduce!\n");
		      send(sock,"Poruka:",7,0);
		      send(sock,msg,strlen(msg),0);
		      send(sock," od ",4,0);
		      send(sock,buf,strlen(buf),0);
		      send(sock,"\n",1,0);
		    }
		  memset(readf,'\0',strlen(readf));
		  memset(msg,'\0',strlen(msg));
		  memset(buf,'\0',strlen(buf)); 
		}
		  else
		    {
		      break;
		    }
	    }
	  
	  continue;
	}
      else if(!strncmp("OUT", buf,3))
	{
	  if(state==not)
	    {
	      complain(sock);
	      continue;
	    }
	  memset(user,'\0',strlen(user));
	  state=not;
	  memset(buf,'\0',strlen(buf));
	  strcpy(buf,"Odjavljeni ste\n\n");
	  send(sock,buf,strlen(buf),0);
	  memset(buf,'\0',strlen(buf));
	}
      else if(!strncmp("help",buf,4))
	{
	  help(sock);
	}
      else if(!strncmp("man",buf,3))
	{
	  man(sock);
	}
      else if(!strncmp("exit",buf,4))
	{
	  memset(buf,'\0',strlen(buf));
	  strcpy(buf,"Server je zatvorio vezu\n\n");
	  return;
	}
      else
	{
	  memset(buf,'\0',strlen(buf));
	  strcpy(buf,"Uneli ste nepoznatu komandu, za popis komandi kucajte help\n\n");
	  send(sock,buf,strlen(buf),0);
	  memset(buf,'\0',strlen(buf));
	}
      
    }

}
void complain(int sock)
{
  char buf[100];
  strcpy(buf,"NISTE ULOGOVANI, KORISTITE KOMANDU HELP ZA POMOC\n\n");
  send(sock, buf,strlen(buf),0);
}
void man(int sock)
{
  char buf[250];
  strcpy(buf,"************ komanda MSG ***********\n\n");
  send(sock,buf,strlen(buf),0);
  memset(buf,'\0',strlen(buf));
  strcpy(buf,"Da biste poslali poruku, otkucajte MSG [ime korisnika], zatim u novom redu otkucajte poruku\n");
  send(sock,buf,strlen(buf),0);
  memset(buf,'\0',strlen(buf));
  strcpy(buf,"**********************************\n\n");
  send(sock,buf,strlen(buf),0);
}

void help(int sock)
{
  char buf[150];
  strcpy(buf, "****** Verzija servera 1.0 ******\n\n");
  send(sock,buf,strlen(buf),0);
  memset(buf,'\0',strlen(buf));
  strcpy(buf, "Lista komandi: \n\n");
  send(sock,buf,strlen(buf),0);
  memset(buf,'\0',strlen(buf));
  strcpy(buf,"LOG- sluzi da se ulogujete\n\n");
  send(sock,buf,strlen(buf),0);
  memset(buf,'\0',strlen(buf));
  strcpy(buf, "MSG- sluzi da posaljete poruku korisniku,za vise detalja kucajte man\n\n");
  send(sock,buf,strlen(buf),0);
  memset(buf,'\0',strlen(buf));
  strcpy(buf,"LIST- sluzi da vidite sve korisnike\n\n");
  send(sock,buf,strlen(buf),0);
  memset(buf,'\0',strlen(buf));
  strcpy(buf,"INBOX- sluzi da vidite sve poruke koje vam je neko poslao\n\n");
  send(sock,buf,strlen(buf),0);
  memset(buf,'\0',strlen(buf));
  strcpy(buf,"OUT- sluzi da se izlogujete\n\n");
  send(sock,buf,strlen(buf),0);
  memset(buf,'\0',strlen(buf));
  strcpy(buf,"exit- sluzi da napustite vezu\n\n");
  send(sock, buf,strlen(buf),0);
  memset(buf,'\0',strlen(buf));
  strcpy(buf,"help- sluzi za pomoc novim korisnicima\n\n");
  send(sock,buf, strlen(buf),0);
  memset(buf,'\0',strlen(buf));
  strcpy(buf, "\n*********************************\n\n");
  send(sock,buf,strlen(buf),0);
  memset(buf,'\0',strlen(buf));
}
