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

#define max(a, b) (((a) >= (b)) ? (a) : (b))
#define min(a, b) (((a) <= (b)) ? (a) : (b))

static long gstyles[5] = {
    /* G_SOLID */       PS_SOLID,
    /* G_DASHED */      PS_DASH,
    /* G_DOTTED */      PS_DOT,
    /* G_LONGDASHED */  PS_DASH,
    /* G_SHORTDASHED */ PS_DASH,
};

static char MSNEAR grays[][4] = {
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

static int curcursori = -1;

static void bezier (PIXpoint_t, PIXpoint_t, PIXpoint_t, PIXpoint_t);
static HFONT findfont (char *, int);
static void setgattr (Gwidget_t *, Ggattr_t *);

static PIXrect_t  rdrawtopix (Gwidget_t *, Grect_t);
static PIXpoint_t pdrawtopix (Gwidget_t *, Gpoint_t);
static PIXsize_t  sdrawtopix (Gwidget_t *, Gsize_t);
static Gsize_t    spixtodraw (Gwidget_t *, PIXsize_t);
static Grect_t    rpixtodraw (Gwidget_t *, PIXrect_t);

int GCcreatewidget (Gwidget_t *parent, Gwidget_t *widget,
        int attrn, Gwattr_t *attrp) {
    PIXsize_t ps;
    LOGPALETTE pal[2]; /* the 2 here is to provide enough space
                          for palPalEntry[0] and [1] */
    HBRUSH brush;
    HPEN pen;
    HBITMAP bmap;
    HCURSOR cursor;
    DWORD wflags;
    int color, ai, i, r, g, b;

    if (!parent) {
        Gerr (POS, G_ERRNOPARENTWIDGET);
        return -1;
    }
    wflags = WS_CHILDWINDOW;
    WCU->func = NULL;
    WCU->needredraw = FALSE;
    WCU->buttonsdown = 0;
    WCU->bstate[0] = WCU->bstate[1] = WCU->bstate[2] = 0;
    ps.x = ps.y = MINCWSIZE;
    for (ai = 0; ai < attrn; ai++) {
        switch (attrp[ai].id) {
        case G_ATTRSIZE:
            GETSIZE (attrp[ai].u.s, ps, MINCWSIZE);
            break;
        case G_ATTRBORDERWIDTH:
            wflags |= WS_BORDER;
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
    Gadjustwrect (parent, &ps);
    WCU->wrect.o.x = 0.0, WCU->wrect.o.y = 0.0;
    WCU->wrect.c.x = 1.0, WCU->wrect.c.y = 1.0;
    WCU->vsize.x = ps.x, WCU->vsize.y = ps.y;
    if (!(widget->w = CreateWindow ("CanvasClass", "canvas", wflags, 0, 0,
            ps.x, ps.y, parent->w, (HMENU) (widget - &Gwidgets[0]),
            hinstance, NULL))) {
        Gerr (POS, G_ERRCANNOTCREATE);
        return -1;
    }
    ShowWindow (widget->w, SW_SHOW);
    UpdateWindow (widget->w);
    SetCursor (LoadCursor ((HINSTANCE) NULL, IDC_ARROW));
    GC = GetDC (widget->w);
    WCU->ncolor = 2;
    pal[0].palVersion = 0x300; /* HA HA HA */
    pal[0].palNumEntries = 2;
    pal[0].palPalEntry[0].peRed = 255;
    pal[0].palPalEntry[0].peGreen = 255;
    pal[0].palPalEntry[0].peBlue = 255;
    pal[0].palPalEntry[0].peFlags = 0;
    pal[0].palPalEntry[1].peRed = 0;
    pal[0].palPalEntry[1].peGreen = 0;
    pal[0].palPalEntry[1].peBlue = 0;
    pal[0].palPalEntry[1].peFlags = 0;
    WCU->cmap = CreatePalette (&pal[0]);
    WCU->colors[0].color = pal[0].palPalEntry[0];
    for (i = 1; i < G_MAXCOLORS; i++)
        WCU->colors[i].color = pal[0].palPalEntry[1];
    SelectPalette (GC, WCU->cmap, FALSE);
    RealizePalette (GC);
    WCU->colors[0].inuse = TRUE;
    WCU->colors[1].inuse = TRUE;
    for (i = 2; i < G_MAXCOLORS; i++)
        WCU->colors[i].inuse = FALSE;
    WCU->gattr.color = 1;
    brush = CreateSolidBrush (PALETTEINDEX (1));
    SelectObject (GC, brush);
    pen = CreatePen (PS_SOLID, 1, PALETTEINDEX (1));
    SelectObject (GC, pen);
    SetTextColor (GC, PALETTEINDEX (1));
    SetBkMode (GC, TRANSPARENT);
    WCU->gattr.width = 0;
    WCU->gattr.mode = G_SRC;
    WCU->gattr.fill = 0;
    WCU->gattr.style = 0;
    WCU->defgattr = WCU->gattr;
    WCU->font = NULL;
    if (Gdepth == 1) {
        for (i = 0; i < 17; i++) {
            if (!(bmap = CreateBitmap (4, 4, 1, 1, &grays[i][0])))
                continue;
            WCU->grays[i] = CreatePatternBrush (bmap);
        }
    }
    for (ai = 0; ai < attrn; ai++) {
        switch (attrp[ai].id) {
        case G_ATTRCURSOR:
            if (Strcmp (attrp[ai].u.t, "watch") == 0) {
                curcursori = 1;
                cursor = LoadCursor ((HINSTANCE) NULL, IDC_WAIT);
            } else if (Strcmp (attrp[ai].u.t, "default") == 0) {
                curcursori = -1;
                cursor = LoadCursor ((HINSTANCE) NULL, IDC_ARROW);
            } else {
                Gerr (POS, G_ERRNOSUCHCURSOR, attrp[ai].u.t);
                return -1;
            }
            SetCursor (cursor);
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
            WCU->colors[color].color.peRed = r;
            WCU->colors[color].color.peGreen = g;
            WCU->colors[color].color.peBlue = b;
            WCU->colors[color].color.peFlags = 0;
            if (color >= WCU->ncolor)
                ResizePalette (WCU->cmap, color + 1), WCU->ncolor = color + 1;
            SetPaletteEntries (WCU->cmap, (int) color, 1,
                    &WCU->colors[color].color);
            RealizePalette (GC);
            WCU->colors[color].inuse = TRUE;
            if (color == WCU->gattr.color)
                WCU->gattr.color = -1;
            break;
        case G_ATTRVIEWPORT:
            if (attrp[ai].u.s.x == 0)
                attrp[ai].u.s.x = 1;
            if (attrp[ai].u.s.y == 0)
                attrp[ai].u.s.y = 1;
            WCU->vsize.x = attrp[ai].u.s.x + 0.5;
            WCU->vsize.y = attrp[ai].u.s.y + 0.5;
            SetWindowPos (widget->w, (HWND) NULL, 0, 0, WCU->vsize.x,
                    WCU->vsize.y, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
            break;
        case G_ATTRWINDOW:
            if (attrp[ai].u.r.o.x == attrp[ai].u.r.c.x)
                attrp[ai].u.r.c.x = attrp[ai].u.r.o.x + 1;
            if (attrp[ai].u.r.o.y == attrp[ai].u.r.c.y)
                attrp[ai].u.r.c.y = attrp[ai].u.r.o.y + 1;
            WCU->wrect = attrp[ai].u.r;
            break;
        }
    }
    if (parent && parent->type == G_ARRAYWIDGET)
        Gawinsertchild (parent, widget);
    Gadjustclip (widget);
    return 0;
}

int GCsetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    HCURSOR cursor;
    Gwidget_t *parent;
    PIXsize_t ps;
    DWORD wflags1;
    int ai, color, r, g, b;

    parent = (widget->pwi == -1) ? NULL : &Gwidgets[widget->pwi];
    wflags1 = SWP_NOMOVE | SWP_NOZORDER;
    for (ai = 0; ai < attrn; ai++) {
        switch (attrp[ai].id) {
        case G_ATTRSIZE:
            GETSIZE (attrp[ai].u.s, ps, MINCWSIZE);
            Gadjustwrect (parent, &ps);
            SetWindowPos (widget->w, (HWND) NULL, 0, 0, ps.x, ps.y, wflags1);
            break;
        case G_ATTRBORDERWIDTH:
            Gerr (POS, G_ERRCANNOTSETATTR2, "borderwidth");
            return -1;
        case G_ATTRCURSOR:
            if (Strcmp (attrp[ai].u.t, "watch") == 0) {
                curcursori = 1;
                cursor = LoadCursor ((HINSTANCE) NULL, IDC_WAIT);
            } else if (Strcmp (attrp[ai].u.t, "default") == 0) {
                curcursori = -1;
                cursor = LoadCursor ((HINSTANCE) NULL, IDC_ARROW);
            } else {
                Gerr (POS, G_ERRNOSUCHCURSOR, attrp[ai].u.t);
                return -1;
            }
            SetCursor (cursor);
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
            WCU->colors[color].color.peRed = r;
            WCU->colors[color].color.peGreen = g;
            WCU->colors[color].color.peBlue = b;
            WCU->colors[color].color.peFlags = 0;
            if (color >= WCU->ncolor)
                ResizePalette (WCU->cmap, color + 1), WCU->ncolor = color + 1;
            SetPaletteEntries (WCU->cmap, (int) color, 1,
                    &WCU->colors[color].color);
            RealizePalette (GC);
            WCU->colors[color].inuse = TRUE;
            if (color == WCU->gattr.color)
                WCU->gattr.color = -1;
            break;
        case G_ATTRVIEWPORT:
            if (attrp[ai].u.s.x == 0)
                attrp[ai].u.s.x = 1;
            if (attrp[ai].u.s.y == 0)
                attrp[ai].u.s.y = 1;
            WCU->vsize.x = attrp[ai].u.s.x + 0.5;
            WCU->vsize.y = attrp[ai].u.s.y + 0.5;
            ps.x = WCU->vsize.x, ps.y = WCU->vsize.y;
            Gadjustwrect (&Gwidgets[widget->pwi], &ps);
            SetWindowPos (widget->w, (HWND) NULL, 0, 0, ps.x,
                    ps.y, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
            Gadjustclip (widget);
            break;
        case G_ATTRWINDOW:
            if (attrp[ai].u.r.o.x == attrp[ai].u.r.c.x)
                attrp[ai].u.r.c.x = attrp[ai].u.r.o.x + 1;
            if (attrp[ai].u.r.o.y == attrp[ai].u.r.c.y)
                attrp[ai].u.r.c.y = attrp[ai].u.r.o.y + 1;
            WCU->wrect = attrp[ai].u.r;
            Gadjustclip (widget);
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
    return 0;
}

int GCgetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    PALETTEENTRY *cp;
    RECT r;
    int color, ai;

    for (ai = 0; ai < attrn; ai++) {
        switch (attrp[ai].id) {
        case G_ATTRSIZE:
            GetWindowRect (widget->w, &r);
            attrp[ai].u.s.x = r.right - r.left;
            attrp[ai].u.s.y = r.bottom - r.top;
            break;
        case G_ATTRBORDERWIDTH:
            Gerr (POS, G_ERRCANNOTGETATTR, "borderwidth");
            return -1;
        case G_ATTRCURSOR:
            attrp[ai].u.t = (curcursori == -1) ?
                    "default" : "watch";
            break;
        case G_ATTRCOLOR:
            color = attrp[ai].u.c.index;
            if (color < 0 || color > G_MAXCOLORS) {
                Gerr (POS, G_ERRBADCOLORINDEX, color);
                return -1;
            }
            if (WCU->colors[color].inuse) {
                cp = &WCU->colors[color].color;
                attrp[ai].u.c.r = cp->peRed / 257.0;
                attrp[ai].u.c.g = cp->peGreen / 257.0;
                attrp[ai].u.c.b = cp->peBlue / 257.0;
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
    Gwidget_t *parent;

    parent = (widget->pwi == -1) ? NULL : &Gwidgets[widget->pwi];
    if (parent && parent->type == G_ARRAYWIDGET)
        Gawdeletechild (parent, widget);
    DestroyWindow (widget->w);
    return 0;
}

int GCcanvasclear (Gwidget_t *widget) {
    RECT r;
    HBRUSH brush, pbrush;

    /* FIXME: drain repaint messages */
    brush = CreateSolidBrush (PALETTEINDEX (0));
    pbrush = SelectObject (GC, brush);
    GetClientRect (widget->w, &r);
    Rectangle (GC, r.left, r.top, r.right, r.bottom);
    SelectObject (GC, pbrush);
    WCU->needredraw = FALSE;
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
    MoveTo (GC, pp1.x, pp1.y), LineTo (GC, pp2.x, pp2.y);
    MoveTo (GC, pa.x, pa.y), LineTo (GC, pp2.x, pp2.y);
    MoveTo (GC, pb.x, pb.y), LineTo (GC, pp2.x, pp2.y);
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
    MoveTo (GC, pp1.x, pp1.y);
    LineTo (GC, pp2.x, pp2.y);
    return 0;
}

int GCbox (Gwidget_t *widget, Grect_t gr, Ggattr_t *ap) {
    PIXrect_t pr;
    Grect_t gr2;

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
        Rectangle (GC, pr.o.x, pr.o.y, pr.c.x, pr.c.y);
    else {
        Gppp[0].x = pr.o.x, Gppp[0].y = pr.o.y;
        Gppp[1].x = pr.c.x, Gppp[1].y = pr.o.y;
        Gppp[2].x = pr.c.x, Gppp[2].y = pr.c.y;
        Gppp[3].x = pr.o.x, Gppp[3].y = pr.c.y;
        Gppp[4].x = pr.o.x, Gppp[4].y = pr.o.y;
        Polyline (GC, Gppp, 5);
    }
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
        Polygon (GC, Gppp, (int) gpn);
    } else
        Polyline (GC, Gppp, (int) gpn);
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
        Polygon (GC, Gppp, (int) Gppi);
    } else
        Polyline (GC, Gppp, (int) Gppi);
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
    double a1, a2;

    gr.o.x = gc.x - gs.x, gr.o.y = gc.y - gs.y;
    gr.c.x = gc.x + gs.x, gr.c.y = gc.y + gs.y;
    if (!ISVISIBLE (gr))
        return 1;
    pc = pdrawtopix (widget, gc), ps = sdrawtopix (widget, gs);
    setgattr (widget, ap);
    a1 = ang1 * M_PI / 180, a2 = ang2 * M_PI / 180;
    if (WCU->gattr.fill)
        Chord (GC, pc.x - ps.x, pc.y - ps.y, pc.x + ps.x, pc.y + ps.y,
                (int) (cos (a1) * ps.x), (int) (sin (a1) * ps.x),
                (int) (cos (a2) * ps.x), (int) (sin (a2) * ps.x));
    else
        Arc (GC, pc.x - ps.x, pc.y - ps.y, pc.x + ps.x, pc.y + ps.y,
                (int) (cos (a1) * ps.x), (int) (sin (a1) * ps.x),
                (int) (cos (a2) * ps.x), (int) (sin (a2) * ps.x));
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
    HFONT font;
    DWORD size;
    RECT r;
    int x, y, w, h, i;

    po = pdrawtopix (widget, go);
    gs.x = 0, gs.y = fs;
    ps = sdrawtopix (widget, gs);
    if (!(font = findfont (fn, ps.y))) {
        Rectangle (GC, po.x, po.y, po.x + 1, po.y + 1);
        return 0;
    }
    setgattr (widget, ap);
    SETFONT (font);
    for (w = h = 0, i = 0; i < n; i++) {
        if (tlp[i].n)
            size = GetTextExtent (GC, tlp[i].p, (int) tlp[i].n);
        else
            size = GetTextExtent (GC, "M", (int) 1);
        tlp[i].w = LOWORD (size), tlp[i].h = HIWORD (size);
        w = max (w, LOWORD (size)), h += HIWORD (size);
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
        y = po.y + i * tlp[i].h;
        r.left = x, r.top = y;
        r.right = x + tlp[i].w, r.bottom = y + tlp[i].h;
        DrawText (GC, tlp[i].p, (int) tlp[i].n, &r, DT_LEFT | DT_TOP);
    }
    return 0;
}

int GCgettextsize (Gwidget_t *widget, Gtextline_t *tlp, int n, char *fn,
        double fs, Gsize_t *gsp) {
    Gsize_t gs;
    PIXsize_t ps;
    HFONT font;
    int i;
    DWORD size;

    gs.x = 0, gs.y = fs;
    ps = sdrawtopix (widget, gs);
    if (!(font = findfont (fn, ps.y))) {
        gsp->x = 1, gsp->y = 1;
        return 0;
    }
    SETFONT (font);
    for (ps.x = ps.y = 0, i = 0; i < n; i++) {
        size = GetTextExtent (GC, tlp[i].p, (int) tlp[i].n);
        ps.x = max (ps.x, LOWORD (size)), ps.y += HIWORD (size);
    }
    *gsp = spixtodraw (widget, ps);
    return 0;
}

static HFONT findfont (char *name, int size) {
    HFONT font;
    int fi;

    if (name[0] == '\000')
        return Gfontp[0].font;

    sprintf (&Gbufp[0], name, size);
    for (fi = 0; fi < Gfontn; fi++)
        if (Strcmp (&Gbufp[0], Gfontp[fi].name) == 0 && Gfontp[fi].size == size)
            return Gfontp[fi].font;
    font = CreateFont ((int) size,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &Gbufp[0]);
    if (!font)
        font = Gfontp[0].font;

    Gfontp = Marraygrow (Gfontp, (long) (Gfontn + 1) * FONTSIZE);
    Gfontp[Gfontn].name = strdup (&Gbufp[0]);
    Gfontp[Gfontn].size = size;
    Gfontp[Gfontn].font = font;
    Gfontn++;
    return font;
}

int GCgetmousecoords (Gwidget_t *widget, Gpoint_t *gpp, int *count) {
    PIXpoint_t pp;
    POINT p;
    int n1, n2, n3;

    GetCursorPos (&p);
    ScreenToClient (widget->w, &p);
    pp.x = p.x, pp.y = p.y;
    *gpp = ppixtodraw (widget, pp);
    n1 = GetAsyncKeyState (VK_LBUTTON);
    n2 = GetAsyncKeyState (VK_MBUTTON);
    n3 = GetAsyncKeyState (VK_RBUTTON);
    *count = (n1 < 0 ? 1 : 0) + (n2 < 0 ? 1 : 0) + (n3 < 0 ? 1 : 0);
    return 0;
}

static void setgattr (Gwidget_t *widget, Ggattr_t *ap) {
    HBRUSH brush, pbrush;
    HPEN pen, ppen;
    PALETTEENTRY *colorp;
    long color, mode, style, width, flag, pati;
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
    flag = FALSE;
    mode = ap->mode;
    if (mode != WCU->gattr.mode) {
        WCU->gattr.mode = mode;
        SetROP2 (GC, (int) mode);
    }
    WCU->gattr.fill = ap->fill;
    color = ap->color;
    if (color >= G_MAXCOLORS || !(WCU->colors[color].inuse))
        color = 1;
    if (color != WCU->gattr.color)
        WCU->gattr.color = color, flag = TRUE;
    width = ap->width;
    if (width != WCU->gattr.width)
        WCU->gattr.width = width, flag = TRUE;
    style = ap->style;
    if (style != WCU->gattr.style)
        WCU->gattr.style = style, style = TRUE;

    if (!flag)
        return;
    WCU->gattr.color = color;
    if (Gdepth == 1) {
        colorp = &WCU->colors[color].color;
        intens = (0.3 * colorp->peBlue + 0.59 * colorp->peRed +
                0.11 * colorp->peGreen) / 255.0;
        pati = (intens <= 0.0625) ? 16 : -16.0 * (log (intens) / 2.7725887222);
        brush = WCU->grays[pati];
    } else
        brush = CreateSolidBrush (PALETTEINDEX (WCU->gattr.color));
    pbrush = SelectObject (GC, brush);
    if (Gdepth != 1)
        DeleteObject (pbrush);
    pen = CreatePen ((int) gstyles[WCU->gattr.style], WCU->gattr.width,
            PALETTEINDEX (WCU->gattr.color));
    ppen = SelectObject (GC, pen);
    DeleteObject (ppen);
    SetTextColor (GC, PALETTEINDEX (WCU->gattr.color));
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

Gpoint_t ppixtodraw (Gwidget_t *widget, PIXpoint_t pp) {
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

void Gadjustclip (Gwidget_t *widget) {
    Gwidget_t *parent;
    PIXrect_t pr;
    RECT r1, r2, r3;

    parent = &Gwidgets[widget->pwi];
    GetWindowRect (widget->w, &r1);
    GetClientRect (parent->w, &r2);
    GetWindowRect (parent->w, &r3);
    pr.o.x = max (0, -(r1.left - r3.left));
    pr.o.y = max (0, -(r1.top - r3.top));
    pr.c.x = min (r1.right - r1.left, pr.o.x + r2.right - r2.left);
    pr.c.y = min (r1.bottom - r1.top, pr.o.y + r2.bottom - r2.top);
    pr.c.x = max (pr.o.x, pr.c.x);
    pr.c.y = max (pr.o.y, pr.c.y);
    WCU->clip = rpixtodraw (widget, pr);
}
