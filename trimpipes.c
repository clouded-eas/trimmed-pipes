/* trimpipes.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <strings.h>

#define recv(x,y,z,a) read(x,y,z)
#define send(x,y,z,a) write(x,y,z)
#define closesocket(s) close(s)

  typedef int SOCKET;

#ifndef ADDR_NULLIFY
#define ADDR_NULLIFY 0xffffffff
#endif

struct client_t
{
  int inuse;
  SOCKET csock, osock;
  time_t activity;
};

#define MCLIENT 5
#define WAITUP 450

int main(int argc, char *argv[])
{ 
  SOCKET lsock;
  char buf[4096];
  struct sockaddr_in laddr, oaddr;
  int i;
  struct client_t clients[MCLIENT];

  if (argc != 5) {
    fprintf(stderr,"%s lh/lp/rh/rp\n",argv[0]);
    return 30;
  }

  for (i = 0; i < MCLIENT; i++)
    clients[i].inuse = 0;

  bzero(&laddr, sizeof(struct sockaddr_in));
  laddr.sin_family = AF_INET;
  laddr.sin_port = htons((unsigned short) atol(argv[2]));
  laddr.sin_addr.s_addr = inet_addr(argv[1]);
  if (!laddr.sin_port) {
    fprintf(stderr, "invalid listener port\n");
    return 20;
  }
  if (laddr.sin_addr.s_addr == ADDR_NULLIFY) {
    struct hostent *n;
    if ((n = gethostbyname(argv[1])) == NULL) {
      perror("gethostbyname");
      return 20;
    }    
    bcopy(n->h_addr, (char *) &laddr.sin_addr, n->h_length);
  }

  bzero(&oaddr, sizeof(struct sockaddr_in));
  oaddr.sin_family = AF_INET;
  oaddr.sin_port = htons((unsigned short) atol(argv[4]));
  if (!oaddr.sin_port) {
    fprintf(stderr, "bad port.\n");
    return 25;
  }
  oaddr.sin_addr.s_addr = inet_addr(argv[3]);
  if (oaddr.sin_addr.s_addr == ADDR_NULLIFY) {
    struct hostent *n;
    if ((n = gethostbyname(argv[3])) == NULL) {
      perror("gethostbyname");
      return 25;
    }    
    bcopy(n->h_addr, (char *) &oaddr.sin_addr, n->h_length);
  }

  if ((lsock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    return 20;
  }
  if (bind(lsock, (struct sockaddr *)&laddr, sizeof(laddr))) {
    perror("bind");
    return 20;
  }
  if (listen(lsock, 5)) {
    perror("listen");
    return 20;
  }

  laddr.sin_port = htons(0);
  
  while (1)
  {
    fd_set fdsr;
    int maxsock;
    struct timeval tv = {1,0};
    time_t now = time(NULL);

    FD_ZERO(&fdsr);
    FD_SET(lsock, &fdsr);
    maxsock = (int) lsock;
    for (i = 0; i < MCLIENT; i++)
      if (clients[i].inuse) {
        FD_SET(clients[i].csock, &fdsr);
        if ((int) clients[i].csock > maxsock)
          maxsock = (int) clients[i].csock;
        FD_SET(clients[i].osock, &fdsr);
        if ((int) clients[i].osock > maxsock)
          maxsock = (int) clients[i].osock;
      }      
    if (select(maxsock + 1, &fdsr, NULL, NULL, &tv) < 0) {
      return 30;
    }

    if (FD_ISSET(lsock, &fdsr))
    {
      SOCKET csock = accept(lsock, NULL, 0);
     
      for (i = 0; i < MCLIENT; i++)
        if (!clients[i].inuse) break;
      if (i < MCLIENT)
      {

        SOCKET osock;
        if ((osock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
          perror("socket");
          closesocket(csock);
        }
        else if (bind(osock, (struct sockaddr *)&laddr, sizeof(laddr))) {
          perror("bind");
          closesocket(csock);
          closesocket(osock);
        }
        else if (connect(osock, (struct sockaddr *)&oaddr, sizeof(oaddr))) {
          perror("connect");
          closesocket(csock);
          closesocket(osock);
        }
        else {
          clients[i].osock = osock;
          clients[i].csock = csock;
          clients[i].activity = now;
          clients[i].inuse = 1;
        }
      } else {
        fprintf(stderr, "max connect reached.\n");
        closesocket(csock);
      }        
    }

    for (i = 0; i < MCLIENT; i++)
    {
      int nbyt, closeneeded = 0;
      if (!clients[i].inuse) {
        continue;
      } else if (FD_ISSET(clients[i].csock, &fdsr)) {
        if ((nbyt = recv(clients[i].csock, buf, sizeof(buf), 0)) <= 0 ||
          send(clients[i].osock, buf, nbyt, 0) <= 0) closeneeded = 1;
        else clients[i].activity = now;
      } else if (FD_ISSET(clients[i].osock, &fdsr)) {
        if ((nbyt = recv(clients[i].osock, buf, sizeof(buf), 0)) <= 0 ||
          send(clients[i].csock, buf, nbyt, 0) <= 0) closeneeded = 1;
        else clients[i].activity = now;
      } else if (now - clients[i].activity > WAITUP) {
        closeneeded = 1;
      }
      if (closeneeded) {
        closesocket(clients[i].csock);
        closesocket(clients[i].osock);
        clients[i].inuse = 0;
      }      
    }
    
  }
  return 0;
}
