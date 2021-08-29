
/*! \file

    \brief The file that handles all the modifications of the configuration file

    */
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "parser_local.h"

/*! \brief Function used to add a new group
    
     The function adds a new group with name given by \c name after the entry \c group

     \param group the group entry to which we need to append the new group
     \param name the name of the new group

     \retval group the newly created group entry
     */
struct entry *addgroup(struct entry *group, char *name)
{
	struct entry *grp;

	grp = calloc(sizeof(*grp), 1);
	if(grp == NULL)
		exit(-1);

	grp->name = strdup(name); // We also might need to validate
	grp->type = ENTRY_GROUP;

	grp->next = group->next;
	group->next = grp;

	return grp;
}

/*! \brief Function to add a new group at \c index.

    This function adds a new group with name given by \c name at index given by \c index.

    \param file the parsefile in which to add the new group
    \param index at which the group is to be added. Give \b -1 to add to the end of list.
    \param name the name of the group

    \retval group the newly created group entry

*/
struct entry *addgroup_pos(struct parsefile *file, int index, char *name) // could be changed
{
	struct entry *root, *group;

	root = file->root;

	group = calloc(sizeof(*group), 1);
	if(group == NULL)
		exit(-1);

	group->name = strdup(name);
	group->type = ENTRY_GROUP;
	
	if(!index)
	{
		group->next = root;
		file->root = group;

		return group;
	}

	while(--index)
	{
		if(!root->next)
			break;
		root = root->next;
	}
	
	group->next = root->next;
	root->next = group;

	return group;
}

/*! \brief Function to add a new(empty) line next to an existing group or line.

    This function can be used to add a new(empty) line next to a group or a line. If \c entry is a line, then the new line will be added as the next line of \c entry. If \c entry is a group, the new line will be added as the first line of the group. If \c entry is neither, no new line will be added and \c NULL will be returned.

    \param entry to which the new line need to be added.

    \retval line entry of the newly created line
            NULL if some error occurred.
	    */
struct entry *addline(struct entry *entry) // BEWARE: this will add only an empty line 
{
	struct entry *line;

	line = calloc(sizeof(*line), 1);
	if(line == NULL)
		exit(-1);

	line->name = strdup(""); 
	if(line->name == NULL)
		exit(-1);

	line->type = ENTRY_LINE;

	switch(type(entry))
	{
	case ENTRY_LINE:
		line->next = entry->next;
		entry->next = line;
		break;
	
	case ENTRY_GROUP:
		line->next = entry->sub;
		entry->sub = line;
		break;

	case ENTRY_KEY: // We should never reach here
	case ENTRY_VALUE:
	default:
		free(line->name);
		free(line);
		return NULL;

	}

	return line;
}

/*! \if API

  \brief Function that is used to rebuild a line

  The fucntion is used to rebuild a modified line. ie, If the user had added/removed any key/value from a line, the ENTRY_LINE parent node should also be corrected. The user normally calls reformat() to do this work, which ultimately calls this function. In buildline(), we traverse recursively througth the various keys and values and build the \c line as we go on. The built line is returned through \c line argument itself.

  \note If entry is \c NULL, \c *line is not modified at all.

  See exmaple code below to know how to use the function:

  \code

  char *str = NULL;

  buildline(entry, &str);
  if(str == NULL)
  	printf("Empty line\n");
  else
  	printf("The new line is %s\n", str);

  \endcode

  \warning The function does not check the type of \c entry. It is the responsiblity of the user to provide either a key or a value to the function. Never give group or line to it. 

  \param entry The key/value entry that need to be appended to \c line
  \param line The line to be built

  \endif
  */
static void buildline(struct entry *entry, char **line)
{
	int c;
	char *val;

	if(entry == NULL)
		return;

	val = puttoken(name(entry));

	c = (*line) ? strlen(*line) : 0 ;

	*line = realloc(*line, c + strlen(val) + 2); // including a null and a space/'=' in the end
	if(*line == NULL)
		exit(-1);

	strcpy(*line + c, val);
	free(val);

	strcat(*line , (type(entry) == ENTRY_KEY) ? "=" : " " );

	buildline(entry->sub, line);
	buildline(entry->next, line);
}

