#include "strutl.h"
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>

int strcountcmp(const char *s1, const char *e1, const char *s2, const char *e2)
{
	for (; s1 != e1 && s2 != e2 && *s1 == *s2; s1++, s2++) ;

	if (s1 == e1 && s2 == e2)
		return 0;

	if (s1 == e1)
		return 1;
	if (s2 == e2)
		return -1;
	if (*s1 < *s2)
		return -1;
	return 1;
}

/*
 * Strips leading and trailing spaces from a string; also strips trialing
 * newline characters from the string
 */
char *strstrip(char *buf)
{
	char *end;

	/* skip initial spaces */
	for (; *buf != 0 && isspace(*buf); buf++) ;

	if (*buf == 0)
		return buf;

	end = buf + strlen(buf) - 1;
	while (end != buf - 1)
	{
		if (isspace(*end) == 0)
			break;

		*end = 0;
		end--;
	}
	return buf;
}

/* 
 * concatenates arbitrary number of strings to a buffer, with bounds
 * checking
 */
void strvacat(char *buf, size_t maxlen, ...)
{
	va_list ap;
	char *str;
	size_t len = strlen(buf);

	va_start(ap, maxlen);
	
	while (1)
	{
		str = va_arg(ap, char *);
		if (str == NULL)
			break;
		if (len + strlen(str) > maxlen)
			break;
		strcat(buf, str);
		len += strlen(str);
	}
	va_end(ap);
}

int strparsecword(char **inbuf, char *outbuf, size_t maxlen)
{
	char buffer[1024];
	char *buf = buffer;
	const char *c = *inbuf;

	for (; *c != 0 && *c == ' '; c++);

	if (*c == 0) 
		return 0;
	if (strlen(*inbuf) > sizeof(buffer)) 
		return 0;
	for (; *c != 0; c++)
	{
		if (*c == '"')
		{
			for (; *c != 0 && *c != '"'; c++)
				*buf++ = *c;
			if (*c == 0)
				return 0;
			continue;
		}
		
		if (c != *inbuf && isspace(*c) != 0 && isspace(c[-1]) != 0)
			continue;
		if (isspace(*c) == 0)
			return 0;
		*buf++ = ' ';
	}
	*buf = 0;
	strncpy(outbuf, buffer, maxlen);
	*inbuf = (char *)c;

	return 1;
}

int strparsequoteword(char **inbuf, char *outbuf, size_t maxlen)
{
	char buffer[1024];
	char tmp[3];
	char *start, *i;
	const char *c = *inbuf;
	for (; *c != 0 && *c == ' '; c++); 
	if (*c == 0)
		return 0;
	
	for (; *c != 0 && isspace(*c) == 0; c++)
	{
		if (*c == '"')
		{
			for (c++; *c != 0 && *c != '"'; c++);
			if (*c == 0)
				return 0;
		}
		if (*c == '[')
		{
			for (c++; *c != 0 && *c != ']'; c++);
			if (*c == 0)
				return 0;
		}
	}

	/* dequote the string */
	start = *inbuf;
	for (i = buffer; i < buffer + sizeof(buffer) && start != c; i++)
	{
		if (*start == '%' && start + 2 < c)
		{
			tmp[0] = start[1];
			tmp[1] = start[2];
			tmp[2] = 0;
			*i = (char)strtol(tmp, 0, 16);
			start += 3;
			continue;
		}
		if (*start != '"')
			*i = *start;
		else
			i--;
		start++;
	}
	*i = 0;
	
	strncpy(outbuf, buffer, maxlen);

	for (; *c != 0 && isspace(*c); c++);
	*inbuf = (char *)c;
	return 1;
}

int strcmdsplit(char *inbuf, char **argv, size_t maxnarg)
{
	int argc = 0;
	int inspace = 1;
	
	for (; *inbuf != 0; inbuf++)
	{
		if (isspace(*inbuf) != 0)
		{
			inspace = 1;
			*inbuf = 0;
		}
		else if (inspace != 0)
		{
			inspace = 0;
			argv[argc++] = inbuf;
			if (argc >= maxnarg)
				break;
		}
	}

	return argc;
}

void strescape(const char *inbuf, char *outbuf, const size_t maxlen)
{
	char *p = inbuf;
	int i = 0;
	while (*p != 0 && i < maxlen-1)
	{
		if (*p == '\\' || *p == '"')
		{
			if (i + 4 >= maxlen) break;
			outbuf[i] = '%';
			sprintf(&outbuf[i+1], "%02X", (unsigned int)*p);
			p++;
			i += 3;
		}
		else
			outbuf[i++] = *p++;
	}
	outbuf[i] = 0;
}
