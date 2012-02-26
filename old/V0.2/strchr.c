/*	Find character within string
**
**	See the ANSI definition of strchr() - was unix index()
*/
#ifdef OLD_CODE
char * my_index(char *s, char c)
{
    char *p;
    for(p=s; *p && (*p!=c); p++) /*scan */ ;
    return *p ? p : 0;
}
#endif