/*! \brief Function to be called to reformat the name of a line entry.

    This function can be used by the user to reformat a line entry after adding/removing values inside it.

    \param line entry which need to be reformatted

*/
void reformat(struct entry *line)
{
	char *nline = NULL;

	if(type(line) != ENTRY_LINE)
		return;

	buildline(line->sub, &nline);
	if(nline == NULL) // this will happen when all the values have been deleted.
		nline = strdup("");

	free(line->name);

	line->name = nline;
}

/*! \brief Function to add a new value.

    This function can be used to add a new value to a group, line, value or a key. When a new value is added to group or line, it becomes the first value of the first line in that group. If we are adding the value to a key, the value of the key is changed to this new one. Any value existed before will be removed. If the new value is added to a value, the new value will be the next value of it.

    \param entry to which the \c val need to be added
    \param val the name of the value

    \retval value entry of the new value if everything goes fine
            NULL if some error occurred

*/            
struct entry *addvalue(struct entry *entry, char *val) // entry should be LINE/KEY/VALUE
{
	struct entry *value;

	value = calloc(sizeof(*value), 1);
	if(value == NULL)
		exit(-1);

	value->name = strdup(val); // We also might need to validate
	value->type = ENTRY_VALUE;

	switch(type(entry))
	{
	case ENTRY_VALUE:
		value->next = entry->next;
		entry->next = value;
		break;

	case ENTRY_KEY:
		if(entry->sub)
			free(entry->sub);
		entry->sub = value;
		break;

	case ENTRY_LINE:
		value->next = entry->sub;
		entry->sub = value;
		break;

	case ENTRY_GROUP: // we should not be using this case
		value->next = entry->sub->sub;
		entry->sub->sub = value;
		break;

	default: // We should never reach here. But only god knows what all bugs still remain.
		free(value->name);
		free(value);
		return NULL;
	}

	return value;	
}


/*! \if API

  \brief Function to add a new key

  The function creates a new \c entry of type \c ENTRY_KEY and fills its name as given by \c k. Its all other fields are \c NULL.

  \param entry The entry to which we need to append the newly created entry(key)
  \param k The name of the key

  \retval key The newly created and appended key entry
          NULL If the type of \c entry is unknown

  \warning When the new \c ENTRY_KEY is created by the function, its \c sub entry is \c NULL, which is something that would be unexpected for an end user. Always make sure that you create a proper \c sub entry (value) for the new entry after calling this function. 

  \endif
  */
static struct entry *addkey(struct entry *entry, char *k) // entry should be LINE/VALUE
{
	struct entry *key;

	key = calloc(sizeof(*key), 1);
	if(key == NULL)
		exit(-1);

	key->name = strdup(k); // We also might need to validate
	key->type = ENTRY_KEY;

	switch(type(entry))
	{
	case ENTRY_KEY: // we should not be using this case

	case ENTRY_VALUE:
		key->next = entry->next;
		entry->next = key;
		break;

	case ENTRY_LINE:
		key->next = entry->sub;
		entry->sub = key;
		break;
	
	case ENTRY_GROUP:
		key->next = entry->sub->sub;
		entry->sub->sub = key;
		break;

	default: // We should never reach here
		free(key->name);
		free(key);
		return NULL;
	}

	return key;	
}

/*! \brief Function to add new key/value pair.

    This function adds a new key/value pair next to a group, line, key or value. All the working is the same as that of addvalue() except that if \c entry is a key, the new key/value pair is added as the next entry and not as a sub entry.

    \param entry the one to which the new pair is to be added
    \param key the name of the key
    \param val the value of the key/value pair

    \retval key entry of the new key/val pair
            NULL if some error occurred
*/
struct entry *addkeyval(struct entry *entry, char *key, char *val)
{
	struct entry *k;

	k = addkey(entry, key);

	if(k)
		addvalue(k, val);

	return k;	
}

