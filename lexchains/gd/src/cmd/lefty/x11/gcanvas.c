/*
 * Copyright (c) AT&T Corp. 1994, 1995.
 * This code is licensed by AT&T Corp.  For the
 * terms and conditions of the license, see
 * http://www.research.att.com/orgs/ssr/book/reuse
 */

#pragma prototyped
/* Lefteris Koutsofios - AT&T Bell Laboratories */

#include "common.h"
#include "g.h"
#include "gcommon.h"
#include "mem.h"

#define WCU widget->u.c
#define WINDOW widget->u.c->window
#define GC widget->u.c->gc
#define ISVISIBLE(r) ( \
    (r.o.x <= WCU->clip.c.x) && (r.c.x >= WCU->clip.o.x) && \
    (r.o.y <= WCU->clip.c.y) && (r.c.y >= WCU->clip.o.y) \
)

#define IS8BIT(font) ((font)->min_byte1 == 0 && (font)->max_byte1 == 0)

static struct cursormap_t {
    Cursor id;
    char name[40];
} cursormap[XC_num_glyphs];
static int curcursori = -1;

#define max(a, b) (((a) >= (b)) ? (a) : (b))
#define min(a, b) (((a) <= (b)) ? (a) : (b))

static char gstyles[][2] = {
    /* G_SOLID */       { 16,  0, },
    /* G_DASHED */      {  4,  4, },
    /* G_DOTTED */      {  2,  2, },
    /* G_LONGDASHED */  {  4, 12, },
    /* G_SHORTDASHED */ { 12,  4, },
};

static char grays[][4] = {
  { 0x00,0x00,0x00,0x00, },
  { 0x08,0x00,0x00,0x00, },
  { 0x08,0x00,0x02,0x00, },
  { 0x0A,0x00,0x02,0x00, },
  { 0x0A,0x00,0x0A,0x00, },
  { 0x0A,0x04,0x0A,0x00, },
  { 0x0A,0x04,0x0A,0x01, },
  { 0x0A,0x05,0x0A,0x01, },
  { 0x0A,0x05,0x0A,0x05, },
  { 0x0E,0x05,0x0A,0x05, },
  { 0x0E,0x05,0x0B,0x05, },
  { 0x0F,0x05,0x0B,0x05, },
  { 0x0F,0x05,0x0F,0x05, },
  { 0x0F,0x0D,0x0F,0x05, },
  { 0x0F,0x0D,0x0F,0x07, },
  { 0x0F,0x0F,0x0F,0x07, },
  { 0x0F,0x0F,0x0F,0x0F, },
};

static void bezier (PIXpoint_t, PIXpoint_t, PIXpoint_t, PIXpoint_t);
static XFontStruct *findfont (char *, int);
static void setgattr (Gwidget_t *, Ggattr_t *);

static PIXrect_t  rdrawtopix (Gwidget_t *, Grect_t);
static PIXpoint_t pdrawtopix (Gwidget_t *, Gpoint_t);
static PIXsize_t  sdrawtopix (Gwidget_t *, Gsize_t);
static Gpoint_t   Gppixtodraw (Gwidget_t *, PIXpoint_t);
static Gsize_t    spixtodraw (Gwidget_t *, PIXsize_t);
static Grect_t    rpixtodraw (Gwidget_t *, PIXrect_t);
static void       adjustclip (Gwidget_t *);

static Bool cwvpredicate (Display *, XEvent *, XPointer);
static void cweventhandler (Widget, XtPointer, XEvent *, Boolean *);
static Bool cwepredicate (Display *, XEvent *, XPointer);

