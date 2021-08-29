
#ifndef __PARSER_H__
#define __PARSER_H__

/*! \file
    \brief This is the common header file to be used by both internal as well as external source files.

    */

#define MAX(x, y) ((x) < (y))? (y) : (x)

// file.c

struct parsefile;

struct parsefile *savefile(struct parsefile *file, char *name, char *backup);
struct parsefile *parsecopy(struct parsefile *oldfile, char *newname);

// format.c

// transform.c
#define ENTRY_GROUP     0  /*!< \brief Used for entry of group */
#define ENTRY_LINE      1  /*!< \brief Used for entry of line */
#define ENTRY_KEY       2  /*!< \brief Used for entry of key inside a line */
#define ENTRY_VALUE     3  /*!< \brief Used for entry of value inside a key or a line */

/*! \brief The structure that is used to represents all the elements in a configuration file.
    
    It includes groups, lines, keys and values.
    When a configuration file is opened with parseopen(), it goes through each line in the file and convert them to one or more struct entry values, unless the line is empty or top comment.\n
    \par Comments
    Comments are those text preceded by a \c #, unless the \c # is within a string. There are basically two types of comments:
    \li Top Comments
    \li Side Comments

    Top comments are those comments that are attached on to the top of a line. For example:
    \verbatim
# This is the top comment of mygroup.
[mygroup]

	# This is the top comment of a line under mygroup.
	key1 = value1
    \endverbatim

    Side comments on the other hand are those comments that are written at the end of a line as shown below:
    \verbatim
[mygroup] # This is a side comment of mygroup

	key1=value1 key2=value2 # This is a side comment of a line inside mygroup
	\endverbatim

	\par Types
	There are basically four types of entries:
	\li \c ENTRY_GROUP that represents each groups defined in the configuration file
	\li \c ENTRY_LINE that represents each line present inside the group
	\li \c ENTRY_KEY that represents the keys of a key/value pairs contained in a line
	\li \c ENTRY_VALUE that represents the values present in a line or a key

	\par Hierarchy
	When a configuration file is converted to various <tt> struct entry </tt> elements, they are arranged in a tree like structure.
	The top of the tree is the field named \c root of type <tt> struct entry * </tt> inside the <tt> struct parsefile * file </tt> returned by parseopen().
	\c root points to the first group inside the file followed by \c root->next in a linked list.
	For each group that is accessed, it will have a \c sub entry that represents the first line in the group that is also arranged in a linked list of all the lines.
	Under each line, the \c sub will point to the various entities within the line which can be keys or values which again is also grouped in a linked list.
	Each key will have a \c sub that points to the value associated with that key.\n\n
	As can be observed, there can be four levels of entries:
	\li <b> Level 1 </b> can contain only groups
	\li <b> Level 2 </b> can contain only lines
	\li <b> Level 3 </b> can contain either keys or values(not associated)
	\li <b> Level 4 </b> can contain only values(associated)
	
*/
struct entry
{
        char *name; /*!< This is the name of entry. It can be group name, key, value or the line itself.
		         \note This field will never be \c NULL */
	char *top;  /*!< This field represents the presence of top comment for entries of type \c ENTRY_GROUP
		         or \c ENTRY_LINE. If no comment exist, this field will be \c NULL.*/
	char *side; /*!< This field represents the presence of side comment for entries of type \c ENTRY_GROUP
			 or \c ENTRY_LINE. If no comment exist, this field will be \c NULL.*/
        int type; /*!< This represents the type of entry. The possible values are:
		     \li \c ENTRY_GROUP
		     \li \c ENTRY_LINE
		     \li \c ENTRY_KEY
		     \li \c ENTRY_VALUE*/
        struct entry *sub; /*!< If there exists an entry in the next lower level of hierarchy, this field will
			        point to that, but otherwise \c NULL.
				\note This is always null for entries of type \c ENTRY_VALUE. */
        struct entry *next; /*!<  If there exists an entry in the same level of hierarchy, this field will
			          point to that, but otherwise \c NULL.
			          \note This will always be null for entries of type \c ENTRY_KEY. */
};

