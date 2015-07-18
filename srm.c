/*************************************************************************

   Program:    srm
   File:       srm.c
   
   Version:    V2.3
   Date:       27.01.04
   Function:   
   
   Copyright:  (c) Dr. Andrew C. R. Martin 2004
   Author:     Dr. Andrew C. R. Martin
   EMail:      andrew@andrew-martin.org
               
**************************************************************************

   This program is not in the public domain, but it may be copied
   according to the conditions laid out in the accompanying file
   COPYING.DOC

   The code may be modified as required, but any modifications must be
   documented so that the person responsible can be identified. If someone
   else breaks this code, I don't want to be blamed for code that does not
   work! 

   The code may not be sold commercially or included as part of a 
   commercial product except as described in the file COPYING.DOC.

**************************************************************************

   Description:
   ============
   A 'safe-delete' program for Unix - overwrites a file before 
   deleting it.

**************************************************************************

   Usage:
   ======

**************************************************************************

   Revision History:
   =================
   V1.0  01.03.00  First version
   V2.0  01.01.03  Rewritten
   V2.1  01.02.03  Modified
   V2.2  27.10.03  Added super-scrubbing
   V2.3  27.01.04  Added super-scrubbing on disk cleaning and routine
                   to clear the cache on the disk

*************************************************************************/
/* Includes
*/
#include <sys/types.h>
#include <sys/stat.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>


/************************************************************************/
/* Defines and macros
*/
typedef unsigned short int BOOL;
typedef unsigned long ULONG;
typedef unsigned char UCHAR;
#define TRUE 1
#define FALSE 0

#define DEF_COUNT 3
#define MAXBUFF 256
#define MODE_FILE 1
#define MODE_DIR  2
#define CACHESIZE 8 * 1024 * 1024

/************************************************************************/
/* Globals
*/

/************************************************************************/
/* Prototypes
*/
BOOL CleanFreeSpace(char *dirname, int count);
BOOL DeleteFile(char *filename, int count, BOOL thorough);
BOOL ScrubFile(char *filename, int pass);
void Usage(void);
BOOL SuperScrubFile(char *filename, BOOL verbose, BOOL thorough);
void ClearCache(char *filename, ULONG buffSize);

/************************************************************************/
/*>int main(int argc, char **argv)
   -------------------------------
*/
int main(int argc, char **argv)
{
   int  count = DEF_COUNT,
        mode  = MODE_FILE;
   BOOL thorough = FALSE;
   
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
         case 's':
            count = (-1);
            break;
         case 'c':
            mode = MODE_DIR;
            break;
         case 't':
            thorough = TRUE;
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
            DeleteFile(argv[0], count, thorough);
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
/*>BOOL DeleteFile(char *filename, int count, BOOL thorough)
   ---------------------------------------------------------
   Deletes a file performing the specified number of over-writes.
   The last over-write is with random characters. If count is -1
   then the super-scrub routine is called instead.

   If (thorough) then the ClearCache() routine is called after every
   sync() otherwise just before the last random pass.

   27.10.03 Added call to sync() and superscrubbing
   27.01.04 Added call to ClearCache() and thorough parameter
*/
BOOL DeleteFile(char *filename, int count, BOOL thorough)
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

   if(count == (-1))
   {
      SuperScrubFile(filename, FALSE, thorough);
   }
   else
   {
      for(i=0;i<count-1;i++)
      {
         if(!ScrubFile(filename, i%2))
         {
            return(FALSE);
         }
         sync();  /* Added 27.10.03                                     */
         if(thorough) ClearCache(filename, CACHESIZE);
      }
      if(!thorough) ClearCache(filename, CACHESIZE);
      if(!ScrubFile(filename, (-1)))
      {
         return(FALSE);
      }
      sync();
   }

   unlink(filename);

   return(TRUE);
}

