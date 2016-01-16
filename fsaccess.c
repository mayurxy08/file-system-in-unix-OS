/*Authors:
Dheeraj Urs 	(dxu150230@utdallas.edu)
Mayur Talole 	(mnt150230@utdallas.edu)
*/

/* Unix File system Modified*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>



#define BLOCK_SIZE 512
#define ISIZE 32
#define inode_alloc 0100000        //Flag value to allocate an inode
#define pfile 000000               //To define file as a plain file
#define lfile 010000               //To define file as a large file
#define directory 040000           //To define file as a Directory
#define array_max_chain 256
//#define null_size 512


/*************** super block structure**********************/
typedef struct {
unsigned short isize;
unsigned short fsize;
unsigned short nfree;
unsigned short free[100];
unsigned short ninode;
unsigned short inode[100];
char flock;
char ilock;
char fmod;
unsigned short time[2];
} superb;

/************** directory contents***************************/
typedef struct 
{
        unsigned short inode;
        char filename[13];
}dir;
			
/****************inode structure ************************/
typedef struct {
unsigned short flags;
char nlinks;
char uid;
char gid;
char size0;
unsigned short size1;
unsigned short addr[8];				
unsigned short actime[2];
unsigned short modtime[2];
} filesys_inode;

filesys_inode initial_inode;
superb super;




int fd ;		//file descriptor 
int rootfd;
const char *rootname;
unsigned short arry_chain[array_max_chain];		

//functions used:
unsigned short get_free_data_block();

dir newdir;			
dir dir1;
/*************** functions used *****************/
int fs_init(char* path, unsigned short parse_blocks,unsigned short total_inodes);
void root_dir();
void blk_read(unsigned short *target, unsigned short block_entry_num);
void block_to_array(unsigned short *target, unsigned short block_entry_num);
void copy_into_inode(filesys_inode current_inode, unsigned int new_inode);
void freeblock(unsigned short block);
void blocks_chains(unsigned short parse_blocks);
unsigned short allocateinode();
void cpin(const char *pathname1 , const char *pathname2);
void out_plainfile(const char *pathname1 , const char *pathname2 , int blocks_allocated);
void out_largefile(const char *pathname1 , const char *pathname2 , int blocks_allocated);
void cpout(const char *pathname1 , const char *pathname2);
void largefile(const char *pathname1 , const char *pathname2 , int blocks_allocated);
void plainfile(const char *pathname1 , const char *pathname2 , int blocks_allocated);
void makedir(const char *pathname);
void update_rootdir(const char *pathname , unsigned short in_number);
void show_all_files();

/**** file system initilization function************/
int fs_init(char* path, unsigned short parse_blocks,unsigned short total_inodes)
{
printf("\n filesystem intialization started \n");
char buffer[BLOCK_SIZE];
int no_of_bytes;
if(((total_inodes*32)%512) == 0)
		super.isize = ((total_inodes*32)/512);
	else
		super.isize = ((total_inodes*32)/512)+1;	

super.fsize = parse_blocks;

unsigned short i = 0;

if((fd = open(path,O_RDWR|O_CREAT,0600))== -1)          
    {
        printf("\n file opening error [%s]\n",strerror(errno));
        return 1;
    }

for (i = 0; i<100; i++)
	super.free[i] =  0;			

super.nfree = 0;


super.ninode = 100;
for (i=0; i < 100; i++)
	super.inode[i] = i;		


super.flock = 'f'; 					
super.ilock = 'i';					
super.fmod = 'f';
super.time[0] = 0000;
super.time[1] = 1970;
lseek(fd,BLOCK_SIZE,SEEK_SET);

lseek(fd,0,SEEK_SET);
write(fd, &super, 512);

 if((no_of_bytes =write(fd,&super,BLOCK_SIZE)) < BLOCK_SIZE)
 {
 printf("\nERROR : error in writing the super block\n");
 return 0;
 }

for (i=0; i<BLOCK_SIZE; i++)  
	buffer[i] = 0;
lseek(fd,1*512,SEEK_SET);
for (i=0; i < super.isize; i++)
	write(fd,buffer,BLOCK_SIZE);

// calling chaining data blocks procedure
blocks_chains(parse_blocks);	

for (i=0; i<100; i++) 
{

super.free[super.nfree] = i+2+super.isize; //get a free block
++super.nfree;

}

root_dir();

return 1;
}