// parse.c

struct phandler ;
struct parsefile ;

/*! \brief This is the prototype for the callback routines used for automatic file processing.

    \param group The group entry of the group to be processed
    \param handler Pointer to the <tt> struct phandler </tt> that caused this invocation
    \param file The <tt> struct parsefile * </tt> that represents the configuration file.

    \note If the execution of the routine has occurred due to default handler invocation, \c handler will be \c NULL.
    \warning The use of \c file is only to get the name of the file that's been parsed. In future this field may be removed.

*/
typedef int (*phandler)(struct entry *group, struct phandler *handler, struct parsefile *file);

/*! \brief This structure represents each file opened by the program using parseopen()

*/
struct parsefile
{
	struct entry *root; /*!< Points to the first group(called as root) in the file. */
	char *name; /*!< The name of the file as was given in parseopen() */
	char *interpreter; /*!< The interpreter for this configuration file */
	phandler handler; /*!< The default handler to be used for this file */
	char *orphan; /*!< Represents any orphan comment found in the file */
};

/*! \brief This structure is used to register handler for a particular group name

  */
struct phandler
{
	char *group; /*!< The name of the group to match */
	void *arg; /*!< The value to be used by the handler if required */
	phandler handler; /*!< The address of handler routine */
};

struct parsefile * parseopen(char *filename, struct phandler *list, phandler handler);
void parseclose(struct parsefile *file);
struct entry * getkeyorvalue(struct entry *line, char *key, int index);

/*! \brief This will iterate through each line present in a given group entry.

    \param group The group entry to be processed
    \param line The <tt> struct entry * </tt> to which each line entry is to be assigned
    */
#define foreachline(group,line) \
	for(line = group->sub ; line ; line = line->next)

/*! \brief This will iterate through each value present in a given line entry 

    \param line The line entry to be processed
    \param value The <tt> struct entry * </tt> to which each key/value entry is to be assigned
    */
#define foreach(line, value) \
	for(value = line->sub ; value ; value = value->next)

/*! \brief This macro checks if the \c entry is a group */
#define isgroup(entry) (((entry)->type) == ENTRY_GROUP)

/*! \brief This macro checks if the \c entry is a line */
#define isline(entry) (((entry)->type) == ENTRY_LINE)

/*! \brief This macro checks if the \c entry is a key */
#define iskey(entry) (((entry)->type) == ENTRY_KEY)

/*! \brief This macro checks if the \c entry is a value */
#define isvalue(entry) (((entry)->type) == ENTRY_VALUE)

/*! \brief This macro gets the name of \c entry */
#define name(entry) ((entry)->name)

/*! \brief This macro gets the value of the key \c entry */
#define keyvalue(entry) (name(entry->sub))

/*! \brief This macro gets the type of \c entry */
#define type(entry) ((entry)->type)

/*! \brief This macro gets the argument value contained within the <tt> struct phandler * hlist </tt> */
#define harg(hlist) ((hlist)->arg)

/*! \brief This macro gets the top comment of \c entry */
#define topcomment(entry) ((entry)->top)

/*! \brief This macro gets the side comment of \c entry */
#define sidecomment(entry) ((entry)->side)

// modify.c

struct entry *addgroup(struct entry *group, char *name);
struct entry *addgroup_pos(struct parsefile *file, int index, char *name);

struct entry *addline(struct entry *entry);
void reformat(struct entry *line);

struct entry *addvalue(struct entry *entry, char *val);
struct entry *addkeyval(struct entry *entry, char *key, char *val);

void settop(struct entry *entry, char *top);
void setside(struct entry *entry, char *side);
void setorphan(struct parsefile *file, char *orphan);
void setname(struct entry *entry, char *name);
void setfilename(struct parsefile *file, char *name);
void setinterp(struct parsefile *file, char *interpreter);

void removesub(struct entry *entry);
void removenext(struct entry *entry);
void removeroot(struct parsefile *file);

#endif

