/*		Handle a VMS Help request from a WWW client	VMSHelpGate.c
**		===========================================
**
** Authors
**	JFG	Jean-Francois Groff, CERN, Geneva	jfg@info.cern.ch
**
** History:
** 1.1  26 Feb 92 (JFG) Enabled strange topics
** 1.0	 7 Oct 91 (JFG) First release
** 0.4	 2 Oct 91 (JFG) Handle help summary. Definitive address format :
**			//node[:port]/HELP[/@library][/topic[/subtopic...]]
** 0.3	27 Sep 91 (JFG)	Created from h2h.c
*/

/* (c) CERN WorldWideWeb project 1990-1992. See Copyright.html for details */


/** Headers taken from HTRetrieve.c **/

#define BUFFER_SIZE 4096	/* Arbitrary size for efficiency */

#include "HTUtils.h"
#include "tcp.h"

extern int WWW_TraceFlag;	/* Control diagnostic output */
extern FILE * LogFile;		/* Log file output */
extern char HTClientHost[16];	/* Client name to be output */
extern int HTWriteASCII(int soc, char * s);	/* In HTDaemon.c */


/** Headers taken from h2h.c **/

/* Maximum line size for buffers */
#define LSIZE 256

/* Maximum command line size for VMS */
#define CSIZE 256

/* Maximum VMS file name size (on 5.3, it's really 41, yuk! hope for more) */
#define FSIZE 80

/* Maximum anchor size */
#define ASIZE 80

/* Diagnostics processing utilities */
static const char *module_name = "VMSHelpGate: ";
char diag[LSIZE];  /* Diagnostic string */
/* Horrible kludge, but saves a lot of pain */
#define DIAGNOSE(sprintf_arg_list, error_code) { \
  sprintf sprintf_arg_list; \
  if (TRACE) fprintf (stderr, "%s%s", module_name, diag); \
  HTWriteASCII (soc, diag); \
  return error_code; \
  }

/*******************************************************************************

anchor_strcpy : Converts a string to a name suitable for use as an anchor and
                for comparisons, i.e. trim it to its first word (consisting of
		alphanumeric characters plus '_', '-', '$' and '/'),
		make it uppercase and convert each '/', '-' and '$' to '_'.
		If the string does not begin with a "good" character, escape it.

Inputs :
	char *dest : destination string
	char *src : source string

Returns the end position of the destination string.

------------------------------------------------------------------------------*/

static char *anchor_strcpy
#ifdef __STDC__
  (char *dest, char *src)
#else
  (dest, src)
    char *dest;
    char *src;
#endif
{
  char *end = dest;
  /* Trim to first word */
  for ( ; *end =
            isalnum (*src) ? toupper(*src)
          : *src == '/' || *src == '_' || *src == '-' || *src == '$' ? '_'
          : '\0' ;
       ++end, ++src);
  if (end == dest) {  /* src doesn't begin with a word or /_-$ : escape it */
    for ( ; *src ; ++src) {
	*end++ =
	  isalnum (*src) ? toupper (*src)
	: 'A' + (*src % 26);  /* Hash a letter from the bad character */
    }
    *end = 0;  /* Terminate the string */
  }
  return end;
}

/*******************************************************************************

hlp_to_html : Reads in .HLP help file and outputs desired info in HTML format.

Inputs :
	FILE *hlp : pointer to the .HLP file to be scanned
	char *query : data to be found, as blank-separated keywords
	              NOTE : this string is altered (tokenized) by the function
	int soc : HTML output socket

Status returned :
	 0 : Success
Unused	-1 : Help file not found
	-2 : Query not found
Unused	-3 : Empty query
	-4 : Structure error in help file (may create some text before that)
Unused	-5 : Write error to HTML file (may create some text before that)
Unused	-6 : Can't open HTML output file

------------------------------------------------------------------------------*/

static int hlp_to_html
#ifdef __STDC__
  (const FILE *hlp, const char *query, const int soc)
