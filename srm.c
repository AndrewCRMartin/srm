/************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef unsigned short int BOOL;
#define TRUE 1
#define FALSE 0

#define DEF_COUNT 3
#define MAXBUFF 256
#define MODE_FILE 1
#define MODE_DIR  2


BOOL CleanFreeSpace(char *dirname, int count);
BOOL DeleteFile(char *filename, int count);
BOOL ScrubFile(char *filename, int pass);
void Usage(void);

/************************************************************************/
int main(int argc, char **argv)
{
   int count = DEF_COUNT,
       mode  = MODE_FILE;
   
   argc--;
   argv++;
   
   if(!argc)
   {
      Usage();
      return(0);
   }
   
   while(argc)
   {
      if(argv[0][0] == '-')
      {
         switch(argv[0][1])
         {
         case 'n':
            argc--;
            argv++;
            if((!argc) || !sscanf(argv[0],"%d", &count))
            {
               Usage();
               return(1);
            }
            break;
         case 'c':
            mode = MODE_DIR;
            break;
         case 'h':
            Usage();
            return(0);
         default:
            Usage();
            return(1);
         }
      }
      else
      {
         if(mode==MODE_FILE)
         {
            DeleteFile(argv[0], count);
         }
         else
         {
            if(!CleanFreeSpace(argv[0], count))
            {
               fprintf(stderr,"Specified file was not a directory: %s\n", 
                          argv[0]);
               return(1);
            }
         }
      }
         
      argc--;
      argv++;
   }
   return(0);
}

/************************************************************************/
BOOL DeleteFile(char *filename, int count)
{
   struct stat s_buff;
   int         i;

   stat(filename, &s_buff);
   if(!(S_ISREG(s_buff.st_mode)))
   {
      fprintf(stderr,"Not a regular file: %s\n",filename);
      return(FALSE);
   }

   /* Do our over-writing here                                          */
   fprintf(stderr,"Deleting %s\n", filename);

   for(i=0;i<count;i++)
   {
      if(!ScrubFile(filename, i%2))
      {
         return(FALSE);
      }
   }

   unlink(filename);

   return(TRUE);
}

/************************************************************************/
void Usage(void)
{
   fprintf(stderr,"\nsrm V2.0 (c) Andrew C.R. Martin\n");

   fprintf(stderr,"\nUsage: srm [-n count] [-c] file [file...]\n");

   fprintf(stderr,"\n       -c Clean all free space on a disk\n");
   fprintf(stderr,"       -n Specifify number of over-writes [%d]\n",
           DEF_COUNT);

   fprintf(stderr,"\nsrm does a safe delete of a file overwriting it \
with junk before unlinking\n");
   fprintf(stderr,"it. Alternatively, with the -c flag, it fills the \
disk by writing temporary\n");
   fprintf(stderr,"files to the specified directories. These files are \
over-writen multiple\n");
   fprintf(stderr,"times.\n\n");
}

/************************************************************************/
BOOL ScrubFile(char *filename, int pass)
{
   FILE          *fp  = NULL;
   long          fileSize,
                 offset;
   unsigned char patt = ((pass)?0xAA:0x55);

   /* Open file                                                         */
   if((fp=fopen(filename,"r+"))==NULL)
   {
      return(FALSE);
   }
   
   /* Seek to end                                                       */
   if(fseek(fp, 0, SEEK_END))
   {
      fclose(fp);
      return(FALSE);
   }

   /* Find location of end of file                                      */
   fileSize = ftell(fp);
   
   /* Now step through file writing patt to each location               */
   fseek(fp, 0, SEEK_SET);
   for(offset=0; offset<=fileSize; offset++)
   {
      fputc(patt, fp);
   }

   fclose(fp);

   return(TRUE);
}


/************************************************************************/
BOOL CleanFreeSpace(char *dirname, int count)
{
   struct stat s_buff;
   FILE        *fp;
   int         i,
               fCount,
               cCount,
               nFiles;
   BOOL        full = FALSE;
   char        filename[MAXBUFF];
   
   stat(dirname, &s_buff);
   if(!(S_ISDIR(s_buff.st_mode)))
   {
      return(FALSE);
   }

   /* Do our big file creation here                                     */
   fprintf(stderr,"Creating files in %s\n", dirname);
   for(fCount=0;;fCount++)
   {
      sprintf(filename, "%s/.srmfill.%d", dirname, fCount);
      if((fp=fopen(filename,"w"))==NULL)
      {
         break;
      }
      
      for(cCount=0; cCount<LONG_MAX; cCount++)
      {
         if(fputc(((cCount%2)?0xFF:0x00),fp)==EOF)
         {
            full = TRUE;
            break;
         }
      }

      fclose(fp);
      if(full)
      {
         break;
      }
   }
   
   nFiles = fCount;
   for(fCount=0;fCount<=nFiles;fCount++)
   {
      sprintf(filename, "%s/.srmfill.%d", dirname, fCount);
      fprintf(stderr,"Scrubbing file: %s\n", filename);
      for(i=0;i<count;i++)
      {
         fprintf(stderr,"pass %d/%d\n", i+1, count);
         ScrubFile(filename, i%2);
      }
      unlink(filename);
   }

   return(TRUE);
}

