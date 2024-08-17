#include <stdio.h>     
#include <stdlib.h>   
#include <stdint.h>  
#include <inttypes.h>  
#include <errno.h>     // for EINTR
#include <fcntl.h>     
#include <unistd.h>    
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h> //to get ID
#include <string.h>  // For strcpy and strcat
#include <sys/mman.h>  //for mmap
#include <time.h>      //for system time
#include "common.h"
#include "common_threads.h"  //for GetTime function
//prototype
void *tree(void *arg);

typedef struct {
    uint8_t* base;        // pointing to data mapped by mmap()
    int start_block;      // index of the first block this thread will handle
    int end_block;        // Index of the last block this thread will handle 
    int root_id;           //thread ID of first thread
    int current_index;
    int max_threads;      // maximum number of threads allowed (from argument)
    uint32_t hashValue; 
} ThreadArg;


// Print out the usage of the program and exit.
void Usage(char*);
uint32_t jenkins_one_at_a_time_hash(const uint8_t* , uint64_t );

// block size
#define BSIZE 4096
#define MAX_HASH 11 //max digits for uint32_t(hash) plus null terminator

int* upperbound;  //arrays used in parsing blocks depending on thcount
int* lowerbound;
ThreadArg* args;
int rootID;

int 
main(int argc, char** argv) 
{
  int32_t fd;
  uint32_t nblocks;

  // input checking 
  if (argc != 3)
    Usage(argv[0]);

  // open input file
  fd = open(argv[1], O_RDWR);
  if (fd == -1) {
    perror("open failed");
    exit(EXIT_FAILURE);
  }
  // use fstat to get file size
  
  uint64_t file_size;
  struct stat sb;
  fstat(fd, &sb);
  file_size = sb.st_size;
  
//mapping the file to memory
 uint8_t* arr =  (uint8_t*)mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd,0);
 if (arr == MAP_FAILED) {
 perror("mmap failed");
 exit(EXIT_FAILURE);
 }   
  
  // calculate nblocks
  
  nblocks = file_size/BSIZE; 
  printf(" no. of blocks = %u \n", nblocks);
  
  
  int thcount = atoi(argv[2]);      //number of threads from user input
  printf("User Input for Number of Threads: %d\n", thcount);

  if(thcount <1){                                  //Too little threads error
      printf("Error: Invalid number of threads! Must be greater than 0.\n");
      munmap(arr,sb.st_size); //unmap
      close(fd);
      exit(EXIT_FAILURE);
  }
  if(thcount > nblocks){                           //too many threads error
      printf("Error: Too many threads for file size.\n"); //unmap
      munmap(arr,sb.st_size);
      close(fd);
      exit(EXIT_FAILURE);
  }
  
  
  
  
  int bound = nblocks/thcount;      //number used in parsing calculation
  
  
  upperbound = (int*)calloc(thcount, sizeof(int));  //allocates memory for upper bound array based on element number
  lowerbound = (int*)calloc(thcount, sizeof(int));  //allocates memory for lower bound array
  
  if (upperbound == NULL || lowerbound == NULL) {	//error checking for malloc
        printf("Error: Memory allocation failed.\n");
         free(upperbound);
         free(lowerbound);
         munmap(arr,sb.st_size); //unmap
       close(fd);
       exit(EXIT_FAILURE);
        }
  
  
  if(thcount == 1){                                  //edge case - very slow!
      upperbound[0] = 1;
      lowerbound[0] = 0;
  }

  int x = 0;
  for( ; x<thcount; x++){
      upperbound[x] =((x+1)*bound)-1;
      lowerbound[x] = bound*x;
//adjust the end block for the last thread
    if (x == thcount - 1) {
        upperbound[x] = nblocks - 1;  // last thread covers all the remaining blocks
    }
  }
  
  
 // upperbound[x-1] = (int)nblocks;
 

 args = malloc(thcount * sizeof(ThreadArg)); //// array of arguments for each thread
for (int i = 0; i < thcount; i++) {
        args[i].base = arr;
        args[i].start_block = lowerbound[i];
        args[i].end_block = upperbound[i];
        args[i].max_threads = thcount;
        args[i].current_index = i;
        printf("Thread %d - Max threads: %d\n", i, args[i].max_threads);


    }
    double start, end;
    pthread_t p1;
    uint32_t* rootHashResult = NULL;
    
start = GetTime();

Pthread_create(&p1, NULL, tree, &args[0]);


pthread_join(p1, (void**)&rootHashResult);


end = GetTime();

  printf("hash value = %u \n", *rootHashResult);
  printf("time taken = %f \n", (end - start));
  
  free(args);
  free(upperbound);  //frees up memory after bound arrays were used
  free(lowerbound);
  munmap(arr,sb.st_size); //unmap
  close(fd);
  
  return EXIT_SUCCESS;
}