int GCcreatewidget (Gwidget_t *parent, Gwidget_t *widget,
        int attrn, Gwattr_t *attrp) {
    PIXsize_t ps;
    Dimension width, height;
    XSetWindowAttributes xswa;
    XEvent ev;
    XColor *cp;
    int curi, color, ai, r, g, b, i;

    if (!parent) {
        Gerr (POS, G_ERRNOPARENTWIDGET);
        return -1;
    }
    WCU->func = NULL;
    WCU->needredraw = FALSE;
    WCU->buttonsdown = 0;
    WCU->bstate[0] = WCU->bstate[1] = WCU->bstate[2] = 0;
    ps.x = ps.y = MINCWSIZE;
    RESETARGS;
    for (ai = 0; ai < attrn; ai++) {
        switch (attrp[ai].id) {
        case G_ATTRSIZE:
            GETSIZE (attrp[ai].u.s, ps, MINCWSIZE);
            break;
        case G_ATTRBORDERWIDTH:
            ADD2ARGS (XtNborderWidth, attrp[ai].u.i);
            break;
        case G_ATTRCURSOR:
            /* will do it after the widget is created */
            break;
        case G_ATTRCOLOR:
            /* will do it after the widget is created */
            break;
        case G_ATTRVIEWPORT:
            /* will do it after the widget is created */
            break;
        case G_ATTRWINDOW:
            /* will do it after the widget is created */
            break;
        case G_ATTREVENTCB:
            WCU->func = attrp[ai].u.func;
            break;
        case G_ATTRUSERDATA:
            widget->udata = attrp[ai].u.u;
            break;
        default:
            Gerr (POS, G_ERRBADATTRID, attrp[ai].id);
            return -1;
        }
    }
    ADD2ARGS (XtNwidth, ps.x);
    ADD2ARGS (XtNheight, ps.y);
    if (!(widget->w = XtCreateWidget ("graphics", coreWidgetClass,
            parent->w, argp, argn))) {
        Gerr (POS, G_ERRCANNOTCREATE);
        return -1;
    }
    XtOverrideTranslations (widget->w, Gcwanytable);
    XtAddEventHandler (widget->w, VisibilityChangeMask | ExposureMask,
            FALSE, cweventhandler, (XtPointer) 0);
    Glazymanage (widget->w);
    Gflushlazyq ();
#ifdef DOBACKINGSTORE
    xswa.backing_store = WhenMapped;
    XChangeWindowAttributes (Gdisplay, XtWindow (widget->w),
            CWBackingStore, &xswa);
#endif
    /* wait for window to become visible */
    XPeekIfEvent (Gdisplay, &ev, cwvpredicate, (XPointer) XtWindow (widget->w));
    RESETARGS;
    ADD2ARGS (XtNwidth, &width);
    ADD2ARGS (XtNheight, &height);
    XtGetValues (widget->w, argp, argn);
    ps.x = width, ps.y = height;
    WCU->window = XtWindow (widget->w);
    WCU->cmap = DefaultColormap (Gdisplay, Gscreenn);
    WCU->gc = XCreateGC (Gdisplay, WCU->window, 0, NULL);
    WCU->colors[0].color.pixel = WhitePixel (Gdisplay, Gscreenn);
    XQueryColor (Gdisplay, WCU->cmap, &WCU->colors[0].color);
    WCU->colors[0].inuse = TRUE;
    WCU->colors[1].color.pixel = BlackPixel (Gdisplay, Gscreenn);
    XQueryColor (Gdisplay, WCU->cmap, &WCU->colors[1].color);
    WCU->colors[1].inuse = TRUE;
    for (i = 2; i < G_MAXCOLORS; i++)
        WCU->colors[i].inuse = FALSE;
    WCU->gattr.color = 1;
    XSetForeground (Gdisplay, GC, WCU->colors[1].color.pixel);
    WCU->gattr.width = 0;
    WCU->gattr.mode = G_SRC;
    WCU->gattr.fill = 0;
    WCU->gattr.style = 0;
    WCU->defgattr = WCU->gattr;
    WCU->font = NULL;
    WCU->wrect.o.x = 0.0, WCU->wrect.o.y = 0.0;
    WCU->wrect.c.x = 1.0, WCU->wrect.c.y = 1.0;
    WCU->vsize.x = ps.x, WCU->vsize.y = ps.y;
    if (Gdepth == 1) {
        XSetFillStyle (Gdisplay, GC, FillTiled);
        for (i = 0; i < 17; i++)
            WCU->grays[i] = XCreatePixmapFromBitmapData (Gdisplay, WCU->window,
                    &grays[i][0], 4, 4, BlackPixel (Gdisplay, Gscreenn),
                    WhitePixel (Gdisplay, Gscreenn), 1);
    }
    for (ai = 0; ai < attrn; ai++) {
        switch (attrp[ai].id) {
        case G_ATTRCURSOR:
            if ((curi = XmuCursorNameToIndex (attrp[ai].u.t)) == -1) {
                if (Strcmp (attrp[ai].u.t, "default") == 0) {
                    XUndefineCursor (Gdisplay, XtWindow (widget->w));
                    curcursori = -1;
                } else {
                    Gerr (POS, G_ERRNOSUCHCURSOR, attrp[ai].u.t);
                    return -1;
                }
            } else {
                if (!cursormap[curi].id) {
                    cursormap[curi].id = XCreateFontCursor (Gdisplay, curi);
                    strcpy (cursormap[curi].name, attrp[ai].u.t);
                }
                XDefineCursor (Gdisplay, XtWindow (widget->w),
                        cursormap[curi].id);
                curcursori = curi;
            }
            break;
        case G_ATTRCOLOR:
            color = attrp[ai].u.c.index;
            if (color < 0 || color > G_MAXCOLORS) {
                Gerr (POS, G_ERRBADCOLORINDEX, color);
                return -1;
            }
            r = attrp[ai].u.c.r * 257;
            g = attrp[ai].u.c.g * 257;
            b = attrp[ai].u.c.b * 257;
            cp = &WCU->colors[color].color;
            if (WCU->colors[color].inuse)
                if (cp->red != r || cp->green != g || cp->blue != b)
                     XFreeColors (Gdisplay, WCU->cmap, &cp->pixel, 1, 0);
            cp->red = r, cp->green = g, cp->blue = b;
            if (XAllocColor (Gdisplay, WCU->cmap, cp))
                WCU->colors[color].inuse = TRUE;
            /* XAllocColor may change the rgb values */
            cp->red = r, cp->green = g, cp->blue = b;
            if (color == WCU->gattr.color)
                WCU->gattr.color = -1;
            break;
        case G_ATTRVIEWPORT:
            WCU->vsize.x = attrp[ai].u.s.x + 0.5;
            WCU->vsize.y = attrp[ai].u.s.y + 0.5;
            RESETARGS;
            ADD2ARGS (XtNwidth, WCU->vsize.x);
            ADD2ARGS (XtNheight, WCU->vsize.y);
            XtSetValues (widget->w, argp, argn);
            break;
        case G_ATTRWINDOW:
            WCU->wrect = attrp[ai].u.r;
            break;
        }
    }
    adjustclip (widget);
    return 0;
}

int GCsetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    PIXsize_t ps;
    XColor *cp;
    int curi, color, ai, r, g, b;

    RESETARGS;
    for (ai = 0; ai < attrn; ai++) {
        switch (attrp[ai].id) {
        case G_ATTRSIZE:
            GETSIZE (attrp[ai].u.s, ps, MINCWSIZE);
            ADD2ARGS (XtNwidth, ps.x);
            ADD2ARGS (XtNheight, ps.y);
            break;
        case G_ATTRBORDERWIDTH:
            ADD2ARGS (XtNborderWidth, attrp[ai].u.i);
            break;
        case G_ATTRCURSOR:
            if ((curi = XmuCursorNameToIndex (attrp[ai].u.t)) == -1) {
                if (Strcmp (attrp[ai].u.t, "default") == 0) {
                    XUndefineCursor (Gdisplay, XtWindow (widget->w));
                    curcursori = -1;
                } else {
                    Gerr (POS, G_ERRNOSUCHCURSOR, attrp[ai].u.t);
                    return -1;
                }
            } else {
                if (!cursormap[curi].id) {
                    cursormap[curi].id = XCreateFontCursor (Gdisplay, curi);
                    strcpy (cursormap[curi].name, attrp[ai].u.t);
                }
                XDefineCursor (Gdisplay, XtWindow (widget->w),
                        cursormap[curi].id);
                curcursori = curi;
            }
            Gsync ();
            break;
        case G_ATTRCOLOR:
            color = attrp[ai].u.c.index;
            if (color < 0 || color > G_MAXCOLORS) {
                Gerr (POS, G_ERRBADCOLORINDEX, color);
                return -1;
            }
            r = attrp[ai].u.c.r * 257;
            g = attrp[ai].u.c.g * 257;
            b = attrp[ai].u.c.b * 257;
            cp = &WCU->colors[color].color;
            if (WCU->colors[color].inuse)
                if (cp->red != r || cp->green != g || cp->blue != b)
                     XFreeColors (Gdisplay, WCU->cmap, &cp->pixel, 1, 0);
            cp->red = r, cp->green = g, cp->blue = b;
            if (XAllocColor (Gdisplay, WCU->cmap, cp))
                WCU->colors[color].inuse = TRUE;
            /* XAllocColor may change the rgb values */
            cp->red = r, cp->green = g, cp->blue = b;
            if (color == WCU->gattr.color)
                WCU->gattr.color = -1;
            break;
        case G_ATTRVIEWPORT:
            WCU->vsize.x = attrp[ai].u.s.x + 0.5;
            WCU->vsize.y = attrp[ai].u.s.y + 0.5;
            ADD2ARGS (XtNwidth, WCU->vsize.x);
            ADD2ARGS (XtNheight, WCU->vsize.y);
            XtSetValues (widget->w, argp, argn);
            adjustclip (widget);
            RESETARGS;
            break;
        case G_ATTRWINDOW:
            WCU->wrect = attrp[ai].u.r;
            XtSetValues (widget->w, argp, argn);
            adjustclip (widget);
            RESETARGS;
            break;
        case G_ATTREVENTCB:
            WCU->func = attrp[ai].u.func;
            break;
        case G_ATTRUSERDATA:
            widget->udata = attrp[ai].u.u;
            break;
        default:
            Gerr (POS, G_ERRBADATTRID, attrp[ai].id);
            return -1;
        }
    }
    XtSetValues (widget->w, argp, argn);
    return 0;
}

int GCgetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    XColor *cp;
    Dimension width, height;
    int ai, color;

    for (ai = 0; ai < attrn; ai++) {
        RESETARGS;
        switch (attrp[ai].id) {
        case G_ATTRSIZE:
            ADD2ARGS (XtNwidth, &width);
            ADD2ARGS (XtNheight, &height);
            XtGetValues (widget->w, argp, argn);
            attrp[ai].u.s.x = width, attrp[ai].u.s.y = height;
            break;
        case G_ATTRBORDERWIDTH:
            ADD2ARGS (XtNborderWidth, &width);
            XtGetValues (widget->w, argp, argn);
            attrp[ai].u.i = width;
            break;
        case G_ATTRCURSOR:
            attrp[ai].u.t = (curcursori == -1) ?
                    "default" : cursormap[curcursori].name;
            break;
        case G_ATTRCOLOR:
            color = attrp[ai].u.c.index;
            if (color < 0 || color > G_MAXCOLORS) {
                Gerr (POS, G_ERRBADCOLORINDEX, color);
                return -1;
            }
            if (WCU->colors[color].inuse) {
                cp = &WCU->colors[color].color;
                attrp[ai].u.c.r = cp->red / 257.0;
                attrp[ai].u.c.g = cp->green / 257.0;
                attrp[ai].u.c.b = cp->blue / 257.0;
            } else {
                attrp[ai].u.c.r = -1;
                attrp[ai].u.c.g = -1;
                attrp[ai].u.c.b = -1;
            }
            break;
        case G_ATTRVIEWPORT:
            attrp[ai].u.s = WCU->vsize;
            break;
        case G_ATTRWINDOW:
            attrp[ai].u.r = WCU->wrect;
            break;
        case G_ATTREVENTCB:
            attrp[ai].u.func = WCU->func;
            break;
        case G_ATTRUSERDATA:
            attrp[ai].u.u = widget->udata;
            break;
        default:
            Gerr (POS, G_ERRBADATTRID, attrp[ai].id);
            return -1;
        }
    }
    return 0;
}

int GCdestroywidget (Gwidget_t *widget) {
    XtDestroyWidget (widget->w);
    return 0;
}

int GCcanvasclear (Gwidget_t *widget) {
    XEvent ev;
    int gotit;

    XClearWindow (Gdisplay, WINDOW);
    /* avoid a redraw */
    WCU->needredraw = FALSE;
    XSync (Gdisplay, False);
    gotit = FALSE;
    while (XCheckIfEvent (Gdisplay,
            &ev, cwepredicate, (XPointer) WINDOW) == True)
        gotit = TRUE;
    if (gotit)
        adjustclip (widget);
    return 0;
}

