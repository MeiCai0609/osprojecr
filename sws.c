/* 
 * File: sws.c
 * Author: Alex Brodsky
 * Purpose: This file contains the implementation of a simple web server.
 *          It consists of two functions: main() which contains the main 
 *          loop accept client connections, and serve_client(), which
 *          processes each client request.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "network.h"
#include <pthread.h>
#define MAX_HTTP_SIZE 8192                 /* size of buffer to allocate */

#define MAX_REQUEST_NUM 100
#define RR_QUANTUM 10
#define HIGHEST_QUANTUM 2
#define MEDIUM_QUANTUM 4
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX_REQUESTS 100

Queue SJF;
Queue RR;
Queue MLFB_Highest;
Queue MLFB_Medium;
Queue MLFB_Low;
int sequence = 0;
int _SJF = 0;
int _RR = 0;
int _MLFB = 0;


//Task2.
Semaphore *spaces;
Semaphore *items;
Queue WORK;







void schedule_Aux(Queue *q1, Queue *q2,Request *request);
int MLFB_schedule();
int RR_schedule(Queue *queue, int print);
int SJF_schedule();
void* worker_entry(void *args);
void saveToWorkQueue(int fd);
void preprocessingRequest(Request *request);
/* This function takes a file handle to a client, reads in the request, 
 *    parses the request, and sends back the requested file.  If the
 *    request is improper or the file is not available, the appropriate
 *    error is sent back.
 * Parameters: 
 *             fd : the file descriptor to the client connection
 * Returns: None
 */



void serve_client( int fd ) {
  sequence++;
  printf("Request sequence : %d\n",sequence);
  printf("fd: %d\n",fd );
  static char *buffer;                              /* request buffer */
  char *req = NULL;                                 /* ptr to req file */
  char *brk;                                        /* state used by strtok */
  char *tmp;                                        /* error checking ptr */
  FILE *fin;                                        /* input file handle */
  int len;                                          /* length of data read */

  if( !buffer ) {                                   /* 1st time, alloc buffer */
    buffer = malloc( MAX_HTTP_SIZE );
    if( !buffer ) {                                 /* error check */
      perror( "Error while allocating memory" );
      abort();
    }
  }

  memset( buffer, 0, MAX_HTTP_SIZE );
  if( read( fd, buffer, MAX_HTTP_SIZE ) <= 0 ) {    /* read req from client */
    perror( "Error while reading request" );
    abort();
  } 

  /* standard requests are of the form
   *   GET /foo/bar/qux.html HTTP/1.1
   * We want the second token (the file path).
   */
  tmp = strtok_r( buffer, " ", &brk );              /* parse request */
  if( tmp && !strcmp( "GET", tmp ) ) {
    req = strtok_r( NULL, " ", &brk );
  }
 
  if( !req ) {                                      /* is req valid? */
    len = sprintf( buffer, "HTTP/1.1 400 Bad request\n\n" );
    write( fd, buffer, len );                       /* if not, send err */
    close( fd );                                     /* close client connection*/
  } else {                                          /* if so, open file */
    req++;                                          /* skip leading / */
    fin = fopen( req, "r" );                        /* open file */
    if( !fin ) {                                    /* check if successful */
      len = sprintf( buffer, "HTTP/1.1 404 File not found\n\n" );  
      write( fd, buffer, len );                     /* if not, send err */
      close( fd );                                     /* close client connection*/
    } else {                                         /* if so, send file */

      struct stat st;
      stat(req, &st);
      Request *request = (Request*)malloc(sizeof(Request));
      request->filePath = (char*)check_malloc(strlen(req)+1);
      strcpy(request->filePath,req);
      request->sequence = sequence;
      request->file = fin;
      request->fileDes = fd;
      request->remainingBytes = st.st_size;
      request->fileSize = st.st_size;
      printf("file path : %s\n",request->filePath);
      printf("file size : %d\n",request->fileSize);
      if (_MLFB == 1)
        queue_push(&MLFB_Highest,request);
      else if (_SJF == 1)
        queue_push(&SJF,request);
      else 
        queue_push(&RR,request);
      len = sprintf( buffer, "HTTP/1.1 200 OK..\n\n" );/* send success code */
      write( fd, buffer, len );
    }
  }
}


/* This function is where the program starts running.
 *    The function first parses its command line parameters to determine port #
 *    Then, it initializes, the network and enters the main loop.
 *    The main loop waits for a client (1 or more to connect, and then processes
 *    all clients by calling the seve_client() function for each one.
 * Parameters: 
 *             argc : number of command line parameters (including program name
 *             argv : array of pointers to command line parameters
 * Returns: an integer status code, 0 for success, something else for error.
 */
