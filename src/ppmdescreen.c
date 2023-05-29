/*
 * ppmdescreen.c (aka undither.c):
 *
 * Copyright (c) 2002 James McKenzie <james@fishsoup.dhs.org>,
 * All rights reserved.
 *
 */

static char rcsid[] = "$Id: ppmdescreen.c,v 1.5 2002/10/14 18:48:48 root Exp $";

/*
 * $Log: undither.c,v $
 * Revision 1.5  2002/10/14 18:48:48  root
 * *** empty log message ***
 *
 * Revision 1.4  2002/10/14 18:48:17  root
 * *** empty log message ***
 *
 * Revision 1.3  2002/10/14 18:27:47  root
 * *** empty log message ***
 *
 * Revision 1.2  2002/10/14 18:04:14  root
 * *** empty log message ***
 *
 * Revision 1.1  2002/10/14 16:47:11  root
 * Initial revision
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include <fftw.h>
#include <rfftw.h>
#include <string.h>

#include "params.h"

rfftwnd_plan fplan, bplan;
FILE *ws;

void
usage (char *why)
{

    fprintf (stderr, "Usage: (called because %s)\n", why);
    fprintf(stderr,"\n");
    fprintf(stderr,"Automatic mode\n");
    fprintf(stderr,"  ppmdescreen -i inputfile -a level -o outfile\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Generate power spectrum mode\n");
    fprintf(stderr,"  ppmdescreen -i inputfile [-a level] -p psfile\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Filter using kill file\n");
    fprintf(stderr,"  ppmdescreen -i inputfile -k killfile -o outfile\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"if you want to know how to make this work\n");
    fprintf(stderr,"go to http://www.madingley.org/james/resources/undither\n");
    fprintf(stderr,"or https://github.com/ImageProcessing-ElectronicPublications/ppmdescreen\n");
    exit (1);
}

void
open_wisdom (void)
{

    ws = fopen ("wisdom", "r+");
    if (!ws)
        ws = fopen ("wisdom", "w+");
    if (!ws)
        usage ("Can't create wisdom file");

    fftw_import_wisdom_from_file (ws);
}

void
close_wisdom (void)
{
    rewind (ws);
    fftw_export_wisdom_to_file (ws);
    fclose (ws);
}

int
open_and_map (char *name, unsigned char **ptr, int mode, size_t len)
{
    size_t ps;
    int fd;
    unsigned char c;

    ps = getpagesize ();
    ps--;

    fd = open (name, O_RDWR | O_CREAT, 0666);

    if (!len)
    {
        len = lseek (fd, 0, SEEK_END);
        len += ps;
        len &= ~ps;
    }
    else
    {
        len += ps;
        len &= ~ps;
        lseek (fd, len - 1, SEEK_SET);
        read (fd, &c, 1);
        lseek (fd, len - 1, SEEK_SET);
        write (fd, &c, 1);
    }

    *ptr = (void *) 0;

    switch (mode)
    {
    case O_RDONLY:
        *ptr =
            (unsigned char *) mmap ((void *) 0, len, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, fd, 0);
        break;
    case O_RDWR:
        *ptr =
            (unsigned char *) mmap ((void *) 0, len, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, fd, 0);
        break;
    default:
        usage ("unknown mode");
    }

    if (!*ptr)
    {
        perror ("mmap");
        usage ("mmap failed");
    }

    return fd;
}

void
check_suitability (char *in, char *out, int *off, int *len, int *w, int *h)
{
    FILE *f;
    FILE *g;
    char buf[1024];

    f = fopen (in, "r");
    if (!f)
        usage ("open input file");
    if (out)
    {
        g = fopen (out, "w");
        if (!g)
            usage ("open output file");
    }
    else
    {
        g = NULL;
    }

    bzero (buf, sizeof (buf));
    fgets (buf, sizeof (buf), f);
    if ((buf[0] != 'P') || (buf[1] != '6'))
        usage ("ppm files only");
    if (g)
        fputs (buf, g);
    do
    {
        fgets (buf, sizeof (buf), f);
        if (g)
            fputs (buf, g);
    }
    while (buf[0] == '#');
    sscanf (buf, "%d %d", w, h);
    do
    {
        fgets (buf, sizeof (buf), f);
        if (g)
            fputs (buf, g);
    }
    while (buf[0] == '#');

    *off = ftell (f);
    fseek (f, 0, SEEK_END);
    *len = ftell (f);
    if (g)
        fclose (g);
    fclose (f);
}

void
fgetsnc (char *buf, int i, FILE * f)
{
    buf[0] = '#';

    while (buf[0] == '#')
        fgets (buf, i, f);
}

fftw_real *
do_powerspectrum (unsigned char *iptr, int w, int h)
{
    int bx, by, x, y, tx, ty;
    fftw_real *ps;

    ps = malloc (sizeof (fftw_real) * NY * NQ * 3);
    bzero (ps, sizeof (fftw_real) * NY * NQ * 3);

    {
        unsigned char *brp = iptr;

        fftw_real *data;
        fftw_complex *fft;

        data = malloc (sizeof (fftw_real) * NX * NY * 3);

        fft = malloc (sizeof (fftw_complex) * NY * NQ * 3);
        bzero (fft, sizeof (fftw_complex) * NY * NQ * 3);

        printf ("Calculating power spectrum\n");

        for (by = 0; by < (h); by += NY)
        {
            unsigned char *bp = brp;

            ty = h - by;
            if (ty > NY)
                ty = NY;

            for (bx = 0; bx < (w); bx += NX)
            {
                unsigned char *rp = bp;
                fftw_real *rptr = data;

                printf ("\t%d\t%d,\treading", bx, by);
                fflush (stdout);

                tx = w - bx;
                if (tx > NX)
                    tx = NX;

                bzero (data, sizeof (fftw_real) * NX * NY * 3);

                for (y = 0; y < ty; y++)
                {
                    unsigned char *p = rp;
                    fftw_real *ptr = rptr;
                    for (x = 0; x < tx; x++)
                    {
                        *ptr = ((fftw_real) * (p++)) / 255.0f;
                        *(ptr + OD) = ((fftw_real) * (p++)) / 255.0f;
                        *(ptr + OD2) = ((fftw_real) * (p++)) / 255.0f;

                        ptr++;
                    }
                    rp += 3 * w;
                    rptr += NX;
                }

                printf (", forwards");
                fflush (stdout);
                rfftwnd_real_to_complex (fplan, 3, data, 1, OD, fft, 1, OF);

                printf (", analyse");
                fflush (stdout);

                {
                    int n = NQ * NY * 3;
                    fftw_complex *cptr = fft;
                    fftw_real *ptr = ps;

                    while (n--)
                    {
                        fftw_real f =
                            ((cptr->re) * (cptr->re)) + ((cptr->im) * (cptr->im));
                        if (f > (*ptr))
                            *ptr = f;
                        cptr++;
                        ptr++;
                    }
                }

                printf (", done\n");
                fflush (stdout);

                bp += (3 * NX);
            }
            brp += (3 * w * NY);
        }
        free (fft);
        free (data);
    }
    printf ("done\n");

    return ps;
}

void
do_filter (unsigned char *iptr, unsigned char *optr, unsigned char *filter,
           int w, int h)
{
    int bx, by, x, y, tx, ty;
    unsigned char *brp = iptr;
    unsigned char *borp = optr;

    fftw_real *data;
    fftw_complex *fft;

    data = malloc (sizeof (fftw_real) * NX * NY * 3);

    fft = malloc (sizeof (fftw_complex) * NY * NQ * 3);
    bzero (fft, sizeof (fftw_complex) * NY * NQ * 3);

    printf ("Filtering\n");

    for (by = 0; by < (h); by += NY)
    {
        unsigned char *bp = brp;
        unsigned char *bop = borp;

        ty = h - by;
        if (ty > NY)
            ty = NY;

        for (bx = 0; bx < (w); bx += NX)
        {
            unsigned char *rp = bp;
            unsigned char *orp = bop;
            fftw_real *rptr = data;

            printf ("\t%d\t%d,\treading", bx, by);
            fflush (stdout);

            tx = w - bx;
            if (tx > NX)
                tx = NX;

            bzero (data, sizeof (fftw_real) * NX * NY * 3);

            for (y = 0; y < ty; y++)
            {
                unsigned char *p = rp;
                fftw_real *ptr = rptr;
                for (x = 0; x < tx; x++)
                {
                    *ptr = ((fftw_real) * (p++)) / 255.0f;
                    *(ptr + OD) = ((fftw_real) * (p++)) / 255.0f;
                    *(ptr + OD2) = ((fftw_real) * (p++)) / 255.0f;

                    ptr++;
                }
                rp += 3 * w;
                rptr += NX;
            }

            printf (", forwards");
            fflush (stdout);
            rfftwnd_real_to_complex (fplan, 3, data, 1, OD, fft, 1, OF);

            printf (", filter");
            fflush (stdout);

            {
                int n = NQ * NY;
                fftw_complex *cptr = fft;
                unsigned char *ptr = filter;

                while (n--)
                {
                    if (*(ptr++))
                    {
                        cptr->re = 0;
                        cptr->im = 0;
                    }
                    cptr++;
                }

                n = NQ * NY;
                ptr = filter;

                while (n--)
                {
                    if (*(ptr++))
                    {
                        cptr->re = 0;
                        cptr->im = 0;
                    }
                    cptr++;
                }

                n = NQ * NY;
                ptr = filter;

                while (n--)
                {
                    if (*(ptr++))
                    {
                        cptr->re = 0;
                        cptr->im = 0;
                    }
                    cptr++;
                }
            }

            printf (", backwards");
            fflush (stdout);

            rfftwnd_complex_to_real (bplan, 3, fft, 1, OF, data, 1, OD);

            printf (", writing");
            fflush (stdout);

            {
                fftw_real scale = ((fftw_real) (NX * NY)) / 255.0;
                rptr = data;

#define INNARDS(o) \
  i=(int) ((*(o))/scale); \
  if (i<0) i=0;\
  if (i>255) i=255;\

                for (y = 0; y < ty; y++)
                {
                    unsigned char *op = orp;
                    fftw_real *ptr = rptr;
                    for (x = 0; x < tx; x++)
                    {
                        int i;
                        INNARDS (ptr);
                        *(op++) = i;
                        INNARDS (ptr + OD);
                        *(op++) = i;
                        INNARDS (ptr + OD2);
                        *(op++) = i;

                        ptr++;
                    }
                    orp += 3 * w;
                    rptr += NX;
                }
#undef INNARDS

            }
            printf (", done\n");
            fflush (stdout);

            bp += (3 * NX);
            bop += (3 * NX);
        }
        brp += (3 * w * NY);
        borp += (3 * w * NY);
    }

    free (fft);
    free (data);

    printf ("done\n");

}

unsigned char *
ps_to_ppm (fftw_real * ps)
{
    int n = NQ * NY;
    fftw_real l = 0, s = 1e20;
    int c;
    int skip;
    fftw_real *ptr = ps;

    unsigned char *ret = malloc (NQ * NY * 3);
    unsigned char *iptr = ret;
    if (!ret)
        usage ("out of memory");

    while (n--)
    {
        if ((*ptr) > l)
            l = *ptr;
        ptr++;
    }

    ptr = ps;

    l = log (l);

#define DR 25.0f

#define INNARDS(o) \
        if ((*(o))<0) { \
	   c=0; \
	} else { \
        f=log(*(o))-l;\
        f=((f+DR)*230.0f)/DR;\
        c = (int) (f + .5f);\
        if (c<0) c=0;\
        if (c>230) c=230; \
        c+=23; \
	}

    n = NQ * NY;

    /* Turn it upside down */
    skip = n >> 1;

    ptr = ps + skip;
    n -= skip;

    while (n--)
    {
        fftw_real f;
        INNARDS (ptr);
        *(iptr++) = c;
        INNARDS (ptr + OF);
        *(iptr++) = c;
        INNARDS (ptr + OF2);
        *(iptr++) = c;
        ptr++;
    }

    ptr = ps;
    n = skip;

    while (n--)
    {
        fftw_real f;
        INNARDS (ptr);
        *(iptr++) = c;
        INNARDS (ptr + OF);
        *(iptr++) = c;
        INNARDS (ptr + OF2);
        *(iptr++) = c;
        ptr++;
    }