int GCsetgfxattr (Gwidget_t *widget, Ggattr_t *ap) {
    setgattr (widget, ap);
    WCU->defgattr = WCU->gattr;
    return 0;
}

int GCgetgfxattr (Gwidget_t *widget, Ggattr_t *ap) {
    if ((ap->flags & G_GATTRCOLOR))
        ap->color = WCU->gattr.color;
    if ((ap->flags & G_GATTRWIDTH))
        ap->width = WCU->gattr.width;
    if ((ap->flags & G_GATTRMODE))
        ap->mode = WCU->gattr.mode;
    if ((ap->flags & G_GATTRFILL))
        ap->fill = WCU->gattr.fill;
    if ((ap->flags & G_GATTRSTYLE))
        ap->style = WCU->gattr.style;
    return 0;
}

int GCarrow (Gwidget_t *widget, Gpoint_t gp1, Gpoint_t gp2, Ggattr_t *ap) {
    PIXpoint_t pp1, pp2, pa, pb, pd;
    Grect_t gr;
    double tangent, l;

    if (gp1.x < gp2.x)
        gr.o.x = gp1.x, gr.c.x = gp2.x;
    else
        gr.o.x = gp2.x, gr.c.x = gp1.x;
    if (gp1.y < gp2.y)
        gr.o.y = gp1.y, gr.c.y = gp2.y;
    else
        gr.o.y = gp2.y, gr.c.y = gp1.y;
    if (!ISVISIBLE (gr))
        return 1;
    pp1 = pdrawtopix (widget, gp1), pp2 = pdrawtopix (widget, gp2);
    pd.x = pp1.x - pp2.x, pd.y = pp1.y - pp2.y;
    if (pd.x == 0 && pd.y == 0)
        return 0;
    tangent = atan2 ((double) pd.y, (double) pd.x);
    if ((l = sqrt ((double) (pd.x * pd.x + pd.y * pd.y))) > 30)
        l = 30;
    pa.x = l * cos (tangent + M_PI / 7) + pp2.x;
    pa.y = l * sin (tangent + M_PI / 7) + pp2.y;
    pb.x = l * cos (tangent - M_PI / 7) + pp2.x;
    pb.y = l * sin (tangent - M_PI / 7) + pp2.y;
    setgattr (widget, ap);
    XDrawLine (Gdisplay, WINDOW, GC, pp1.x, pp1.y, pp2.x, pp2.y);
    XDrawLine (Gdisplay, WINDOW, GC, pa.x, pa.y, pp2.x, pp2.y);
    XDrawLine (Gdisplay, WINDOW, GC, pb.x, pb.y, pp2.x, pp2.y);
    return 0;
}

int GCline (Gwidget_t *widget, Gpoint_t gp1, Gpoint_t gp2, Ggattr_t *ap) {
    PIXpoint_t pp1, pp2;
    Grect_t gr;

    if (gp1.x < gp2.x)
        gr.o.x = gp1.x, gr.c.x = gp2.x;
    else
        gr.o.x = gp2.x, gr.c.x = gp1.x;
    if (gp1.y < gp2.y)
        gr.o.y = gp1.y, gr.c.y = gp2.y;
    else
        gr.o.y = gp2.y, gr.c.y = gp1.y;
    if (!ISVISIBLE (gr))
        return 1;
    pp1 = pdrawtopix (widget, gp1), pp2 = pdrawtopix (widget, gp2);
    setgattr (widget, ap);
    XDrawLine (Gdisplay, WINDOW, GC, pp1.x, pp1.y, pp2.x, pp2.y);
    return 0;
}

int GCbox (Gwidget_t *widget, Grect_t gr, Ggattr_t *ap) {
    PIXrect_t pr;
    Grect_t gr2;
    double n;

    if (gr.o.x <= gr.c.x)
        gr2.o.x = gr.o.x, gr2.c.x = gr.c.x;
    else
        gr2.o.x = gr.c.x, gr2.c.x = gr.o.x;
    if (gr.o.y <= gr.c.y)
        gr2.o.y = gr.o.y, gr2.c.y = gr.c.y;
    else
        gr2.o.y = gr.c.y, gr2.c.y = gr.o.y;
    if (!ISVISIBLE (gr2))
        return 1;
    pr = rdrawtopix (widget, gr);
    setgattr (widget, ap);
    if (WCU->gattr.fill)
        XFillRectangle (Gdisplay, WINDOW, GC,
                pr.o.x, pr.o.y, pr.c.x - pr.o.x + 2, pr.c.y - pr.o.y + 2);
    else
        XDrawRectangle (Gdisplay, WINDOW, GC,
                pr.o.x, pr.o.y, pr.c.x - pr.o.x + 1, pr.c.y - pr.o.y + 1);
    return 0;
}

