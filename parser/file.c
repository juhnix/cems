
/*! \file
    \brief This file contains all the file handling routines
    */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "parser.h"
#include "parser_local.h"

/*! \if API
  \brief Function that converts data to a valid format to be written to the file

  The aim of this function is to convert the data(name) given in the
  struct entry to a string that can be written to the configuration
  file upon saving. The function puts quotes if they are needed (if \c
  token contains whitespace, =, ', or ") and escapes characters inside
  the quotes if necessary. \see gettoken()

  \note puttoken uses double quotes for the purpose of quoting.

  \param token The string to be formatted.

  \retval string The malloc'ed string that is the formated output of \c token.	
  \retval NULL on failure

  \endif
*/
char *puttoken(const char *token)
{
    if(token == NULL)
	return NULL;

    int toklen = strlen(token);

    //special case for empty string
    if(toklen == 0)
	{
	    char *str = calloc(2+1, sizeof(char));
	    if(str == NULL)
		{
		    exit(-1);
		}
	    str[0] = str[1] = '"';
	    str[2] = '\0';
	    return str;
	}

    //first count special characters
    int spaces = 0, quotes = 0, bslashes = 0;

    const char *current = token;
    do
	{
	    current = strpbrk(current, " \t\n\r\"\'\\");
	    if(current != NULL)
		{
		    switch(current[0])
			{
			case ' ' :
			case '\t':
			case '\n':
			case '\r':
			    spaces++;
			    break;

			case '"' :
			case '\'':
			    quotes++;
			    break;

			case '\\':
			    bslashes++;
			    break;

			default:
			    assert(0); //should never hit this
			}
		    current++;
		}
	}
    while(current != NULL);

    if(spaces != 0 || quotes != 0)
	{//if there are spaces or quotes we will need to quote them
	    toklen += 2;
	    //quotes ands backslashes will need an extra byte for the escaping backslash
	    toklen += quotes + bslashes;
	}

    char *string = calloc(toklen+1, sizeof(char));
    if(string == NULL)
	{
	    exit(-1);
	}

    int written = 0, seen = 0;
    if(spaces != 0 || quotes != 0)
	{
	    string[written++] = '"';
	}

    char *needs_escape = NULL;
    do
	{
	    if(spaces != 0 || quotes != 0)
		{//we only need to escape things if we're inside quotes
		    needs_escape = strpbrk(token+seen, "\"\\");
		}

	    int writelen = (int)((((int)needs_escape)? ((int)needs_escape - (int)token) : (int)strlen(token)) - seen);
	    strncpy(string+written, token+seen, writelen);
	    seen += writelen;
	    written += writelen;

	    if(needs_escape != NULL)
		{
		    string[written++] = '\\';
		    string[written++] = token[seen++];
		}
	}
    while(needs_escape);

    if(spaces != 0 || quotes != 0)
	{
	    string[written++] = '"';
	}

    string[written] = '\0';

    return string;
}


/*! \if API

  \brief Used to calculate the width of a line in the file

  The function calculates the number of bytes that are required to store a line in the file where \c fp represents an open file. The returned length includes the newline also. If the pointer has reached the eof without a newline, then the returned length will be the actual line width plus \c one. This has been done for the provision of a null character in the end of the stored string.

  \param fp The opened file whose current position is used to calculate the line width

  \retval width The memory to be allocated to store the entire line(with newline)

  \note This function will make sure that \c fp is back where it was, so don't worry about that. But, here we won't reset the eof status of the file. Beware of that !

  \endif
  */
static int linewidth(FILE *fp)
{
	int width;
	char ch;

	width = 0;
	while(fread(&ch,1,1,fp))
	{
		width++;
		if(ch == '\n')
			break;
	}
	
	fseek(fp, -width, SEEK_CUR);

	if(feof(fp))
		width++;

	return width;
}

