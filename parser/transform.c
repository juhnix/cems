#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "parser.h"
#include "parser_local.h"

/*! \file
  \brief This file contains the routines that are used to tranform the file data into the library internal format
  
*/

/*! \if API
  \brief Checks whether the line is empty

  The function checks whether the line is empty of not. Any line that contains only whitespace character are considered to be empty.

  \param str The string to be checked.

  \retval 1 If \c str is empty
  \retval 0 otherwise

  \endif
*/
static int isempty(char *str)
{
    int status;
    char firstchar;
	
    status = str?sscanf(str, " %1c", &firstchar): EOF; //find first non-whitespace

    if( status == EOF || firstchar == '\0' )
	{
	    //no non-whitespace chars found
	    return 1;
	}
    return 0;
}

/*! \if API
  \brief Get a pointer to the end quote

  Given a pointer to a string starting with a quote char (' or ") returns a
  pointer to a matching closing quote, not counting escaped quotes.

  \param str The string to search in.

  \retval char* A pointer to one character after the closing quote

  \endif
*/
char *skipquotes(char *str)
{
    assert( str[0] == '\'' || str[0] == '"' );
    char *endquote = str;

    do {
	endquote = strchr(endquote+1, str[0]);
	if (endquote == NULL) {
	    //haven't found a closing str[0], let the terminating NULL close the str[0]
	    return (char*)(str + strlen(str));
	}
    }//keep going if the quote is escaped with backslash
    while (endquote[-1] == '\\' && endquote[-2] != '\\');

    return (char*)(endquote+1);
}

/*! \if API
  \brief Get maximum length of first token in str

  \param str The string upon which to operate
  \param tokstart The function will point this to the beginning of the token inside str  
  \param tokend The function will point this to the end of the token inside str

  \retval length A number greater than or equal to the length of this token as returned by gettoken() not including terminating NULL
  \retval -1 if there are no tokens.

  \note Current implementation gives 1 extra byte per escaped char.

  \endif
*/
static int gettoklen(char *str, char **tokstart, char **tokend)
{
    char firstchar;
    int status, offset;

    status = str?sscanf(str, " %n%1c", &offset, &firstchar): EOF; //find first non-whitespace  

    if( status == EOF || firstchar == '\0' )
	{
	    //no non-whitespace chars found
	    return -1;
	}
    *tokstart = str+offset;

    if( '"' == firstchar || '\'' == firstchar )
	{
	    *tokend = skipquotes(*tokstart);
	}
    else if( '=' == firstchar )
	{//= type tokens coalesce into one token of =
	    int skippedchars;
	    sscanf(*tokstart, "%*[= \n\r\t]%n", &skippedchars);
	    *tokend = *tokstart + skippedchars;
	    return 1;
	}		
    else //token doesn't start with quote so it ends with whitespace or quote or =
	{ 
	    //is there a better way to specify whitespace?
	    char *tokenclose = strpbrk(*tokstart, " \t=\"\'\n\r");
	    if(tokenclose == NULL)
		{//no tokenclose found at the end, token is whole string
		    *tokend = str + strlen(str);
		}
	    else
		{
		    *tokend = tokenclose;
		}
	}

    return (*tokend) - (*tokstart) -
	(('\'' == firstchar || '"' == firstchar)?1:0) -
	(('\'' == (*tokend)[-1] || '"' == (*tokend)[-1])?1:0);
}

/*! \if API
  \brief Copy from src to dest, up to stopping before end

  \param dest String to copy to

  \param src String to copy from

  \param end Pointer to char in src that signifies the end of
  characters to copy

  \retval length The number of characters written

  \endif
*/
int writeupto(char *dest, const char *src, const char *end)
{
    int len = end - src;
    strncpy(dest, src, len);
    return len;
}

