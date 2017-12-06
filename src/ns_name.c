/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#define __OPTIMIZE__ 1
#include <libc-symbols.h>
#include <arpa/nameser.h>

#include <glibc-errno.h>
#include <glibc-string.h>

# define EMSGSIZE 90
# define SPRINTF(x) ((size_t)sprintf x)

/* Data. */

static const char	digits[] = "0123456789";

/* Forward. */

static int		special(int);
static int		printable(int);
static int		dn_find(const u_char *, const u_char *,
				const u_char * const *,
				const u_char * const *);
static int		labellen(const u_char *);

/* Public. */

/*%
 *	Convert an encoded domain name to printable ascii as per RFC1035.

 * return:
 *\li	Number of bytes written to buffer, or -1 (with errno set)
 *
 * notes:
 *\li	The root is returned as "."
 *\li	All other domains are returned in non absolute form
 */
int
__ns_name_ntop(const u_char *src, char *dst, size_t dstsiz)
{
	const u_char *cp;
	char *dn, *eom;
	u_char c;
	u_int n;
	int l;

	cp = src;
	dn = dst;
	eom = dst + dstsiz;

	while ((n = *cp++) != 0) {
		if ((n & NS_CMPRSFLGS) == NS_CMPRSFLGS) {
			/* Some kind of compression pointer. */
			__set_errno (EMSGSIZE);
			return (-1);
		}
		if (dn != dst) {
			if (dn >= eom) {
				__set_errno (EMSGSIZE);
				return (-1);
			}
			*dn++ = '.';
		}
		if ((l = labellen(cp - 1)) < 0) {
			__set_errno (EMSGSIZE);
			return(-1);
		}
		if (dn + l >= eom) {
			__set_errno (EMSGSIZE);
			return (-1);
		}
		for ((void)NULL; l > 0; l--) {
			c = *cp++;
			if (special(c)) {
				if (dn + 1 >= eom) {
					__set_errno (EMSGSIZE);
					return (-1);
				}
				*dn++ = '\\';
				*dn++ = (char)c;
			} else if (!printable(c)) {
				if (dn + 3 >= eom) {
					__set_errno (EMSGSIZE);
					return (-1);
				}
				*dn++ = '\\';
				*dn++ = digits[c / 100];
				*dn++ = digits[(c % 100) / 10];
				*dn++ = digits[c % 10];
			} else {
				if (dn >= eom) {
					__set_errno (EMSGSIZE);
					return (-1);
				}
				*dn++ = (char)c;
			}
		}
	}
	if (dn == dst) {
		if (dn >= eom) {
			__set_errno (EMSGSIZE);
			return (-1);
		}
		*dn++ = '.';
	}
	if (dn >= eom) {
		__set_errno (EMSGSIZE);
		return (-1);
	}
	*dn++ = '\0';
	return (dn - dst);
}
libresolv_hidden_def (__ns_name_ntop)


/*%
 *	Convert an ascii string into an encoded domain name as per RFC1035.
 *
 * return:
 *
 *\li	-1 if it fails
 *\li	1 if string was fully qualified
 *\li	0 is string was not fully qualified
 *
 * notes:
 *\li	Enforces label and domain length limits.
 */