#undef INNARDS

    return ret;
}

void
write_ppm_to_file (unsigned char *ppm, char *fname)
{
    FILE *fh = fopen (fname, "w");
    int n = NQ * NY;

    if (!fh)
        usage ("Can't open powerspectrum file");

    fprintf (fh, "P6\n%d %d\n255\n", NQ, NY);

    fwrite (ppm, NQ * 3, NY, fh);
    fclose (fh);
}

unsigned char *
read_ppm_from_file (char *fname)
{
    FILE *in = fopen (fname, "r");
    char buf[1024];
    unsigned char *ppm;

    int n = NQ * NY;

    if (!in)
        usage ("Can't open kill file");

    fgetsnc (buf, sizeof (buf), in);

    if ((buf[0] != 'P') || (buf[1] != '6'))
        usage ("kill file is not a ppm raw file");

    fgetsnc (buf, sizeof (buf), in);
    {
        int x, y;

        sscanf (buf, "%d %d", &x, &y);
        if ((x != NQ) || (y != NY))
            usage ("kill file has wrong dimensions");
    }
    fgetsnc (buf, sizeof (buf), in);

    ppm = (unsigned char *) malloc (NQ * NY * 3);

    fread (ppm, NQ * 3, NY, in);
    fclose (in);

    return ppm;
}