//function to read integer array from the required block
void blk_read(unsigned short *target, unsigned short block_entry_num)
{
int flag=0;
        if (block_entry_num > super.isize + super.fsize )
              	flag = 1;
		
	else{			
        lseek(fd,block_entry_num*BLOCK_SIZE,SEEK_SET);
        read(fd, target, BLOCK_SIZE);
	    }
}


//function to write integer array to the required block
void block_to_array(unsigned short *target, unsigned short block_entry_num)// writing array to block
{
int flag1, flag2;
		int no_of_bytes;

        if (block_entry_num > super.isize + super.fsize )
              
		flag1 = 1	;		
	else{
        
	lseek(fd,block_entry_num*BLOCK_SIZE,0);
        no_of_bytes=write(fd, target, BLOCK_SIZE);

	if((no_of_bytes) < BLOCK_SIZE)
		flag2=1;		
	    }
if (flag2 == 1)
{
printf("problem with block number %d",block_entry_num);
}


}

//
// Data blocks chaining procedure
void blocks_chains(unsigned short parse_blocks)
{
unsigned short emptybuffer[256];   
unsigned short linked_count;
unsigned short partitions_blks = parse_blocks/100;			
unsigned short blks_not_in_partitions = parse_blocks%100;		
unsigned short index = 0;
int i=0;
for (index=0; index <= 255; index++)
{	emptybuffer[index] = 0;				
	arry_chain[index] = 0;				
}
	
//chaining for chunks of blocks 100 blocks at a time
for (linked_count=0; linked_count < partitions_blks; linked_count++)
{
	arry_chain[0] = 100;
	
	for (i=0;i<100;i++)
	{
		if((linked_count == (partitions_blks - 1)) && (blks_not_in_partitions == 0)) 
		{
			if ((blks_not_in_partitions == 0) && (i==0))	
			{
				if ((linked_count == (partitions_blks - 1)) && (i==0))			
				{
					arry_chain[i+1] = 0;
					//continue;
				}
			}

		}

		arry_chain[i+1] = i+(100*(linked_count+1))+(super.isize + 2 );




}	
	block_to_array(arry_chain, 2+super.isize+100*linked_count);
	
	for (i=1; i<=100;i++)
		block_to_array(emptybuffer, 2+super.isize+i+ 100*linked_count);
}

//chaining for remaining blocks
arry_chain[0] = blks_not_in_partitions;
arry_chain[1] = 0;
for (i=1;i<=blks_not_in_partitions;i++)
		arry_chain[i+1] = 2+super.isize+i+(100*linked_count);

block_to_array(arry_chain, 2+super.isize+(100*linked_count));

for (i=1; i<=blks_not_in_partitions;i++)
		block_to_array(arry_chain, 2+super.isize+1+i+(100*linked_count));
}
	







//function to write to an inode given the inode number	
void copy_into_inode(filesys_inode current_inode, unsigned int new_inode)
{
int no_of_bytes;
lseek(fd,2*BLOCK_SIZE + new_inode*ISIZE,0);
no_of_bytes=write(fd,&current_inode,ISIZE);

if((no_of_bytes) < ISIZE)
	printf("\n Error in inode number : %d\n", new_inode);		
}