/*! \if API
  \brief Get the next token from a string, or NULL if none is found.

  A token is delimited by whitespace or =. To include delimiters in
  tokens use single or double quotes, quotes may escaped with
  backslash. Multiple ='s (which may be interspersed with whitespace)
  are returned as a single =.

  \param str The string to get the token from.

  \param end This is set to the character immediately following the
  token. It is allowed to be a pointer to str.

  \retval token A malloc'd buffer containing the first token found in str
  \retval NULL if none was found.

  \endif
*/
char * gettoken(char *str, char **end)
{
    char *token;
    char *tokstart, *tokend;
    int toklen;

    toklen = gettoklen(str, &tokstart, &tokend);

    if(toklen == -1)
	return NULL;	

    token = malloc((toklen + 1) * sizeof(char));

    if(token == NULL)
	{
	    exit(-1);
	}

    char firstchar = *tokstart;

    if('=' == firstchar)
	{
	    token[0] = '=';
	    token[1] = '\0';

	    *end = tokend;
	    return token;
	}

    int written = 0, seen = 0;
    char *closequote = tokend;
    if( '\'' == *tokstart  || '"' == *tokstart )
	{
	    if( *tokstart == tokend[-1] )
		closequote = tokend-1;

	    tokstart++;//don't copy the quote

	    char *bslash = strpbrk(tokstart, "\\");
	    //keep looking for backslashes while we find backslashes before
	    //end of token
	    while( bslash != NULL &&
		   bslash < closequote )
		{
		    int writelen = writeupto(token+written, tokstart+seen, bslash);
		    written += writelen;
		    seen += writelen;

		    token[written] = bslash[1]; //copy char after backslash
		    seen += 2;
		    written++;
		    bslash = strpbrk(bslash+2, "\\");
		}
	}
    written += writeupto(token+written, tokstart+seen, closequote);

    token[written] = '\0';

    /*there might be some unused bytes because escaped chars 
     *use 1 byte in token for 2 bytes in str*/
    token = realloc(token, (written+1) * sizeof(char));
    if(token == NULL)
	{
	    exit(-1);
	}

    *end = tokend;
    return token;
}

/*! \if API
  \brief Get the next key-value pair from str

  A key-value pair has the form:
  <code> <i>key</i>[[:blank:]=]*<i>value</i> </code>
  where \a key and \a value are tokens

  If str is not of this form, then value is ignored, and the function
  behaves like gettoken()
  
  \note \a key may be the empty string
  
  \param str The string to read the key-value pair from

  \param value This is set to a malloc'd buffer containing the token \a value from \c str

  \param end This is set to the character immediately following the
  token. It is allowed to be a pointer to \c str.

  \retval token The token \a key from \c str, any ='s and whitespace are thrown away.
  \retval NULL If no token could be found

  \endif
*/
char *getkeyval(char *str, char **value, char **end)
{
    char *key = gettoken(str, end);

    *value = NULL;
    if(key && strlen(key) == 1 && key[0] == '=')
	{
	    *value = gettoken(*end, end);
	    key = realloc(key, sizeof(char)*1);
	    if(key == NULL)
		exit(-1);
	    key[0] = '\0';

	    if(*value == NULL)
		{
		    *value = malloc(sizeof(char)*1);
		    if(*value == NULL) exit(-1);
		    *value[0] = '\0';
		}
	}
    else
	{
	    int tlen;
	    char *tstart, *tend;

	    tlen = gettoklen(*end, &tstart, &tend);
	    if(tlen == 1 && '=' == *tstart )
		{
		    *value = gettoken(tend, end);

		    if(*value == NULL)
			{
			    *value = malloc(sizeof(char)*1);
			    if(*value == NULL) exit(-1);
			    *value[0] = '\0';
			    *end = tend;
			}
		}
	}

    return key;
}