int
__ns_name_pton(const char *src, u_char *dst, size_t dstsiz)
{
	u_char *label, *bp, *eom;
	int c, n, escaped;
	char *cp;

	escaped = 0;
	bp = dst;
	eom = dst + dstsiz;
	label = bp++;

	while ((c = *src++) != 0) {
		if (escaped) {
			if ((cp = strchr(digits, c)) != NULL) {
				n = (cp - digits) * 100;
				if ((c = *src++) == 0 ||
				    (cp = strchr(digits, c)) == NULL) {
					__set_errno (EMSGSIZE);
					return (-1);
				}
				n += (cp - digits) * 10;
				if ((c = *src++) == 0 ||
				    (cp = strchr(digits, c)) == NULL) {
					__set_errno (EMSGSIZE);
					return (-1);
				}
				n += (cp - digits);
				if (n > 255) {
					__set_errno (EMSGSIZE);
					return (-1);
				}
				c = n;
			}
			escaped = 0;
		} else if (c == '\\') {
			escaped = 1;
			continue;
		} else if (c == '.') {
			c = (bp - label - 1);
			if ((c & NS_CMPRSFLGS) != 0) {	/*%< Label too big. */
				__set_errno (EMSGSIZE);
				return (-1);
			}
			if (label >= eom) {
				__set_errno (EMSGSIZE);
				return (-1);
			}
			*label = c;
			/* Fully qualified ? */
			if (*src == '\0') {
				if (c != 0) {
					if (bp >= eom) {
						__set_errno (EMSGSIZE);
						return (-1);
					}
					*bp++ = '\0';
				}
				if ((bp - dst) > MAXCDNAME) {
					__set_errno (EMSGSIZE);
					return (-1);
				}
				return (1);
			}
			if (c == 0 || *src == '.') {
				__set_errno (EMSGSIZE);
				return (-1);
			}
			label = bp++;
			continue;
		}
		if (bp >= eom) {
			__set_errno (EMSGSIZE);
			return (-1);
		}
		*bp++ = (u_char)c;
	}
	c = (bp - label - 1);
	if ((c & NS_CMPRSFLGS) != 0) {		/*%< Label too big. */
		__set_errno (EMSGSIZE);
		return (-1);
	}
	if (label >= eom) {
		__set_errno (EMSGSIZE);
		return (-1);
	}
	*label = c;
	if (c != 0) {
		if (bp >= eom) {
			__set_errno (EMSGSIZE);
			return (-1);
		}
		*bp++ = 0;
	}
	if ((bp - dst) > MAXCDNAME) {	/*%< src too big */
		__set_errno (EMSGSIZE);
		return (-1);
	}
	return (0);
}
libresolv_hidden_def (__ns_name_pton)


/*%
 *	Unpack a domain name from a message, source may be compressed.
 *
 * return:
 *\li	-1 if it fails, or consumed octets if it succeeds.
 */
int
__ns_name_unpack(const u_char *msg, const u_char *eom, const u_char *src,
	       u_char *dst, size_t dstsiz)
{
	const u_char *srcp, *dstlim;
	u_char *dstp;
	int n, len, checked, l;

	len = -1;
	checked = 0;
	dstp = dst;
	srcp = src;
	dstlim = dst + dstsiz;
	if (srcp < msg || srcp >= eom) {
		__set_errno (EMSGSIZE);
		return (-1);
	}
	/* Fetch next label in domain name. */
	while ((n = *srcp++) != 0) {
		/* Check for indirection. */
		switch (n & NS_CMPRSFLGS) {
		case 0:
			/* Limit checks. */
			if ((l = labellen(srcp - 1)) < 0) {
				__set_errno (EMSGSIZE);
				return(-1);
			}
			if (dstp + l + 1 >= dstlim || srcp + l >= eom) {
				__set_errno (EMSGSIZE);
				return (-1);
			}
			checked += l + 1;
			*dstp++ = n;
			memcpy(dstp, srcp, l);
			dstp += l;
			srcp += l;
			break;

		case NS_CMPRSFLGS:
			if (srcp >= eom) {
				__set_errno (EMSGSIZE);
				return (-1);
			}
			if (len < 0)
				len = srcp - src + 1;
			srcp = msg + (((n & 0x3f) << 8) | (*srcp & 0xff));
			if (srcp < msg || srcp >= eom) {  /*%< Out of range. */
				__set_errno (EMSGSIZE);
				return (-1);
			}
			checked += 2;
			/*
			 * Check for loops in the compressed name;
			 * if we've looked at the whole message,
			 * there must be a loop.
			 */
			if (checked >= eom - msg) {
				__set_errno (EMSGSIZE);
				return (-1);
			}
			break;

		default:
			__set_errno (EMSGSIZE);
			return (-1);			/*%< flag error */
		}
	}
	*dstp = '\0';
	if (len < 0)
		len = srcp - src;
	return (len);
}
libresolv_hidden_def (__ns_name_unpack)


/*%
 *	Pack domain name 'domain' into 'comp_dn'.
 *
 * return:
 *\li	Size of the compressed name, or -1.
 *
 * notes:
 *\li	'dnptrs' is an array of pointers to previous compressed names.
 *\li	dnptrs[0] is a pointer to the beginning of the message. The array
 *	ends with NULL.
 *\li	'lastdnptr' is a pointer to the end of the array pointed to
 *	by 'dnptrs'.
 *
 * Side effects:
 *\li	The list of pointers in dnptrs is updated for labels inserted into
 *	the message as we compress the name.  If 'dnptr' is NULL, we don't
 *	try to compress names. If 'lastdnptr' is NULL, we don't update the
 *	list.
 */