#else
  (hlp, query, soc)
    FILE *hlp;
    char *query;
    int soc;
#endif
{
  char *sep = " ";  /* Authorized keyword separators in the query */
  char line[LSIZE];  /* Input buffer */
  char out[LSIZE];  /* Output buffer */
  /* Possible states of the scanning algorithm */
  enum { PARSE, TEXT, MENU, DONE } state;
  int depth;  /* Current depth in the help tree */
  char *key;  /* Currently searched keyword from the query */
  int prefix;  /* Prefix number on a line of the help file, indicating depth */
  char *title;  /* Pointer to the rest of a title line after depth prefix */
  char anchor[ASIZE];  /* Anchor name built from menu title and item */
  char *menu_item;  /* Pointer to menu item within anchor name */
  char *s, *t;  /* Temporary string pointers */

  /* Initial things to send out */
  /* HTWriteASCII (soc, "<ISINDEX>\n");  /* DON'T Enable keyword searches */

  state = PARSE;  /* Searching for the query's keywords */
  depth = 1;
  key = strtok (query, sep);  /* Read first keyword from the query */
  anchor_strcpy (key, key); /* Normalise key to help matches */

  while (fgets (line, LSIZE, hlp)) {
    if (isdigit (*line)) {  /* This is a title line */
      prefix = strtol (line, &title, 10);
      while (isspace (*title))  /* Find the real beginning of the title */
	title++;
      s = title;
      while (*(++s) != '\n');  /* Find the real end of the title */
      while (isspace (*(--s)));
      *(++s) = '\0';  /* Trim title to a clean string */

      switch (state) {
      case PARSE:  /* Check title against query */
	menu_item = anchor_strcpy (anchor, title);
	/* Is this a match ? */
	if (prefix == depth && !strncmp(anchor, key, strlen(key))) {
	  if (depth == 1) {  /* Root match */
	    /* Start the HTML title with the first word of 'title' */
	    sprintf (out, "<TITLE>Help %.*s", menu_item - anchor, title);
	    HTWriteASCII (soc, out);
	  } else {  /* Deep match : continue HTML title in the same way */
	    sprintf (out, " %.*s", menu_item - anchor, title);
	    HTWriteASCII (soc, out);
	  }
	  if (key = strtok (NULL, sep)) {  /* More levels to descend */
	    depth++;
	    anchor_strcpy (key, key); /* Normalise key to help matches */
	  }
	  else {  /* Query fully parsed */
	    HTWriteASCII (soc, "</TITLE>\n");  /* End the HTML title */
	    sprintf (out, "<H1>%s</H1>\n", title);  /* Print full heading */
	    HTWriteASCII (soc, out);
	    *menu_item++ = '/';  /* Append field separator to anchor */
	    *menu_item = '\0';  /* Ready for anchor completion */
	    state = TEXT;
	    HTWriteASCII (soc, "<XMP>\n");  /* Will copy help text verbatim */
	  }
	}
	break;

      case TEXT:
	HTWriteASCII (soc, "</XMP>\n");  /* Help text is finished, folks ! */
	if (prefix <= depth) {
	  /* We reached the next item at this depth or it was the last one */
	  state = DONE;
	  break;
	} else if (prefix == depth + 1) {  /* This item has a menu */
	  state = MENU;
	  depth++;
	  HTWriteASCII (soc, "<H3>Additional information available:</H3>\n<DIR>\n");
	} else {  /* The help file skipped the next depth */
	  fclose (hlp);
	  DIAGNOSE ((diag, "Help file corrupted.\n"), -4);
	}
	/* No break here : flow through case MENU if state transition */
	/* (Yeah, you may find this ugly...) */

      case MENU:
	if (prefix < depth) {  /* Seen all menu items */
	  HTWriteASCII (soc, "</DIR>\n");  /* That's all, folks ! */
	  state = DONE;
	} else if (prefix == depth) {  /* Next menu item */
	  anchor_strcpy (menu_item, title);  /* Append item to anchor prefix */
	  sprintf (out, "<LI><A HREF=%s>%s</A>\n", anchor, title);
	  HTWriteASCII (soc, out);
	} /* if prefix > depth, there's a submenu : ignore it. */
	break;
	
      case DONE:  /* Shouldn't happen here */
	fprintf (stderr, "Internal error : invalid state DONE.\n");
	break;

      }  /* End of switch(state) */
    }  /* End of title line processing */

    else { /* This is a text line */
      if (state == TEXT && *line != '!')
	/* Copy line verbatim to HTML file, except if it's a comment ('!') */
	HTWriteASCII (soc, line);
    }  /* End of text line processing */

    if (state == DONE) {  /* Have we finished yet ? */
      fclose (hlp);
      /* FIXME Insert here HTML file termination */
      return 0;  /* success */
    }

  }  /* EOF reached on help file */
  fclose (hlp);
  if (state != PARSE) {  /* EOF reached while outputting HTML */
    if (state == MENU)
      HTWriteASCII (soc, "</UL>\n");  /* End the menu cleanly */
    return 0;  /* success */
  } else {  /* key not found */
    if (depth > 1)  /* Partial match : end the title cleanly */
      HTWriteASCII (soc, "</TITLE>\n");
    DIAGNOSE ((diag, "\"%s\" not found in help file.\n", key), -2);
  }
}