//function to create root directory and its corresponding inode.
void root_dir()
{
rootname = "root";
rootfd = creat(rootname, 0600);
rootfd = open(rootname , O_RDWR | O_APPEND);
unsigned int i = 0;
unsigned short no_of_bytes;
unsigned short datablock = get_free_data_block();	
for (i=0;i<14;i++)
	newdir.filename[i] = 0; 
		
newdir.filename[0] = '.';			//root directory's file name is .
newdir.filename[1] = '\0';
newdir.inode = 1;					// root directory's inode number is 1.

initial_inode.flags = inode_alloc | directory | 000077;   		// flag for root directory 
initial_inode.nlinks = 2;
initial_inode.uid = '0';
initial_inode.gid = '0';
initial_inode.size0 = '0';
initial_inode.size1 = ISIZE;
initial_inode.addr[0] = datablock;
for (i=1;i<8;i++)
	initial_inode.addr[i] = 0;
		
initial_inode.actime[0] = 0;
initial_inode.modtime[0] = 0;
initial_inode.modtime[1] = 0;

copy_into_inode(initial_inode, 0); 		
lseek(rootfd , 512 , SEEK_SET);
write(rootfd , &initial_inode , 32);
lseek(rootfd, datablock*BLOCK_SIZE, SEEK_SET);

	//filling 1st entry with .
no_of_bytes = write(rootfd, &newdir, 16);
//dir.inode = 0;
if((no_of_bytes) < 16)
	printf("\n Error in writing root directory \n ");
   
	newdir.filename[0] = '.';
	newdir.filename[1] = '.';
	newdir.filename[2] = '\0';
	// filling with .. in next entry(16 bytes) in data block.

no_of_bytes = write(rootfd, &newdir, 16);	
if((no_of_bytes) < 16)
	printf("\n Error in writing root directory\n ");
   close(rootfd);
}
	
// //free data blocks and initialize free array
// void freeblock(unsigned short block)            // not used yet
// {
// super.free[super.nfree] = block;
// ++super.nfree;
// }

//function to get a free data block. Also decrements nfree for each pass
unsigned short get_free_data_block()
{
unsigned short block;

super.nfree--;
block = super.free[super.nfree];
super.free[super.nfree] = 0;  

if (super.nfree == 0)
{
int n=0;
		blk_read(arry_chain, block);
		super.nfree = arry_chain[0];  
		for(n=0; n<100; n++)
			super.free[n] = arry_chain[n+1]; 
}
return block;
}

/*************** Function to allocate inode****************/
unsigned short allocateinode()
{
unsigned short inumber;
unsigned int i = 0;
super.ninode--;
inumber = super.inode[super.ninode];
return inumber;
}