unsigned char *
ppm_to_filter (unsigned char *ppm)
{
    unsigned char *filter, *ptr;
    int n = NQ * NY;
    int skip;

    ptr = filter = (unsigned char *) malloc (NQ * NY);
    bzero (filter, NQ * NY);

    skip = n >> 1;
    n -= skip;
    ptr += skip;

    while (n--)
    {
        ppm++;
        if (!(*ppm))
            *ptr = 255;
        ppm++;
        ppm++;
        ptr++;
    }
    n = skip;
    ptr = filter;

    while (n--)
    {
        ppm++;
        if (!(*ppm))
            *ptr = 255;
        ppm++;
        ppm++;
        ptr++;
    }

    return filter;
}

void
autokill (unsigned char *ppm, float level)
{
    int x, y;
    int *sval;
    unsigned char *trptr = ppm;
    unsigned char *mrptr = ppm + (3 * NQ);
    unsigned char *brptr = ppm + (6 * NQ);
    int *srptr;
    int r, g, b, a, s, ms = 0;
    int radius;

    sval = malloc (sizeof (int) * NY * NQ);
    bzero (sval, sizeof (int) * NY * NQ);

    srptr = sval + NQ;

    printf ("Autokill: range");
    fflush (stdout);

    for (y = 1; y < (NY - 1); ++y)
    {
        unsigned char *tptr = trptr + 3;
        unsigned char *mptr = mrptr + 3;
        unsigned char *bptr = brptr + 3;
        int *sptr = srptr + 1;

        for (x = 1; x < (NQ - 1); ++x)
        {
            r = (*(tptr - 3)) + (*(tptr)) + (*(tptr + 3));
            r += (*(mptr - 3)) + (*(mptr)) + (*(mptr + 3));
            r += (*(bptr - 3)) + (*(bptr)) + (*(bptr + 3));
            g = (*(tptr - 2)) + (*(tptr + 1)) + (*(tptr + 4));
            g += (*(mptr - 2)) + (*(mptr + 1)) + (*(mptr + 4));
            g += (*(bptr - 2)) + (*(bptr + 1)) + (*(bptr + 4));
            b = (*(tptr - 1)) + (*(tptr + 2)) + (*(tptr + 5));
            b += (*(mptr - 1)) + (*(mptr + 2)) + (*(mptr + 5));
            b += (*(bptr - 1)) + (*(bptr + 2)) + (*(bptr + 5));

            if (((x * x) + ((NY - y) * (NY - y))) > (ARADIUS * ARADIUS))
            {
                a = (r + g + b) / 3;
                s = (r - a) * (r - a);
                if (s > (*sptr))
                    *sptr = s;
                s = (g - a) * (g - a);
                if (s > (*sptr))
                    *sptr = s;
                s = (b - a) * (b - a);
                if (s > (*sptr))
                    *sptr = s;

                if ((*sptr) > ms)
                    ms = (*sptr);
            }
            tptr += 3;
            mptr += 3;
            bptr += 3;
            sptr++;
        }
        trptr += 3 * NQ;
        mrptr += 3 * NQ;
        brptr += 3 * NQ;
        srptr += NQ;
    }

    ms = (int) ((level * (float) ms) + .5);

    printf (", apply");
    fflush (stdout);

    trptr = ppm;
    mrptr = ppm + (3 * NQ);
    brptr = ppm + (6 * NQ);
    srptr = sval + NQ;

    for (y = 1; y < (NY - 1); ++y)
    {
        unsigned char *tptr = trptr + 3;
        unsigned char *mptr = mrptr + 3;
        unsigned char *bptr = brptr + 3;
        int *sptr = srptr + 1;

        for (x = 1; x < (NQ - 1); ++x)
        {
            if ((*(sptr++)) > ms)
            {
                bzero (tptr - 3, 9);
                bzero (mptr - 3, 9);
                bzero (bptr - 3, 9);
            }
            tptr += 3;
            mptr += 3;
            bptr += 3;
        }
        trptr += 3 * NQ;
        mrptr += 3 * NQ;
        brptr += 3 * NQ;
        srptr += NQ;
    }

    printf (", done\n");
    fflush (stdout);
}

