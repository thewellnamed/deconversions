# Copyright (c) AT&T Corp. 1994, 1995.
# This code is licensed by AT&T Corp.  For the
# terms and conditions of the license, see
# http://www.research.att.com/orgs/ssr/book/reuse

BEGIN	{ s = ARGV[1]; gsub("\\.","_",s); printf("char *%s[] = {\n",s); }
/^#/	{ printf("/*%s */\n",substr($0,2)); next;}
		{ gsub("\\\\","\\\\",$0); printf("\"%s\",\n",$0); }
END		{ printf("(char*)0 };\n"); }