/************************************************************************/
/*>void Usage(void)
   ----------------
*/
void Usage(void)
{
   fprintf(stderr,"\nsrm V2.3 (c) Andrew C.R. Martin\n");

   fprintf(stderr,"\nUsage: srm [-s] [-t][-n count] [-c] file \
[file...]\n");

   fprintf(stderr,"\n       -s Super-scrub (-n ignored)\n");
   fprintf(stderr,"       -c Clean all free space on a disk\n");
   fprintf(stderr,"       -t Thorough - clear disk cache on every \
pass\n");
   fprintf(stderr,"       -n Specifify number of over-writes [%d]\n",
           DEF_COUNT);

   fprintf(stderr,"\nsrm does a safe delete of a file overwriting it \
with junk before unlinking\n");
   fprintf(stderr,"it. Alternatively, with the -c flag, it fills the \
disk by writing temporary\n");
   fprintf(stderr,"files to the specified directories. These files are \
over-writen multiple\n");
   fprintf(stderr,"times.\n\n");
   fprintf(stderr,"If 'super-scrub' is selected, then 35 passes are used \
specifically\n");
   fprintf(stderr,"aimed at different types of disks. For details, see \
the paper at:\n");
           fprintf(stderr,"http://www.cs.auckland.ac.nz/~pgut001/pubs/\
secure_del.html\n\n");
}

/************************************************************************/
/*>BOOL ScrubFile(char *filename, int pass)
   ----------------------------------------
   Does the actual work of opening and over-writing a file
   If pass is >=0 then bit patterns are written depending if it is
   odd or even.
   If pass is -1 then a wandom pattern is written
*/
BOOL ScrubFile(char *filename, int pass)
{
   FILE  *fp  = NULL;
   long  fileSize,
         offset;
   UCHAR patt = ((pass)?0xAA:0x55);

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

   if(pass == (-1))
   {
      srand((unsigned int)time(NULL));
      
      for(offset=0; offset<=fileSize; offset++)
      {
         patt = (UCHAR)(255 * rand()/(RAND_MAX+1.0));
         
         fputc(patt, fp);
      }
   }
   else
   {
      for(offset=0; offset<=fileSize; offset++)
      {
         fputc(patt, fp);
      }
   }
   

   fclose(fp);

   return(TRUE);
}


/************************************************************************/
/*>BOOL CleanFreeSpace(char *dirname, int count)
   ---------------------------------------------
   Clears a hard disk by opening file(s) in the specified directory
   and scrubbing them (count times) or super-scrubbing them (if count
   is -1)

   26.01.04 Added super-scrubbing
*/
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
      if(count==(-1))  /* Added 26.01.04 */
      {
         SuperScrubFile(filename, TRUE, FALSE);
      }
      else
      {
         for(i=0;i<count;i++)
         {
            fprintf(stderr,"pass %d/%d\n", i+1, count);
            ScrubFile(filename, i%2);
         }
      }
      unlink(filename);
   }

   return(TRUE);
}

