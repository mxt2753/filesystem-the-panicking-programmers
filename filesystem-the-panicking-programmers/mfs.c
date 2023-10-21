#include "mfs.h"

// UINT8_T BLOCKS
uint8_t data[NUM_BLOCKS][BLOCK_SIZE];
uint8_t *free_blocks; // 65536 bytes = 64 blocks
uint8_t *free_inodes; // 256 * 1

struct directoryEntry *directory;

struct inode *inodes;

FILE *fp;
char image_name[64];
uint8_t image_open = 0;

// FUNCTIONS
int32_t findFreeBlock()
{
  int i;
  for (i = 0; i < NUM_BLOCKS; i++)
  {
    if (free_blocks[i])
    {
      return i + 1065; // PROF CHANGED IT TO 1065
    }
  }
  return -1;
}

int32_t findFreeInode()
{
  int i;
  for (i = 0; i < NUM_FILES; i++)
  {
    if (free_inodes[i])
    {
      return i;
    }
  }
  return -1;
}

int32_t findFreeInodeBlock(int32_t inode)
{
  int i;
  for (i = 0; i < BLOCKS_PER_FILE; i++)
  {
    if (inodes[inode].blocks[i] == -1)
    {
      return i;
    }
  }
  return -1;
}

void init()
{
  directory = (struct directoryEntry *)&data[0][0];
  inodes = (struct inode *)&data[20][0];
  free_blocks = (uint8_t *)&data[1000][0];
  free_inodes = (uint8_t *)&data[19][0];

  for (int i = 0; i < NUM_FILES; i++)
  {
    directory[i].in_use = 0;
    directory[i].inode = -1;
    free_inodes[i] = 1;

    memset(directory[i].filename, 0, 64);

    int j;
    for (j = 0; j < NUM_BLOCKS; j++)
    {
      inodes[i].blocks[j] = -1;
    }
    inodes[i].in_use = 0;
    inodes[i].attribute = 0;
    inodes[i].file_size = 0;
  }

  int j;
  for (j = 0; j < NUM_BLOCKS; j++)
  {
    free_blocks[j] = 1;
  }
}

uint32_t df()
{
  int j = 0;
  int count = 0;

  for (j = FIRST_DATA_BLOCK; j < NUM_BLOCKS; j++)
  {
    if (free_blocks[j] == 1)
    {
      count++;
    }
  }

  return count * BLOCK_SIZE;
}

void createfs(char *filename)
{
  if (image_open == 1)
  {
    closefs();
  }

  fp = fopen(filename, "w");

  strncpy(image_name, filename, strlen(filename) + 1);

  memset(data, 0, NUM_BLOCKS * BLOCK_SIZE);

  init();

  image_open = 1;

  // Save empty struct data into file so it's recognized as a valid image file
  savefs();
}

void savefs()
{
  if (image_open == 0)
  {
    printf("ERROR: Disk image is not open.\n");
    return;
  }

  // close file and open in write mode
  fclose(fp);

  fp = fopen(image_name, "w");

  fwrite(&data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);
}

void openfs(char *filename)
{

  // verify the file exits
  struct stat buf;
  int ret = stat(filename, &buf);

  if (ret == -1)
  {
    printf("ERROR: File doesn't exist.\n");
    return;
  }
  // verify that the file is the right size
  if (buf.st_size != IMAGE_FILE_SIZE)
  {
    printf("ERROR: File is not a valid image file.\n");
    return;
  }

  // If another file is open close it before opening new file
  if (image_open == 1)
  {
    closefs();
  }

  //assigns fp
  fp = fopen(filename, "r");

  strncpy(image_name, filename, strlen(filename));

  fread(&data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);

  image_open = 1;
}

void closefs()
{
  if (image_open == 0)
  {
    printf("Close: File not open.\n");
    return;
  }

  fclose(fp);

  image_open = 0;

  memset(image_name, 0, 64);
}

