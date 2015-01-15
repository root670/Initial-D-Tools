#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

typedef struct pacHeader {
	char magic[4];
	uint32_t numEntries;
	uint32_t unknown1;
	uint32_t unknown2; // Usually 0x20
	char archiveName[16]; // Uppercase only?
} pacHeader_t;

typedef struct entryHeader {
	char name[16];
	uint32_t offset;
	uint32_t length;
	uint32_t fileType;
	uint32_t extra;
} entryHeader_t;

/* When building a PAC file, the data from each file to be stored
 * in the PAC will be loaded into memory first and then written all
 * at once. This is required because we need to know the sizes of all
 * the entries before writing the header data to ensure the offsets
 * are correct.
 */
typedef struct entryBuffer {
	uint32_t length;
	uint8_t *data;
} entryBuffer_t;

/* File types */
#define GIM_TEXTURE	0x01
#define	SMD			0x03
#define OTHER		0x06

char cwd[260];

// Returns a pointer to text located after the last slash in path
// TODO: Support unixey paths with forward slashes.
static char* filenameFromPath(char *path)
{
	assert(path);

	char* endPtr = path + (strlen(path) - 1);
	char* carrot = endPtr;

	do {
		carrot--;
	} while(carrot > path && *carrot != '\\');

	if(*carrot == '\\')
		return carrot+1;
	else
		return carrot;
}

// Pre: fixed points to a char array of size equal to original.
// Post: fixed points to filename from original with extension removed.
static void removeFileExtention(const char* original, char *fixed)
{
	if(!original)
		return;

	const char* end = original + strlen(original)-1;
	while(end > original)
	{
		if(*end != '.')
			end--;
		else
			break;
	}

	strncpy(fixed, original, end - original);
}

// Pre: filename
// Returns identifier for fileType based on filename's extension
static uint32_t getFileType(const char* filename)
{
	const char* ext = filename;
	size_t length = strlen(filename);

	while(ext < (ext+length) && *ext++ != '.');

	if(strncmp(ext, "gim", 3) == 0)
		return GIM_TEXTURE;
	if(strncmp(ext, "smd", 3) == 0)
		return SMD;
	else
		return OTHER;

}

// Returns 1 if true, 0 if false
static int pathIsDirectory(char* path)
{
	struct stat s;
	if(stat(path, &s) == 0)
	{
		if(S_ISDIR(s.st_mode))
			return !(_access(path, F_OK) == ENOTDIR);
	}

	return 0;
}

/*
 * DoExtract(char *)
 * Extract a PAC file. The extraction directory is the name of the PAC file with an appended underscore.
 */
static void DoExtract(char *path)
{
	uint8_t* 		data;
	pacHeader_t 	header;
	entryHeader_t* 	entries;
	char 			folderName[255];
	char			*PACfilename = path;


	FILE *PACfile = fopen(path, "rb");
	if(!PACfile)
	{
		printf("Error opening %s\n", path);
		return;
	}

	fread(&header, sizeof(pacHeader_t), 1, PACfile);

	if(strncmp(header.magic, "PAC\0", 4) == 0)
	{
		entries = malloc(sizeof(entryHeader_t) * header.numEntries);
		fread(entries, sizeof(entryHeader_t), header.numEntries, PACfile);
		printf("Extracting %d entries from %s... ", header.numEntries, PACfilename);

		/* Directory must be different than file name */
		snprintf(folderName, 255, "%s_", path);
		mkdir(folderName);
		chdir(folderName);

		for(int i = 0; i < header.numEntries; i++)
		{
			char filename[32];
			fseek(PACfile, entries[i].offset, SEEK_SET);
			data = malloc(entries[i].length);
			fread(data, 1, entries[i].length, PACfile);

			switch(entries[i].fileType)
			{
				case GIM_TEXTURE:
					snprintf(filename, 32, "%.*s.gim", 16, entries[i].name);
					break;

				case SMD:
					snprintf(filename, 32, "%.*s.smd", 16, entries[i].name);
					break;

				default:
					snprintf(filename, 32, "%.*s.%03x", 16, entries[i].name, entries[i].fileType);
			}

			FILE *entryfile = fopen(filename, "wb");
			if(!entryfile)
			{
				printf("Error creating %s\n", entries[i].name);
				free(data);
				continue;
			}

			fwrite(data, 1, entries[i].length, entryfile);
			free(data);
			fclose(entryfile);

			printf("\rExtracting %d entries from %s... %.0f%%", header.numEntries, PACfilename, ((double)(i+1)/(double)header.numEntries)*100);
		}

		printf("\rExtracting %d entries from %s... done!\n", header.numEntries, PACfilename);
		chdir(cwd);
		free(entries);
	}
	else
	{
		printf("Not a PAC file.\n");
	}

	fclose(PACfile);
}

/*
 * DoCreate(char *)
 * Create a PAC file using all files in the directory given by the argument.
 * Directory name must be in the format "SOMENAME.PAC_", where the resulting PAC file will be named "SOMENAME.PAC"
 * Note that the appended underscore is required!
 */