/************************************************************************/
/*>BOOL SuperScrubFile(char *filename, BOOL verbose, BOOL thorough)
   ----------------------------------------------------------------
   This is Peter Gutmann's super-scrubbing scheme from 
   http://www.cs.auckland.ac.nz/~pgut001/pubs/secure_del.html
   where each pattern is specificvally aimes at (1,7)RLL, (2,7)RLL or MFM
   drives

   If(thorough) the ClearCache() routine is called after each pass
   if the filesize is < 2*CACHESIZE. Otherwise it is just called before
   the last random pass

   27.10.03 Original
   26.01.04 Added verbose option and calls to ClearCache()
*/
BOOL SuperScrubFile(char *filename, BOOL verbose, BOOL thorough)
{
   FILE  *fp  = NULL;
   long  fileSize,
         offset;
   UCHAR patt[16];
   BOOL  random;
   int   npatt, 
         count,
         pass;


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

   for(pass=0; pass<35; pass++)
   {
      random = FALSE;
      
      switch(pass)
      {
      case 0:
      case 1:
      case 2:
      case 3:
         random = TRUE;
         break;
      case 4:
         patt[0] = (UCHAR)0x55;
         npatt = 1;
         break;
      case 5:
         patt[0] = (UCHAR)0xAA;
         npatt = 1;
         break;
      case 6:
      case 25:
         patt[0] = (UCHAR)0x92;
         patt[1] = (UCHAR)0x49;
         patt[2] = (UCHAR)0x24;
         npatt = 3;
         break;
      case 7:
      case 26:
         patt[0] = (UCHAR)0x49;
         patt[1] = (UCHAR)0x24;
         patt[2] = (UCHAR)0x92;
         npatt = 3;
         break;
      case 8:
      case 27:
         patt[0] = (UCHAR)0x24;
         patt[1] = (UCHAR)0x92;
         patt[2] = (UCHAR)0x49;
         npatt = 3;
         break;
      case 9:
         patt[0] = (UCHAR)0x00;
         npatt = 1;
         break;
      case 10:
         patt[0] = (UCHAR)0x11;
         npatt = 1;
         break;
      case 11:
         patt[0] = (UCHAR)0x22;
         npatt = 1;
         break;
      case 12:
         patt[0] = (UCHAR)0x33;
         npatt = 1;
         break;
      case 13:
         patt[0] = (UCHAR)0x44;
         npatt = 1;
         break;
      case 14:
         patt[0] = (UCHAR)0x55;
         npatt = 1;
         break;
      case 15:
         patt[0] = (UCHAR)0x66;
         npatt = 1;
         break;
      case 16:
         patt[0] = (UCHAR)0x77;
         npatt = 1;
         break;
      case 17:
         patt[0] = (UCHAR)0x88;
         npatt = 1;
         break;
      case 18:
         patt[0] = (UCHAR)0x99;
         npatt = 1;
         break;
      case 19:
         patt[0] = (UCHAR)0xAA;
         npatt = 1;
         break;
      case 20:
         patt[0] = (UCHAR)0xBB;
         npatt = 1;
         break;
      case 21:
         patt[0] = (UCHAR)0xCC;
         npatt = 1;
         break;
      case 22:
         patt[0] = (UCHAR)0xDD;
         npatt = 1;
         break;
      case 23:
         patt[0] = (UCHAR)0xEE;
         npatt = 1;
         break;
      case 24:
         patt[0] = (UCHAR)0xFF;
         npatt = 1;
         break;
      case 28:
         patt[0] = (UCHAR)0x6D;
         patt[1] = (UCHAR)0xB6;
         patt[2] = (UCHAR)0xDB;
         npatt = 3;
         break;
      case 29:
         patt[0] = (UCHAR)0xB6;
         patt[1] = (UCHAR)0xDB;
         patt[2] = (UCHAR)0x6D;
         npatt = 3;
         break;
      case 30:
         patt[0] = (UCHAR)0xDB;
         patt[1] = (UCHAR)0x6D;
         patt[2] = (UCHAR)0xB6;
         npatt = 3;
         break;
      case 31:
      case 32:
      case 33:
      case 34:
         random = TRUE;
         break;
      }
      
      if(verbose)
      {
         printf("pass %d/35\n", pass+1);
      }
      
      /* Write the data                                                 */
      if(random)
      {
         srand((unsigned int)time(NULL));
         
         for(offset=0; offset<=fileSize; offset++)
         {
            patt[0] = (UCHAR)(255 * rand()/(RAND_MAX+1.0));
            
            fputc(patt[0], fp);
         }
      }
      else
      {
         for(offset=0; offset<=fileSize; offset+=npatt)
         {
            for(count=0; count<npatt; count++)
            {
               fputc(patt[count], fp);
            }
         }
      }

      /* Close and re-open the file                                     */
      fclose(fp);
      sync();
      if(thorough && (fileSize < 2*CACHESIZE)) 
      {
         ClearCache(filename, CACHESIZE);            /* Added 27.01.04  */
      }
      else if(pass==33)
      {
         ClearCache(filename, CACHESIZE);            /* Added 27.01.04  */
      }
            
      if((fp=fopen(filename,"r+"))==NULL)
      {
         return(FALSE);
      }
      fseek(fp, 0, SEEK_SET);
   }
   
   fclose(fp);

   return(TRUE);
}

/************************************************************************/
/*>void ClearCache(char *filename, ULONG buffSize)
   ------------------------------------------------
   A simple routine to open and write a file of the specified size.
   This is used after writing to the file to be deleted in order to
   fill the cache on the hard disk forcing it to be written to
   physical disk.

   A filename for this temporary file is derived from the specified
   filename to ensure it's on the same disk!

   27.01.04 Original   By: ACRM
*/
void ClearCache(char *filename, ULONG buffSize)
{
   char *filename2;
   FILE *fp;
   ULONG cCount;
   UCHAR patt;
   

   if((filename2=malloc((strlen(filename)+16)*sizeof(char)))!=NULL)
   {
      sprintf(filename2, "%s.srmAOWIUFHN", filename);
      if((fp=fopen(filename2, "w"))!=NULL)
      {
         for(cCount=0; cCount<buffSize; cCount++)
         {
            patt = (UCHAR)(255 * rand()/(RAND_MAX+1.0));
            if(fputc(patt,fp)==EOF)
            {
               break;
            }
         }
         fclose(fp);
         sync();
         unlink(filename2);
         sync();
      }
      free(filename2);
   }
}