int GCpolygon (Gwidget_t *widget, int gpn, Gpoint_t *gpp, Ggattr_t *ap) {
    Grect_t gr;
    int n, i;

    if (gpn == 0)
        return 0;
    gr.o = gpp[0], gr.c = gpp[0];
    for (i = 1; i < gpn; i++) {
        gr.o.x = min (gr.o.x, gpp[i].x);
        gr.o.y = min (gr.o.y, gpp[i].y);
        gr.c.x = max (gr.c.x, gpp[i].x);
        gr.c.y = max (gr.c.y, gpp[i].y);
    }
    if (!ISVISIBLE (gr))
        return 1;
    if (gpn + 1 > Gppn) {
        n = (((gpn + 1) + PPINCR - 1) / PPINCR) * PPINCR;
        Gppp = Marraygrow (Gppp, (long) n * PPSIZE);
        Gppn = n;
    }
    for (i = 0; i < gpn; i++)
        Gppp[i] = pdrawtopix (widget, gpp[i]);
    setgattr (widget, ap);
    if (WCU->gattr.fill) {
        if (Gppp[gpn - 1].x != Gppp[0].x || Gppp[gpn - 1].y != Gppp[0].y)
            Gppp[gpn] = Gppp[0], gpn++;
        XFillPolygon (Gdisplay, WINDOW, GC, Gppp, gpn,
                Complex, CoordModeOrigin);
    } else
        XDrawLines (Gdisplay, WINDOW, GC, Gppp, gpn, CoordModeOrigin);
    return 0;
}

int GCsplinegon (Gwidget_t *widget, int gpn, Gpoint_t *gpp, Ggattr_t *ap) {
    PIXpoint_t p0, p1, p2, p3;
    Grect_t gr;
    int n, i;

    if (gpn == 0)
        return 0;
    gr.o = gpp[0], gr.c = gpp[0];
    for (i = 1; i < gpn; i++) {
        gr.o.x = min (gr.o.x, gpp[i].x);
        gr.o.y = min (gr.o.y, gpp[i].y);
        gr.c.x = max (gr.c.x, gpp[i].x);
        gr.c.y = max (gr.c.y, gpp[i].y);
    }
    if (!ISVISIBLE (gr))
        return 1;
    Gppi = 1;
    if (Gppi >= Gppn) {
        n = (((Gppi + 1) + PPINCR - 1) / PPINCR) * PPINCR;
        Gppp = Marraygrow (Gppp, (long) n * PPSIZE);
        Gppn = n;
    }
    Gppp[0] = p3 = pdrawtopix (widget, gpp[0]);
    for (i = 1; i < gpn; i += 3) {
        p0 = p3;
        p1 = pdrawtopix (widget, gpp[i]);
        p2 = pdrawtopix (widget, gpp[i + 1]);
        p3 = pdrawtopix (widget, gpp[i + 2]);
        bezier (p0, p1, p2, p3);
    }
    setgattr (widget, ap);
    if (WCU->gattr.fill) {
        if (Gppp[Gppi - 1].x != Gppp[0].x || Gppp[Gppi - 1].y != Gppp[0].y)
            Gppp[Gppi] = Gppp[0], Gppi++;
        XFillPolygon (Gdisplay, WINDOW, GC, Gppp, Gppi,
                Complex, CoordModeOrigin);
    } else
        XDrawLines (Gdisplay, WINDOW, GC, Gppp, Gppi, CoordModeOrigin);
    return 0;
}

static void bezier (PIXpoint_t p0, PIXpoint_t p1,
        PIXpoint_t p2, PIXpoint_t p3) {
    Gpoint_t gp0, gp1, gp2;
    Gsize_t s;
    PIXpoint_t p;
    double t;
    int n, i, steps;

    if ((s.x = p3.x - p0.x) < 0)
        s.x = - s.x;
    if ((s.y = p3.y - p0.y) < 0)
        s.y = - s.y;
    if (s.x > s.y)
        steps = s.x / 5 + 1;
    else
        steps = s.y / 5 + 1;
    for (i = 0; i <= steps; i++) {
        t = i / (double) steps;
        gp0.x = p0.x + t * (p1.x - p0.x);
        gp0.y = p0.y + t * (p1.y - p0.y);
        gp1.x = p1.x + t * (p2.x - p1.x);
        gp1.y = p1.y + t * (p2.y - p1.y);
        gp2.x = p2.x + t * (p3.x - p2.x);
        gp2.y = p2.y + t * (p3.y - p2.y);
        gp0.x = gp0.x + t * (gp1.x - gp0.x);
        gp0.y = gp0.y + t * (gp1.y - gp0.y);
        gp1.x = gp1.x + t * (gp2.x - gp1.x);
        gp1.y = gp1.y + t * (gp2.y - gp1.y);
        p.x = gp0.x + t * (gp1.x - gp0.x) + 0.5;
        p.y = gp0.y + t * (gp1.y - gp0.y) + 0.5;
        if (Gppi >= Gppn) {
            n = (((Gppi + 1) + PPINCR - 1) / PPINCR) * PPINCR;
            Gppp = Marraygrow (Gppp, (long) n * PPSIZE);
            Gppn = n;
        }
        Gppp[Gppi++] = p;
    }
}

int GCarc (Gwidget_t *widget, Gpoint_t gc, Gsize_t gs, double ang1,
        double ang2, Ggattr_t *ap) {
    PIXpoint_t pc;
    PIXsize_t ps;
    Grect_t gr;

    gr.o.x = gc.x - gs.x, gr.o.y = gc.y - gs.y;
    gr.c.x = gc.x + gs.x, gr.c.y = gc.y + gs.y;
    if (!ISVISIBLE (gr))
        return 1;
    pc = pdrawtopix (widget, gc), ps = sdrawtopix (widget, gs);
    setgattr (widget, ap);
    if (WCU->gattr.fill)
        XFillArc (Gdisplay, WINDOW, GC, pc.x - ps.x, pc.y - ps.y,
                ps.x * 2, ps.y * 2, (int) (ang1 * 64), (int) (ang2 * 64));
    else
        XDrawArc (Gdisplay, WINDOW, GC, pc.x - ps.x, pc.y - ps.y,
                ps.x * 2, ps.y * 2, (int) (ang1 * 64), (int) (ang2 * 64));
    return 0;
}

