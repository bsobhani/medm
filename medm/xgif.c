/*
*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

Argonne National Laboratory (ANL), with facilities in the States of
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods.
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (630-252-2000).
*/
/*****************************************************************************
 *
 *     Original Author : Mark Anderson
 *     Second Author   : Frederick Vong
 *     Third Author    : Kenneth Evans, Jr.
 *
 *****************************************************************************
*/
/*****************************************************************************
 *
 * xgif.c - displays GIF pictures on an X11 display
 *
 *  Original Author: John Bradley, University of Pennsylvania
 *                   (bradley@cis.upenn.edu)
 *
 *	MDA - editorial comment:
 *		MAJOR MODIFICATIONS to make this sensible.
 *		be careful about running on non-32-bit machines
 *
 *****************************************************************************
*/

#define DEBUG_GIF 0

/* include files */
#include "medm.h"
#include "xgif.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define MAXEXPAND 16

/* Function prototypes */

/* KE: The following is the prototype for _XGetBitsPerPixel, which is a
 *   convenience function found in the MIT distribution in
 *   /opt/local/share/src/R5/mit/lib/X/XImUtil.c (at the APS)
 */
int _XGetBitsPerPixel(Display *dpy, int depth);

void AddToPixel(GIFData *gif, Byte Index);

/*
 * initialize for GIF processing
 */
Boolean initializeGIF(
  DisplayInfo *displayInfo,
  DlImage *dlImage)
{
    GIFData *gif;
    int x, y;
    unsigned int w, h;
    Boolean success;

    x = dlImage->object.x;
    y = dlImage->object.y;
    w = dlImage->object.width;
    h = dlImage->object.height;

  /* free any existing GIF resources */
    freeGIF(dlImage);

    if (!(gif = (GIFData *) malloc(sizeof(GIFData)))) {
	medmPrintf(1,"\ninitializeGIF: Memory allocation error\n");
	return(False);
    }

  /* (MDA) programmatically set nostrip to false - see what happens */
    gif->nostrip = False;
    gif->strip = 0;

    gif->theImage = NULL;
    gif->expImage = NULL;
    gif->fcol = 0;
    gif->bcol = 0;
    gif->mfont= 0;
    gif->mfinfo = NULL;
    gif->theCmap   = cmap;	/* MDA - used to be DefaultColormap() */
    gif->theGC     = DefaultGC(display,screenNum);
    gif->theVisual = DefaultVisual(display,screenNum);
    gif->numcols   = 0;

    gif->dispcells = DisplayCells(display, screenNum);
    if (gif->dispcells<255) {
	medmPrintf(1,"\ninitializeGIF: At least an 8-plane display is required");
	return(False);
    }

    dlImage->privateData = (XtPointer) gif;

  /*
   * open/read the file
   */
    success = loadGIF(displayInfo,dlImage);
    if (!success) {
	return(False);
    }

    gif->iWIDE = gif->theImage->width;
    gif->iHIGH = gif->theImage->height;

    gif->eWIDE = gif->iWIDE;
    gif->eHIGH = gif->iHIGH;

    resizeGIF(displayInfo,dlImage);

#if DEBUG_GIF
    {
	int i,j,n;
	Pixel pixel;
	int ntimes=11;

	n=0;
	printf("Pixel Map\n"
	  "        x    y    Pixel\n");
	for(j=0; j < gif->iHIGH; j++) {
	    for(i=0; i < gif->iWIDE; i++) {
		if(n++ < ntimes) {
		    pixel=XGetPixel(gif->theImage,i,j);
		    printf("%4d %4d %4d %8x\n",
		      n,i,j,pixel);
		}
	    }
	}
    }
#endif    
    
    if (gif->expImage) {
	XSetForeground(display,gif->theGC,gif->bcol);
	XFillRectangle(display,XtWindow(displayInfo->drawingArea),
	  gif->theGC,x,y,w,h);
#if 1
#ifdef WIN32
      /* Exceed has the byte_order wrong for 24-bit */
	gif->expImage->byte_order=LSBFirst;
#endif	

	XPutImage(display,XtWindow(displayInfo->drawingArea),
	  gif->theGC,gif->expImage,
	  0,0,x,y,w,h);
	XPutImage(display,displayInfo->drawingAreaPixmap,
	  gif->theGC,gif->expImage,
	  0,0,x,y,w,h);
#else
#ifdef WIN32
      /* Exceed has the byte_order wrong for 24-bit */
	gif->theImage->byte_order=LSBFirst;
#endif	

	XPutImage(display,XtWindow(displayInfo->drawingArea),
	  gif->theGC,gif->theImage,
	  0,0,x,y,gif->theImage->width,gif->theImage->height);
	XPutImage(display,displayInfo->drawingAreaPixmap,
	  gif->theGC,gif->theImage,
	  0,0,x,y,gif->theImage->width,gif->theImage->height);
#endif
	XSetForeground(display,gif->theGC,gif->fcol);
    }
    return(True);

}