int main( int argc, char **argv ) {
  int port = -1;                                    /* server port # */
  int fd;                                           /* client file descriptor */
  char *scheduler = malloc(sizeof(char)*5); 
  int workerNumber;
  pthread_t *workers;
  
  /*
  Initialization.
  */
  construct(&SJF,MAX_HTTP_SIZE);
  construct(&RR,RR_QUANTUM);
  construct(&MLFB_Highest,HIGHEST_QUANTUM);
  construct(&MLFB_Medium,MEDIUM_QUANTUM);
  construct(&MLFB_Low,RR_QUANTUM);
  _construct(&WORK);


  spaces = make_semaphore(MAX_REQUESTS);
  items = make_semaphore(0);

  /* check for and process parameters 
   */

  if( ( argc < 4 ) || ( sscanf( argv[1], "%d", &port ) < 1 )) {
    printf( "usage: sms <port> <scheduler> <thread number>\n" );
    return 0;
  }

  sscanf(argv[2],"%s",scheduler);
  sscanf(argv[3],"%d",&workerNumber);
 
  if (strcmp(scheduler,"SJF") == 0){
    _SJF = 1;
  }
  else if (strcmp(scheduler,"RR") == 0 ){
    _RR = 1;
  }
  else if (strcmp(scheduler,"MLFB") == 0 ){
    _MLFB = 1;
  }
  else{
    printf("Please enter in the correct scheduler type.\n");
    return 0;
  }
  workers = (pthread_t*)check_malloc(sizeof(pthread_t)*workerNumber);
  int i;
  for (i = 0; i<workerNumber; i++){
    pthread_create(&workers[i],NULL,worker_entry,NULL);
  }






  network_init( port );                             /* init network module */
  printf("The server is listenning on the port : %d\n",port);
  

  for( ;; ) {                                       /* main loop */

    network_wait();                                 /* wait for clients */
    
    for( fd = network_open(); fd >= 0; fd = network_open() ) { /* get clients */
      saveToWorkQueue(fd); 
    }
  }
}

void schedule_Aux(Queue *q1, Queue *q2,Request *request){
  static char *buffer; 
  if( !buffer ) {                                   /* 1st time, alloc buffer */
    buffer = malloc( MAX_HTTP_SIZE );
    if( !buffer ) {                                 /* error check */
      perror( "Error while allocating memory" );
      abort();
    }
  }
  memset( buffer, 0, MAX_HTTP_SIZE );
  int len;
  len = fread( buffer, 1, MIN(q1->quantum,request->remainingBytes), request->file );  /* read file chunk */
  if( len < 0 ) {                             /* check for errors */
    perror( "Error while writing to client" );
  } else if( len > 0 ) {                      /* if none, send chunk */
    len = write( request->fileDes, buffer, len );
    printf("Send <%d> bytes of file <%s>.\n\n",len,request->filePath);
    fflush(stdout);
    if( len < 1 ) {                           /* check for errors */
    perror( "Error while writing to client" );
    }
  }
  request->remainingBytes -= MIN(q1->quantum,request->remainingBytes);
  if (request->remainingBytes == 0){
    fclose( request->file );
    close(request->fileDes);
    printf("Request for file <%s> completed.\n-------------------------------------------\n",request->filePath );
    fflush(stdout);
    free(request);
  }
  else{
    queue_push(q2,request);
  }
}
int MLFB_schedule(){
  Request *request = queue_pop(&MLFB_Highest);
  if (request == NULL){
    request = queue_pop(&MLFB_Medium);
    if (request == NULL){
      return RR_schedule(&MLFB_Low,0);
    }
    else{
      schedule_Aux(&MLFB_Medium,&MLFB_Low,request);
    }
  }
  else{
    schedule_Aux(&MLFB_Highest,&MLFB_Medium,request);
  }
  return 0;
}
int RR_schedule(Queue *queue,int print){
  Request *request = queue_pop(queue);
  if (request == NULL){
    return -1;
  }
  schedule_Aux(queue,queue,request);
  return 0;
}