/*! \if API

  \brief Function that converts all \c null characters to \c space

  After reading a line from a file, we need to mark the end of the line with null character as we do for normal null terminated strings. But, if the line actually has a null(when it was in the file), then our string termination would occur prematurely. To prevent that, we're converting all the \c null characters to \c space.

  \param line The start address of the string
  \param width The width of the line

  \retval void

  \endif
  */
static inline void null2space(char *line, int width)
{
	while(width--)
		if(*line == '\0')
			*line = ' ';
}

/*! \if API

  \brief Extract a line from the file

  The funtion tries to extract a line of text from the file and converts it into a null-terminated string.

  \param fp The opened file

  \retval line The malloc'ed string that contains a line of text read from \c fp

  \note Here the \c newline character is replaced with \c null character to terminate the string. In case if the last line doesn't end with a \c newline, the an extra byte is allocated for \c null and this is done by \c linewidth().

  \endif
  */
char *parsGetline(FILE *fp)
{
	int width;
	char *line;

	width = linewidth(fp);
	if(width == 0)
		return NULL;

	line = malloc(width);
	if(line == NULL)
		exit(-1);
		
	fread(line, width, 1, fp);
	null2space(line,width); // convert all null to space
	line[width - 1] = '\0';

	return line;
}

/*! \if API

  \brief Function that writes a side comment and a newline to a file.

  \param fp The opened file pointer at which the comment will be written
  \param entry The entry whose side comment needs to be written

  \endif
  */
static inline void write_side(FILE *fp, struct entry *entry)
{
	if(entry->side != NULL)
		fprintf(fp, "#%s", entry->side);
	fprintf(fp, "\n");
}

/*! \if API

  \brief Function that writes a top comment to a file

  \param fp The opened file pointer at which the comment will be written
  \param entry The entry whose top comment needs to be written

  \endif
  */
static inline void write_top(FILE *fp, struct entry *entry)
{
	char *comment;
	const char *front;

	if(entry->top)
	{
		front = (entry->type == ENTRY_LINE) ? "\t#" : "#" ;

		fprintf(fp, "%s", front);

		for(comment = entry->top ; *comment ; comment++)
		{
			fwrite(comment, 1, 1, fp);
			if(*comment == '\n')
				fprintf(fp, "%s", front);
		}

		fprintf(fp, "\n");
	}
}

/*! \if API

  \brief Function that writes an orphan comment to a file

  \param fp The opened file pointer at which the comment will be written.
  \param orphan The orphan comment to be written

  \endif
*/
static inline void write_orphan(FILE *fp, char *orphan)
{
	fprintf(fp, "#");

	while(*orphan)
	{
		fwrite(orphan, 1, 1, fp);
		if(*orphan == '\n')
			fprintf(fp, "#");
		orphan++;
	}
}

/*! \if API

  \brief Function used to write all the information contained within a node and its relatives to a file

  This function is responsible for writing all the \c sub and \c next
  entries to a file. It takes care of flushing the configuration file
  date on to a file with proper formatting. The function calls itself
  recursively to flush the entries to the file in proper order. Groups
  are recognized within the function and written properly while
  comments are handled by write_side() and write_top().

  \param fp The file pointer where the data needs to be written to
  \param entry The starting entry from where to write data from

  \endif
  */
static void flushentries(FILE *fp, struct entry *entry)
{
    char *tmp = NULL;

    if (entry == NULL)
	return;

    write_top(fp, entry);

    switch(entry->type)
	{
	case ENTRY_GROUP:
	    fprintf(fp, "[%s] ", tmp = puttoken(name(entry)));
	    break;
	case ENTRY_LINE:
	    fprintf(fp, "\t");
	    break;
	case ENTRY_VALUE:
	    fprintf(fp, "%s ", tmp = puttoken(name(entry)));
	    break;
	case ENTRY_KEY:
	    fprintf(fp, "%s=", tmp = puttoken(name(entry)));
	    break;
	default:
	    break;
	}
    
    if(tmp)
	free(tmp);

    if (entry->type == ENTRY_GROUP)
	write_side(fp, entry);

    flushentries(fp, entry->sub);
    
    if (entry->type == ENTRY_LINE)
	write_side(fp, entry);

    if (entry->type == ENTRY_GROUP)
	fprintf(fp, "\n");
    
    flushentries(fp, entry->next);  // tail recursion - can be optimized by compiler
}