static void DoCreate(char *path)
{
	struct dirent*	direntPtr;
	DIR* 			dirPtr;
	FILE*			entryFile;
	pacHeader_t		head;
	entryHeader_t	entries[512];
	entryBuffer_t   entryBuffers[512];
	char			PACFilename[255];
	FILE*			PACFile;
	int				totalPACSize = 0;

	if(!(path[strlen(path)-4] == 'P' && path[strlen(path)-3] == 'A' &&
		 path[strlen(path)-2] == 'C' && path[strlen(path)-1] == '_'))
	{
		printf("Invalid directory name \"%s\". Must be in the format <something>.PAC_\n", path);
		return;
	}

	strncpy(head.magic, "PAC\0", 4);
	head.numEntries = 0;
	head.unknown1 = 0x00;
	head.unknown2 = 0x20;

	// Remove '.PAC_'
	snprintf(PACFilename, 255, "%.*s", strlen( filenameFromPath(path) ) - 5, filenameFromPath(path));

	// archiveName might get truncated, but this is required to make it fit.
	strncpy(head.archiveName, PACFilename, 16);
	strncat(PACFilename, ".PAC", 255);

	if(pathIsDirectory(path))
	{
		PACFile = fopen(PACFilename, "wb");
		if(!PACFile)
		{
			printf("Error creating %s\n", PACFilename);
			return;
		}

		dirPtr = opendir(path);
		chdir(path);

		printf("Building %s... ", PACFilename);

		while(( direntPtr = readdir(dirPtr) ) != NULL)
		{
			if( !pathIsDirectory( direntPtr->d_name ) )
			{
				int i = head.numEntries;

				entryFile = fopen(direntPtr->d_name, "rb");
				if(!entryFile)
				{
					printf("Error opening %s, skipping...\n", direntPtr->d_name);
					continue;
				}

				fseek(entryFile, 0, SEEK_END);
				entries[i].length = entryBuffers[i].length = ftell(entryFile);
				fseek(entryFile, 0, SEEK_SET);

				entries[i].fileType = getFileType(direntPtr->d_name);
				if(entries[i].fileType == SMD)
					entries[i].extra = 0x01; // Not sure what the game needs this for, but it has to be set.
				else
					entries[i].extra = 0x00;

				char temp[16] = {};
				removeFileExtention(direntPtr->d_name, temp);
				strncpy(entries[i].name, temp, 16);

				// NOTE: offset needs to be updated when the total number of entries is known! This happens later...
				entries[i].offset = totalPACSize;
				entryBuffers[i].data = malloc(entryBuffers[i].length);
				fread(entryBuffers[i].data, entryBuffers[i].length, 1, entryFile);

				totalPACSize += entries[i].length;
				if(totalPACSize%16 != 0) // Entry data must be aligned to 16-byte offsets, so padding has to be made
					totalPACSize += (16 - totalPACSize%16);

				head.numEntries++;
				fclose(entryFile);
			}
		}
		closedir(dirPtr);

		printf("done!\n");
		printf("Saving %s... ", PACFilename);

		fwrite(&head, sizeof(pacHeader_t), 1, PACFile);

		/* Adjust offsets since data will start after all the headers */
		int firstOffset = sizeof(pacHeader_t) + (sizeof(entryHeader_t) * head.numEntries);
		for(int i = 0; i < head.numEntries; i++)
		{
			entries[i].offset += firstOffset;
		}
		fwrite(entries, sizeof(entryHeader_t), head.numEntries, PACFile);

		/* Write data for each file entry */
		for(int i = 0; i < head.numEntries; i++)
		{
			fwrite(entryBuffers[i].data, entryBuffers[i].length, 1, PACFile);

			/* Pad to 16-byte alignment */
			if(ftell(PACFile)%16 != 0)
			{
				for(int j = 0; j < (16 - entries[i].length%16); j++)
					fputc(0, PACFile);
			}

			free(entryBuffers[i].data);
		}

		printf("done! Contains %d entries.\n", head.numEntries);
		chdir(cwd);
		fclose(PACFile);
	}
	else
	{
		printf("%s is not a directory.\n", path);
	}

}

int main(int argc, char *argv[])
{
	if(argc < 3)
	{
		printf("pactool for Initial D Special Stage\nUsage:\nCreate:\t\t %s c <directory> ...\nExtract:\t %s e <.pac file> ...\n", argv[0], argv[0]);
		return EXIT_FAILURE;
	}

	getcwd(cwd, 260);

	if(tolower(*argv[1]) == 'e')
	{
		for(int i = 2; i < argc; i++)
			DoExtract(argv[i]);
	}

	else if(tolower(*argv[1]) == 'c')
	{
		for(int i = 2; i < argc; i++)
			DoCreate(argv[i]);
	}

	else
	{
		printf("Invalid option '%c'\n", argv[2]);
		printf("Usage:\nCreate:\t\t %s c <directory> ...\nExtract:\t %s e <.pac file> ...\n", argv[0], argv[0]);
		return EXIT_FAILURE;
	}

    return 0;
}