/*! \if API

  \brief Find out the length of the group name contained within \c str

  The function tries to find out whether a group name exist in \c str
  and also tries to find out its length

  \param str The string in which to search the group name
  \param grpstart Set to the index of the start of group name
  \param grpend Set to the index of the end of group name

  \retval length The of the first group in \c str before escaping,
  after stripping quotes, not including the terminating NULL
  
  \retval -1 if there are no groups in \c str and puts offsets in
  tokstart and tokend.

  \endif
*/
static int getgrouplen(char *str, int *grpstart, int *grpend)
{
    char *openbracket;
  
    //find first [
    openbracket = strchr(str, '[');

    if( openbracket == NULL )
	{
	    //no [ found
	    return -1;
	}
    *grpstart = (openbracket - str);
	
    (*grpstart)++; //drop [ from group
    //need to find closing ]

    char *quote_bracket;
    *grpend = *grpstart;
    do
	{
	    quote_bracket = strpbrk(str+(*grpend), "]\"'");
	    if( quote_bracket == NULL )
		{ //haven't found any closing ]
		    *grpend = strlen(str) + 1; //let the terminating NULL close the group
		}
	    else if ( quote_bracket[0] == '\'' ||  quote_bracket[0] == '"' )
		{
		    char *tokend;
		    tokend = skipquotes(quote_bracket);
		    *grpend = tokend-str;
		}
	    else
		{
		    assert(quote_bracket[0] == ']');
		    *grpend = (quote_bracket - str);
		}
	}
    while( quote_bracket != NULL  && quote_bracket[0] != ']');

    return MAX( (*grpend) - (*grpstart), 0 );
}

/*!\if API
  \brief Get the first group from string

  A group is defined as <code> [<i>\<token\></i>] </code>. \see gettoken()

  \param string The string to read the group from.

  \retval groupname A malloc'd buffer containg the group
  \retval NULL if none is found.

  \endif
*/
char * getgroup(char *string)
{
    int grpstart, grpend, grplen;
    grplen = getgrouplen(string, &grpstart, &grpend);
    if(grplen == -1)
	return NULL;
	
    char *group = malloc((grplen+1) * sizeof(char));

    if(group == NULL)
	{
	    exit(-1);
	}

    //look for a closing ] or opening quote
    char *bracket_or_quote = string;
    int curroffset = grpstart, written = 0;

    do
	{
	    bracket_or_quote = strpbrk(string+curroffset, "]\"'");
	    if(bracket_or_quote == NULL)
		{//no closing ]
		    written += writeupto(group+written, string+curroffset, string+grpend);
		}
	    else if(*bracket_or_quote == ']')
		{
		    written += writeupto(group+written, string+curroffset, bracket_or_quote);
		}
	    else
		{
		    assert(*bracket_or_quote == '\'' || *bracket_or_quote == '"');
			
		    //copy stuff up to quote
		    int len = writeupto(group+written, string+curroffset, bracket_or_quote);
		    written += len;
		    curroffset += len;

		    //copy stuff in quotes
		    char *after_quote;
		    char *tmp = gettoken(bracket_or_quote, &after_quote);

		    len = after_quote - bracket_or_quote;
		    curroffset = (after_quote - string);
		    strncpy(group+written, tmp, len);
		    written += len;
		    free(tmp);
		}
	}
    while(bracket_or_quote != NULL && (*bracket_or_quote) != ']');

    group[written] = '\0';

    group = realloc(group, (written+1) * sizeof(char));
    if(group == NULL)
	{
	    exit(-1);
	}
    return group;
}
	

/*! \if API
  \brief Remove a \#comment from str.

  Search \c str to find a comment (starting with '#'), if one is found:
  Replace '#' with a '\\0' in \c str, and return the comment in malloc'd
  buffer.

  \param str The string to read the line with a comment from. The
  function replaces the '#' beginning the comment with '\\0'. #'s
  placed in quotes don't start comments.

  \retval comment A malloc'd buffer containg the comment (text following '#')
  \retval NULL if none is found.

  \endif
*/
char *cutcomment(char *str)
{
    if(str == NULL) return NULL;

    char *lastquote = strrchr(str, '#');

    char *hash = strchr((lastquote == NULL)?str:lastquote, '#');

    if(hash != NULL)
	{
	    *hash = '\0';
	    int len = strlen(hash+1);
	    char *comment = malloc( (len+1)*sizeof(char) );
	    strncpy(comment, hash+1, len+1);

	    return comment;
	}
    else
	return NULL;
}