/*******************************************************************************

lis_to_html : Reads in .LIS library directory and outputs menu in HTML format.

Inputs :
	FILE *dir : pointer to the .LIS file to be scanned
	char *lib_name : name of the help library (for HTML title)
	int soc : HTML output socket

Status returned :
	 0 : Success
	-2 : Structure error in .LIS file

------------------------------------------------------------------------------*/

static int lis_to_html
#ifdef __STDC__
  (const FILE *dir, const char *lib_name, const int soc)
#else
  (dir, lib_name, soc)
    FILE *dir;
    char *lib_name;
    int soc;
#endif
{
  char line[LSIZE];  /* Input buffer */
  char out[LSIZE];  /* Output buffer */
  char *entry;  /* Pointer to current directory entry */
  char *at_prefix;
  
  if (lib_name)  /* Help library specified */
    at_prefix = "@";
  else {  /* Plain HELP request */
    at_prefix = "";
    lib_name = "HELP";  /* Root level help */
  }
    
  /* Initial things to send out */
/*  HTWriteASCII (soc, "<ISINDEX>\n<TITLE>Help Library "); */
  HTWriteASCII (soc, "<TITLE>Help Library ");
  HTWriteASCII (soc, lib_name);
  HTWriteASCII (soc, " Contents</TITLE>\n<H1>");
  HTWriteASCII (soc, lib_name);
  HTWriteASCII (soc, "</H1>\n<DIR>\n");

  do {  /* Find the first empty line */
    if (! fgets (line, LSIZE, dir))
      DIAGNOSE ((diag, "Structure error in help library directory.\n"), -2);
  } while (*line != '\n');

  /* Now scan the entries */
  while (fgets (line, LSIZE, dir) && (entry = strtok (line, " \n"))) {
    sprintf (out, "<LI><A HREF=%s%s/%s>%s</A>\n",
	     at_prefix, lib_name, entry, entry);
    HTWriteASCII (soc, out);
  }  /* End of entry processing */

  fclose (dir);
  HTWriteASCII (soc, "</DIR>\n");  /* End the menu cleanly */
  return 0;  /* success */
}


/*******************************************************************************

open_hlp : Opens desired .HLP module from specified library. Extracts it from
           .HLB help library if necessary. If no module is specified, opens
	   (and perhaps creates) .LIS library directory file.

Inputs :
	char *help_lib : library name
	char *module : name of the module to extract

Returns FILE * pointing to relevant .HLP or .LIS if success, NULL on error.

------------------------------------------------------------------------------*/

static FILE *open_hlp
#ifdef __STDC__
  (const char *help_lib, const char *module, const int soc)