/**************** Main function ********************/
int main(int argc, char *argv[])
{
char c;

printf("\n Clearing screen \n");
//execve("/usr/bin/clear");
system("clear");
		int i=0;
	char *tmp = (char *)malloc(sizeof(char) * 200);

	char *cmd1 = (char *)malloc(sizeof(char) * 200);
signal(SIGINT, SIG_IGN);

	printf("\n\nModified_Unix_File_system_V6 ");
	fflush(stdout);

int filesys_count = 0;

char *my_argv, cmd[256];
unsigned short n = 0, j , k;
char buffer1[BLOCK_SIZE];
//int i =0;
unsigned short no_of_bytes;
char *name_direct;


char *cpoutext;
char *cpoutsource;
unsigned int blk_no =0, inode_no=0;
char *cpinext;
char *cpinsource;
char *fs_path;
char *arg1, *arg2, *num3, *num4;

printf("\n __@Authors: Mayur Talole, Dheeraj Urs.\n");
printf("\n>>MTdhee$");

while(1)
{
scanf(" %[^\n]s", cmd);
my_argv = strtok(cmd," ");

if(strcmp(my_argv, "initfs")==0)
{

fs_path = strtok(NULL, " ");
arg1 = strtok(NULL, " ");
arg2 = strtok(NULL, " ");

	if(access(fs_path, X_OK) != -1)
	{
	 //if((fd = open(fs_path,O_RDWR,0600))== -1)
   	//	{
     	// printf("\n file system exists but error in opening");         
	//	return 1;
    //		}
	printf("filesystem already exists. \n");
	printf("same file system will be used\n");
	filesys_count=1;
	}
	else
	{
	if (!arg1 || !arg2)
	  printf(" insufficient arguments to proceed\n");
		else
		{
		blk_no = atoi(arg1);
		inode_no = atoi(arg2);
		
		if(fs_init(fs_path,blk_no, inode_no))
		{
		printf("file system initialized \n");
		filesys_count = 1;
		}
		else
		{
		printf("Error:file system cannot be initialized \n");
		//return 1;
		}
		}
	}
	 my_argv = NULL;
	printf("\n>>MTdhee$");
}
	else if(strcmp(my_argv, "mkdir")==0)
	{
	
	 if(filesys_count == 0)
	 printf("cant see init_FS. Retry after initializing file system\n");
	 else
	 {
	  name_direct = strtok(NULL, " ");
	//  if(name_direct == NULL)
	  if(!name_direct){	    
		printf("No directory name entered. Please retry\n");
		}	  
		else
	  {
	  unsigned int dirinum = allocateinode();
	  if(dirinum < 0)
		{
		 printf("insufficient inodes \n");
		exit(1);		
		 //return 1;
		}
	 // mkdirectory(name_direct,dirinum);
    if(access(name_direct, R_OK) == 0)
    {
      printf("Directory already exists\n");
    }
    else
       makedir(name_direct);
	  }
	  }
	   my_argv = NULL;
printf("\n>>MTdhee$");	
}
	else if(strcmp(my_argv, "cpin")==0)
	{
		
	  if(filesys_count == 0)
	  printf("cant see init_FS. Retry after initializing file system\n");
	  else
	  {

	  cpinsource = strtok(NULL, " ");
	  cpinext = strtok(NULL, " ");
	 
	  if(!cpinsource || !cpinext )
	    printf("Insufficient paths \n");
	  else if((cpinsource) && (cpinext )) // made changes
	  {
	  	
	   cpin(cpinsource,cpinext);
	  }
	  }
	   my_argv = NULL;
printf("\n>>MTdhee$");
	}
	else if(strcmp(my_argv, "cpout")==0)
	{
		
	 if(filesys_count == 0)
	  printf("cant see init_FS. Retry after initializing file system\n");
	 else
	  {
	  
	  cpoutsource = strtok(NULL, " ");
	  cpoutext = strtok(NULL, " ");
	 
	  if(!cpoutsource || !cpoutsource )
	    printf("Required file names(source and target file names) have not been entered. Please retry\n");
	  else if((cpinsource) && (cpinext )) // made changes
	  {
	  	
	   cpout(cpoutsource,cpoutext);
	  }
	  }
	   my_argv = NULL;
printf("\n>>MTdhee$");
	}
else if (strcmp(my_argv, "help")==0)
{

printf("\n sample commands format for file system is \n \n ");
printf("\n initfs :: initialization of file system \n");
printf("\n \t>>initfs <file system name> <number of blocks> <number of inodes>");
printf("\n cpin :: creates new mv6 file and fill the contents of ");
printf("\n \t>>cpin <external file> <mv6-file> ");
printf("\n cpout :: creates external file and copies content of mv6 file into external file ");
printf("\n \t>>cpout <mv6-file> <external file>");
printf("\n mkdir :: making directory in current directory");
printf("\n \t>>mkdir v6-dir");
printf("\n q :: save all changes and quiet");
printf("\n \t>>q\n\n");

printf("\n>>MTdhee$");
}

   else if(strcmp(my_argv , "ls") ==0)
   {
//      show_all_files();
printf("listing");
fd=open(arg1,O_RDWR,0600);
printf("fd >>> %d",fd);
lseek(fd, 2 * BLOCK_SIZE  * sizeof(filesys_inode), SEEK_SET);

read(fd, &initial_inode, sizeof(initial_inode));
	dir temp_entry;
	int index = 0;

	while(index < initial_inode.size0 / sizeof(dir))
	{
		if(index % (BLOCK_SIZE / sizeof(dir)) == 0)
		{
			lseek(fd, offset_set(initial_inode.addr[index / (BLOCK_SIZE / sizeof(dir))]), SEEK_SET);
		}
		read(fd, &temp_entry, sizeof(temp_entry));
		printf("%-d %s \n", temp_entry.inode, temp_entry.filename);
		index++;
	}



printf("\n>>MTdhee$");

   }
	else if(strcmp(my_argv, "q")==0)
	{
	printf("\nquitting file system ..............");
	printf("\nThank you\n");
	lseek(fd,BLOCK_SIZE,0);

	 if((no_of_bytes =write(fd,&super,BLOCK_SIZE)) < BLOCK_SIZE)
	 {
	 printf("\nerror in writing the super block");
	 //return 1;
	 } 
		lseek(fd,BLOCK_SIZE,0);
	return 0;
//exit(1);
	}	
	else
	{
	printf("\nInvalid command\n ");
	printf("\n>>MTdhee$");

	}
	}
}
/************** Function to copy file contents from external file to mv6 filesystem************/
void cpin(const char *pathname1 , const char *pathname2)
{
   struct stat stats;
   int blksize , blks_allocated , req_blocks;
   int filesize;
   stat(pathname1 , &stats);
   blksize = stats.st_blksize;
   blks_allocated = stats.st_blocks;
   filesize = stats.st_size;
   req_blocks = filesize / 512;
   if(blks_allocated <= 8)
   {
   	printf("plain file , %d\n" , blks_allocated);
      plainfile(pathname1 , pathname2 , blks_allocated); // If the external is a small file
   }
   else
   {
   	printf("Large file , %d\n" , blks_allocated);
   	largefile(pathname1 , pathname2 , blks_allocated); // If external file is a large file
   }
   
}
/************ Function to copy from a Small File into mv6 File system*******************/
void plainfile(const char *pathname1 , const char *pathname2 , int blocks_allocated)
{
   int fdes , fd1 ,i ,j,k,l;
   unsigned short size;
   unsigned short i_number;
   i_number = allocateinode();
   struct stat s1;
   stat(pathname1 , &s1);
   size = s1.st_size;
   unsigned short buff[100];
   filesys_inode in;
   superb sb;
   sb.isize = super.isize;
   sb.fsize = super.fsize;
   sb.nfree = 0;
   for(l=0; l<100; l++)
   {
      sb.free[sb.nfree] = l+2+sb.isize ;
      sb.nfree++;
   }
   //blocks_chains(sb.fsize , sb.isize);
   in.flags = inode_alloc | pfile | 000077; 
   in.size0 =0;
   in.size1 = size;
   int blks_allocated = size/512;
   fdes = creat(pathname2, 0775);
   fdes = open(pathname2 , O_RDWR | O_APPEND);
   lseek(fdes , 0 , SEEK_SET);
   write(fdes ,&sb , 512);
   update_rootdir(pathname2 , i_number);
   unsigned short bl;
   for(i=0; i<blks_allocated; i++)
   {
      //printf("here\n");
      //bl = get_free_data_block(sb);
      in.addr[i] = sb.free[i];
      //printf("%d\n" , sb.free[i]);
      sb.free[i] = 0;
   }
   close(fdes);
   lseek(fdes , 512 , SEEK_SET);
   write(fdes , &in , 32);
   close(fdes);
   unsigned short bf[256];
   //unsigned int bl;
   fd1 = open(pathname1 ,O_RDONLY);
   fdes = open(pathname2 , O_RDWR | O_APPEND);
   for(j =0; j<=blks_allocated; j++)
   {
      lseek(fd1 , 512*j , SEEK_SET);
      read(fd1 ,&bf , 512);
      //bl = get_free_data_block();
      //lseek(fd , (bl+j)*512 , SEEK_SET);
      lseek(fdes , 512*(in.addr[j]) , SEEK_SET);
      //lseek(fd , 512*(1+j) , SEEK_SET);
      write(fdes , &bf , 512);
   }
   close(fdes);
   close(fd1);
   fd = open(pathname2 , O_RDONLY); 
}