uint32_t 
jenkins_one_at_a_time_hash(const uint8_t* key, uint64_t length) 
{
  uint64_t i = 0;
  uint32_t hash = 0;

  while (i != length) {
    hash += key[i++];
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  return hash;
}



void* tree(void* arg) {
    ThreadArg* tArg = (ThreadArg*)arg;
    int currentIndex = tArg->current_index;
    int maxThreads = tArg->max_threads;
    int leftIndex = 2 * currentIndex + 1;
    int rightIndex = 2 * currentIndex + 2;
    size_t startOffset = (size_t)tArg->start_block *(size_t) BSIZE;  //from what byte will we start reading
   size_t endOffset = ((size_t)tArg->end_block + 1) *(size_t) BSIZE; //from what byte will we stop reading, including the very last block
    size_t length = endOffset - startOffset; /// calculate the length of the data to hash

   printf("Entering tree function - Thread ID: %ld, Current Index: %d, Max Threads: %d\n",(long)syscall(SYS_gettid), currentIndex, maxThreads);
//printf("Thread %ld starting: blocks %d to %d\n", (long)syscall(SYS_gettid), tArg->start_block, tArg->end_block);
// printf("Thread %ld, Index %d: Start %zu, End %zu, Length %zu, Hash: %u\n",(long)syscall(SYS_gettid), currentIndex, startOffset, endOffset, length, hash);

    //debug print to check child index
    printf("Calculated Left Index: %d, Right Index: %d for Current Index: %d\n", 
           leftIndex, rightIndex, currentIndex);

    //leaf node determination based on child indices being out of bounds!
    if (leftIndex >= maxThreads && rightIndex >= maxThreads) {
        printf("Leaf Thread (id: %ld) at index %d - no children to spawn.\n",
               (long)syscall(SYS_gettid), currentIndex);
    uint32_t hash = jenkins_one_at_a_time_hash(tArg->base + startOffset, length);
       tArg->hashValue = hash; //store the hash calculated from the data block
        printf("Leaf Thread (id: %ld) at index %d - Computed Hash: %u\n", (long)syscall(SYS_gettid), currentIndex, tArg->hashValue);
       pthread_exit(&(tArg->hashValue)); //return the address of the hash value
    }

    pthread_t lChild, rChild;
    int lCreated = -1, rCreated = -1;
    uint32_t *lResult = NULL, *rResult = NULL; //pointers to store the hash value result we recieve when children do "pthread_exit(&(tArg->hashValue))"
    
    //for string concatenate 
    char hashStr[MAX_HASH]; //buffer to hold the current thread's hash as a string
   char resultStr[3 * MAX_HASH]; //make enough space to store both hash of itself, and children

 
 
    //create left child if within bounds
    if (leftIndex < maxThreads) {
       printf("Preparing to create left child at index %d for thread %d\n", leftIndex, currentIndex);
   // printf("Passing args[leftIndex]: base=%p, start_block=%d, end_block=%d, current_index=%d\n",
           //args[leftIndex].base, args[leftIndex].start_block, args[leftIndex].end_block, args[leftIndex].current_index);
        lCreated = pthread_create(&lChild, NULL, tree, &args[leftIndex]);
        if (lCreated != 0) {
            perror("Failed to create left child thread");
        }
    }

    // Create right child if within bounds
    if (rightIndex < maxThreads) {
        printf("Creating right child at index %d\n", rightIndex);
        rCreated = pthread_create(&rChild, NULL, tree, &args[rightIndex]);
        if (rCreated != 0) {
            perror("Failed to create right child thread");
        }
    }
//hash is computed while children are working
uint32_t hash = jenkins_one_at_a_time_hash(tArg->base + startOffset, length);
 sprintf(hashStr, "%u", hash); 
 strcpy(resultStr, hashStr); 
  // Join child threads after computation is done
    if (lCreated == 0) {
        pthread_join(lChild, (void**)&lResult);
    }
    if (rCreated == 0) {
        pthread_join(rChild, (void**)&rResult);  
    }
    
     if (lResult) {
            char lHashStr[MAX_HASH];
            sprintf(lHashStr, "%u", *lResult);
            strcat(resultStr, lHashStr);
        }
     if (rResult) {
            char rHashStr[MAX_HASH];
            sprintf(rHashStr, "%u", *rResult);
            strcat(resultStr, rHashStr);
        }
    
    
 uint32_t finalHash = jenkins_one_at_a_time_hash((uint8_t*)resultStr, strlen(resultStr));
tArg->hashValue = finalHash; //store final hash in struct for later use
pthread_exit(&(tArg->hashValue)); // Pass the final hash value up to the parent

    

    return NULL;
}






void 
Usage(char* s) 
{
  fprintf(stderr, "Usage: %s filename num_threads \n", s);
  exit(EXIT_FAILURE);
}