int
__ns_name_pack(const u_char *src, u_char *dst, int dstsiz,
	     const u_char **dnptrs, const u_char **lastdnptr)
{
	u_char *dstp;
	const u_char **cpp, **lpp, *eob, *msg;
	const u_char *srcp;
	int n, l, first = 1;

	srcp = src;
	dstp = dst;
	eob = dstp + dstsiz;
	lpp = cpp = NULL;
	if (dnptrs != NULL) {
		if ((msg = *dnptrs++) != NULL) {
			for (cpp = dnptrs; *cpp != NULL; cpp++)
				(void)NULL;
			lpp = cpp;	/*%< end of list to search */
		}
	} else
		msg = NULL;

	/* make sure the domain we are about to add is legal */
	l = 0;
	do {
		int l0;

		n = *srcp;
		if ((n & NS_CMPRSFLGS) == NS_CMPRSFLGS) {
			__set_errno (EMSGSIZE);
			return (-1);
		}
		if ((l0 = labellen(srcp)) < 0) {
			__set_errno (EINVAL);
			return(-1);
		}
		l += l0 + 1;
		if (l > MAXCDNAME) {
			__set_errno (EMSGSIZE);
			return (-1);
		}
		srcp += l0 + 1;
	} while (n != 0);

	/* from here on we need to reset compression pointer array on error */
	srcp = src;
	do {
		/* Look to see if we can use pointers. */
		n = *srcp;
		if (n != 0 && msg != NULL) {
			l = dn_find(srcp, msg, (const u_char * const *)dnptrs,
				    (const u_char * const *)lpp);
			if (l >= 0) {
				if (dstp + 1 >= eob) {
					goto cleanup;
				}
				*dstp++ = (l >> 8) | NS_CMPRSFLGS;
				*dstp++ = l % 256;
				return (dstp - dst);
			}
			/* Not found, save it. */
			if (lastdnptr != NULL && cpp < lastdnptr - 1 &&
			    (dstp - msg) < 0x4000 && first) {
				*cpp++ = dstp;
				*cpp = NULL;
				first = 0;
			}
		}
		/* copy label to buffer */
		if ((n & NS_CMPRSFLGS) == NS_CMPRSFLGS) {
			/* Should not happen. */
			goto cleanup;
		}
		n = labellen(srcp);
		if (n + 1 > eob - dstp) {
			goto cleanup;
		}
		memcpy(dstp, srcp, n + 1);
		srcp += n + 1;
		dstp += n + 1;
	} while (n != 0);

	if (dstp > eob) {
cleanup:
		if (msg != NULL)
			*lpp = NULL;
		__set_errno (EMSGSIZE);
		return (-1);
	}
	return (dstp - dst);
}
libresolv_hidden_def (__ns_name_pack)


/*%
 *	Expand compressed domain name to presentation format.
 *
 * return:
 *\li	Number of bytes read out of `src', or -1 (with errno set).
 *
 * note:
 *\li	Root domain returns as "." not "".
 */
int
__ns_name_uncompress(const u_char *msg, const u_char *eom, const u_char *src,
		   char *dst, size_t dstsiz)
{
	u_char tmp[NS_MAXCDNAME];
	int n;

	if ((n = __ns_name_unpack(msg, eom, src, tmp, sizeof tmp)) == -1)
		return (-1);
	if (__ns_name_ntop(tmp, dst, dstsiz) == -1)
		return (-1);
	return (n);
}
libresolv_hidden_def (__ns_name_uncompress)


/*%
 *	Compress a domain name into wire format, using compression pointers.
 *
 * return:
 *\li	Number of bytes consumed in `dst' or -1 (with errno set).
 *
 * notes:
 *\li	'dnptrs' is an array of pointers to previous compressed names.
 *\li	dnptrs[0] is a pointer to the beginning of the message.
 *\li	The list ends with NULL.  'lastdnptr' is a pointer to the end of the
 *	array pointed to by 'dnptrs'. Side effect is to update the list of
 *	pointers for labels inserted into the message as we compress the name.
 *\li	If 'dnptr' is NULL, we don't try to compress names. If 'lastdnptr'
 *	is NULL, we don't update the list.
 */
int
__ns_name_compress(const char *src, u_char *dst, size_t dstsiz,
		 const u_char **dnptrs, const u_char **lastdnptr)
{
	u_char tmp[NS_MAXCDNAME];

	if (__ns_name_pton(src, tmp, sizeof tmp) == -1)
		return (-1);
	return (__ns_name_pack(tmp, dst, dstsiz, dnptrs, lastdnptr));
}
libresolv_hidden_def (__ns_name_compress)