/************ Function to copy from a Large File into mv6 File system*******************/
void largefile(const char *pathname1 , const char *pathname2 , int blocks_allocated)
{
   int size ,fdes1 ,fd1 ,i ,j ,h ,z ,x ,v , icount , c ,d ,m, w ,e ,t , l,f,sizer;
   unsigned short buff[100];
   unsigned short b;
   unsigned short fb;
   unsigned short i_number;
   i_number = allocateinode();
   //printf("here\n");
   struct stat s1;
   stat(pathname1 , &s1);
   size = s1.st_size;
   int blks_allocated = size/512;
   superb sb;
   sb.isize = super.isize;
   sb.fsize = super.fsize;
   sb.nfree = 0;
   for(l=0; l<100; l++)
   {
      sb.free[sb.nfree] = l+2+sb.isize;
      sb.nfree++;
   }
   //blocks_chains(sb.fsize , sb.isize);
   filesys_inode in;
   in.flags = inode_alloc | lfile | 000077; 
   fdes1 = creat(pathname2, 0775);
   fdes1 = open(pathname2 , O_RDWR | O_APPEND);
   unsigned short b1;
   lseek(fdes1 , 0 , SEEK_SET);
   write(fdes1 ,&sb , 512);
   for(i=0; i<8; i++)
   {
      //if(i==8)
         //break;
      b1 = get_free_data_block(); // Get a Free Block
      //printf("here\n");
      in.addr[i] = b1;
      //printf("%d\n" , in.addr[i]);
      //b++;
   }
   close(fdes1);
   fdes1 = open(pathname2 , O_RDWR | O_APPEND);
   lseek(fdes1 , 512 , SEEK_SET);
   write(fdes1 , &in , 32);
   close(fdes1);
   //printf("here1\n");
   if(blks_allocated > 67336)
   {
      printf("\nExternal File too large\n");
   }
   else
   {
      if(blks_allocated < 2048)
      {
         unsigned short bf[8][256];
         unsigned short bz[8][256];
         unsigned short b2;
         int k =8;
         //printf("here2\n");
         //fd = open(pathname2 , O_RDWR | O_APPEND);
            for(j =0; j<8; j++)
            {
               //printf("here2\n");
               if(k < blks_allocated)
               {
                  //printf("here2\n");
                  fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                  for(m=0; m<256; m++)
                  {
                     if(m > blks_allocated)
                        break;
                        //printf("here2\n");
                     b2 = get_free_data_block();
                     bf[j][m] = b2;
                     //printf("here2\n");
                     //printf("%d\n" , bf[j][m]);
                     //printf("%d\n" , j);
                  }
                  close(fdes1);
                  for(e=0; e<256; e++)
                  {
                     fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                     lseek(fdes1 , ((in.addr[j])*512)+(2*e) , SEEK_SET);
                     write(fdes1 , &bf[j][e] , 2);
                      close(fdes1);
                  }
               }  
               else
                  break;
               k = k + 256;
            }
            //printf("here2\n");
            unsigned short buffer[256];
            unsigned short bu[256][256];
            int m=0;
            int r=0;
            int count =0;
            fd1 = open(pathname1 , O_RDONLY);
           // fd = open(pathname2 , O_RDWR | O_APPEND);
            for(h=0; h<=blks_allocated; h++)
            {
               if(r<8)
               {
                  lseek(fd1 , h*512 , SEEK_SET);
                  //read(fd1 , &buffer[h] , 512);
                  //fd = open(pathname2 , O_RDWR | O_APPEND);
                  for(x=0; x<256; x++)
                  {
                     if(count > blks_allocated)
                        break;
                     read(fd1 , &buffer , 512);
                     fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                     //bl = get_free_data_block();
                     lseek(fdes1 , 512*(bf[r][x]) , SEEK_SET);
                     write(fdes1 , &buffer , 512);
                     close(fdes1);
                     count++;
                  }
               }
               r++;
            }
            close(fd1);
      }
      else
      {
         unsigned short bf[8][256];
         unsigned short bz[256];
         unsigned int b2;
         int k =7;
         int r=0;
         fdes1 = open(pathname2 , O_RDWR | O_APPEND);
            for(j =0; j<7; j++)
            {
               if(k < blks_allocated)
               {
                  //fd = open(pathname2 , O_RDWR | O_APPEND);
                  fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                  for(m=0; m<256; m++)
                  {
                     b2 = get_free_data_block();
                     bf[j][m] = b2;
                     r++;
                  }
                  close(fdes1);
                  fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                  for(e=0; e<256; e++)
                  {
                     lseek(fdes1 , ((in.addr[j])*512)+ 2*e , SEEK_SET);
                     write(fdes1 , &bf[j][e] , 2);
                  }
                  //lseek(fd , (in.addr[j])*512 , SEEK_SET);
                  //write(fd , &bf[j] , 512);
                  close(fdes1);
               }  
               else
                  break;
               k = k + 256;
            }
            //code for double indirect block
            unsigned short lbuff[256][256];
            //unsigned short lbuff1[256];
            fdes1 = open(pathname2 , O_RDWR | O_APPEND);
            for(c=0; c<256; c++)
            {
               lseek(fdes1 , bf[7][c]*512 , SEEK_SET);
               for(d=0; d<256;d++)
               {
                  if(r > blks_allocated)
                     break;
                   b2 = get_free_data_block(sb);
                   lbuff[c][d] = b2; 
                   r++; 
               }
               for(t=0; t<256; t++)
               {
                  lseek(fdes1 , (bf[7][c]*512)+2*t , SEEK_SET);
                  write(fdes1 , &lbuff[c][t] , 2);
               }
            }
            close(fdes1);
            unsigned short buffer[256];
            unsigned short bu;
            int s=0;
            int ko=0;
            fd1 = open(pathname1 , O_RDONLY);
           // fd = open(pathname2 , O_RDWR | O_APPEND);
            for(h=0; h<=blks_allocated; h++)
            {
               if(ko<7)
               {
                  lseek(fd1 , h*512 , SEEK_SET);
                  read(fd1 , &buffer , 512);
                  //fdes = open(pathname2 , O_RDWR | O_APPEND);
                  for(x=0; x<256; x++)
                  {
                     //lseek(fd1 , h*512 , SEEK_SET);
                     //read(fd1 , &buffer , 512);
                     fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                     //bl = get_free_data_block();
                     lseek(fdes1 , (bf[ko][x])*512 , SEEK_SET);
                     write(fdes1 , &buffer , 512);
                     close(fdes1);
                     s++;
                  }
                  ko++;
               }
               else
               {
                  lseek(fd1 , h*512 , SEEK_SET);
                  read(fd1 , &buffer , 512);
                  //fd = open(pathname2 , O_RDWR | O_APPEND);
                  unsigned short dilbuf[256];
                  fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                  for(v=0; v<256; v++)
                  {
                     for(w=0; w<256; w++)
                     {
                        if(s > blks_allocated)
                           break;
                        //fd = open(pathname2 , O_RDWR | O_APPEND);
                        //bl = get_free_data_block();
                        lseek(fdes1 , lbuff[v][w]*512 , SEEK_SET);
                        write(fdes1 , &buffer , 512);
                        s++;
                     }
                     //close(fd);
                  }
                  close(fdes1);

               }
            }
            close(fd1);
            

      }
   }
   
}