void insert(char *filename)
{
  // verify the filename isnt NULL

  if (filename == NULL)
  {
    printf("ERROR: Filename is NULL\n");
    return;
  }

  // verify the file exits
  struct stat buf;
  int ret = stat(filename, &buf);

  if (ret == -1)
  {
    printf("INSERT ERROR: File doesn't exist.\n");
    return;
  }
  // verify that the file isnt too big
  if (buf.st_size > MAX_FILE_SIZE)
  {
    printf("INSERT ERROR: File is too large.\n");
    return;
  }

  // verify that there is enough space
  if (buf.st_size > df())
  {
    printf("INSERT ERROR: Not enough disk space.\n");
    return;
  }

  // find an empty directory entry
  int i = 0;
  int directory_entry = -1;
  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use == 0)
    {
      directory_entry = i;
      break;
    }
  }
  if (directory_entry == -1)
  {
    printf("ERROR: Could not find a free directory entry\n");
    return;
  }

  // find free inodes and place the file
  FILE *ifp = fopen(filename, "r");
  printf("Reading %d bytes from %s\n", (int)buf.st_size, filename);

  // Save off the size of the input file since we'll use it in a couple of places and
  // also initialize our index variables to zero.
  int32_t copy_size = buf.st_size;

  // We want to copy and write in chunks of BLOCK_SIZE. So to do this
  // we are going to use fseek to move along our file stream in chunks of BLOCK_SIZE.
  // We will copy bytes, increment our file pointer by BLOCK_SIZE and repeat.
  int32_t offset = 0;

  // We are going to copy and store our file in BLOCK_SIZE chunks instead of one big
  // memory pool. Why? We are simulating the way the file system stores file data in
  // blocks of space on the disk. block_index will keep us pointing to the area of
  // the area that we will read from or write to.
  int32_t block_index = -1;

  // find a free inode
  int32_t inode_index = findFreeInode();
  if (inode_index == -1)
  {
    printf("ERROR: Can not find a free inode.\n");
    return;
  }

  // place the file info in the directory
  directory[directory_entry].in_use = 1;
  directory[directory_entry].inode = inode_index;
  strncpy(directory[directory_entry].filename, filename, strlen(filename));

  inodes[inode_index].file_size = buf.st_size;
  inodes[inode_index].in_use = 1;
  free_inodes[inode_index] = 0;

  // copy_size is initialized to the size of the input file so each loop iteration we
  // will copy BLOCK_SIZE bytes from the file then reduce our copy_size counter by
  // BLOCK_SIZE number of bytes. When copy_size is less than or equal to zero we know
  // we have copied all the data from the input file.
  while (copy_size > 0)
  {
    fseek(ifp, offset, SEEK_SET);

    // Read BLOCK_SIZE number of bytes from the input file and store them in our
    // data array.

    // find a free block
    block_index = findFreeBlock();

    if (block_index == -1)
    {
      printf("ERROR: Can not find a free block.\n");
      return;
    }

    int32_t bytes = fread(data[block_index], BLOCK_SIZE, 1, ifp);

    // save the block in the inode
    int32_t inode_block = findFreeInodeBlock(inode_index);
    inodes[inode_index].blocks[inode_block] = block_index;

    // If bytes == 0 and we haven't reached the end of the file then something is
    // wrong. If 0 is returned and we also have the EOF flag set then that is OK.
    // It means we've reached the end of our input file.
    if (bytes == 0 && !feof(ifp))
    {
      printf("An error occured reading from the input file.\n");
      return;
    }

    // Clear the EOF file flag.
    clearerr(ifp);

    // Reduce copy_size by the BLOCK_SIZE bytes.
    copy_size -= BLOCK_SIZE;

    // Increase the offset into our input file by BLOCK_SIZE.  This will allow
    // the fseek at the top of the loop to position us to the correct spot.
    offset += BLOCK_SIZE;

    // Increment the index into the block array
    // DO NOT just increment block index in your file system
    free_blocks[block_index - 1065] = 0;
    free_blocks[block_index] = 0;
  }

  // We are done copying from the input file so close it out.
  fclose(ifp);
}

void encrypt_block(uint8_t *str, char key, uint32_t len)
{
  int i;
  for (i = 0; i < len; i++)
  {
    str[i] ^= key;
  }
}

void encrypt(char *filename, char cypher)
{
  int file_index = -1;

  int i;
  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use && !strcmp(filename, directory[i].filename))
    {
      file_index = i;
      break;
    }
  }

  if (file_index == -1)
  {
    printf("ERROR: File not found in file system\n");
    return;
  }

  struct inode *file_inode = &inodes[directory[file_index].inode];

  if (!file_inode->in_use)
  {
    printf("ERROR: inode is not in use\n");
    return;
  }

  i = 0;
  uint32_t encrypt_size = file_inode->file_size;
  int block_index = file_inode->blocks[0];

  while (block_index != -1 && i < BLOCKS_PER_FILE && encrypt_size > 0)
  {
    uint32_t block_len;

    if (encrypt_size < BLOCK_SIZE)
    {
      block_len = encrypt_size;
    }
    else
    {
      block_len = BLOCK_SIZE;
    }

    encrypt_block(&data[block_index][0], cypher, block_len);

    encrypt_size -= block_len;
    i++;
    block_index = file_inode->blocks[i];
  }
}