/*! \brief Function to set/reset the top comment.

    This function is used to set/reset the top comment of a line or group. If we need to reset the top comment, we pass \c NULL to \c top. If there were any previous comments, its memory will be freed.

    \param entry the group/line entry to which operate on
    \param top the new top comment. If \c NULL is provided, the top comemnt is reset.

*/
void settop(struct entry *entry, char *top)
{
    switch(type(entry))	{
    case ENTRY_GROUP:
    case ENTRY_LINE:
	if (entry->top != NULL)
	    free(entry->top);

	entry->top = (top) ? strdup(top) : NULL;
	
	break;
    default:
	break;
    }
}


/*! \brief Function to set/reset the side comment.

    This function is used to set/reset the side comment of a line or group. If we need to reset the side comment, we pass \c NULL to \c side. If there were any previous comments, its memory will be freed.

    \param entry the group/line entry to which operate on
    \param side the new side comment. If \c NULL is provided, the side comemnt is reset.

*/
void setside(struct entry *entry, char *side)
{
    switch(type(entry)) {
    case ENTRY_GROUP:
    case ENTRY_LINE:
	if(entry->side != NULL)
	    free(entry->side);
	
	entry->side = (side) ? strdup(side) : NULL;
	break;
    default:
	break;
    }
}

/*! \brief Function to set/reset the orphan comment.

    This function is used to set/reset the orphan comment of a file. If we need to reset the orphan comment, we pass \c NULL to \c orphan. If there were any previous comments, its memory will be freed.

    \param file the file to which operate on
    \param orphan the new orphan comment. If \c NULL is provided, the orphan comemnt is reset.

*/
void setorphan(struct parsefile *file, char *orphan)
{
	if(file->orphan != NULL)
		free(file->orphan);

	file->orphan = (orphan) ? strdup(orphan) : NULL;
}

/*! \brief Function to set/reset the interpreter for the configuration file.

    This function is used to set/reset the interpreter of a file. If we need to reset the interpreter, we pass \c NULL to \c interpreter. If there were any previous value, its memory will be freed.

    \param file the file to which operate on
    \param interpreter the new interpreter. If \c NULL is provided, the interpreter is reset.

*/
void setinterp(struct parsefile *file, char *interpreter)
{
	if(file->interpreter != NULL)
		free(file->interpreter);

	file->interpreter = (interpreter) ? strdup(interpreter) : NULL;
}

/*! \brief Function to change the name of an entry.

    This function is used to change the name of a group, key or a value.

    \note This function cannot be used to change the name of line entry. Instead, after changing the keys/values of a line, we need to run reformat() over the line. 

    \param entry the group/key/value entry to which operate on
    \param name the new name. This field should never be \c NULL

*/
void setname(struct entry *entry, char *name)
{
	if(type(entry) == ENTRY_LINE) // we are not supposed to manipulate it directly
		return;

	free(entry->name);

	entry->name = strdup(name); // We might wanna do some validation
}

/*! \brief Function to change the file name of a struct parsefile.

    This function is used to change the file name in a parsefile structure.

    \param file the struct parsefile on which to operate
    \param name the new name. \c NULL can be used to reset the same
*/
void setfilename(struct parsefile *file, char *name)
{
	if(file->name)
		free(file->name);

	file->name = (name) ? strdup(name) : NULL;
}

/*! \brief Function to remove a sub entry

    This function can be used to recursively remove the sub entry of an entry.

    \param entry the entry whose sub entry is to be removed

    */
void removesub(struct entry *entry) // entry->sub should not be null
{
	freeentries(entry->sub);
	entry->sub = NULL;

	//User has to manually do reformat()

}


/*! \brief Function to remove a next entry

    This function can be used to recursively remove the next entry of an entry.

    \param entry the entry whose next entry is to be removed

    */
void removenext(struct entry *entry) // entry->next should not be null
{
	struct entry *next;

	next = entry->next;
	entry->next = entry->next->next;

	freeentries(next);	
}


/*! \brief Function to remove the root entry

    This function can be used to recursively remove the root entry of a \c parsefile

    \param file the parsefile whose root entry need to be removed

*/
void removeroot(struct parsefile *file) // file->root should not be null
{
	struct entry *root;

	root = file->root;
	file->root = file->root->next;

	freeentries(root);
}


