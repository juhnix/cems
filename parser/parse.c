
/*! \file
    \brief This file contains the basic startup routines for users.
    */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "parser_local.h"

/*! \if API

  \brief The library default parser handler

  While doing the automatic processing of the configuration file, a situation may arise in which there is not handler routing registered for a particular group. In that case the library calls the default handler registered forthe file through the third argument of parseopen(). But, if the user had not specied the default handler for thatconfiguration file, the this library function defaultparser() is called by the parseopen() during automatic processing.

  \note Its not a good thing to see this function getting invoked. When automatic processing is being done, its the responsibility of the user to provide a default handler and thus handle the unknown groups. 

  \param group The group entry that need to be handled
  \param handler The registered phandler structure which in this case would be \c NULL
  \param file The parsefile structure of the file being processed

  \retval 0 Always returns \c 0(zero)

  \endif
  */
static int defaultparser(struct entry *group, struct phandler *handler, struct parsefile *file)
{
	fprintf(stderr,"parser.c::defaultparser() : %s:%s:\n\n", file->name, group->name);

	return 0;
}

/*! \if API

  \brief Function that is responsible for finding out and running the proper handler function for a group entry

  The function iterates through the \c list and finds the matching handler for the \c group. If no matching entry is found, then the file default handler is called.

  \param group The group for which the handler is to be found
  \param list The list of handlers in which to find a match
  \param file The parsefile of the \c group

  \retval value The return value of the handler

  \endif
  */
static int runhandler(struct entry *group, struct phandler *list, struct parsefile *file)
{
	while(list->group) // last entry should have its group as null
	{
		if(list->group && !strcmp(group->name, list->group))
			if(list->handler)
				return list->handler(group, list, file);
		list++;
	}
	
	return file->handler(group, NULL, file);
}

/*! \if API

  \brief Function that finds a \c key in a \c line

  This function can be used to find a key inside a line with an index of occurance. ie, if \c occur is 0, the function returns the first occurance of such a key. If -1 is given, it gives the last one.

  \param line The \c line entry in which to search for.
  \param key The name of the key
  \param occur The index of occurance of the key

  \retval key The matching key entry
  	  NULL If not match is found

  \endif
  */
static inline struct entry * key2value(struct entry *line, char *key, int occur)
{
	struct entry *val;

	foreach(line, val)
		if(iskey(val) && !strcmp(name(val), key) && !occur--)
			return val->sub;

	return NULL;
}

/*! \if API
  \brief Function to find a value(or key) at an \c index

  The function returns the entry at index given by \c index.

  \note if \c index exceeds the available ones, the function returns the last one found.

  \param line The line in which to search for
  \param index The required index

  \retval entry The discovered entry

  \endif
  */
static inline struct entry * findvalue(struct entry *line, int index)
{
	struct entry *val;

	foreach(line, val)
		if(!index--)
			break;

	return val;
}

/*! \brief Function to get the key/value inside a line.

    The function tries to find out the value associated with a key or an entry at \c index.
    If \c key is specified, the returned entry will be the value associated with that key, otherwise \c NULL.
    If \c key is \c NULL, then the value located at \c index will be returned.

    \param line the line in which to search the key/value
    \param key the name of the key if we are looking for its value, otherwise \c NULL
    \note When \c key is <tt> not NULL </tt>, \c index specifies the key that matches \c key at the \c index time. \a ie, when \c index is \b 0, the first key that matches \c key will be used.
    \param index the index of the key/value to be found.

    \retval NULL if no match was found, otherwise
            value if \c key was <tt> not NULL </tt>
	    entry if \c key was NULL. Return value can be eiher key or value.
*/
struct entry * getkeyorvalue(struct entry *line, char *key, int index)
{
	if(key)
		return key2value(line, key, index);

	return findvalue(line, index);
}

/*! \if API
  \brief Get the interpreter for this configuration script file

  The function tries to find out whether the first line in the file has an interpreter specified. If its specified, an malloc'ed version is returned and the <tt> FILE * fp </tt> is advanced accordingly. Otherwise the function returns \c NULL and \c fp stays on top of the file.

  \param fp The opened file

  \retval NULL if no interpreter could be found
          interpreter The malloc'ed version of the interpreter.
  
  \endif
*/
static char *getinterp(FILE *fp)
{
	char *line;
	
	if((line = parsGetline(fp)) == NULL)
		return NULL;

	if(line[0] == '#' && line[1] == '!')
	{
		char *interpreter = strdup(&line[2]); // should we exit(-1) upon failure?
		free(line);
		return interpreter;
	}

	fseek(fp, 0, SEEK_SET);
	return NULL;
}

/*! \brief The function to be called for loading/creating a configuration file

    This function is to be called by the user to load and parse a configuration file. When this function is called, each lines in the opened file is iterated through and then converted into various <tt> struct entry </tt> values internally.
    
    The user can also create a new (unnamed) configuration file by passing the \c filename argument as \c NULL.

    \param filename The name of the file. When creating a new file, this should be \c NULL
    \param list The list of handler routines
    \param handler The default handler for the opened file

    \retval NULL if the open failed, otherwise a valid <tt> struct parserfile * </tt>
    */
struct parsefile * parseopen(char *filename, struct phandler *list, phandler handler)
{
	char *name, *orphan, *interpreter;
	struct entry *root;
	struct parsefile *file;

	root = NULL;
	name = NULL;
	interpreter = NULL;
	orphan = NULL;

	if(filename != NULL)
	{
		FILE *fp;

		fp = fopen(filename,"r");
		if(fp == NULL)
			return NULL;

		name = strdup(filename);
		if(name == NULL)
			exit(-1);

		interpreter = getinterp(fp);

		root = transform(fp, &orphan);

		fclose(fp);
	}

	file = malloc(sizeof(*file));
	if(file == NULL)
		exit(-1);

	file->name = name;
	file->root = root;
	file->orphan = orphan;
	file->interpreter = interpreter;
	file->handler = (handler) ? handler : defaultparser;

	if(list == NULL)
		return file;

	while(root)
	{
		if(isgroup(root))
			runhandler(root, list, file);

		root = root->next;
	}	

	return file;
}


/*! \if API
  \brief Function used to free an entry completely(recursively)

  This function goes recursively through an entry freeing itself as well as its relatives.

  \param root The root entry from which to start the excavation
	
  \endif
  */
void freeentries(struct entry *root)
{
	struct entry *next;
	
	if(!root)
		return;

	if(root->name)  // This check can be removed.
		free(root->name);
	if(root->top)
		free(root->top);
	if(root->side)
		free(root->side);
	
	freeentries(root->sub);  // free all the subentries before freeing ourself.
	
	next = root->next;
	free(root); // Lets free this one now itself for helping compiler to optimise the next line.
	freeentries(next);  // This tail recursion can be easily optimized by compiler	
}

/*! \brief This function is called to close an already opened file.

    Calling parseclose() will free all the <tt> struct entry </tt> data present in the <tt> struct parsefile * file </tt> argument. After calling this function, \c file is no more a valid parsefile handle.

    \param file the parsefile handle to close

    */
void parseclose(struct parsefile *file)
{
	freeentries(file->root);

	if(file->name)
		free(file->name);
	
	if(file->interpreter)
		free(file->interpreter);

	if(file->orphan)
		free(file->orphan);

	free(file);
}