void retrieve(char *FName, char *NFName)
{
  if (FName == NULL)
  {
    printf("ERROR: Filename is not here?");
    return;
  }
  int FFound = 0;
  int DirEntry = -1;
  for (int i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use && strcmp(directory[i].filename, FName) == 0)
    {
      FFound = 1;
      DirEntry = i;
      break;
    }
  }
  if (!FFound)
  {
    printf("ERROR: File not found\n");
    return;
  }
  int32_t IIdx = directory[DirEntry].inode;
  if (NFName == NULL)
  {
    NFName = FName;
  }
  FILE *OutPFile = fopen(NFName, "w");
  if (OutPFile == NULL)
  {
    printf("ERROR:can't output file creation");
    return;
  }
  int32_t CSize = inodes[IIdx].file_size;
  int32_t OffS = 0;
  while (CSize > 0)
  {
    int32_t BIdx = inodes[IIdx].blocks[OffS / BLOCK_SIZE];
    int32_t BTW = (CSize > BLOCK_SIZE) ? BLOCK_SIZE : CSize;
    fwrite(data[BIdx], BTW, 1, OutPFile);

    CSize -= BLOCK_SIZE;
    OffS += BLOCK_SIZE;
  }
  fclose(OutPFile);
}

void delete(char *filename)
{
  // verify the filename isnt NULL
  if (filename == NULL)
  {
    printf("ERROR: Filename is NULL. Cannot delete.\n");
    return;
  }

  // FIND DIRECTORY IT IS UNDER
  int directory_entry = -1;
  for (int i = 0; i < NUM_FILES; i++)
  {
    if ((directory[i].in_use != 0) && (strcmp(directory[i].filename, filename) == 0))
    {
      // finds directory in use
      directory_entry = i;

      //checks if the read attribute is set
      if (inodes[i].attribute & READONLY)
      {
        printf("This file is marked read-only. Cannot be deleted.\n");
      }
      else
      {
        // DELETE PROCESS
        directory[i].in_use = false;           // sets inuse directory to false
        inodes[directory[i].inode].in_use = 0; // sets inode to free
        free_inodes[directory[i].inode] = 1;

        // FREE INODES
        int blockNum = 0;
        for (int j = 0; j < BLOCKS_PER_FILE; j++)
        {
          // set blockNum to inodes's directory[i] block
          blockNum = inodes[directory[i].inode].blocks[j];
          if (blockNum != -1) // if block is in use
          {
            free_blocks[blockNum - 1065] = 1; // set free_blocks blockNum to 1
            free_blocks[blockNum] = 1;
          }
        }
        break;
      }
    }
  }
}

void undelete(char *filename)
{
  // verify the filename isnt NULL
  if (filename == NULL)
  {
    printf("UNDELETE: Can not find the file.\n");
    return;
  }

  // FIND DIRECTORY IT IS UNDER
  int directory_entry = -1;
  for (int i = 0; i < NUM_FILES; i++)
  {
    if ((directory[i].in_use == 0) && (strcmp(directory[i].filename, filename) == 0))
    {
      // finds directory in use
      directory_entry = i;

      // UNDELETE PROCESS
      directory[i].in_use = true;            // sets inuse directory to true
      inodes[directory[i].inode].in_use = 1; // sets inode to in_use
      free_inodes[directory[i].inode] = 0;   // sets free_inodes to 0

      // FREE INODES
      int blockNum = 0;
      for (int j = 0; j < BLOCKS_PER_FILE; j++)
      {
        // set blockNum to inodes's directory block
        blockNum = inodes[directory[i].inode].blocks[j];
        if (blockNum != -1) // if block is in use
        {
          free_blocks[blockNum - 1065] = 0; // set free_blocks blockNum to 0
          free_blocks[blockNum] = 0;
        }
      }
      break;
    }
  }
}

void read_file(char *filename, int start, int len)
{
  int i;
  int file_index = -1;
  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use && strcmp(filename, directory[i].filename) == 0)
    {
      file_index = i;
      break;
    }
  }
  if (file_index == -1)
  {
    printf("ERROR: file not found in disk image\n");
    return;
  }

  struct inode *file_inode = &inodes[directory[i].inode];

  int block_index = file_inode->blocks[(start + 1) / BLOCK_SIZE];

  for (i = start; i < (len + start) && i < file_inode->file_size; i++)
  {
    printf("%x ", data[block_index][i % BLOCK_SIZE]);
    block_index = file_inode->blocks[(i + 1) / BLOCK_SIZE];
  }
  printf("\n");
}