/*! \if API

  \brief Function that is used to duplicate(recursively) an entry.

  This funtion is used to duplicate an entry completely. Completely
  means that the function goes recursively through each \c next and \c
  sub nodes.

  \param prev The address of the struct entry pointer where the new entry need to be stored
  \param oldentry The entry that need to be duplicated

  \endif
  */
void dupentries(struct entry **prev, struct entry *oldentry)
{
    struct entry *newentry;

    if (oldentry == NULL)
	return;

    newentry = *prev = calloc(sizeof(*newentry), 1);
    if (newentry == NULL)
	exit(-1);

    if (oldentry->name) {
	newentry->name = strdup(oldentry->name);
	if (newentry->name == NULL)
	    exit(-1);
    }

    if (oldentry->top) {
	newentry->top = strdup(oldentry->top);
	if (newentry->top == NULL)
	    exit(-1);
    }

    if (oldentry->side)	{
	newentry->side = strdup(oldentry->side);
	if (newentry->side == NULL)
	    exit(-1);
    }

    newentry->type = oldentry->type;

    dupentries(&newentry->sub, oldentry->sub);

    dupentries(&newentry->next, oldentry->next);  // compiler can optimize this tail recursion
}

/*!

  \brief Copy an entire <tt>struct parsefile</tt> with a new name.

  \param oldfile The parsefile that need to be duplicated
  \param newname The name of the new parsefile

  \retval newfile The duplicate parsefile
  \retval NULL If some non-fatal error has occurred

  */
struct parsefile *parsecopy(struct parsefile *oldfile, char *newname)
{
	struct parsefile *newfile;

	newfile = malloc(sizeof(*newfile));
	if(newfile == NULL)
		exit(-1);

	newfile->name = (newname) ? strdup(newname) : NULL ; // not checking the success of strdup!!

	newfile->interpreter = (oldfile->interpreter) ? strdup(oldfile->interpreter) : NULL;
	newfile->orphan = (oldfile->orphan) ? strdup(oldfile->orphan) : NULL;

	newfile->handler = oldfile->handler;

	dupentries(&newfile->root, oldfile->root);

	return newfile;
}

/*! \brief Used to save a configuration file

    This function can be used to save a file opened using parseopen either with the same name or different. The function can also keep a backup copy of the overwritten(only if overwritten) file.

    If you want to save the changes made to the original file, then we
    specify the second argument as \c NULL. If we don't want to play
    with the original file and rather would be comforatable with
    saving the configuration in a different name, then we specify the
    name of the file to which the configuration has to be saved in the
    second argument.  If we specify the third argument, any file
    overwritten by this function will be saved in the name specifed by
    \c backup before writing.

    In case you are trying to save an unnamed file, second argument \c name is mandatory or otherwise the function will fail.

    \param file the opened parsefile handle
    \param name the name of the file to write the contents, otherwise left blank
    \param backup the name of the backup file

    \retval file if we could successfully save the file
	    NULL on failure

    \note If \c backup is being specified, any existing file with the same name will be overwritten without mercy. As a programmer its your responsibility that you specify a unique non-existing file name.

*/
struct parsefile *savefile(struct parsefile *file, char *name, char *backup)
{
	FILE *fp;

	if(name == NULL)
		name = file->name;

	if(name == NULL)
		return NULL;

	if(backup)
	{
		unlink(backup);
		rename(name, backup);
	}

	fp = fopen(name, "w");
	if(fp == NULL)
		return NULL;

	if(file->interpreter != NULL)
		fprintf(fp, "#!%s\n", file->interpreter);

	flushentries(fp, file->root);

	if(file->orphan)
		write_orphan(fp, file->orphan);

	fclose(fp);

	return file;
}