void drawGIF(
  DisplayInfo *displayInfo,
  DlImage *dlImage)
{
    GIFData *gif;
    int x, y;
    unsigned int w, h;

    gif = (GIFData *) dlImage->privateData;

    if (gif->expImage != NULL) {
	x = dlImage->object.x;
	y = dlImage->object.y;
	w = dlImage->object.width;
	h = dlImage->object.height;

      /* draw to pixmap, since traversal will copy pixmap to window..*/
#if 1
#ifdef WIN32
      /* Exceed has the byte_order wrong for 24-bit */
	gif->expImage->byte_order=LSBFirst;
#endif	

	XPutImage(display,displayInfo->drawingAreaPixmap,
	  gif->theGC,gif->expImage,
	  0,0,x,y,w,h);
#else
#ifdef WIN32
      /* Exceed has the byte_order wrong for 24-bit */
	gif->theImage->byte_order=LSBFirst;
#endif	

	XPutImage(display,displayInfo->drawingAreaPixmap,
	  gif->theGC,gif->theImage,
	  0,0,x,y,gif->theImage->width,gif->theImage->height);
#endif
    }
}

/***********************************/
#ifdef __cplusplus
void resizeGIF(DisplayInfo *,DlImage *dlImage)
#else
void resizeGIF(DisplayInfo *displayInfo,DlImage *dlImage)
#endif
{
    GIFData *gif;
    unsigned int w,h;

    static char *rstr = "Resizing Image.  Please wait...";

  /* warning:  this code'll only run machines where int=32-bits */

    gif = (GIFData *) dlImage->privateData;
    w = dlImage->object.width;
    h = dlImage->object.height;

  /* simply return if no GIF image attached */
    if (gif->expImage == NULL && gif->theImage == NULL) return;


    gif->currentWidth = w;
    gif->currentHeight= h;

    if ((int)w==gif->iWIDE && (int)h==gif->iHIGH) {
      /* Very special case (size = actual GIF size) */
        if (gif->expImage != gif->theImage) {
            if (gif->expImage != NULL) {
		free(gif->expImage->data);
		gif->expImage->data = NULL;
		XDestroyImage((XImage *)(gif->expImage));
	    }
            gif->expImage = gif->theImage;
            gif->eWIDE = gif->iWIDE;  gif->eHIGH = gif->iHIGH;
	}
    } else {				/* have to do some work */
      /* if it's a big image, this'll take a while.  mention it */
        if (w*h>(400*400)) {
	  /* (MDA) - could change cursor to stopwatch...*/
	}
	
      /* first, kill the old gif->expImage, if one exists */
	if (gif->expImage && gif->expImage != gif->theImage) {
            free(gif->expImage->data);
	    gif->expImage->data = NULL;
            XDestroyImage((XImage *)(gif->expImage));
	}

      /* create gif->expImage of the appropriate size */
        
        gif->eWIDE = w;  gif->eHIGH = h;
	
	switch (DefaultDepth(display,screenNum)) {
	case 8 : {
	    int  ix,iy,ex,ey;
	    Byte *ximag,*ilptr,*ipptr,*elptr,*epptr;
	    
	    ximag = (Byte *) malloc(w*h);
	    gif->expImage = XCreateImage(display,gif->theVisual,
	      DefaultDepth(display,screenNum),ZPixmap,
	      0,(char *)ximag, gif->eWIDE,gif->eHIGH,8,gif->eWIDE);
	    
	    if (!ximag || !gif->expImage) {
		medmPrintf(1,"\nresizeGIF: Unable to create a %dx%d image\n",
		  w,h);
		exit(-1);
	    }
	    
	    elptr = epptr = (Byte *) gif->expImage->data;

	    for (ey=0;  ey<gif->eHIGH;  ey++, elptr+=gif->eWIDE) {
		iy = (gif->iHIGH * ey) / gif->eHIGH;
		epptr = elptr;
		ilptr = (Byte *) gif->theImage->data + (iy * gif->iWIDE);
		for (ex=0;  ex<gif->eWIDE;  ex++,epptr++) {
		    ix = (gif->iWIDE * ex) / gif->eWIDE;
		    ipptr = ilptr + ix;
		    *epptr = *ipptr;
		}
	    }
	    break;
	}
	case 24 : {
	    int sx, sy, dx, dy;
	    int sh, dh, sw, dw;
	    char *ximag,*ilptr,*ipptr,*elptr,*epptr;
	    int bytesPerPixel = gif->theImage->bits_per_pixel/8;

	    ximag = (char *) malloc(w*h*bytesPerPixel);
	    gif->expImage = XCreateImage(display,gif->theVisual,
	      DefaultDepth(display,screenNum),ZPixmap,
	      0,(char *)ximag, gif->eWIDE,gif->eHIGH,32,
	      0);

	    if (!ximag || !gif->expImage) {
		medmPrintf(1,"\nresizeGIF: Unable to create a %dx%d image\n",
		  w,h);
		medmPrintf(0,"  24 bit: ximag = %08x, expImage = %08x\n",ximag,
		  gif->expImage);
		exit(-1);
	    }

#if 1
	    sw = gif->theImage->width;
	    sh = gif->theImage->height;
	    dw = gif->expImage->width;
	    dh = gif->expImage->height;
	    elptr = epptr = (char *) gif->expImage->data;
	    for (dy=0;  dy<dh; dy++) {
		sy = (sh * dy) / dh;
		epptr = elptr;
		ilptr = gif->theImage->data + (sy * gif->theImage->bytes_per_line);
		for (dx=0;  dx <dw;  dx++) {
		    sx = (sw * dx) / dw;
		    ipptr = ilptr + sx*bytesPerPixel;
		    *epptr++ = *ipptr++;
		    *epptr++ = *ipptr++;
		    *epptr++ = *ipptr++;
		    if (bytesPerPixel == 4) {
			*epptr++ = *ipptr++;
		    }
		}
		elptr += gif->expImage->bytes_per_line;
	    }
#endif
	    break;
	}
	}
    }
}
                