void attrib(char *typeAttrib, char *filename)
{
  // FIND DIRECTORY IT IS IN
  int directory_entry = -1;
  for (int i = 0; i < NUM_FILES; i++)
  {
    if ((directory[i].in_use != 0) && (strcmp(directory[i].filename, filename) == 0))
    {
      // finds directory in use
      directory_entry = i;

      // USE ATTRIBUTES
      if (strcmp("+h", typeAttrib) == 0)
      {
        inodes[directory[i].inode].attribute |= HIDDEN;
      }
      else if (strcmp("-h", typeAttrib) == 0)
      {
        inodes[directory[i].inode].attribute &= ~HIDDEN;
      }
      else if (strcmp("+r", typeAttrib) == 0)
      {
        inodes[directory[i].inode].attribute |= READONLY;
      }
      else if (strcmp("-r", typeAttrib) == 0)
      {
        inodes[directory[i].inode].attribute &= ~READONLY;
      }
      else
      {
        printf("ATTRIB: File not found.\n");
      }
    }
  }
}

void print_bin(uint8_t value)
{
  int bin = 0;
  int i;

  // using switch case because there's very limited possible values
  switch(value)
  {
    case 0:
      printf("00000000\n");
      break;
    case 1:
      printf("00000001\n");
      break;
    case 2:
      printf("00000010\n");
      break;
    case 3:
      printf("00000011\n");
      break;
    default:
      break;
  }
}

void list(char *token, char * token2)
{
  // flags for parameters
  int hidden = 0;
  int attribute8Bit = 0;

  if(token != NULL)
  {
    if (strcmp("-h", token) == 0)
    {
      // triggers the flag for hidden that will be used later
      hidden = 1;
    }
    else if (strcmp("-a", token) == 0)
    {
      // triggers the flag for attribute that will be used later
      attribute8Bit = 1;
    }
  }
  if(token2 != NULL)
  {
    if (strcmp("-h", token2) == 0)
    {
      // triggers the flag for hidden that will be used later
      hidden = 1;
    }
    else if (strcmp("-a", token2) == 0)
    {
      // triggers the flag for attribute that will be used later
      attribute8Bit = 1;
    }
  }

  int i;
  int not_found = 1;

  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use)
    {
      not_found = 0;
      char filename[65];
      memset(filename, 0, 65);
      strncpy(filename, directory[i].filename, strlen(directory[i].filename));

      // if it is not hidden print out and does not have '-a'
      if ((!(inodes[i].attribute & HIDDEN)) && (attribute8Bit == 0))
      {
        printf("%s\n", filename);
      }
      // if the hidden flag is triggered and '-a' is not triggered
      else if ((inodes[i].attribute & HIDDEN) && (hidden == 1) && (attribute8Bit == 0))
      {
        printf("%s\n", filename);
      }
      // if the attribute is triggered (8-bit) and not hidden
      else if ((attribute8Bit == 1) && !(inodes[i].attribute & HIDDEN))
      {
        printf("%s ", filename);
        print_bin(inodes[directory[i].inode].attribute);
      }
      // if both flags trigger and hidden
      else if ((attribute8Bit == 1) && (inodes[i].attribute & HIDDEN) && (hidden == 1))
      {
        printf("%s ", filename);
        print_bin(inodes[directory[i].inode].attribute);
      }
    }
  }
  if (not_found)
  {
    printf("LIST: No files found.\n");
  }
}

void free_tokens(char *token[], int token_count)
{
  int i;
  for (i = 0; i < token_count; i++)
  {
    if (token[i] != NULL)
    {
      free(token[i]);
      token[i] = NULL;
    }
  }
}