/************ Function to copy from mv6 File System into an external File*******************/
void cpout(const char *pathname1 , const char *pathname2)
{
   struct stat stats;
   int blksize , blks_allocated , no_of_blocks;
   int filesize;
   stat(pathname1 , &stats);
   blksize = stats.st_blksize;
   blks_allocated = stats.st_blocks;
   filesize = stats.st_size;
   no_of_blocks = filesize /512;
   //printf("%d\n" , filesize);
   if(blks_allocated < 8)
   {
   	printf("plainfile , %d\n" , no_of_blocks);
      out_plainfile(pathname1 , pathname2 , no_of_blocks);  //If MV6 File is a small file
   }
   else
   {
      printf("largefile , %d\n" , no_of_blocks);
      out_largefile(pathname1 , pathname2 ,  no_of_blocks);  //If MV6 File is a large file
   }

}

/************ Function to copy Small file from mv6 File System into an external File*******************/
void out_plainfile(const char *pathname1 , const char *pathname2 , int blocks_allocated)
{
   int nblocks = blocks_allocated;
   //printf("%d\n" , nblocks);
   int fdes , fd1 , i;
   unsigned short buffer[256];
   unsigned short addr[8];
   fdes = open(pathname1 , O_RDONLY);
   lseek(fdes , 520 , SEEK_SET);
   read(fdes , &addr , 16);
   close(fdes);
   fdes = open(pathname1 , O_RDONLY);
   fd1 = creat(pathname2, 0775);
   fd1 = open(pathname2, O_RDWR | O_APPEND);
   for(i =0 ; i<nblocks; i++)
   {
      lseek(fdes , i*512 , SEEK_SET);
      read(fdes , &buffer , 512);
      lseek(fd1 , 512*i , SEEK_SET);
      write(fd1 , &buffer , 512);
   }
   close(fd1);
   close(fdes);
}