/*! \if API
  \brief Convert a line to a linked list of struct entry's

  The function tries to find out various key/value pairs existing in the \c line and creates a list of struct entry's to represent them.

  \param prev The address of struct entry pointer to which we need to assign the address of the newly created structure
  \param line The line from which to extract the information

  \endif
*/
void getvalues(struct entry **prev, char *line)
{
    char *ret, *val;

    while((ret = getkeyval(line, &val, &line)) != NULL)
	{
	    struct entry *const entry = *prev = calloc(sizeof(*entry), 1);

	    if(entry == NULL)
		exit(-1);

	    entry->name = ret;
	    if(val != NULL) // we've got a key/value pair
		{
		    struct entry *sub;

		    entry->type = ENTRY_KEY;

		    sub = calloc(sizeof(*sub), 1);
		    if(sub == NULL)
			exit(-1);

		    sub->name = val;
		    sub->type = ENTRY_VALUE;

		    entry->sub = sub;		
		} else
		{
		    entry->type = ENTRY_VALUE;	
		}

	    prev = &entry->next;
	}		
}

/*! \if API
  \brief Function that transforms a file into a struct entry based list

  The function takes as input an open file and then converts it into cluster of entries.

  \note The function assumes that fp is not NULL

  \param fp The file that we need to use
  \param orphan The address of a character pointer where we would like to store the malloc'ed orphan comment.

  \retval entry The formatted entries
  \retval NULL If no valid lines were found

  \endif
*/
struct entry *transform(FILE *fp, char **orphan)
{
    char *line, *comment = NULL;
    struct entry *groot, *sroot, *gsub;

    for(gsub = groot = sroot = NULL ; (line = parsGetline(fp)) != NULL ; free(line))
	{
	    char *group, *comm;
	    struct entry *tmp;

	    comm = cutcomment(line);

	    if(isempty(line))
		{
		    if(comm != NULL)
			{
			    if(comment != NULL)
				{
				    comment = realloc(comment, strlen(comment) + strlen(comm) + 2);
				    if(comment == NULL)
					exit(-1);
				    strcat(comment, "\n");
				    strcat(comment, comm);
				} else
				comment = comm;
			}
		    continue;
		}

	    tmp = calloc(sizeof(*tmp), 1);
	    if(tmp == NULL)
		exit(-1);

	    tmp->side = comm;
	    tmp->top = comment;
	    comment = NULL;

	    if((group = getgroup(line)) != NULL)
		{
		    tmp->name = group;
		    tmp->type = ENTRY_GROUP;

		    if(groot == NULL)
			groot = tmp;
		    else
			gsub->next = tmp;

		    gsub = tmp;
		    sroot = NULL;
		} else
		{
		    if(__builtin_expect(gsub == NULL, 0))
			groot = gsub = ({
				struct entry *entry;
				entry = calloc(sizeof(*entry), 1);
				if(entry == NULL)
				    exit(-1);
				entry->name = strdup(DEFAULT_GRPNAME);
				if(entry->name == NULL)
				    exit(-1);
				entry->type = ENTRY_GROUP;
				entry;
			    });
		    tmp->type = ENTRY_LINE;
		    getvalues(&tmp->sub, line);
		    reformat(tmp);  // build proper name

		    if(sroot == NULL)
			gsub->sub = tmp;
		    else
			sroot->next = tmp;
		    sroot = tmp;
		}
	}

    *orphan = comment;

    return groot;
}





