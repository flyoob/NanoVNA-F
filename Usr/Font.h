/*-----------------------------------------------------------------------------/
 * Module       : Font.h
 * Create       : 2019-07-11
 * Copyright    : hamelec.taobao.com
 * Author       : huanglong
 * Brief        : 
/-----------------------------------------------------------------------------*/
#ifndef _FONT_H
#define _FONT_H

typedef unsigned short	MWIMAGEBITS;	/* bitmap image unit size*/ 

/* builtin C-based proportional/fixed font structure*/ 
typedef struct { 
	char 			*name;		/* font name*/ 
	int				maxwidth;	/* max width in pixels*/ 
	int				height;		/* height in pixels*/ 
	int				ascent;		/* ascent (baseline) height*/ 
	int				firstchar;	/* first character in bitmap*/ 
	int				size;		/* font size in characters*/ 
	const MWIMAGEBITS 	*bits;		/* 16-bit right-padded bitmap data*/ 
	unsigned short 	*offset;	/* 256 offsets into bitmap data*/ 
	unsigned char 	*width;		/* 256 character widths or 0 if fixed*/ 
} MWCFONT;

#endif
