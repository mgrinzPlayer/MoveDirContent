#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int foldersAllocated=4096;
int filesAllocated=16384;
int currentNumberFolders=0;
int currentNumberFiles=0;
char **folders;
char **files;


static int
ptree(char *path)
{
        static char   ep[260];
        static struct stat st;
        static struct dirent *entryp;

        int epSize;
        DIR *dirp;
        char *p;

        if ((dirp = opendir(path)) == NULL) {printf("Can not open directory %s\n",path); _exit(1);}

        epSize=strlen(path);
        while ( (entryp=readdir(dirp)) != NULL ) {

                // do we care about . and .. ? Nope!
                if (strcmp(entryp->d_name, ".") == 0 || strcmp(entryp->d_name, "..") == 0) continue;

                // build path for new entry
                *(ep+epSize)=0;
                snprintf(ep, sizeof(ep), "%s/%s", path, entryp->d_name);
                while ((p=strchr(ep,'/'))) *p='\\'; // replace all slash to backslash

                // removes ".\" if any
                p=( (*ep=='.' && *(ep+1)=='\\') ? ep+2 : ep);

                if (stat(ep, &st) == -1) {printf("Can not stat %s\n",ep); _exit(1);}
                if (S_ISREG(st.st_mode)) {

                  if ((++currentNumberFiles)>filesAllocated)
                     {
                       filesAllocated+=16384;
                       files=(char**)realloc(files,filesAllocated*sizeof(char*));

                     }

                  files[currentNumberFiles-1]=strdup(p);
                  #ifdef DEBUG
                    printf("F %s\n",p);
                  #endif
                }
                else
                if (S_ISDIR(st.st_mode)) {

                  if ((++currentNumberFolders)>foldersAllocated)
                     {
                       foldersAllocated+=4096;
                       folders=(char**)realloc(folders,foldersAllocated*sizeof(char*));

                     }

                  folders[currentNumberFolders-1]=strdup(p);
                  #ifdef DEBUG
                    printf("D %s\n",p);
                  #endif

                  ptree(ep);
                }
        }

        closedir(dirp);
        return 0;
}

static void
removePathExcludedChars(char *path)
{
  char *p;
  while ((p=strchr(path,'/'))) *p='\\';  // replace all slash to backslash

  while (strlen(path) && *( path+strlen(path)-1 )=='\\')
                         *( path+strlen(path)-1 )=0; // remove backslashes at the end

  while (strlen(path) && *( path+strlen(path)-1 )=='"')
                         *( path+strlen(path)-1 )=0; // remove " at the end

  while (*path=='"' )  path++;   // remove " at the beginning
  while (*path=='\\' ) path++;   // remove slash at the beginning
}


int
main(int argc, char * argv[])
{
#ifndef _WIN32
  printf("For now, only for Windows.\n"); return 1;
#endif

  if (argc!=3) { printf("Too few parameters...\n%s SRCDIR DSTDIR\n",argv[0]); return 1;}

  folders=malloc(foldersAllocated*sizeof(char*));
  files=malloc(filesAllocated*sizeof(char*));

  char *srce=argv[1];
  char *dest=argv[2];

  removePathExcludedChars(srce);
  removePathExcludedChars(dest);

  DIR *dirp;
  char command[7680];
  int ret;

  snprintf(command, 7680, "MOVE \"%s\" \"%s\" >NUL 2>NUL", srce, dest);
  if ((dirp = opendir(dest)) == NULL)
     if ((ret=system(command) != 0 )) // just rename src if dest folder not exist
        { printf("Error while renaming SRCDIR\n"); return ret;}
     else return 0;                 // rename successful, exit
  else closedir(dirp); // folder already exist, close it



  ptree(srce); // gather files and folders

  //remove duplicates and those already taken into account ( if there is "a\b\c\" we can skip "a\b\" )
  int i=0;
  int j=0;
  again:
  for (i=0;i<currentNumberFolders-1;i++)
   for (j=i+1;j<currentNumberFolders;j++)
    {
      if ( folders[j]==NULL || folders[i]==NULL ) continue;
      if (  strstr(folders[i],folders[j]) ) {free(folders[j]);folders[j]=NULL; goto again;}
      if (  strstr(folders[j],folders[i]) ) {free(folders[i]);folders[i]=NULL; goto again;}
    }

  // rearrange pointers
  int count=0;
  for (i=0;i<currentNumberFolders;i++)
     if (folders[i]!=NULL) folders[count++]=folders[i];

  // remove main folder from path
  int offset=strlen(srce)+1;
  for (i=0;i<count;i++) folders[i]=folders[i]+offset;

  #ifdef DEBUG
  //print files
  for (i=0;i<currentNumberFiles;i++)
    printf("%s\n",files[i]);
  #endif

  #ifdef DEBUG
  //print folders
  for (i=0;i<count;i++)
    printf("%s\n",folders[i]);
  #endif

  // create subfolders (if they already exists, nothing bad happens)
  for (i=0;i<count;i++) {
    snprintf(command, 7680, "MD \"%s\\%s\" >NUL 2>NUL", dest, folders[i]);
    system(command);
   }

  // move files
  for (i=0;i<currentNumberFiles;i++) {
    snprintf(command, 7680, "MOVE /Y \"%s\" \"%s\\%s\" >NUL 2>NUL", files[i], dest, files[i]+offset);
    #ifdef DEBUG
      printf("%s\n",command);
    #endif
    if ((ret=system(command)!= 0)) {printf("Error while moving file %s\n",files[i]); return ret;}
   }

   // still here, so everything is good, remove source dir
   snprintf(command, 7680, "RD /S /Q \"%s\" >NUL 2>NUL", srce);
   if ((ret=system(command) != 0)) {printf("Error while removing dir %s\n",srce); return ret;}

  return 0;
}