#else
  (help_lib, module, soc)
    char *help_lib;
    char *module;
    int soc;
#endif
{
  char hlpfile[FSIZE];
  char *help_prefix;
  char *s, *t;
  FILE *hlp;
  char command[CSIZE];

  /* First, create the relevant (unique) file name */
  if (! help_lib)  /* Plain HELP request */
    help_lib = "HELPLIB";  /* Open default help library */
  s = strchr (help_lib, ':');
  help_prefix = s ? '\0' : "SYS$HELP:";
  t = strchr (help_lib, ']');
  s = (t && t > s) ? t + 1 : (s ? s + 1 : help_lib);  /* Find beginning */
  if (! (t = strrchr (help_lib, '.')))  /* Find end */
    t = help_lib + strlen (help_lib);
  if (module) {
    sprintf (hlpfile, "WWW$TEMP_DIR:%.*s_", t - s, s);
    anchor_strcpy (hlpfile + strlen (hlpfile), module);  /* Append valid module name */
    strcat (hlpfile, ".HLP");
  }
  else
    sprintf (hlpfile, "WWW$TEMP_DIR:%.*s.LIS", t - s, s);
  if (hlp = fopen (hlpfile, "r"))  /* Does it already exist ? */
    return hlp;  /* Yes : return it, else create it */
  if (module)
    sprintf (command, "LIBRARY /HELP %s%s /EXTRACT=\"%s\" /OUTPUT=%s",
	     help_prefix, help_lib, module, hlpfile);
  else
    sprintf (command, "LIBRARY /HELP %s%s /LIST=%s",
	     help_prefix, help_lib, hlpfile);
  if (! (system (command)) & 1) {  /* VMS error status ? */
    if (! module)
      return NULL;  /* Failure */
    /* Try again in default help libraries (FIXME scan a list here) */
    sprintf (command, "LIBRARY /HELP SYS$HELP:HELPLIB /EXTRACT=\"%s\" /OUTPUT=%s",
	   module, hlpfile);
    if (! (system (command)) & 1)  /* VMS error status ? */
      return NULL;  /* Failure */
  }
  return fopen (hlpfile, "r");
}


/*******************************************************************************

HTRetrieve : Retrieves information from VMS help library.

Inputs :
	char *arg : HT address
	char *keywords : plus-separated keyword list, if any
	int soc : output socket

WARNING : This function relies on the fact that the keywords string
          immediately follows the arg string. It also modifies those strings.

------------------------------------------------------------------------------*/

int HTRetrieve
#ifdef __STDC__
  (const char *arg, const char *keywords, const int soc)
#else
  (arg, keywords, soc)
    char *arg;
    char *keywords;
    int soc;
#endif
{
  FILE *hlp;
  char *help_file;
  char *query;
  char *s;

  if (keywords)
    *(keywords - 1) = '/';  /* Un-split arg from keywords */
  /* address must be of the form /HELP[/@library][/topic[/subtopic...]] */
  s = strtok (arg + 1, "/");
  if (strcasecomp (s, "HELP")) {
    DIAGNOSE ((diag, "Address should begin with /HELP : /%s\n", s), -1)
  }
  help_file = strtok (NULL, "/");
  if (help_file && *help_file == '@'
      && help_file[1] ) {  /* Explicit help library specified */
    help_file++;  /* Skip @ */
    query = strtok (NULL, "/+");
  } else {  /* This was not a help library, but the beginning of the query */
    query = help_file;
    help_file = NULL;
  }
  /* The first word from the query is the module name (perhaps empty) */
  if (! (hlp = open_hlp (help_file, query, soc))) {
    DIAGNOSE ((diag, "Help library or module not found : %s/%s\n",
	       help_file, query), -1)
  }
  if (query) {  /* Parse rest of address and process help file */
    while (s = strtok (NULL, "/+"))
      *(s - 1) = ' ';
    return hlp_to_html (hlp, query, soc);
  }
  else  /* Process library directory */
    return lis_to_html (hlp, help_file, soc);
}