/*
 *  xgifload.c contents
 */

#define NEXTBYTE (*ptr++)
#define IMAGESEP 0x2c
#define INTERLACEMASK 0x40
#define COLORMAPMASK 0x80

FILE *fp;

int BitOffset,			/* Bit Offset of next code */
  XC, YC,			/* Output X and Y coords of current pixel */
  Pass,			/* Used by output routine if interlaced pic */
  OutCount,			/* Decompressor output 'stack count' */
  RWidth, RHeight,		/* screen dimensions */
  Width, Height,		/* image dimensions */
  LeftOfs, TopOfs,		/* image offset */
  BitsPerPixel,		/* Bits per pixel, read from GIF header */
  BytesPerScanline,		/* Bytes per scanline in output raster */
  ColorMapSize,		/* number of colors */
  Background,			/* background color */
  CodeSize,			/* Code size, read from GIF header */
  InitCodeSize,		/* Starting code size, used during Clear */
  Code,			/* Value returned by ReadCode */
  MaxCode,			/* limiting value for current code size */
  ClearCode,			/* GIF clear code */
  EOFCode,			/* GIF end-of-information code */
  CurCode, OldCode, InCode,	/* Decompressor variables */
  FirstFree,			/* First free code, generated per GIF spec */
  FreeCode,			/* Decompressor, next free slot in hash table */
  FinChar,			/* Decompressor variable */
  BitMask,			/* AND mask for data size */
  ReadMask,			/* Code AND mask for current code size */
  BytesOffsetPerPixel,        /* Bytes offset per pixel */   
  ScreenDepth;                /* Bits per Pixel */

Boolean Interlace, HasColormap;
#if DEBUG_GIF
Boolean verbose = True;
#else
Boolean verbose = False;
#endif