#define YSCALE ((WCU->vsize.y) / (WCU->wrect.c.y - WCU->wrect.o.y))

int GCtext (Gwidget_t *widget, Gtextline_t *tlp, int n, Gpoint_t go,
        char *fn, double fs, char *justs, Ggattr_t *ap) {
    Gsize_t gs;
    PIXpoint_t po;
    PIXsize_t ps;
    PIXrect_t pr;
    Grect_t gr;
    XFontStruct *font;
    int dir, asc, des, x, y, w, h, i;
    XCharStruct txtinfo;

    po = pdrawtopix (widget, go);
    gs.x = 0, gs.y = fs;
    ps = sdrawtopix (widget, gs);
    if (!(font = findfont (fn, ps.y))) {
        XDrawRectangle (Gdisplay, WINDOW, GC, po.x, po.y, 1, 1);
        return 0;
    }
    setgattr (widget, ap);
    SETFONT (font);
    for (w = h = 0, i = 0; i < n; i++) {
        if (IS8BIT (font))
            XTextExtents (font, tlp[i].p, tlp[i].n, &dir, &asc, &des, &txtinfo);
        else
            XTextExtents16 (font, (XChar2b *) tlp[i].p, tlp[i].n / 2,
                    &dir, &asc, &des, &txtinfo);
        tlp[i].w = txtinfo.width, tlp[i].h = asc + des;
        w = max (w, txtinfo.width), h += asc + des;
    }
    switch (justs[0]) {
    case 'l': po.x += w / 2; break;
    case 'r': po.x -= w / 2; break;
    }
    switch (justs[1]) {
    case 'd': po.y -= h; break;
    case 'c': po.y -= h / 2; break;
    }
    pr.o.x = po.x - w / 2, pr.o.y = po.y;
    pr.c.x = po.x + w / 2, pr.c.y = po.y + h;
    gr = rpixtodraw (widget, pr);
    if (!ISVISIBLE (gr))
        return 1;
    for (i = 0; i < n; i++) {
        switch (tlp[i].j) {
        case 'l': x = po.x - w / 2; break;
        case 'n': x = po.x - tlp[i].w / 2; break;
        case 'r': x = po.x - (tlp[i].w - w / 2); break;
        }
        y = po.y + (i + 1) * tlp[i].h - des;
        if (IS8BIT (font))
            XDrawString (Gdisplay, WINDOW, GC, x, y, tlp[i].p, tlp[i].n);
        else
            XDrawString16 (Gdisplay, WINDOW, GC, x, y,
                    (XChar2b *) tlp[i].p, tlp[i].n / 2);
    }
    return 0;
}

int GCgettextsize (Gwidget_t *widget, Gtextline_t *tlp, int n, char *fn,
        double fs, Gsize_t *gsp) {
    Gsize_t gs;
    PIXsize_t ps;
    XFontStruct *font;
    int i, dir, asc, des;
    XCharStruct txtinfo;

    gs.x = 0, gs.y = fs;
    ps = sdrawtopix (widget, gs);
    if (!(font = findfont (fn, ps.y))) {
        gsp->x = 1, gsp->y = 1;
        return 0;
    }
    SETFONT (font);
    for (ps.x = ps.y = 0, i = 0; i < n; i++) {
        XTextExtents16 (font, (XChar2b *) tlp[i].p, tlp[i].n / 2,
                &dir, &asc, &des, &txtinfo);
        ps.x = max (ps.x, txtinfo.width), ps.y += asc + des;
    }
    *gsp = spixtodraw (widget, ps);
    return 0;
}

static XFontStruct *findfont (char *name, int size) {
    XFontStruct *font;
    int fi, n, i;

    if (name[0] == '\000')
        return Gfontp[0].font;

    sprintf (&Gbufp[0], name, size);
    for (fi = 0; fi < Gfontn; fi++)
        if (Strcmp (&Gbufp[0], Gfontp[fi].name) == 0)
            return Gfontp[fi].font;
    if (!(font = XLoadQueryFont (Gdisplay, &Gbufp[0]))) {
        n = strlen (&Gbufp[0]) + 1;
        for (i = 1; i < size; i++) {
            sprintf (&Gbufp[n], name, size - i);
            if ((font = XLoadQueryFont (Gdisplay, &Gbufp[n])))
                break;
            sprintf (&Gbufp[n], name, size + i);
            if ((font = XLoadQueryFont (Gdisplay, &Gbufp[n])))
                break;
        }
    }
    if (!font)
        font = Gfontp[0].font;

    Gfontp = Marraygrow (Gfontp, (long) (Gfontn + 1) * FONTSIZE);
    Gfontp[Gfontn].name = strdup (&Gbufp[0]);
    Gfontp[Gfontn].font = font;
    Gfontn++;
    return font;
}

int GCgetmousecoords (Gwidget_t *widget, Gpoint_t *gpp, int *count) {
    PIXpoint_t pp;
    Window rwin, cwin;
    int rx, ry, x, y;
    unsigned int mask;

    XQueryPointer (Gdisplay, WINDOW, &rwin, &cwin, &rx, &ry, &x, &y, &mask);
    pp.x = x, pp.y = y;
    *gpp = Gppixtodraw (widget, pp);
    *count = ((mask & Button1Mask) ? 1 : 0) +
            ((mask & Button2Mask) ? 1 : 0) +
            ((mask & Button3Mask) ? 1 : 0);
    return 0;
}

