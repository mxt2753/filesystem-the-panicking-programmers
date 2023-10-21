#ifndef _MFS_H_
#define _MFS_H_

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

#define WHITESPACE " \t\n" // We want to split our command line up into tokens
                           // so we need to define what delimits our tokens.
                           // In this case  white space
                           // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255 // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5 // Mav File System only supports ten arguments

#define NUM_BLOCKS 65536 // File System supports this number of blocks

#define BLOCK_SIZE 1024 // The size of each block

#define IMAGE_FILE_SIZE 67108864 // Defines expected size for disk image

#define BLOCKS_PER_FILE 1024    

#define NUM_FILES 256 // Max number of files

#define FIRST_DATA_BLOCK 790

#define MAX_FILE_SIZE 1048576 // Max file size in bytes
 
#define HIDDEN 0x1

#define READONLY 0x2


// INODE
struct inode
{
    int32_t blocks[BLOCKS_PER_FILE];
    short in_use;
    uint8_t attribute; // holds hidden and read-only attributes
    uint32_t file_size;
};

// DIRECTORY
struct directoryEntry
{
    char filename[64]; // Filename
    short in_use; // Variable used to check if file has been deleted
    int32_t inode; // holds index for first inode
};

int32_t findFreeBlock();
int32_t findFreeInode();
int32_t findFreeInodeBlock(int32_t inode);
void init();
uint32_t df();
void createFS(char *filename);
void savefs();
void openfs(char *filename);
void closefs();
void insert(char *filename);
void encrypt_block(uint8_t *str, char key, uint32_t len);
void encrypt(char *filename, char cypher);
void retrieve(char *FName, char *NFName);
void delete(char *filename);
void undelete(char *filename);
void read_file(char *filename, int start, int len);
void attrib(char *typeAttrib, char *filename);
void print_bin(uint8_t value);
void list(char *token, char * token2);
void free_tokens(char *token[], int token_count);


#endif