// MAIN
int main()
{
  char *command_string = (char *)malloc(MAX_COMMAND_SIZE);
  init();
  fp = NULL;
  while (1)
  {
    // Print out the msh prompt
    printf("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while (!fgets(command_string, MAX_COMMAND_SIZE, stdin));

    /* Parse input */
    char **token = calloc(MAX_NUM_ARGUMENTS, sizeof(char *));

    for (int i = 0; i < MAX_NUM_ARGUMENTS; i++)
    {
      token[i] = NULL;
    }

    int token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *argument_ptr = NULL;

    char *working_string = strdup(command_string);

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *head_ptr = working_string;

    // Tokenize the input strings with whitespace used as the delimiter
    while (((argument_ptr = strsep(&working_string, WHITESPACE)) != NULL) &&
           (token_count < MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup(argument_ptr, MAX_COMMAND_SIZE);
      if (strlen(token[token_count]) == 0)
      {
        free(token[token_count]);
        token[token_count] = NULL;
      }
      token_count++;
    }

    // Checks for blank input before proceding to prevent segmentation faults
    if (token[0] == NULL);
    // QUIT
    else if (!strcmp(token[0], "quit"))
    {
      // Cleanup allocatec memory and exit program
      if (image_open == 1)
      {
        closefs();
      }
      free_tokens(token, token_count);
      free(token);
      free(head_ptr);
      free(command_string);
      exit(EXIT_SUCCESS);
    }
    // CREATEFS
    else if (strcmp("createfs", token[0]) == 0)
    {
      if (token[1] == NULL)
      {
        printf("CREATEFS: Filename not provided.\n");
      }
      else
      {
        createfs(token[1]);
      }
    }
    // SAVEFS
    else if (strcmp("savefs", token[0]) == 0)
    {
      savefs();
    }
    // CLOSE
    else if (strcmp("close", token[0]) == 0)
    {
      closefs();
    }
    // OPEN
    else if (strcmp("open", token[0]) == 0)
    {
      if (token[1] == NULL)
      {
        printf("OPEN ERROR: There is no filename specified.\n");
      }
      else
      {
        openfs(token[1]);
      }
    }
    // LIST
    else if (strcmp("list", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
      }
      else
      {
        list(token[1], token[2]);
      }
    }
    // DF
    else if (strcmp("df", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
      }
      else
      {
        printf("%d bytes free\n", df());
      }
    }
    // INSERT
    else if (strcmp("insert", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
      }
      else if (token[1] == NULL)
      {
        printf("ERROR: no filename specified\n");
      }
      else
      {
        //checks filename length
        if(strlen(token[1]) <= 64)
        {
          insert(token[1]);
        }
        else
        {
          printf("INSERT ERROR: Filename is too long.\n");
        }
      }
    }
    // ENCRYPT AND DECRYPT
    else if (strcmp("encrypt", token[0]) == 0 || strcmp("decrypt", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
      }
      else if (token[1] == NULL)
      {
        printf("ERROR: no filename specified\n");
      }
      else if (token[2] == NULL)
      {
        printf("ERROR: no cypher specified\n");
      }
      else if (strlen(token[2]) > 1)
      {
        printf("ERROR: cypher is not valid, cypher should include a single 1-byte value only\n");
      }
      else
      {
        encrypt(token[1], token[2][0]);
      }
    }
    // RETRIEVE
    else if (strcmp("retrieve", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
      }

      else if (token[1] == NULL)
      {
        printf("ERROR: no filename specified\n");
      }
      else
      {
        retrieve(token[1], token[2]);
      }
    }
    // DELETE
    else if (strcmp("delete", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
      }
      else if (token[1] == NULL)
      {
        printf("ERROR: no filename specified.\n");
      }
      else
      {
        delete (token[1]);
      }
    }
    // UNDELETE
    else if (strcmp("undelete", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
      }
      else if (token[1] == NULL)
      {
        printf("ERROR: no filename specified.\n");
      }
      else
      {
        undelete(token[1]);
      }
    }
    // ATTRIB
    else if (strcmp("attrib", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("ATTRIB ERROR: Disk image is not opened.\n");
      }
      else if (token[1] == NULL)
      {
        printf("ATTRIB ERROR: attribute not specified.\n");
      }
      else if (token[2] == NULL)
      {
        printf("ATTRIB ERROR: no filename specified.\n");
      }
      else if (strcmp("+h", token[1]) == 0)
      {
        attrib(token[1], token[2]);
      }
      else if (strcmp("-h", token[1]) == 0)
      {
        attrib(token[1], token[2]);
      }
      else if (strcmp("+r", token[1]) == 0)
      {
        attrib(token[1], token[2]);
      }
      else if (strcmp("-r", token[1]) == 0)
      {
        attrib(token[1], token[2]);
      }
      else
      {
        printf("Something has gone wrong. Cannot set attribute\n");
      }
    }
    // READ
    else if (strcmp("read", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("READ ERROR: Disk image is not opened.\n");
      }
      else if (token[1] == NULL)
      {
        printf("READ ERROR: Filename not specified\n");
      }
      else if (token[2] == NULL)
      {
        printf("READ ERROR: Starting byte not specified\n");
      }
      else if (token[3] == NULL)
      {
        printf("READ ERROR: Number of bytes not specified\n");
      }
      else
      {
        read_file(token[1], atoi(token[2]), atoi(token[3]));
      }
    }
    else // COMMAND NOT FOUND
    {
      printf("ERROR: Command not found.\n");
    }

    free_tokens(token, token_count);
    free(token);
    free(head_ptr);
  }
}