int
main (int argc, char *argv[])
{
    int off, len;
    int ifd, ofd;
    unsigned char *iptr, *optr;
    int x, y, w, h;

//Wisdom

    unsigned char *filter;
    int doautokill = 0;
    float autokill_level = 0.01;

    unsigned char *infile = NULL, *outfile = NULL, *killfile = NULL, *psfile =
                                NULL;

    int c;
    extern char *optarg;
    extern int optind;

    while ((c = getopt (argc, argv, "i:k:p:o:a:")) != EOF)
        switch (c)
        {
        case 'a':
            doautokill++;
            autokill_level = atof (optarg);
            break;
        case 'i':
            infile = optarg;
            break;
        case 'o':
            outfile = optarg;
            break;
        case 'p':
            psfile = optarg;
            break;
        case 'k':
            killfile = optarg;
            break;
        case '?':
            usage ("unrecognised comand line option");
        }

    if (!infile)
        usage ("need an input file");

    if ((outfile) && (!killfile) && (!doautokill))
        usage ("can't make output unless I have autokill or a killfile");

    if ((killfile) && (!(outfile)))
        usage ("killfile without outputfile makes no sense");

    if ((!outfile) && (!psfile))
        usage ("no output requested");

    open_wisdom ();

    printf ("Planning...\n");

    fplan =
        rfftw2d_create_plan (NY, NX, FFTW_REAL_TO_COMPLEX,
                             FFTW_MEASURE | FFTW_OUT_OF_PLACE);

    if (!killfile)
    {
        fftw_real *ps;
        unsigned char *ppm;

        check_suitability (infile, NULL, &off, &len, &w, &h);

        ifd = open_and_map (infile, &iptr, O_RDONLY, 0);

        iptr += off;

        ps = do_powerspectrum (iptr, w, h);
        close (ifd);

        ppm = ps_to_ppm (ps);
        free (ps);

        if (doautokill)
            autokill (ppm, autokill_level);

        if (psfile)
            write_ppm_to_file (ppm, psfile);

        if (outfile)
            filter = ppm_to_filter (ppm);

        free (ppm);
    }
    else
    {
        unsigned char *ppm;

        ppm = read_ppm_from_file (killfile);
        filter = ppm_to_filter (ppm);
        free (ppm);
    }

    if (outfile)
    {
        printf ("Plotting...\n");
        bplan =
            rfftw2d_create_plan (NY, NX, FFTW_COMPLEX_TO_REAL,
                                 FFTW_ESTIMATE | FFTW_OUT_OF_PLACE);

        check_suitability (infile, outfile, &off, &len, &w, &h);

        ifd = open_and_map (infile, &iptr, O_RDONLY, 0);
        ofd = open_and_map (outfile, &optr, O_RDWR, len);

        iptr += off;
        optr += off;

        do_filter (iptr, optr, filter, w, h);
        free (filter);

        close (ofd);
        close (ifd);

        truncate (outfile, len);
    }

    close_wisdom ();

    rfftwnd_destroy_plan (bplan);
    rfftwnd_destroy_plan (fplan);

    return 0;
}