int SJF_schedule(){
  Request *request = queue_shortest(&SJF);

  if (request == NULL)
    return -1;
  static char *buffer; 
  if( !buffer ) {                                   /* 1st time, alloc buffer */
    buffer = malloc( MAX_HTTP_SIZE );
    if( !buffer ) {                                 /* error check */
      perror( "Error while allocating memory" );
      abort();
    }
  }

  memset( buffer, 0, MAX_HTTP_SIZE );
  int len;
    do {                                          /* loop, read & send file */
       len = fread( buffer, 1, MAX_HTTP_SIZE, request->file );  /* read file chunk */
      if( len < 0 ) {                             /* check for errors */
          perror( "Error while writing to client" );
      } else if( len > 0 ) {                      /* if none, send chunk */
        len = write( request->fileDes, buffer, len );
        if( len < 1 ) {                           /* check for errors */
          perror( "Error while writing to client" );
        }
        printf("Send <%d> bytes of file <%s>.\n\n",len,request->filePath );
        fflush(stdout);
      }
    } while( len == MAX_HTTP_SIZE );              /* the last chunk < 8192 */
    fclose( request->file );
    close(request->fileDes);
  
  printf("Request for file <%s> completed.\n-------------------------------------------\n",request->filePath );
  fflush(stdout);
  return 0;
}

void preprocessingRequest(Request *request){
  static char *buffer;                              /* request buffer */
  char *req = NULL;                                 /* ptr to req file */
  char *brk;                                        /* state used by strtok */
  char *tmp;                                        /* error checking ptr */
  FILE *fin;                                        /* input file handle */
  int len;                                          /* length of data read */
  if( !buffer ) {                                   /* 1st time, alloc buffer */
    buffer = malloc( MAX_HTTP_SIZE );
    if( !buffer ) {                                 /* error check */
      perror( "Error while allocating memory" );
      abort();
    }
  }

  memset( buffer, 0, MAX_HTTP_SIZE );
  if( read( request->fileDes, buffer, MAX_HTTP_SIZE ) <= 0 ) {    /* read req from client */
    perror( "Error while reading request" );
    abort();
  } 

      /* standard requests are of the form
   *   GET /foo/bar/qux.html HTTP/1.1
   * We want the second token (the file path).
   */
  tmp = strtok_r( buffer, " ", &brk );              /* parse request */
  if( tmp && !strcmp( "GET", tmp ) ) {
    req = strtok_r( NULL, " ", &brk );
  }
 
  if( !req ) {                                      /* is req valid? */
    len = sprintf( buffer, "HTTP/1.1 400 Bad request\n\n" );
    write( request->fileDes, buffer, len );                       /* if not, send err */
    close( request->fileDes );                                     /* close client connection*/
    free(request);
  } else {                                          /* if so, open file */
    req++;                                          /* skip leading / */
    fin = fopen( req, "r" );                        /* open file */
    if( !fin ) {                                    /* check if successful */
      len = sprintf( buffer, "HTTP/1.1 404 File not found\n\n" );  
      write( request->fileDes, buffer, len );                     /* if not, send err */
      close( request->fileDes );                                     /* close client connection*/
      free(request);
    } else {                                         /* if so, send file */
      struct stat st;
      stat(req, &st);
      request->filePath = (char*)check_malloc(strlen(req)+1);
      strcpy(request->filePath,req);
      request->file = fin;
      request->remainingBytes = st.st_size;
      request->fileSize = st.st_size;
      printf("File path : %s\n",request->filePath);
      fflush(stdout);
      printf("File size : %d\n",request->fileSize);
      fflush(stdout);
      if (_MLFB == 1)
        queue_push(&MLFB_Highest,request);
      else if (_SJF == 1)
        queue_push(&SJF,request);
      else 
        queue_push(&RR,request);
      printf("Request for file <%s> admitted.\n\n",req);
      fflush(stdout);
      len = sprintf( buffer, "HTTP/1.1 200 OK..\n\n" );/* send success code */
      write( request->fileDes, buffer, len );
    }
  }
}
void* worker_entry(void *args){
  int flag = 0;
  while(1){
    semaphore_wait(WORK.mutex);
    if(WORK.nodeNumber == 0){
      semaphore_signal(WORK.mutex);
      int value;
      if (_SJF)
        value = SJF_schedule();
      else if (_RR)
        value = RR_schedule(&RR,1);
      else
        value = MLFB_schedule();
      if (value == -1){
        semaphore_wait(items);
        flag = 1;
      }
    }
    else{
      semaphore_signal(WORK.mutex);
      Request *request = queue_pop(&WORK);
      preprocessingRequest(request);
      semaphore_signal(spaces);
      if (flag == 0)
        semaphore_wait(items);
      else
        flag = 0;
    }
  }
  return 0;
}

void saveToWorkQueue(int fd){
  Request *request = (Request*)malloc(sizeof(Request));
  request->fileDes = fd;
  semaphore_wait(spaces);
  queue_push(&WORK,request);
  semaphore_signal(items);
}