/*%
 *	Advance *ptrptr to skip over the compressed name it points at.
 *
 * return:
 *\li	0 on success, -1 (with errno set) on failure.
 */
int
__ns_name_skip(const u_char **ptrptr, const u_char *eom)
{
	const u_char *cp;
	u_int n;

	cp = *ptrptr;
	while (cp < eom && (n = *cp++) != 0) {
		/* Check for indirection. */
		switch (n & NS_CMPRSFLGS) {
		case 0:			/*%< normal case, n == len */
			cp += n;
			continue;
		case NS_CMPRSFLGS:	/*%< indirection */
			cp++;
			break;
		default:		/*%< illegal type */
			__set_errno (EMSGSIZE);
			return (-1);
		}
		break;
	}
	if (cp > eom) {
		__set_errno (EMSGSIZE);
		return (-1);
	}
	*ptrptr = cp;
	return (0);
}
libresolv_hidden_def (__ns_name_skip)


/* Private. */

/*%
 *	Thinking in noninternationalized USASCII (per the DNS spec),
 *	is this character special ("in need of quoting") ?
 *
 * return:
 *\li	boolean.
 */
static int
special(int ch) {
	switch (ch) {
	case 0x22: /*%< '"' */
	case 0x2E: /*%< '.' */
	case 0x3B: /*%< ';' */
	case 0x5C: /*%< '\\' */
	case 0x28: /*%< '(' */
	case 0x29: /*%< ')' */
	/* Special modifiers in zone files. */
	case 0x40: /*%< '@' */
	case 0x24: /*%< '$' */
		return (1);
	default:
		return (0);
	}
}

/*%
 *	Thinking in noninternationalized USASCII (per the DNS spec),
 *	is this character visible and not a space when printed ?
 *
 * return:
 *\li	boolean.
 */
static int
printable(int ch) {
	return (ch > 0x20 && ch < 0x7f);
}

/*%
 *	Thinking in noninternationalized USASCII (per the DNS spec),
 *	convert this character to lower case if it's upper case.
 */
static int
mklower(int ch) {
	if (ch >= 0x41 && ch <= 0x5A)
		return (ch + 0x20);
	return (ch);
}

/*%
 *	Search for the counted-label name in an array of compressed names.
 *
 * return:
 *\li	offset from msg if found, or -1.
 *
 * notes:
 *\li	dnptrs is the pointer to the first name on the list,
 *\li	not the pointer to the start of the message.
 */
static int
dn_find(const u_char *domain, const u_char *msg,
	const u_char * const *dnptrs,
	const u_char * const *lastdnptr)
{
	const u_char *dn, *cp, *sp;
	const u_char * const *cpp;
	u_int n;

	for (cpp = dnptrs; cpp < lastdnptr; cpp++) {
		sp = *cpp;
		/*
		 * terminate search on:
		 * root label
		 * compression pointer
		 * unusable offset
		 */
		while (*sp != 0 && (*sp & NS_CMPRSFLGS) == 0 &&
		       (sp - msg) < 0x4000) {
			dn = domain;
			cp = sp;
			while ((n = *cp++) != 0) {
				/*
				 * check for indirection
				 */
				switch (n & NS_CMPRSFLGS) {
				case 0:		/*%< normal case, n == len */
					n = labellen(cp - 1); /*%< XXX */
					if (n != *dn++)
						goto next;

					for ((void)NULL; n > 0; n--)
						if (mklower(*dn++) !=
						    mklower(*cp++))
							goto next;
					/* Is next root for both ? */
					if (*dn == '\0' && *cp == '\0')
						return (sp - msg);
					if (*dn)
						continue;
					goto next;
				case NS_CMPRSFLGS:	/*%< indirection */
					cp = msg + (((n & 0x3f) << 8) | *cp);
					break;

				default:	/*%< illegal type */
					__set_errno (EMSGSIZE);
					return (-1);
				}
			}
  next: ;
			sp += *sp + 1;
		}
	}
	__set_errno (ENOENT);
	return (-1);
}

/* Return the length of the encoded label starting at LP, or -1 for
   compression references and extended label types.  */
static int
labellen (const unsigned char *lp)
{
  if (*lp <= 63)
    return *lp;
  return -1;
}

/*! \file */