
#include <stdio.h>

#ifndef __PARSER_LOCAL_H__
#define __PARSER_LOCAL_H__

/*! \file
    \brief This file contains all the stuff to be used only be the internal source code
    */

// file.c

struct parsefile;
char *puttoken(const char *token);
char *parsGetline(FILE *fp);

// transform.c

#define DEFAULT_GRPNAME ""  /*!< The group name to be assumed for those lines which does not belong to any group */

struct entry *transform(FILE *fp, char **orphan);

// parse.c

struct phandler ;
struct parsefile ;

void freeentries(struct entry *root);

// modify.c

#endif