void Gcwanyaction (Widget w, XEvent *evp, char **app, unsigned int *anp) {
    Gwidget_t *widget;
    Gevent_t gev;
    PIXpoint_t pp;
    Window rwin, cwin;
    int wi, xtype, rx, ry, x, y, bn;
    unsigned int mask;
    char c;

    widget = findwidget ((unsigned long) w, G_CANVASWIDGET);
    switch ((xtype = evp->type)) {
    case KeyPress:
    case KeyRelease:
        if (!XLookupString ((XKeyEvent *) evp, &c, 1, NULL, NULL))
            return;
        XQueryPointer (Gdisplay, WCU->window, &rwin, &cwin,
                &rx, &ry, &x, &y, &mask);
        gev.type = G_KEYBD;
        gev.code = (xtype == KeyPress) ? G_DOWN : G_UP;
        gev.data = c;
        pp.x = x, pp.y = y;
        gev.p = Gppixtodraw (widget, pp);
        break;
    case ButtonPress:
    case ButtonRelease:
        gev.type = G_MOUSE;
        gev.code = (xtype == ButtonPress) ? G_DOWN : G_UP;
        gev.data = evp->xbutton.button - Button1;
        pp.x = evp->xbutton.x, pp.y = evp->xbutton.y;
        gev.p = Gppixtodraw (widget, pp);
        bn = WCU->bstate[gev.data];
        WCU->bstate[gev.data] = (xtype == ButtonPress) ? 1 : 0;
        bn = WCU->bstate[gev.data] - bn;
        WCU->buttonsdown += bn;
        Gbuttonsdown += bn;
        break;
    default:
        return;
    }
    wi = gev.wi = widget - &Gwidgets[0];
    if (WCU->func)
        (*WCU->func) (&gev);
    if (Gpopdownflag) {
        Gpopdownflag = FALSE;
        if (gev.type == G_MOUSE && gev.code == G_DOWN) {
            gev.code = G_UP;
            widget = &Gwidgets[wi];
            WCU->bstate[gev.data] = 0;
            WCU->buttonsdown--;
            Gbuttonsdown--;
            if (widget->inuse && WCU->func)
                (*WCU->func) (&gev);
        }
    }
}

static void setgattr (Gwidget_t *widget, Ggattr_t *ap) {
    XGCValues gcv;
    XColor *cp;
    int color, width, mode, style, pati;
    double intens;

    if (!(ap->flags & G_GATTRCOLOR))
        ap->color = WCU->defgattr.color;
    if (!(ap->flags & G_GATTRWIDTH))
        ap->width = WCU->defgattr.width;
    if (!(ap->flags & G_GATTRMODE))
        ap->mode = WCU->defgattr.mode;
    if (!(ap->flags & G_GATTRFILL))
        ap->fill = WCU->defgattr.fill;
    if (!(ap->flags & G_GATTRSTYLE))
        ap->style = WCU->defgattr.style;
    color = ap->color;
    if (color >= G_MAXCOLORS || !(WCU->colors[color].inuse))
        color = 1;
    if (color != WCU->gattr.color) {
        WCU->gattr.color = color;
        if (ap->mode == G_XOR)
            XSetForeground (Gdisplay, GC,
                    WCU->colors[WCU->gattr.color].color.pixel ^
                    WCU->colors[0].color.pixel);
        else
            XSetForeground (Gdisplay, GC,
                    WCU->colors[WCU->gattr.color].color.pixel);
        if (Gdepth == 1) {
            cp = &WCU->colors[color].color;
            intens = (0.3 * cp->blue + 0.59 * cp->red +
                    0.11 * cp->green) / 65535.0;
            pati = (intens <= 0.0625) ? 16 :
                    -16.0 * (log (intens) / 2.7725887222);
            XSetTile (Gdisplay, GC, WCU->grays[pati]);
        }
    }
    mode = ap->mode;
    if (mode != WCU->gattr.mode) {
        WCU->gattr.mode = mode;
        XSetFunction (Gdisplay, GC, WCU->gattr.mode);
        if (mode == G_XOR)
            XSetForeground (Gdisplay, GC,
                    WCU->colors[WCU->gattr.color].color.pixel ^
                    WCU->colors[0].color.pixel);
        else
            XSetForeground (Gdisplay, GC,
                    WCU->colors[WCU->gattr.color].color.pixel);
    }
    width = ap->width;
    if (width != WCU->gattr.width) {
        gcv.line_width = WCU->gattr.width = width;
        XChangeGC (Gdisplay, GC, GCLineWidth, &gcv);
    }
    WCU->gattr.fill = ap->fill;
    style = ap->style;
    if (style != WCU->gattr.style) {
        WCU->gattr.style = style;
        if (style == G_SOLID) {
            gcv.line_style = LineSolid;
            XChangeGC (Gdisplay, GC, GCLineStyle, &gcv);
        } else {
            XSetDashes (Gdisplay, GC, 0, gstyles[style], 2);
            gcv.line_style = LineOnOffDash;
            XChangeGC (Gdisplay, GC, GCLineStyle, &gcv);
        }
    }
}

static PIXrect_t rdrawtopix (Gwidget_t *widget, Grect_t gr) {
    PIXrect_t pr;
    double tvx, tvy, twx, twy;

    tvx = WCU->vsize.x, tvy = WCU->vsize.y;
    twx = WCU->wrect.c.x - WCU->wrect.o.x;
    twy = WCU->wrect.c.y - WCU->wrect.o.y;
    pr.o.x = tvx * (gr.o.x - WCU->wrect.o.x) / twx + 0.5;
    pr.o.y = tvy * (1.0  - (gr.c.y - WCU->wrect.o.y) / twy) + 0.5;
    pr.c.x = tvx * (gr.c.x - WCU->wrect.o.x) / twx + 0.5;
    pr.c.y = tvy * (1.0  - (gr.o.y - WCU->wrect.o.y) / twy) + 0.5;
    return pr;
}

