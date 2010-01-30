
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int init_client(const char *name, unsigned int port)
{
  int sfd;
  struct addrinfo *ai;
  struct addrinfo hint;

  //get address info...  
  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_INET;
  hint.ai_socktype = SOCK_STREAM;
  int status = getaddrinfo(name, NULL, &hint, &ai);
  if(status != 0){
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return -1;
  }
  
  //try to open the socket (examine all possible results)
  struct addrinfo *tmp_ai;
  struct sockaddr_in *sinp;
  struct sockaddr_in inaddr;
  for(tmp_ai = ai; tmp_ai != NULL; tmp_ai = tmp_ai->ai_next){
    char addr[1024];
    sinp = (struct sockaddr_in*)tmp_ai->ai_addr;
    inet_ntop(ai->ai_family, &sinp->sin_addr, addr, sizeof(addr));
    fprintf(stderr, "Have result! %s\n", addr);

    sinp->sin_port = htons(port);
    inaddr = *sinp;
    sfd = socket(((struct sockaddr*)sinp)->sa_family, SOCK_STREAM, 0);
    printf("Socket %d\n", sfd);
    if(sfd < 0){
      perror("socket");
      continue;
    }
    break;
  }
  
  if(tmp_ai == NULL){
    fprintf(stderr, "Didn't work!\n");
    return -1;
  }
  
  freeaddrinfo(ai);
  
  status = connect(sfd, (struct sockaddr*)&inaddr, sizeof(inaddr));
  printf("Connecting...\n");
  if(status != 0){
    perror("connect");
    return -1;
  }
  return sfd;
}


/*
initializes server

type - e.g. SOCK_STREAM, SOCK_DGRAM... see 'man socket' (optionaly SOCK_NONBLOCK)
addr
*/
int prepare_server_comm(int type, const struct sockaddr *addr, socklen_t addr_len, 
               int qlen)
{
  int fd;
  int reuse = 1;
  
  fd = socket(addr->sa_family, type, 0);
  if(fd < 0){
    perror("socket:");
    return -1;
  }
  if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0){
    perror("setsockopt:");
    goto error_label;
  }
  
  if(bind(fd, addr, addr_len) != 0){
    perror("bind:");
    goto error_label;
  }
  
  if((type == SOCK_STREAM) || (type == SOCK_SEQPACKET)){
    if(listen(fd, qlen) < 0){
      perror("listen:");
      goto error_label;
    }
  }
  return fd;
  
  //Handle error...
 error_label:
  close(fd);
  return -1;
}

int init_server(unsigned int port)
{
  int sfd;
  
  char name[1024];
  struct addrinfo *ai;
  struct addrinfo hint;
  
  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_INET;
  hint.ai_socktype = SOCK_STREAM;
  
  if(gethostname(name, sizeof(name)) != 0){
    perror("gethostname");
  }else{
    strcpy(name, "localhost");
  }
  
  int status = getaddrinfo(name, NULL, &hint, &ai);
  if(status != 0){
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return -1;
  }
  
  struct addrinfo *tmp_ai;
  for(tmp_ai = ai; tmp_ai != NULL; tmp_ai = tmp_ai->ai_next){
      struct sockaddr_in *sinp = (struct sockaddr_in*)tmp_ai->ai_addr;
      
      sinp->sin_addr.s_addr = htonl(INADDR_ANY);
      sinp->sin_port = htons(port);
      sfd = prepare_server_comm(SOCK_STREAM, (struct sockaddr*)sinp, 
                                tmp_ai->ai_addrlen, 0);
      if(sfd != -1){
        break;
      }
  }
  
  if(tmp_ai == NULL){
    fprintf(stderr, "Didn't work!\n");
    return -1;
  }
  
  freeaddrinfo(ai);
  return sfd;
}

int accept_connection(int socket)
{
  int cfd;
  struct sockaddr_in client;
  socklen_t client_len = sizeof(struct sockaddr);
  printf("Going to wait!\n");
  cfd = accept(socket, (struct sockaddr*)&client, &client_len);
  if(cfd == -1){
    perror("accept::");
    return -1;
  }
  return cfd;
}
