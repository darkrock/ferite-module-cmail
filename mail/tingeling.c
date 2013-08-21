#include <stdio.h>

#include "imap_utility.h"

char *cmail_strndup( char *buf, size_t len ) {
	char *nbuf = calloc( len + 1, sizeof(char) );
	memcpy( nbuf, buf, len );
	return nbuf;
}
FeriteVariable *strip_tags(FeriteScript *script, char *rbuf, int len)
{
	char *tbuf, *buf, *p, *tp, *rp, c, lc;
	int br, i = 0, depth = 0;
	int state = 0;
	FeriteVariable *str;

	buf = (char *)cmail_strndup(rbuf, len);
	c = *buf;
	lc = '\0';
	p = buf;
	rp = rbuf;
	br = 0;
	
	tbuf = tp = NULL;
	
	while (i < len) {
		switch (c) {
		case '<':
			if (isspace(*(p + 1))) {
				goto reg_char;
			}
			if (state == 0) {
				lc = '<';
				state = 1;
			} else if (state == 1) {
				depth++;
			}
			break;
			
		case '(':
			if (state == 2) {
				if (lc != '"' && lc != '\'') {
					lc = '(';
					br++;
				}
			} else if (state == 0) {
				*(rp++) = c;
			}
			break;	
			
		case ')':
			if (state == 2) {
				if (lc != '"' && lc != '\'') {
					lc = ')';
					br--;
				}
			}  else if (state == 0) {
				*(rp++) = c;
			}
			break;	
			
		case '>':
			if (depth) {
				depth--;
				break;
			}
			
			switch (state) {
			case 1: /* HTML/XML */
				lc = '>';
				state = 0;
			       
				break;
						
			case 2: /* PHP */
				if (!br && lc != '\"' && *(p-1) == '?') {
					state = 0;
					tp = tbuf;
				}
				break;
				
			case 3:
				state = 0;
				tp = tbuf;
				break;
				
			case 4: /* JavaScript/CSS/etc... */
				if (p >= buf + 2 && *(p-1) == '-' && *(p-2) == '-') {
					state = 0;
					tp = tbuf;
				}
				break;
				
			default:
				*(rp++) = c;
				break;
			}
			break;
			
		case '"':
		case '\'':
			if (state == 2 && *(p-1) != '\\') {
				if (lc == c) {
					lc = '\0';
				} else if (lc != '\\') {
					lc = c;
				}
			} else if (state == 0) {
				*(rp++) = c;
			} 
			break;
			
		case '!': 
			/* JavaScript & Other HTML scripting languages */
			if (state == 1 && *(p-1) == '<') { 
				state = 3;
				lc = c;
			} else {
				if (state == 0) {
					*(rp++) = c;
				} 
			}
			break;
			
		case '-':
			if (state == 3 && p >= buf + 2 && *(p-1) == '-' && *(p-2) == '!') {
				state = 4;
			} else {
				goto reg_char;
			}
			break;
			
		case '?':
			
			if (state == 1 && *(p-1) == '<') { 
				br=0;
				state=2;
				break;
			}
			
		case 'E':
		case 'e':
			/* !DOCTYPE exception */
			if (state==3 && p > buf+6
			    && tolower(*(p-1)) == 'p'
			    && tolower(*(p-2)) == 'y'
			    && tolower(*(p-3)) == 't'
			    && tolower(*(p-4)) == 'c'
			    && tolower(*(p-5)) == 'o'
			    && tolower(*(p-6)) == 'd') {
				state = 1;
				break;
			}
			/* fall-through */
			
		case 'l':
			
			/* swm: If we encounter '<?xml' then we shouldn't be in
			 * state == 2 (PHP). Switch back to HTML.
			 */
			
			if (state == 2 && p > buf+2 && *(p-1) == 'm' && *(p-2) == 'x') {
				state = 1;
				break;
			}

			/* fall-through */
		default:
		reg_char:
			if (state == 0) {
				*(rp++) = c;
			} 
			break;
		}
		c = *(++p);
		i++;
	}	
	if (rp < rbuf + len) {
		*rp = '\0';
	}
	free(buf);
	
	str = fe_new_str("strip_tags", rbuf, (rp - rbuf), FE_CHARSET_DEFAULT); 
	FE_RETURN_VAR( str );
}