Byte *Image;			/* The result array */
Byte *RawGIF;			/* The heap array to hold it, raw */
Byte *Raster;			/* The raster data stream, unblocked */

    /* The hash table used by the decompressor */
int Prefix[4096];
int Suffix[4096];

    /* An output array used by the decompressor */
int OutCode[1025];

/* The color map, read from the GIF header */
Byte Red[256], Green[256], Blue[256];

char *id = "GIF87a";
char *id89 = "GIF89a";

/*****************************/
#ifdef __cplusplus
Boolean loadGIF(DisplayInfo *, DlImage *dlImage)
#else
Boolean loadGIF(DisplayInfo *displayInfo, DlImage *dlImage)
#endif
/*****************************/
{
    GIFData *gif;
    char *fname;
    Boolean success;
    int            filesize;
    int            bits_per_pixel;
    register Byte  ch, ch1;
    register Byte *ptr, *ptr1;
    register int   i,j;
    static Byte    lmasks[8] = {0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80};
    Byte           lmask;

    char fullPathName[2*MAX_TOKEN_LENGTH], dirName[2*MAX_TOKEN_LENGTH];
    char *dir;
    int startPos;

    gif = (GIFData *) dlImage->privateData;
    fname = dlImage->imageName;

/* initiliaze some globals */
    BitOffset = 0;
    XC = 0;
    YC = 0;
    Pass = 0;
    OutCount = 0;
    ScreenDepth = DefaultDepth(display,screenNum);

    if (strcmp(fname,"-")==0) {
	fp = stdin;
	fname = "<stdin>";
    } else {
#ifdef WIN32
      /* WIN32 opens files in text mode by default and then throws out CRLF */
	fp = fopen(fname,"rb");
#else
	fp = fopen(fname,"r");
#endif
    }

  /* try to get a valid GIF file somewhere */
  /* if not in current directory, look in EPICS_DISPLAY_PATH directory */
    if (fp == NULL) {
	dir = getenv("EPICS_DISPLAY_PATH");
	if (dir != NULL) {
	    startPos = 0;
	    while (fp == NULL &&
	      extractStringBetweenColons(dir,dirName,startPos,&startPos)) {
		strcpy(fullPathName,dirName);
		strcat(fullPathName,"/");
		strcat(fullPathName,fname);
		fp = fopen(fullPathName,"r");
	    }
	}
    }
    if (fp == NULL) {
	medmPrintf(1,"\nloadGIF: File not found:\n  %s\n",fname);
	return(False);
    }

  /* find the size of the file */
    fseek(fp, 0L, SEEK_END);
    filesize = (int) ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    success = True;

    if (!(ptr = RawGIF = (Byte *) malloc(filesize))) {
	medmPrintf(1,"\nloadGIF: Not enough memory to read GIF file\n");
	success = False;
    }

    if (!(Raster = (Byte *) malloc(filesize))) {
	medmPrintf(1,"\nloadGIF: Not enough memory to read GIF file\n");
	success = False;
    }

    if (fread((char *)ptr, filesize, 1, fp) != 1) {
	medmPrintf(1,"\nloadGIF: GIF data read failed\n");
	success = False;
    }

    if (!strncmp((char *)ptr, (char *)id89, 6)) {
	medmPrintf(1,"\nloadGIF: %s is not supported yet\n  %s\n",id89,fname);
	success = False;
    } else if (strncmp((char *)ptr, (char *)id, 6)) {
	medmPrintf(1,"\nloadGIF: Not a valid GIF file\n  %s\n",fname);
	success = False;
    }
    if (!success) return(False);

    ptr += 6;

  /* Get variables from the GIF screen descriptor */

    ch = NEXTBYTE;
    RWidth = ch + 0x100 * NEXTBYTE;	/* screen dimensions... not used. */
    ch = NEXTBYTE;
    RHeight = ch + 0x100 * NEXTBYTE;

#if DEBUG_GIF
    {
	static int ifirst=1;
	
	if(ifirst) {
	    int depth=DefaultDepth(display,screenNum);
	    int bitmap_bit_order=BitmapBitOrder(display);
	    int bitmap_pad=BitmapPad(display);
	    int bitmap_unit=BitmapUnit(display);
	    int byte_order=ImageByteOrder(display);
	    int bits_per_pixel=_XGetBitsPerPixel(display,depth);
	    
	    ifirst=0;
	    printf("\nX Server Image Properties:\n");
	    printf("  depth=%d [%d colors (0-%x)]\n",depth,1<<depth,
	      (int)(1<<depth)-1);
	    printf("  bitmap_bit_order = %d [LSBFirst = %d MSBFirst = %d]\n",
	      bitmap_bit_order,LSBFirst,MSBFirst);
	    printf("  bitmap_pad = %d\n",bitmap_pad);
	    printf("  bitmap_unit = %d\n",bitmap_unit);
	    printf("  byte_order = %d [LSBFirst = %d MSBFirst = %d]\n",
	      byte_order,LSBFirst,MSBFirst);
	    printf("  bits_per_pixel = %d\n",bits_per_pixel);
	}
    }
#endif    

    if (verbose)
      printf("\nloadGIF: %s has screen dimimensions: %dx%d\n",
	fname, RWidth, RHeight);

    ch = NEXTBYTE;
    HasColormap = ((ch & COLORMAPMASK) ? True : False);

    BitsPerPixel = (ch & 7) + 1;
    gif->numcols = ColorMapSize = 1 << BitsPerPixel;
    BitMask = ColorMapSize - 1;

    Background = NEXTBYTE;		/* background color... not used. */

    if (NEXTBYTE) {		/* supposed to be NULL */
	medmPrintf(1,"\nloadGIF: Corrupt GIF file (bad screen descriptor)\n  %s\n",
	  fname);
    }

  /* Read in global colormap. */

    if (HasColormap) {
	if (verbose) {
	    printf("loadGIF: %s is %dx%d, %d bits per pixel, (%d colors).\n",
	      fname, RWidth, RHeight, BitsPerPixel, ColorMapSize);
	}

	for (i = 0; i < ColorMapSize; i++) {
	    Red[i] = NEXTBYTE;
	    Green[i] = NEXTBYTE;
	    Blue[i] = NEXTBYTE;
	}

      /* Allocate the X colors for this picture */

        if (gif->nostrip) {   /* nostrip was set.  try REAL hard to do it */
            j = 0;
            lmask = lmasks[gif->strip];
            for (i=0; i<gif->numcols; i++) {
                gif->defs[i].red   = (Red[i]  &lmask)<<8;
                gif->defs[i].green = (Green[i]&lmask)<<8;
                gif->defs[i].blue  = (Blue[i] &lmask)<<8;
                gif->defs[i].flags = DoRed | DoGreen | DoBlue;
                if (!XAllocColor(display,gif->theCmap,&gif->defs[i])) { 
                    j++;  gif->defs[i].pixel = 0xffff;
		}
                gif->cols[i] = gif->defs[i].pixel;
	    }
	    
            if (j) {		/* failed to pull it off */
                XColor ctab[256];
		
                medmPrintf(1,"\nloadGIF: Failed to allocate %d out of %d colors\n"
		  "  Trying extra hard\n",
		  j,gif->numcols);
                
	      /* read in the color table */
                for (i=0; i<gif->numcols; i++) ctab[i].pixel = i;
                XQueryColors(display,gif->theCmap,ctab,gif->numcols);
                
                for (i=0; i<gif->numcols; i++) {
		    if (gif->cols[i] == 0xffff) {	/* an unallocated pixel */
			int d, mdist, close;
			unsigned long r,g,b;
			
			mdist = 100000;   close = -1;
			r =  Red[i];
			g =  Green[i];
			b =  Blue[i];
			for (j=0; j<gif->numcols; j++) {
			    d = abs((int)(r - (ctab[j].red>>8))) +
			      abs((int)(g - (ctab[j].green>>8))) +
			      abs((int)(b - (ctab[j].blue>>8)));
			    if (d<mdist) { mdist=d; close=j; }
			}
			if (close<0) {
			    medmPrintf(1,"loadGIF: Simply can't do it -- Sorry\n");
			}
			memcpy( (void*)(&gif->defs[i]),
			  (void*)(&gif->defs[close]),
			  sizeof(XColor));
			gif->cols[i] = ctab[close].pixel;
		    }
		}
	    }
	} else {          /* strip wasn't set, do the best auto-strip */
            j = 0;
            while (gif->strip<8) {
                lmask = lmasks[gif->strip];
                for (i=0; i<gif->numcols; i++) {
                    gif->defs[i].red   = (Red[i]  &lmask)<<8;
                    gif->defs[i].green = (Green[i]&lmask)<<8;
                    gif->defs[i].blue  = (Blue[i] &lmask)<<8;
                    gif->defs[i].flags = DoRed | DoGreen | DoBlue;
                    if (!XAllocColor(display,gif->theCmap,&gif->defs[i]))
		      break;
                    gif->cols[i] = gif->defs[i].pixel;
		}
                if (i<gif->numcols) {     /* Failed */
#if DEBUG_GIF
		    printf("Auto strip %d (mask %2x) failed for color %d"
		      " [R=%hx G=%hx B=%hx]\n",
		      gif->strip,lmask,i,gif->defs[i].red,gif->defs[i].green,
		      gif->defs[i].blue);
#endif		    
                    gif->strip++;  j++;
                    XFreeColors(display,gif->theCmap,gif->cols,i,0L);
		} else {
		    break;
		}
	    }
	    
            if (j && gif->strip<8) {
		medmPrintf(0,"\nloadGIF: %s was masked to level %d"
		  " (mask 0x%2X)\n",
		  fname,gif->strip,lmask);
		if (verbose)
		  printf("loadGIF: %s was masked to level %d (mask 0x%2X)\n",
		    fname,gif->strip,lmask);
	    }
	    
            if (gif->strip==8) {
                medmPrintf(1,"\nloadGIF: "
		  "Failed to allocate the desired colors\n");
                for (i=0; i<gif->numcols; i++) gif->cols[i]=i;
	    }
	}
    } else {  /* no colormap in GIF file */
        medmPrintf(1,"\nloadGIF:  Warning!  No colortable in this file."
	  "  Winging it.\n");
        if (!gif->numcols) gif->numcols=256;
        for (i=0; i<gif->numcols; i++) gif->cols[i] = (unsigned long) i;
    }

/* Check for image separator */
    if (NEXTBYTE != IMAGESEP) {
	medmPrintf(1,"\nloadGIF: Corrupt GIF file (No image separator)\n");
    }

/* Now read in values from the image descriptor */
    ch = NEXTBYTE;
    LeftOfs = ch + 0x100 * NEXTBYTE;
    ch = NEXTBYTE;
    TopOfs = ch + 0x100 * NEXTBYTE;
    ch = NEXTBYTE;
    Width = ch + 0x100 * NEXTBYTE;
    ch = NEXTBYTE;
    Height = ch + 0x100 * NEXTBYTE;
    Interlace = ((NEXTBYTE & INTERLACEMASK) ? True : False);

    if (verbose) {
        printf("loadGIF: %s is %dx%d, %d colors, %sinterlaced\n",
	  fname, Width,Height,ColorMapSize,(Interlace) ? "" : "non-");
    }

  /* Note that I ignore the possible existence of a local color map.
   * I'm told there aren't many files around that use them, and the spec
   * says it's defined for future use.  This could lead to an error
   * reading some files. 
   */
    
  /* Start reading the raster data. First we get the intial code size
   * and compute decompressor constant values, based on this code size.
   */
    
    CodeSize = NEXTBYTE;
    ClearCode = (1 << CodeSize);
    EOFCode = ClearCode + 1;
    FreeCode = FirstFree = ClearCode + 2;
    
  /* The GIF spec has it that the code size is the code size used to
   * compute the above values is the code size given in the file, but the
   * code size used in compression/decompression is the code size given in
   * the file plus one. (thus the ++).
   */
    
    CodeSize++;
    InitCodeSize = CodeSize;
    MaxCode = (1 << CodeSize);
    ReadMask = MaxCode - 1;

  /* Read the raster data.  Here we just transpose it from the GIF array
   * to the Raster array, turning it from a series of blocks into one long
   * data stream, which makes life much easier for ReadCode().
   */

    ptr1 = Raster;
    do {
	ch = ch1 = NEXTBYTE;
	while (ch--) *ptr1++ = NEXTBYTE;
	if ((Raster - ptr1) > filesize){
	    medmPrintf(1,"\nloadGIF: Corrupt GIF file (Unblock)\n");
	}
    } while(ch1);

    free((char *)RawGIF);	/* We're done with the raw data now... */

    if (verbose)
      printf("loadGIF: %s decompressing...\n",fname);


/* Allocate the X Image */
    switch (ScreenDepth) {
    case 8 :
        BytesOffsetPerPixel = 1;
        Image = (Byte *) malloc(Width*Height);
        if (!Image) {
	    medmPrintf(1,"\nloadGIF: Not enough memory for XImage\n");
	    return(False);
        }
        gif->theImage = XCreateImage(display,gif->theVisual,
	  ScreenDepth,ZPixmap,0,
	  (char*)Image,Width,Height,32,0);
        break;
    case 24 :
	bits_per_pixel = _XGetBitsPerPixel(display, ScreenDepth);
        BytesOffsetPerPixel = bits_per_pixel/8;
#if DEBUG_GIF	
        printf("LoadGIF: %s (24-bit display, %d bits per pixel)\n"
	  "  BytesOffsetPerPixel = %d\n",
	  fname, bits_per_pixel, BytesOffsetPerPixel);
#endif	
        Image = (Byte *) malloc(BytesOffsetPerPixel*Width*Height);
        if (!Image) {
	    medmPrintf(1,"\nloadGIF: Not enough memory for XImage\n");
	    return(False);
        }
        gif->theImage = XCreateImage(display,gif->theVisual,
	  ScreenDepth,ZPixmap,0,
	  (char*)Image,Width,Height,32,0);
        break;
    }
    BytesPerScanline = gif->theImage->bytes_per_line;

    if (!gif->theImage) {
	medmPrintf(1,"\nloadGIF: Unable to create XImage\n");
	return(False);
    }

  /* Decompress the file, continuing until you see the GIF EOF code.
   * One obvious enhancement is to add checking for corrupt files here.
   */
    Code = ReadCode();
    while (Code != EOFCode) {

      /* Clear code sets everything back to its initial value, then reads the
       * immediately subsequent code as uncompressed data.
       */
	if (Code == ClearCode) {
	    CodeSize = InitCodeSize;
	    MaxCode = (1 << CodeSize);
	    ReadMask = MaxCode - 1;
	    FreeCode = FirstFree;
	    CurCode = OldCode = Code = ReadCode();
	    FinChar = CurCode & BitMask;
	    AddToPixel(gif,(Byte)FinChar);
	}
	else {

	  /* If not a clear code, then must be data: save same as CurCode and InCode */
	    CurCode = InCode = Code;

	  /* If greater or equal to FreeCode, not in the hash table yet;
	   * repeat the last character decoded
	   */
	    if (CurCode >= FreeCode) {
		CurCode = OldCode;
		OutCode[OutCount++] = FinChar;
	    }

	  /* Unless this code is raw data, pursue the chain pointed to by CurCode
	   * through the hash table to its end; each code in the chain puts its
	   * associated output code on the output queue.
	   */

	    while (CurCode > BitMask) {
		if (OutCount > 1024){
		    medmPrintf(1,"\nloadGIF: Corrupt GIF file (OutCount)\n");
		    return False;
		}
		OutCode[OutCount++] = Suffix[CurCode];
		CurCode = Prefix[CurCode];
	    }

	  /* The last code in the chain is treated as raw data. */
	    FinChar = CurCode & BitMask;
	    OutCode[OutCount++] = FinChar;

	  /* Now we put the data out to the Output routine.
	   * It's been stacked LIFO, so deal with it that way...
	   */
	    for (i = OutCount - 1; i >= 0; i--)
	      AddToPixel(gif,(Byte)OutCode[i]);
	    OutCount = 0;

	  /* Build the hash table on-the-fly. No table is stored in the file. */
	    Prefix[FreeCode] = OldCode;
	    Suffix[FreeCode] = FinChar;
	    OldCode = InCode;

	  /* Point to the next slot in the table.  If we exceed the current
	   * MaxCode value, increment the code size unless it's already 12.  If it
	   * is, do nothing: the next code decompressed better be CLEAR
	   */
	    FreeCode++;
	    if (FreeCode >= MaxCode) {
		if (CodeSize < 12) {
		    CodeSize++;
		    MaxCode *= 2;
		    ReadMask = (1 << CodeSize) - 1;
		}
	    }
	}
	Code = ReadCode();
    }

    free((char *)Raster);

    if (verbose)
      printf("loadGIF: %s done\n",fname);

    if (fp != stdin)
      fclose(fp);

    return(True);
}