/************ Function to copy a large file from mv6 File System into an external File*******************/
void out_largefile(const char *pathname1 , const char *pathname2 , int blocks_allocated)
{
   int nblocks = blocks_allocated;
  // printf("%d\n" , nblocks);
   int fdes1 , fd1 , i ,j ,k ,x ,m ,l ,r;
   unsigned short buffer[256];
   unsigned short buff[8][256];
   unsigned short buf[256][256];
   unsigned short addr[8];
   fd1 = creat(pathname2, 0775);
   for(i=0; i<nblocks; i++)
   {
      fdes1 = open(pathname1 , O_RDONLY);
      lseek(fdes1 , i*512 , SEEK_SET);
      read(fdes1 , &buffer , 512);
      fd1 = open(pathname2 , O_RDWR | O_APPEND);
      lseek(fd1 , i*512 , SEEK_SET);
      write(fd1 , &buffer , 512);
      close(fd1);
   }
   close(fdes1);
 
}  

/************ Function to update root directory*******************/
void update_rootdir(const char *pathname , unsigned short in_number)
{
   int i;
   dir ndir;
   int size;
   ndir.inode = in_number;
   //printf("%d\n" , ndir.inode);
   strncpy(ndir.filename, pathname , 14);
   //rootfd = open(rootname , O_RDWR | O_APPEND);
   //lseek(rootfd , 0 , SEEK_CUR);
   size = write(rootfd , &ndir , 16);
   //close(rootfd);
   //printf("%d\n" , size);
}
/************ Function to create a Directory*******************/
void makedir(const char *pathname)
{
   int filedes , i ,j;
   filedes = creat(pathname, 0666);
   filedes = open(pathname , O_RDWR | O_APPEND);
   filesys_inode ino1;
   unsigned short inum;
   inum = allocateinode();
   ino1.flags = inode_alloc | directory | 000077; 
   ino1.nlinks = 2;
   ino1.uid =0;
   ino1.gid =0;
   for(i=0; i<8; i++)
   {
      ino1.addr[i] = 0;
   }
   unsigned short bk;
   //for(j=0; j<8; j++)
   //{
      //printf("here\n");
      bk = get_free_data_block();
      ino1.addr[0] = bk;
   lseek(filedes , 512 , SEEK_SET);
   write(filedes , &ino1 , 32);
   dir direct;
   direct.inode = inum;
   //printf("here\n");
   strncpy(direct.filename, "." , 14);
  // printf("here\n");
   lseek(filedes , ino1.addr[0]*512 , SEEK_SET);
   write(filedes , &direct , 16);
  // printf("here\n");
   strncpy(direct.filename, ".." , 14);
   lseek(filedes , (ino1.addr[0]*512 + 16) , SEEK_SET);
   write(filedes , &direct , 16);
   close(filedes);
   update_rootdir(pathname , inum);
   printf("directory created\n");
}
/************ Function to show all existing files and directories in the File System*******************/
void show_all_files()
{  
   int i , size;
   filesys_inode i_node;
   dir out;
   unsigned short buffer;
   rootfd = open(rootname , O_RDWR | O_APPEND);
   lseek(rootfd , 520 , SEEK_SET);
   size = read(rootfd , &i_node.addr[0] , 2);
   for(i=0; i<10; i++)
   {
      lseek(rootfd , (120*512)+(16*i) , SEEK_SET);
      size = read(fd , &out , 16);
       //for(i=0; i<14; i++)
       //{
          printf("%d , %s\n" , out.inode , out.filename);
       //}
       close(rootfd);
   }
}
int offset_set(int block)
{
	int offset =0;
	return block * BLOCK_SIZE + offset;
}
