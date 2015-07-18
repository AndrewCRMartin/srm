/*************************************************************************

   Program:    
   File:       
   
   Version:    
   Date:       
   Function:   
   
   Copyright:  (c) Dr. Andrew C. R. Martin 1995
   Author:     Dr. Andrew C. R. Martin
   Address:    
   EMail:      andrew@stagleys.demon.co.uk
               
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

**************************************************************************

   Usage:
   ======

**************************************************************************

   Revision History:
   =================

*************************************************************************/
/* Includes
*/
#include <stdio.h>
#include <unistd.h>
#include "bioplib/SysDefs.h"


/************************************************************************/
/* Defines and macros
*/
#define MAXCOUNT 6

/************************************************************************/
/* Globals
*/

/************************************************************************/
/* Prototypes
*/
BOOL delfile(char *filename);
void Usage(void);
int main(int argc, char **argv);

/************************************************************************/
int main(int argc, char **argv)
{
   argc--;
   argv++;

   if(argc==0 || argv[0][0] == '-')
   {
      Usage();
      return(1);
   }
   
   
   while(argc--)
   {
      delfile(*(argv++));
   }
   
   return(0);
}

/************************************************************************/
BOOL delfile(char *filename)
{
   FILE  *fp;
   ULONG pos,count;
   int   i;
   char  c;
   
   
   if((fp=fopen(filename,"r+"))==NULL)
      return(FALSE);
   
   for(i=0; i<MAXCOUNT; i++)
   {
      c = ((i%2)?0xCC:0x55);
      
      fseek(fp,0L,SEEK_END);
      pos = ftell(fp);
      
      fseek(fp,0L,SEEK_SET);
      for(count=0; count<pos; count++)
         fputc(c,fp);
   }
   
   fclose(fp);
   
   unlink(filename);

   return(TRUE);
}

/************************************************************************/
void Usage(void)
{
   fprintf(stderr,"\nsrm V1.0 (c) Andrew C.R. Martin\n");
   fprintf(stderr,"\nUsage: srm file [file...]\n");
   fprintf(stderr,"\nSrm is a 'safe' version of rm which overwrites the \
file before deleting\n");
   fprintf(stderr,"it, such that undelete programs won't work.\n\n");
}