/* Fetch the next code from the raster data stream.  The codes can be
 * any length from 3 to 12 bits, packed into 8-bit Bytes, so we have to
 * maintain our location in the Raster array as a BIT Offset.  We compute
 * the Byte Offset into the raster array by dividing this by 8, pick up
 * three Bytes, compute the bit Offset into our 24-bit chunk, shift to
 * bring the desired code to the bottom, then mask it off and return it. 
 */
ReadCode()
{
    int RawCode, ByteOffset;

    ByteOffset = BitOffset / 8;
    RawCode = Raster[ByteOffset] + (0x100 * Raster[ByteOffset + 1]);
    if (CodeSize >= 8)
      RawCode += (0x10000 * Raster[ByteOffset + 2]);
    RawCode >>= (BitOffset % 8);
    BitOffset += CodeSize;
    return(RawCode & ReadMask);
}

void dump(GIFData *gif) {
    int i;
    for (i=0; i<32; i++) {
	if (i && ((i>>4)<<4) == i) {
	    printf("\n");
	}
	printf("%02x ",(unsigned char) gif->theImage->data[i]);
    }
    printf("\n");
}

void AddToPixel(GIFData *gif, Byte Index)
{
    switch (ScreenDepth) {
    case 8 :
	*(Image + YC * BytesPerScanline + XC) =
	  (unsigned char)gif->cols[Index&(gif->numcols-1)];
	break;
    case 24 : {
	unsigned char *p = (unsigned char *) Image + YC * BytesPerScanline + XC;
	Pixel clr = gif->cols[Index&(gif->numcols-1)];
	if (BytesOffsetPerPixel == 4) {
	    *((Pixel *) p) = clr;
	} else {
	    *p++ = (unsigned char) ((clr & 0x00ff0000) >> 16);
	    *p++ = (unsigned char) ((clr & 0x0000ff00) >> 8);
	    *p = (unsigned char) ((clr & 0x000000ff));
	}
    }
    break;
    }

  /* Update the X-coordinate, and if it overflows, update the Y-coordinate */

    XC = XC + BytesOffsetPerPixel;
    if (XC + BytesOffsetPerPixel > BytesPerScanline) {

      /* If a non-interlaced picture, just increment YC to the next scan line. 
       * If it's interlaced, deal with the interlace as described in the GIF
       * spec.  Put the decoded scan line out to the screen if we haven't gone
       * past the bottom of it
       */
	XC = 0;
	if (!Interlace) {
	    YC++;
	} else {
	    switch (Pass) {
	    case 0:
		YC += 8;
		if (YC >= Height) {
		    Pass++;
		    YC = 4;
		}
		break;
	    case 1:
		YC += 8;
		if (YC >= Height) {
		    Pass++;
		    YC = 2;
		}
		break;
	    case 2:
		YC += 4;
		if (YC >= Height) {
		    Pass++;
		    YC = 1;
		}
		break;
	    case 3:
		YC += 2;
		break;
	    default:
		break;
	    }
	}
    }
}

/*
 * free the X images and color cells in the default colormap (in anticipation
 *   of a new image)
 */
void freeGIF(DlImage *dlImage)
{
    GIFData *gif;

    gif = (GIFData *) dlImage->privateData;
    if (gif) {
      /* kill the old images */
	if (gif->expImage) {
	    free(gif->expImage->data);
	    gif->expImage->data = NULL;
	    XDestroyImage((XImage *)(gif->expImage));
	    gif->expImage = NULL;
	}
	if (gif->theImage) {
	    free(gif->theImage->data);
	    gif->theImage->data = NULL;
	    XDestroyImage((XImage *)(gif->theImage));
	    gif->theImage = NULL;
	}

	if (gif->numcols > 0) {
	    XFreeColors(display,gif->theCmap,gif->cols,gif->numcols,(unsigned long)0);
	    gif->numcols = 0;
	}
/* free existing private data */
	free( (char *) dlImage->privateData);
	dlImage->privateData = NULL;
    }
}