static PIXpoint_t pdrawtopix (Gwidget_t *widget, Gpoint_t gp) {
    PIXpoint_t pp;
    double tvx, tvy, twx, twy;

    tvx = WCU->vsize.x, tvy = WCU->vsize.y;
    twx = WCU->wrect.c.x - WCU->wrect.o.x;
    twy = WCU->wrect.c.y - WCU->wrect.o.y;
    pp.x = tvx * (gp.x - WCU->wrect.o.x) / twx + 0.5;
    pp.y = tvy * (1.0  - (gp.y - WCU->wrect.o.y) / twy) + 0.5;
    return pp;
}

static PIXsize_t sdrawtopix (Gwidget_t *widget, Gsize_t gs) {
    PIXsize_t ps;
    double tvx, tvy, twx, twy;

    tvx = WCU->vsize.x, tvy = WCU->vsize.y;
    twx = WCU->wrect.c.x - WCU->wrect.o.x;
    twy = WCU->wrect.c.y - WCU->wrect.o.y;
    ps.x = tvx * gs.x / twx + 0.5;
    ps.y = tvy * gs.y / twy + 0.5;
    return ps;
}

static Gpoint_t Gppixtodraw (Gwidget_t *widget, PIXpoint_t pp) {
    Gpoint_t gp;
    double tvx, tvy, twx, twy;

    tvx = WCU->vsize.x, tvy = WCU->vsize.y;
    twx = WCU->wrect.c.x - WCU->wrect.o.x;
    twy = WCU->wrect.c.y - WCU->wrect.o.y;
    gp.x = (pp.x / tvx) * twx + WCU->wrect.o.x;
    gp.y = (1.0 - pp.y / tvy) * twy + WCU->wrect.o.y;
    return gp;
}

static Gsize_t spixtodraw (Gwidget_t *widget, PIXsize_t ps) {
    Gsize_t gs;
    double tvx, tvy, twx, twy;

    tvx = WCU->vsize.x, tvy = WCU->vsize.y;
    twx = WCU->wrect.c.x - WCU->wrect.o.x;
    twy = WCU->wrect.c.y - WCU->wrect.o.y;
    gs.x = (ps.x / tvx) * twx;
    gs.y = (ps.y / tvy) * twy;
    return gs;
}

static Grect_t rpixtodraw (Gwidget_t *widget, PIXrect_t pr) {
    Grect_t gr;
    double tvx, tvy, twx, twy, n;

    tvx = WCU->vsize.x, tvy = WCU->vsize.y;
    twx = WCU->wrect.c.x - WCU->wrect.o.x;
    twy = WCU->wrect.c.y - WCU->wrect.o.y;
    gr.o.x = (pr.o.x / tvx) * twx + WCU->wrect.o.x;
    gr.o.y = (1.0 - pr.c.y / tvy) * twy + WCU->wrect.o.y;
    gr.c.x = (pr.c.x / tvx) * twx + WCU->wrect.o.x;
    gr.c.y = (1.0 - pr.o.y / tvy) * twy + WCU->wrect.o.y;
    if (gr.o.x > gr.c.x)
        n = gr.o.x, gr.o.x = gr.c.x, gr.c.x = n;
    if (gr.o.y > gr.c.y)
        n = gr.o.y, gr.o.y = gr.c.y, gr.c.y = n;
    return gr;
}

static void adjustclip (Gwidget_t *widget) {
    Gwidget_t *parent;
    Dimension width, height, pwidth, pheight;
    Position x, y;
    PIXrect_t pr;

    parent = &Gwidgets[widget->pwi];
    RESETARGS;
    ADD2ARGS (XtNx, &x);
    ADD2ARGS (XtNy, &y);
    ADD2ARGS (XtNwidth, &width);
    ADD2ARGS (XtNheight, &height);
    XtGetValues (widget->w, argp, argn);
    RESETARGS;
    ADD2ARGS (XtNwidth, &pwidth);
    ADD2ARGS (XtNheight, &pheight);
    XtGetValues (parent->w, argp, argn);
    pr.o.x = max (0, -x);
    pr.o.y = max (0, -y);
    pr.c.x = min (width, pr.o.x + pwidth);
    pr.c.y = min (height, pr.o.y + pheight);
    pr.c.x = max (pr.o.x, pr.c.x);
    pr.c.y = max (pr.o.y, pr.c.y);
    WCU->clip = rpixtodraw (widget, pr);
}

static Bool cwepredicate (Display *display, XEvent *evp, XPointer data) {
    if (evp->type == Expose && ((XAnyEvent *) evp)->window == (Window) data)
        return True;
    return False;
}

static void cweventhandler (Widget w, XtPointer data, XEvent *evp,
        Boolean *cont) {
    Gwidget_t *widget;

    if (evp->type == Expose) {
        widget = findwidget ((unsigned long) w, G_CANVASWIDGET);
        Gneedredraw = WCU->needredraw = TRUE;
        adjustclip (widget);
    }
}

static Bool cwvpredicate (Display *display, XEvent *evp, XPointer data) {
    if (evp->type == VisibilityNotify &&
            ((XAnyEvent *) evp)->window == (Window) data)
        return True;
    return False;
}
