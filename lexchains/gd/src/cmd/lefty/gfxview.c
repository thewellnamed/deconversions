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
#include "mem.h"
#include "code.h"
#include "tbl.h"
#include "parse.h"
#include "exec.h"
#include "gfxview.h"

#define max(a, b) (((a) >= (b)) ? (a) : (b))
#define min(a, b) (((a) >= (b)) ? (b) : (a))

#define getwintnset(to) \
    if (getint (to, &wattrp[wattri].u.i) != -1) wattri++;
#define getwlongnset(to) \
    if (getlong (to, &wattrp[wattri].u.l) != -1) wattri++;
#define getwpointnset(to) \
    if (getxy (to, &wattrp[wattri].u.p) != -1) wattri++;
#define getwsizenset(to) \
    if (getxy (to, &wattrp[wattri].u.s) != -1) wattri++;
#define getwrectnset(to) \
    if (getrect (to, &wattrp[wattri].u.r) != -1) wattri++;
#define getwstrnset(to) \
    if (getstr (to, &wattrp[wattri].u.t) != -1) wattri++;
#define getwcolornset(ko, vo) \
    if (getcolor (ko, vo, &wattrp[wattri].u.c) != -1) wattri++;

#define getaintnset(to, n, flag) \
    if (to) { \
        getint (to, &n), dp->flags |= flag; \
    }
#define getastrnset(to, s, flag) \
    if (to) \
        getstr (to, &s), dp->flags |= flag;

typedef struct colorname_t {
    char *name;
    unsigned char r, g, b;
} colorname_t;

#include "colors.txt"

typedef struct gfxrect_t {
    struct gfxrect_t *next;
    Tobj ko;
    Grect_t r;
} gfxrect_t;
typedef struct gfxmenu_t {
    struct gfxmenu_t *next;
    Tobj ko;
    long time;
    long mi;
} gfxmenu_t;
#define LISTSIZE 100
typedef struct gfxnode_t {
    int inuse;
    long wi;
    Tobj plvo, pmvo, prvo, pkvo[256];
    Gpoint_t plp, pmp, prp, pkp[256];
    char ls, ms, rs, ks[256];
    struct gfxrect_t *rect[LISTSIZE];
    struct gfxmenu_t *menu[LISTSIZE];
} gfxnode_t;
#define GFXNODEINCR 20
#define GFXNODESIZE sizeof (gfxnode_t)
static gfxnode_t *gfxnodes;
static int gfxnoden;

#define ISAWIDGET(wi) (wi >= 0 && wi < Gwidgetn && Gwidgets[wi].inuse)
#define ISALABEL(wi) (Gwidgets[wi].type == G_LABELWIDGET)
#define ISACANVAS(wi) (Gwidgets[wi].type == G_CANVASWIDGET)
#define ISACANVAS2(wi) (Gwidgets[wi].type == G_CANVASWIDGET || Gwidgets[wi].type == G_PCANVASWIDGET)
#define NODEID(wi) Gwidgets[wi].udata

static Gpoint_t *gpp = NULL;
static long gpn = 0;
#define GPINCR 100
#define GPSIZE sizeof (Gpoint_t)

static Gwattr_t *wattrp = NULL;
static int wattrn = 0;
static int wattri = 0;
#define WATTRINCR 10
#define WATTRSIZE sizeof (Gwattr_t)

static Tobj rootwo;

static void nodeinit (int);
static void nodeterm (int);
static void rectinit (int);
static void rectterm (int);
static void rectinsert (int, Tobj, Grect_t);
static void rectmerge (int, Tobj, Grect_t);
static Tobj rectfind (int, Gpoint_t);
static void rectdelete (int, Tobj);
static void rectprune (int);
static void menuinsert (int, Tobj, long, long);
static long menufind (int, Tobj, long);
static void menuprune (int);
static int getwattr (Tobj, int *);
static int getgattr (Tobj, Ggattr_t *);
static int getrect (Tobj, Grect_t *);
static int getxy (Tobj, Gxy_t *);
static int getint (Tobj, int *);
static int getlong (Tobj, long *);
static int getdouble (Tobj, double *);
static int getstr (Tobj, char **);
static int getcolor (Tobj, Tobj, Gcolor_t *);

void GFXinit (void) {
    int ni;

    Tinss (root, "widgets", (rootwo = Ttable (100)));
    gfxnodes = Marrayalloc ((long) GFXNODEINCR * GFXNODESIZE);
    gfxnoden = GFXNODEINCR;
    for (ni = 0; ni < gfxnoden; ni++)
        gfxnodes[ni].inuse = FALSE;
    ni = 0;
    gpp = Marrayalloc ((long) GPINCR * GPSIZE);
    gpn = GPINCR;
    wattrp = Marrayalloc ((long) WATTRINCR * WATTRSIZE);
    wattrn = WATTRINCR, wattri = 0;
}

void GFXterm (void) {
    int ni;

    Marrayfree (wattrp), wattrp = NULL, wattrn = 0;
    Marrayfree (gpp), gpp = NULL, gpn = 0;
    for (ni = 0; ni < gfxnoden; ni++) {
        if (gfxnodes[ni].inuse) {
            Gdestroywidget (gfxnodes[ni].wi);
            nodeterm (ni);
            gfxnodes[ni].inuse = FALSE;
        }
    }
    Marrayfree (gfxnodes), gfxnodes = NULL, gfxnoden = 0;
    Tdels (root, "widgets");
}

/* callback for mem.c module - removes dead objects from gfxnodes */
void GFXprune (void) {
    gfxnode_t *np;
    int ni, ki;

    for (ni = 0; ni < gfxnoden; ni++) {
        np = &gfxnodes[ni];
        if (np->inuse) {
            rectprune (ni), menuprune (ni);
            if (np->plvo && !M_AREAOF (np->plvo))
                np->plvo = 0;
            if (np->pmvo && !M_AREAOF (np->pmvo))
                np->pmvo = 0;
            if (np->prvo && !M_AREAOF (np->prvo))
                np->prvo = 0;
            for (ki = 0; ki < 256; ki++)
                if (np->pkvo[ki] && !M_AREAOF (np->pkvo[ki]))
                    np->pkvo[ki] = 0;
        }
    }
}

/* callback for g.c module - calls the appropriate lefty function */
void GFXlabelcb (Gevent_t *evp) {
    Tobj fo, to, co, wo;
    gfxnode_t *np;
    char *fn;
    char s[2];
    long fm;
    int ni;

    ni = Gwidgets[evp->wi].udata;
    np = &gfxnodes[ni];
    wo = Tfindi (rootwo, evp->wi);
    switch (evp->type) {
    case G_MOUSE:
        switch (evp->data) {
        case G_LEFT:
            fn = (evp->code == G_UP) ? "leftup" : "leftdown";
            break;
        case G_MIDDLE:
            fn = (evp->code == G_UP) ? "middleup" : "middledown";
            break;
        case G_RIGHT:
            fn = (evp->code == G_UP) ? "rightup" : "rightdown";
            break;
        }
        break;
    case G_KEYBD:
        fn = (evp->code == G_UP) ? "keyup" : "keydown";
        break;
    }
    if (!wo || !(fo = Tfinds (wo, fn)) || Tgettype (fo) != T_CODE)
        if (!(fo = Tfinds (root, fn)) || Tgettype (fo) != T_CODE)
            return;
    fm = Mpushmark (fo);
    to = Ttable (4);
    Mpushmark (to);
    Tinss (to, "widget", Tinteger (evp->wi));
    if (evp->type == G_KEYBD)
        s[0] = evp->data, s[1] = '\000', Tinss (to, "key", Tstring (s));
    if ((co = Pfcall (fo, to)))
        Eunit (co);
    Mpopmark (fm);
}

/* callback for g.c module - calls the appropriate lefty function */
void GFXviewcb (Gevent_t *evp) {
    Tobj fo, to, co, wo;
    char *fn;
    long fm;

    wo = Tfindi (rootwo, evp->wi);
    fn = "closeview";
    if (!wo || !(fo = Tfinds (wo, fn)) || Tgettype (fo) != T_CODE)
        if (!(fo = Tfinds (root, fn)) || Tgettype (fo) != T_CODE)
            exit (0);
    fm = Mpushmark (fo);
    to = Ttable (2);
    Mpushmark (to);
    Tinss (to, "widget", Tinteger (evp->wi));
    if ((co = Pfcall (fo, to)))
        Eunit (co);
    Mpopmark (fm);
}

/* callback for g.c module - calls the appropriate lefty function */
void GFXevent (Gevent_t *evp) {
    Tobj vo, pvo, fo, to, po, co, wo;
    gfxnode_t *np;
    Gpoint_t pp;
    char *fn;
    char s[2];
    long fm;
    int ni;

    ni = Gwidgets[evp->wi].udata;
    np = &gfxnodes[ni];
    wo = Tfindi (rootwo, evp->wi);
    if (!(vo = rectfind (ni, evp->p)))
        vo = null;
    switch (evp->type) {
    case G_MOUSE:
        switch (evp->data) {
        case G_LEFT:
            if (evp->code == G_UP)
                fn = "leftup", pvo = np->plvo, pp = np->plp, np->ls = 0;
            else
                fn = "leftdown", np->plvo = vo, np->plp = evp->p, np->ls = 1;
            break;
        case G_MIDDLE:
            if (evp->code == G_UP)
                fn = "middleup", pvo = np->pmvo, pp = np->pmp, np->ms = 0;
            else
                fn = "middledown", np->pmvo = vo, np->pmp = evp->p, np->ms = 1;
            break;
        case G_RIGHT:
            if (evp->code == G_UP)
                fn = "rightup", pvo = np->prvo, pp = np->prp, np->rs = 0;
            else
                fn = "rightdown", np->prvo = vo, np->prp = evp->p, np->rs = 1;
            break;
        }
        break;
    case G_KEYBD:
        if (evp->code == G_UP) {
            fn = "keyup";
            pvo = np->pkvo[evp->data];
            pp = np->pkp[evp->data];
            np->ks[evp->data] = 0;
        } else {
            fn = "keydown";
            np->pkvo[evp->data] = vo;
            np->pkp[evp->data] = evp->p;
            np->ks[evp->data] = 1;
        }
        break;
    }
    if (!wo || !(fo = Tfinds (wo, fn)) || Tgettype (fo) != T_CODE)
        if (!(fo = Tfinds (root, fn)) || Tgettype (fo) != T_CODE)
            return;

    fm = Mpushmark (fo);
    to = Ttable (4);
    Mpushmark (to);
    Tinss (to, "widget", Tinteger (evp->wi));
    Tinss (to, "obj", vo);
    Tinss (to, "pos", (po = Ttable (2)));
    Tinss (po, "x", Treal (evp->p.x));
    Tinss (po, "y", Treal (evp->p.y));
    if (evp->code == G_UP) {
        Tinss (to, "pobj", pvo);
        Tinss (to, "ppos", (po = Ttable (2)));
        Tinss (po, "x", Treal (pp.x));
        Tinss (po, "y", Treal (pp.y));
    }
    if (evp->type == G_KEYBD)
        s[0] = evp->data, s[1] = '\000', Tinss (to, "key", Tstring (s));
    if ((co = Pfcall (fo, to)))
        Eunit (co);
    Mpopmark (fm);
}

/* called directly from lefty.c, when any button is held down */
void GFXmove (void) {
    gfxnode_t *np;
    char *fn[3];
    Tobj vo[3], fo, to, po, co, wo;
    Gpoint_t cp, pp[3];
    long fm;
    int count, i, ni;

    for (ni = 0; ni < gfxnoden; ni++) {
        np = &gfxnodes[ni];
        if (!np->inuse || !ISACANVAS (np->wi) ||
                !Gwidgets[np->wi].u.c->buttonsdown)
            continue;
        wo = Tfindi (rootwo, np->wi);
        Ggetmousecoords (np->wi, &cp, &count);
        if (!count) {
            Gresetbstate (np->wi);
            continue;
        }
        if (np->ls)
            fn[0] = "leftmove",   pp[0] = np->plp, vo[0] = np->plvo;
        else
            fn[0] = NULL;
        if (np->ms)
            fn[1] = "middlemove", pp[1] = np->pmp, vo[1] = np->pmvo;
        else
            fn[1] = NULL;
        if (np->ls)
            fn[2] = "rightmove",  pp[2] = np->prp, vo[2] = np->prvo;
        else
            fn[2] = NULL;
        for (i = 0; i < 3; i++) {
            if (!fn[i])
                continue;
            if (!wo || !(fo = Tfinds (wo, fn[i])) || Tgettype (fo) != T_CODE)
                fo = Tfinds (root, fn[i]);
            if (fo && Tgettype (fo) == T_CODE) {
                fm = Mpushmark (fo);
                to = Ttable (4);
                Mpushmark (to);
                Tinss (to, "widget", Tinteger (np->wi));
                Tinss (to, "obj", vo[i]);
                Tinss (to, "pos", (po = Ttable (2)));
                Tinss (po, "x", Treal (cp.x));
                Tinss (po, "y", Treal (cp.y));
                Tinss (to, "ppos", (po = Ttable (2)));
                Tinss (po, "x", Treal (pp[i].x));
                Tinss (po, "y", Treal (pp[i].y));
                if ((co = Pfcall (fo, to)))
                    Eunit (co);
                Mpopmark (fm);
            }
        }
    }
}

/* called directly from lefty.c, when any canvas needs to be redrawn */
void GFXredraw (void) {
    gfxnode_t *np;
    Tobj wo, fo, co, to;
    long fm;
    int ni;

    for (ni = 0; ni < gfxnoden; ni++) {
        np = &gfxnodes[ni];
        if (!np->inuse || !ISACANVAS (np->wi) ||
                !Gwidgets[np->wi].u.c->needredraw)
            continue;
        Gwidgets[np->wi].u.c->needredraw = FALSE;
        wo = Tfindi (rootwo, np->wi);
        if (!wo || !(fo = Tfinds (wo, "redraw")) || Tgettype (fo) != T_CODE)
            if (!(fo = Tfinds (root, "redraw")) || Tgettype (fo) != T_CODE)
                return;

        fm = Mpushmark (fo);
        to = Ttable (4);
        Mpushmark (to);
        Tinss (to, "widget", Tinteger (np->wi));
        if ((co = Pfcall (fo, to)))
            Eunit (co);
        Mpopmark (fm);
    }
}

void GFXtextcb (int wi, char *s) {
    Tobj wo, fo, co, to;
    long fm;

    if (!(wo = Tfindi (rootwo, wi)))
        return;

    if (!(fo = Tfinds (wo, "oneline")) || Tgettype (fo) != T_CODE)
        return;

    fm = Mpushmark (fo);
    to = Ttable (4);
    Mpushmark (to);
    Tinss (to, "widget", Tinteger (wi));
    Tinss (to, "text", Tstring (s));
    if ((co = Pfcall (fo, to)))
        Eunit (co);
    Mpopmark (fm);
}

void GFXbuttoncb (int wi, void *data) {
    Tobj wo, fo, co, to;
    long fm;

    if (!(wo = Tfindi (rootwo, wi)))
        return;

    if (!(fo = Tfinds (wo, "pressed")) || Tgettype (fo) != T_CODE)
        return;

    fm = Mpushmark (fo);
    to = Ttable (4);
    Mpushmark (to);
    Tinss (to, "widget", Tinteger (wi));
    if ((co = Pfcall (fo, to)))
        Eunit (co);
    Mpopmark (fm);
}

void GFXarrayresizecb (int wi, Gawdata_t *dp) {
    Tobj wo, fo, co, so, to, lrtno, sxo, syo;
    Tkvindex_t tkvi;
    Gawcarray_t *cp;
    int sx, sy, csx, csy;
    long ci, fm;

    if (!(wo = Tfindi (rootwo, wi)))
        return;

    if (!(fo = Tfinds (wo, "resize")) || Tgettype (fo) != T_CODE) {
        Gawdefcoordscb (wi, dp);
        return;
    }

    fm = Mpushmark (fo);
    to = Ttable (4);
    Mpushmark (to);
    Tinss (to, "widget", Tinteger (wi));
    Tinss (to, "size", (so = Ttable (2)));
    Tinss (so, "x", Treal ((double) dp->sx));
    Tinss (so, "y", Treal ((double) dp->sy));
    if ((co = Pfcall (fo, to)))
        lrtno = Eunit (co);
    Mpopmark (fm);
    if (!lrtno) {
        Gawdefcoordscb (wi, dp);
        return;
    }
    for (Tgetfirst (lrtno, &tkvi); tkvi.kvp; Tgetnext (&tkvi)) {
        if (!T_ISNUMBER (tkvi.kvp->ko) || !T_ISTABLE ((so = tkvi.kvp->vo)))
            continue;
        wi = Tgetnumber (tkvi.kvp->ko);
        if (!ISAWIDGET (wi))
            continue;
        for (ci = 0; ci < dp->cj; ci++) {
            cp = &dp->carray[ci];
            if (!cp->flag || cp->w != Gwidgets[wi].w)
                continue;
            if ((sxo = Tfinds (so, "x")) && T_ISNUMBER (sxo))
                cp->sx = Tgetnumber (sxo);
            if ((syo = Tfinds (so, "y")) && T_ISNUMBER (syo))
                cp->sy = Tgetnumber (syo);
            break;
        }
    }
    sx = dp->sx, sy = dp->sy;
    csx = csy = 0;
    for (ci = 0; ci < dp->cj; ci++) {
        cp = &dp->carray[ci];
        if (!cp->flag)
            continue;
        cp->ox = csx, cp->oy = csy;
        if (dp->type == G_AWVARRAY)
            cp->sx = sx - 2 * cp->bs, csy += cp->sy + 2 * cp->bs;
        else
            cp->sy = sy - 2 * cp->bs, csx += cp->sx + 2 * cp->bs;
    }
    if (dp->type == G_AWVARRAY)
        dp->sy = csy;
    else
        dp->sx = csx;
}

/* callback for when there is input on some file descriptor */
void GFXmonitorfile (int fd) {
    Tobj fo, to, co;
    long fm, tm;

    if (!(fo = Tfinds (root, "monitorfile")) || Tgettype (fo) != T_CODE)
        return;

    fm = Mpushmark (fo);
    to = Ttable (4);
    tm = Mpushmark (to);
    Tinss (to, "fd", Tinteger (fd));
    if ((co = Pfcall (fo, to)))
        Eunit (co);
    Mpopmark (tm);
    Mpopmark (fm);
}

/* callback for when there is no X event and no file input */
void GFXidle (void) {
    Tobj fo, to, co;
    long fm, tm;

    if (!(fo = Tfinds (root, "idle")) || Tgettype (fo) != T_CODE)
        return;

    fm = Mpushmark (fo);
    to = Ttable (4);
    tm = Mpushmark (to);
    if ((co = Pfcall (fo, to)))
        Eunit (co);
    Mpopmark (tm);
    Mpopmark (fm);
}

/* LEFTY builtin */
int GFXcreatewidget (int argc, lvar_t *argv) {
    Tobj pwo, cwo, cho;
    long pwi, wi, rtnm;
    int type, ni;

    type = -1;
    if (getlong (argv[0].o, &pwi) == -1 || getwattr (argv[1].o, &type) == -1)
        return FAILED;
    if (type != G_VIEWWIDGET && type != G_PCANVASWIDGET && !ISAWIDGET (pwi))
        return FAILED;
    if (type == G_CANVASWIDGET || type == G_LABELWIDGET) {
        for (ni = 0; ni < gfxnoden; ni++)
            if (!gfxnodes[ni].inuse)
                break;
        if (ni == gfxnoden) {
            gfxnodes = Marraygrow (gfxnodes,
                    (long) (ni + GFXNODEINCR) * GFXNODESIZE);
            for (ni = gfxnoden; ni < gfxnoden + GFXNODEINCR; ni++)
                gfxnodes[ni].inuse = FALSE;
            ni = gfxnoden, gfxnoden += GFXNODEINCR;
        }
        nodeinit (ni);
        if (wattri >= wattrn) {
            wattrp = Marraygrow (wattrp,
                    (long) (wattrn + WATTRINCR) * WATTRSIZE);
            wattrn += WATTRINCR;
        }
        switch (type) {
        case G_CANVASWIDGET:
            wattrp[wattri].id = G_ATTRUSERDATA;
            wattrp[wattri].u.u = ni, wattri++;
            break;
        case G_LABELWIDGET:
            wattrp[wattri].id = G_ATTRUSERDATA;
            wattrp[wattri].u.u = ni, wattri++;
            break;
        }
        wi = gfxnodes[ni].wi = Gcreatewidget (pwi, type, wattri, wattrp);
        gfxnodes[ni].inuse = TRUE;
        goto done;
    }
    wi = Gcreatewidget (pwi, type, wattri, wattrp);

done:
    Tinsi (rootwo, wi, (cwo = Ttable (4)));
    rtno = Tinteger (wi);
    rtnm = Mpushmark (rtno);
    if (pwi != -1) {
        Tinss (cwo, "parent", Tinteger (pwi));
        if ((pwo = Tfindi (rootwo, pwi))) {
            if (!(cho = Tfinds (pwo, "children")))
                Tinss (pwo, "children", (cho = Ttable (2)));
            Tinsi (cho, wi, Tinteger (pwi));
        }
    }
    Mpopmark (rtnm);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXsetwidgetattr (int argc, lvar_t *argv) {
    long wi;
    int type;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi))
        return FAILED;
    type = Gwidgets[wi].type;
    if (getwattr (argv[1].o, &type) == -1)
        return FAILED;
    Gsetwidgetattr (wi, wattri, wattrp);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXgetwidgetattr (int argc, lvar_t *argv) {
    Tkvindex_t tkvi;
    Tobj po, so, ro, co, co2;
    Gwattrmap_t *mapp;
    int *ap;
    int li, ai, type, color, wattri2;
    long wi, rtnm;

    wattri = 0;
    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) ||
            !T_ISTABLE (argv[1].o))
        return FAILED;
    type = Gwidgets[wi].type;
    for (ap = NULL, li = 0; Gwlist[li].wname; li++) {
        if (type == Gwlist[li].wid) {
            ap = Gwlist[li].attrid;
            break;
        }
    }
    if (!ap)
        return FAILED;
    for (Tgetfirst (argv[1].o, &tkvi); tkvi.kvp; Tgetnext (&tkvi)) {
        if (!T_ISSTRING (tkvi.kvp->vo))
            continue;
        for (ai = 0; ap[ai] != -1; ai++) {
            if (Strcmp (Gwattrmap[ap[ai]].name, Tgetstring (tkvi.kvp->vo)) == 0) {
                if (wattri >= wattrn) {
                    wattrp = Marraygrow (wattrp,
                            (long) (wattrn + WATTRINCR) * WATTRSIZE);
                    wattrn += WATTRINCR;
                }
                if (ap[ai] == G_ATTRCOLOR) {
                    for (color = 0; color < G_MAXCOLORS; color++) {
                        if (wattri >= wattrn) {
                            wattrp = Marraygrow (wattrp,
                                    (long) (wattrn + WATTRINCR) * WATTRSIZE);
                            wattrn += WATTRINCR;
                        }
                        wattrp[wattri].u.c.index = color;
                        wattrp[wattri++].id = ap[ai];
                    }
                } else
                    wattrp[wattri++].id = ap[ai];
                break;
            }
        }
    }
    if (Ggetwidgetattr (wi, wattri, wattrp) == -1)
        return FAILED;
    rtno = Ttable (wattri);
    rtnm = Mpushmark (rtno);
    for (wattri2 = 0; wattri2 < wattri; wattri2++) {
        mapp = &Gwattrmap[wattrp[wattri2].id];
        switch (mapp->type) {
        case G_ATTRTYPEPOINT:
            Tinss (rtno, mapp->name, (po = Ttable (2)));
            Tinss (po, "x", Treal (wattrp[wattri2].u.p.x));
            Tinss (po, "y", Treal (wattrp[wattri2].u.p.y));
            break;
        case G_ATTRTYPESIZE:
            Tinss (rtno, mapp->name, (so = Ttable (2)));
            Tinss (so, "x", Treal (wattrp[wattri2].u.s.x));
            Tinss (so, "y", Treal (wattrp[wattri2].u.s.y));
            break;
        case G_ATTRTYPERECT:
            Tinss (rtno, mapp->name, (ro = Ttable (2)));
            Tinsi (ro, 0, (po = Ttable (2)));
            Tinss (po, "x", Treal (wattrp[wattri2].u.r.o.x));
            Tinss (po, "y", Treal (wattrp[wattri2].u.r.o.y));
            Tinsi (ro, 1, (po = Ttable (2)));
            Tinss (po, "x", Treal (wattrp[wattri2].u.r.c.x));
            Tinss (po, "y", Treal (wattrp[wattri2].u.r.c.y));
            break;
        case G_ATTRTYPETEXT:
            Tinss (rtno, mapp->name, Tstring (wattrp[wattri2].u.t));
            break;
        case G_ATTRTYPEINT:
            Tinss (rtno, mapp->name, Tinteger (wattrp[wattri2].u.i));
            break;
        case G_ATTRTYPECOLOR:
            Tinss (rtno, mapp->name, (co = Ttable (G_MAXCOLORS)));
            for (color = 0; color < G_MAXCOLORS; color++) {
                Tinsi (co, color, (co2 = Ttable (3)));
                Tinss (co2, "r", Treal (wattrp[wattri2 + color].u.c.r));
                Tinss (co2, "g", Treal (wattrp[wattri2 + color].u.c.g));
                Tinss (co2, "b", Treal (wattrp[wattri2 + color].u.c.g));
            }
            wattri2 += (G_MAXCOLORS - 1);
            break;
        }
    }
    Mpopmark (rtnm);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXdestroywidget (int argc, lvar_t *argv) {
    Tkvindex_t tkvi;
    Tobj wo, cho, pwio, pwo;
    lvar_t argv2[1];
    long wi;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi))
        return FAILED;

    wo = Tfindi (rootwo, wi);
    if ((cho = Tfinds (wo, "children"))) {
        while (Tgettablen (cho) > 0) {
            Tgetfirst (cho, &tkvi);
            argv2[0].o = tkvi.kvp->ko;
            GFXdestroywidget (1, argv2);
        }
    }
    if ((pwio = Tfinds (wo, "parent"))) {
        pwo = Tfindi (rootwo, Tgetinteger (pwio));
        cho = Tfinds (pwo, "children");
        Tdeli (cho, wi);
    }
    if (ISACANVAS (wi)) {
        nodeterm (NODEID (wi));
        gfxnodes[NODEID (wi)].inuse = FALSE;
    }
    Gdestroywidget (wi);
    Tdeli (rootwo, wi);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXclear (int argc, lvar_t *argv) {
    long wi;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi))
        return FAILED;
    Gcanvasclear (wi);
    if (ISACANVAS (wi))
        rectterm (NODEID (wi)), rectinit (NODEID (wi));
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXsetgfxattr (int argc, lvar_t *argv) {
    Ggattr_t gattr;
    long wi;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) ||
            getgattr (argv[1].o, &gattr) == -1)
        return FAILED;
    Gsetgfxattr (wi, &gattr);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXgetgfxattr (int argc, lvar_t *argv) {
    Tkvindex_t tkvi;
    Ggattr_t gattr;
    long wi, rtnm;
    char *s;

    gattr.flags = 0;
    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) ||
            !T_ISTABLE (argv[1].o))
        return FAILED;
    for (Tgetfirst (argv[1].o, &tkvi); tkvi.kvp; Tgetnext (&tkvi)) {
        if (!T_ISSTRING (tkvi.kvp->vo))
            continue;
        s = Tgetstring (tkvi.kvp->vo);
        if (Strcmp (s, "color") == 0)
            gattr.flags |= G_GATTRCOLOR;
        else if (Strcmp (s, "width") == 0)
            gattr.flags |= G_GATTRWIDTH;
        else if (Strcmp (s, "mode") == 0)
            gattr.flags |= G_GATTRMODE;
        else if (Strcmp (s, "fill") == 0)
            gattr.flags |= G_GATTRFILL;
        else if (Strcmp (s, "style") == 0)
            gattr.flags |= G_GATTRSTYLE;
    }
    if (Ggetgfxattr (wi, &gattr) == -1)
        return FAILED;
    rtno = Ttable (wattri);
    rtnm = Mpushmark (rtno);
    if (gattr.flags & G_GATTRCOLOR) {
        Tinss (rtno, "color", Tinteger (gattr.color));
    } else if (gattr.flags & G_GATTRWIDTH) {
        Tinss (rtno, "width", Tinteger (gattr.width));
    } else if (gattr.flags & G_GATTRMODE) {
        s = (gattr.mode == G_SRC) ? "src" : "xor";
        Tinss (rtno, "mode", Tstring (s));
    } else if (gattr.flags & G_GATTRFILL) {
        s = (gattr.fill) ? "on" : "off";
        Tinss (rtno, "fill", Tstring (s));
    } else if (gattr.flags & G_GATTRSTYLE) {
        switch (gattr.style) {
        case G_SOLID:       s = "solid";       break;
        case G_DASHED:      s = "dashed";      break;
        case G_DOTTED:      s = "dotted";      break;
        case G_LONGDASHED:  s = "longdashed";  break;
        case G_SHORTDASHED: s = "shortdashed"; break;
        }
        Tinss (rtno, "style", Tstring (s));
    }
    Mpopmark (rtnm);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXarrow (int argc, lvar_t *argv) {
    Ggattr_t gattr;
    Gpoint_t p0, p1;
    long wi;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) || !ISACANVAS2 (wi) ||
            getxy (argv[2].o, &p0) == -1 || getxy (argv[3].o, &p1) == -1 ||
            getgattr ((argc == 5) ? argv[4].o : NULL, &gattr) == -1)
        return FAILED;
    Garrow (wi, p0, p1, &gattr);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXline (int argc, lvar_t *argv) {
    Ggattr_t gattr;
    Gpoint_t p0, p1;
    long wi;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) || !ISACANVAS2 (wi) ||
            getxy (argv[2].o, &p0) == -1 || getxy (argv[3].o, &p1) == -1 ||
            getgattr ((argc == 5) ? argv[4].o : NULL, &gattr) == -1)
        return FAILED;
    Gline (wi, p0, p1, &gattr);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXbox (int argc, lvar_t *argv) {
    Ggattr_t gattr;
    Grect_t r;
    long wi;
    int rtn;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) || !ISACANVAS2 (wi) ||
            getrect (argv[2].o, &r) == -1 ||
            getgattr ((argc == 4) ? argv[3].o : NULL, &gattr) == -1)
        return FAILED;
    rtn = Gbox (wi, r, &gattr);
    if (rtn == 0 && argv[1].o != null && ISACANVAS (wi))
        rectmerge (NODEID (wi), argv[1].o, r);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXpolygon (int argc, lvar_t *argv) {
    Tobj po;
    Ggattr_t gattr;
    long i, pn, wi;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) || !ISACANVAS2 (wi) ||
            getgattr ((argc == 4) ? argv[3].o : NULL, &gattr) == -1)
        return FAILED;
    po = argv[2].o;
    if ((pn = Tgettablen (po)) > gpn) {
        gpp = Marraygrow (gpp, (long) pn * GPSIZE);
        gpn = pn;
    }
    for (i = 0; i < pn; i++)
        if (getxy (Tfindi (po, i), &gpp[i]) == -1)
            return FAILED;
    Gpolygon (wi, pn, gpp, &gattr);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXsplinegon (int argc, lvar_t *argv) {
    Tobj po;
    Ggattr_t gattr;
    long i, pn, wi;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) || !ISACANVAS2 (wi) ||
            getgattr ((argc == 4) ? argv[3].o : NULL, &gattr) == -1)
        return FAILED;
    po = argv[2].o;
    if ((pn = Tgettablen (po)) > gpn) {
        gpp = Marraygrow (gpp, (long) pn * GPSIZE);
        gpn = pn;
    }
    for (i = 0; i < pn; i++)
        if (getxy (Tfindi (po, i), &gpp[i]) == -1)
            return FAILED;
    Gsplinegon (wi, pn, gpp, &gattr);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXarc (int argc, lvar_t *argv) {
    Ggattr_t gattr;
    Grect_t r;
    Gpoint_t op;
    Gsize_t sp;
    long wi;
    int rtn;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) || !ISACANVAS2 (wi) ||
            getxy (argv[2].o, &op) == -1 || getxy (argv[3].o, &sp) == -1 ||
            getgattr ((argc == 5) ? argv[4].o : NULL, &gattr) == -1)
        return FAILED;
    rtn = Garc (wi, op, sp, (double) 0, (double) 360, &gattr);
    if (rtn == 0 && argv[1].o != null && ISACANVAS (wi)) {
        r.o.x = op.x - sp.x, r.o.y = op.y - sp.y;
        r.c.x = op.x + sp.x, r.c.y = op.y + sp.y;
        rectmerge (NODEID (wi), argv[1].o, r);
    }
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXtext (int argc, lvar_t *argv) {
    Ggattr_t gattr;
    Gpoint_t p;
    char *s, *fn, *justs;
    double fs;
    long wi;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) || !ISACANVAS2 (wi) ||
            getxy (argv[2].o, &p) == -1 || getstr (argv[3].o, &s) == -1 ||
            getstr (argv[4].o, &fn) == -1 || getdouble (argv[5].o, &fs) == -1 ||
            getstr (argv[6].o, &justs) == -1 ||
            getgattr ((argc == 8) ? argv[7].o : NULL, &gattr) == -1)
        return SUCCEEDED;
    Gtext (wi, s, p, fn, fs, justs, &gattr);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXtextsize (int argc, lvar_t *argv) {
    Gsize_t sp;
    double fs;
    char *s, *fn;
    long wi, m;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) || !ISACANVAS (wi) ||
            getstr (argv[1].o, &s) == -1 || getstr (argv[2].o, &fn) == -1 ||
            getdouble (argv[3].o, &fs) == -1)
        return FAILED;
    if (Ggettextsize (wi, s, fn, fs, &sp) == -1)
        return FAILED;
    m = Mpushmark ((rtno = Ttable (2)));
    Tinss (rtno, "x", Treal (sp.x)), Tinss (rtno, "y", Treal (sp.y));
    Mpopmark (m);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXclearpick (int argc, lvar_t *argv) {
    long wi;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) || !ISACANVAS (wi))
        return FAILED;
    if (argv[1].o != null)
        rectdelete (NODEID (wi), argv[1].o);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXsetpick (int argc, lvar_t *argv) {
    Grect_t r;
    long wi;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) || !ISACANVAS (wi) ||
            getrect (argv[2].o, &r) == -1)
        return FAILED;
    if (argv[1].o != null)
        rectinsert (NODEID (wi), argv[1].o, r);
    return SUCCEEDED;
}

/* LEFTY builtin */
int GFXdisplaymenu (int argc, lvar_t *argv) {
    Tobj mo, meo;
    char buf[50];
    char *entries[1];
    long wi, mi, mei;
    int rtn;

    if (getlong (argv[0].o, &wi) == -1 || !ISAWIDGET (wi) ||
            !(ISACANVAS (wi) || ISALABEL (wi)))
        return FAILED;
    mo = argv[1].o;
    if ((mi = menufind (NODEID (wi), mo, Tgettime (mo))) != -1) {
        if ((rtn = Gmenudisplay (wi, mi)) != -1)
            rtno = Tinteger (rtn);
        return SUCCEEDED;
    }
    wattri = 0;
    mi = Gcreatewidget (wi, G_MENUWIDGET, wattri, wattrp);
    mei = 0;
    while ((meo = Tfindi (mo, mei))) {
        switch (Tgettype (meo)) {
        case T_STRING:
            entries[0] = Tgetstring (meo);
            break;
        case T_INTEGER:
            sprintf (buf, "%d", (int) Tgetnumber (meo));
            entries[0] = &buf[0];
            break;
        case T_REAL:
            sprintf (buf, "%f", Tgetnumber (meo));
            entries[0] = &buf[0];
            break;
        }
        Gmenuaddentries (mi, 1, &entries[0]);
        mei++;
    }
    menuinsert (NODEID (wi), mo, Tgettime (mo), mi);
    if ((rtn = Gmenudisplay (wi, mi)) != -1)
        rtno = Tinteger (rtn);
    return SUCCEEDED;
}

static int getwattr (Tobj ao, int *type) {
    Tkvindex_t tkvi;
    Tobj co;
    int *ap;
    char *ts;
    int li, ai;

    wattri = 0;
    if (!ao && Tgettype (ao) != T_TABLE)
        return -1;
    if (*type == -1) {
        if (getstr (Tfinds (ao, "type"), &ts) == -1)
            return -1;
        for (ap = NULL, li = 0; Gwlist[li].wname; li++) {
            if (Strcmp (ts, Gwlist[li].wname) == 0) {
                *type = Gwlist[li].wid;
                ap = Gwlist[li].attrid;
                break;
            }
        }
        if (*type == -1)
            return -1;
        if (wattri >= wattrn) {
            wattrp = Marraygrow (wattrp,
                    (long) (wattrn + WATTRINCR) * WATTRSIZE);
            wattrn += WATTRINCR;
        }
        switch (*type) {
        case G_TEXTWIDGET:
            wattrp[wattri].id = G_ATTRNEWLINECB;
            wattrp[wattri].u.func = GFXtextcb, wattri++;
            break;
        case G_ARRAYWIDGET:
            wattrp[wattri].id = G_ATTRRESIZECB;
            wattrp[wattri].u.func = GFXarrayresizecb, wattri++;
            break;
        case G_BUTTONWIDGET:
            wattrp[wattri].id = G_ATTRBUTTONCB;
            wattrp[wattri].u.func = GFXbuttoncb, wattri++;
            break;
        case G_LABELWIDGET:
            wattrp[wattri].id = G_ATTREVENTCB;
            wattrp[wattri].u.func = GFXlabelcb, wattri++;
            break;
        case G_CANVASWIDGET:
            wattrp[wattri].id = G_ATTREVENTCB;
            wattrp[wattri].u.func = GFXevent, wattri++;
            break;
        case G_VIEWWIDGET:
            wattrp[wattri].id = G_ATTREVENTCB;
            wattrp[wattri].u.func = GFXviewcb, wattri++;
            break;
        }
    } else {
        for (ap = NULL, li = 0; Gwlist[li].wname; li++) {
            if (*type == Gwlist[li].wid) {
                ap = Gwlist[li].attrid;
                break;
            }
        }
    }
    for (ai = 0; ap[ai] != -1; ai++) {
        if (wattri >= wattrn) {
            wattrp = Marraygrow (wattrp,
                    (long) (wattrn + WATTRINCR) * WATTRSIZE);
            wattrn += WATTRINCR;
        }
        wattrp[wattri].id = ap[ai];
        switch (Gwattrmap[ap[ai]].type) {
        case G_ATTRTYPEPOINT:
            getwpointnset (Tfinds (ao, Gwattrmap[ap[ai]].name));
            break;
        case G_ATTRTYPESIZE:
            getwsizenset (Tfinds (ao, Gwattrmap[ap[ai]].name));
            break;
        case G_ATTRTYPERECT:
            getwrectnset (Tfinds (ao, Gwattrmap[ap[ai]].name));
            break;
        case G_ATTRTYPETEXT:
            getwstrnset (Tfinds (ao, Gwattrmap[ap[ai]].name));
            break;
        case G_ATTRTYPEINT:
            getwintnset (Tfinds (ao, Gwattrmap[ap[ai]].name));
            break;
        case G_ATTRTYPECOLOR:
            if ((co = Tfinds (ao, Gwattrmap[ap[ai]].name)) &&
                    Tgettype (co) == T_TABLE) {
                for (Tgetfirst (co, &tkvi); tkvi.kvp; Tgetnext (&tkvi))
                    getwcolornset (tkvi.kvp->ko, tkvi.kvp->vo);
            }
            break;
        }
    }
    return 0;
}

static int getgattr (Tobj ao, Ggattr_t *dp) {
    Tkvindex_t tkvi;
    char *s, *s2;

    dp->flags = 0;
    if (!ao)
        return 0;
    if (Tgettype (ao) != T_TABLE)
        return -1;
    for (Tgetfirst (ao, &tkvi); tkvi.kvp; Tgetnext (&tkvi)) {
        if (Tgettype (tkvi.kvp->ko) != T_STRING)
            continue;
        s = Tgetstring (tkvi.kvp->ko);
        if (Strcmp (s, "color") == 0) {
            getaintnset (tkvi.kvp->vo, dp->color, G_GATTRCOLOR);
        } else if (Strcmp (s, "width") == 0) {
            getaintnset (tkvi.kvp->vo, dp->width, G_GATTRWIDTH);
        } else if (Strcmp (s, "mode") == 0) {
            getastrnset (tkvi.kvp->vo, s2, G_GATTRMODE);
            if (dp->flags & G_GATTRMODE) {
                if (Strcmp (s2, "src") == 0)
                    dp->mode = G_SRC;
                else if (Strcmp (s2, "xor") == 0)
                    dp->mode = G_XOR;
                else
                    dp->flags &= ~G_GATTRMODE;
            }
        } else if (Strcmp (s, "fill") == 0) {
            getastrnset (tkvi.kvp->vo, s2, G_GATTRFILL);
            if (dp->flags & G_GATTRFILL) {
                if (Strcmp (s2, "on") == 0)
                    dp->fill = 1;
                else if (Strcmp (s2, "off") == 0)
                    dp->fill = 0;
                else
                    dp->flags &= ~G_GATTRFILL;
            }
        } else if (Strcmp (s, "style") == 0) {
            getastrnset (tkvi.kvp->vo, s2, G_GATTRSTYLE);
            if (dp->flags & G_GATTRSTYLE) {
                if (Strcmp (s2, "solid") == 0)
                    dp->style = G_SOLID;
                else if (Strcmp (s2, "dashed") == 0)
                    dp->style = G_DASHED;
                else if (Strcmp (s2, "dotted") == 0)
                    dp->style = G_DOTTED;
                else if (Strcmp (s2, "longdashed") == 0)
                    dp->style = G_LONGDASHED;
                else if (Strcmp (s2, "shortdashed") == 0)
                    dp->style = G_SHORTDASHED;
                else
                    dp->flags &= ~G_GATTRSTYLE;
            }
        }
    }
    return 0;
}

static int getrect (Tobj ro, Grect_t *rp) {
    if (ro && Tgettype (ro) == T_TABLE)
        if (getxy (Tfindi (ro, 0), &rp->o) != -1 &&
                getxy (Tfindi (ro, 1), &rp->c) != -1)
            return 0;
    return -1;
}

static int getxy (Tobj po, Gxy_t *xyp) {
    Tobj xo, yo;

    if (po && Tgettype (po) == T_TABLE) {
        xo = Tfinds (po, "x"), yo = Tfinds (po, "y");
        if (xo && T_ISNUMBER (xo) && yo && T_ISNUMBER (yo)) {
            xyp->x = Tgetnumber (xo), xyp->y = Tgetnumber (yo);
            return 0;
        }
    }
    return -1;
}

static int getint (Tobj to, int *ip) {
    if (to && T_ISNUMBER (to)) {
        *ip = Tgetnumber (to);
        return 0;
    }
    return -1;
}

static int getlong (Tobj to, long *lp) {
    if (to && T_ISNUMBER (to)) {
        *lp = Tgetnumber (to);
        return 0;
    }
    return -1;
}

static int getdouble (Tobj to, double *dp) {
    if (to && T_ISNUMBER (to)) {
        *dp = Tgetnumber (to);
        return 0;
    }
    return -1;
}

static int getstr (Tobj so, char **s) {
    if (so && T_ISSTRING (so)) {
        *s = Tgetstring (so);
        return 0;
    }
    return -1;
}

static int getcolor (Tobj ko, Tobj co, Gcolor_t *cp) {
    Tobj ro, go, bo, ho, so, vo;
    char *s, *s1, *s2;
    float hc, sc, vc, fc, pc, qc, tc;
    int cni, ic;

    if (ko && T_ISNUMBER (ko))
        cp->index = Tgetnumber (ko);
    else
        return -1;
    if (!co || !(T_ISSTRING (co) || T_ISTABLE (co)))
        return -1;
    if (T_ISSTRING (co)) {
        s = Tgetstring (co);
        for (cni = 0; cni < sizeof (colornames) / sizeof (colorname_t); cni++) {
            for (s1 = colornames[cni].name, s2 = s; *s1 && *s2; s1++, s2++) {
                if (*s2 == ' ' || *s2 == '_')
                    s2++;
                else if (*s1 != *s2)
                    break;
            }
            if (*s1 != *s2)
                continue;
            cp->r = colornames[cni].r;
            cp->g = colornames[cni].g;
            cp->b = colornames[cni].b;
            return 0;
        }
        return -1;
    } else {
        ro = Tfinds (co, "r");
        go = Tfinds (co, "g");
        bo = Tfinds (co, "b");
        if (ro && T_ISNUMBER (ro) && go && T_ISNUMBER (go) &&
                bo && T_ISNUMBER (bo)) {
            cp->r = Tgetnumber (ro);
            cp->g = Tgetnumber (go);
            cp->b = Tgetnumber (bo);
            return 0;
        } else {
            ho = Tfinds (co, "h");
            so = Tfinds (co, "s");
            vo = Tfinds (co, "v");
            if (ho && T_ISNUMBER (ho) && so && T_ISNUMBER (so) &&
                    vo && T_ISNUMBER (vo)) {
                hc = Tgetnumber (ho);
                sc = Tgetnumber (so);
                vc = Tgetnumber (vo);
                if (hc == 1)
                    hc = 0;
                hc = hc * 6;
                ic = hc;
                fc = hc - ic;
                pc = vc * (1 - sc);
                qc = vc * (1 - (2 * fc));
                tc = vc * (1 - (sc * (1 - fc)));
                switch (ic) {
                case 0:
                    cp->r = vc * 255, cp->g = tc * 255, cp->b = pc * 255;
                    break;
                case 1:
                    cp->r = qc * 255, cp->g = vc * 255, cp->b = pc * 255;
                    break;
                case 2:
                    cp->r = pc * 255, cp->g = vc * 255, cp->b = tc * 255;
                    break;
                case 3:
                    cp->r = pc * 255, cp->g = qc * 255, cp->b = vc * 255;
                    break;
                case 4:
                    cp->r = tc * 255, cp->g = pc * 255, cp->b = vc * 255;
                    break;
                case 5:
                    cp->r = vc * 255, cp->g = pc * 255, cp->b = qc * 255;
                    break;
                }
                return 0;
            } else
                return -1;
        }
    }
    return -1;
}

static void nodeinit (int ni) {
    int li, ki;

    for (li = 0; li < LISTSIZE; li++)
        gfxnodes[ni].rect[li] = NULL, gfxnodes[ni].menu[li] = NULL;
    gfxnodes[ni].ls = gfxnodes[ni].ms = gfxnodes[ni].rs = 0;
    for (ki = 0; ki < 256; ki++)
        gfxnodes[ni].ks[ki] = 0;
    gfxnodes[ni].plvo = gfxnodes[ni].pmvo = gfxnodes[ni].prvo = 0;
    for (ki = 0; ki < 256; ki++)
        gfxnodes[ni].pkvo[ki] = 0;
}

static void nodeterm (int ni) {
    gfxrect_t **rp;
    gfxrect_t *crp, *nrp;
    gfxmenu_t **mp;
    gfxmenu_t *cmp, *nmp;
    int li;

    for (li = 0; li < LISTSIZE; li++) {
        rp = &gfxnodes[ni].rect[li];
        for (crp = *rp; crp; crp = nrp)
            nrp = crp->next, free (crp);
        *rp = NULL;
        mp = &gfxnodes[ni].menu[li];
        for (cmp = *mp; cmp; cmp = nmp)
            nmp = cmp->next, free (cmp);
        *mp = NULL;
   }
}

static void rectinit (int ni) {
    int li;

    for (li = 0; li < LISTSIZE; li++)
        gfxnodes[ni].rect[li] = NULL;
}

static void rectterm (int ni) {
    gfxrect_t **rp;
    gfxrect_t *crp, *nrp;
    int li;

    for (li = 0; li < LISTSIZE; li++) {
        rp = &gfxnodes[ni].rect[li];
        for (crp = *rp; crp; crp = nrp)
            nrp = crp->next, free (crp);
        *rp = NULL;
   }
}

static void rectinsert (int ni, Tobj ko, Grect_t r) {
    gfxrect_t **rp;
    gfxrect_t *crp;

    rp = &gfxnodes[ni].rect[(unsigned long) ko % LISTSIZE];
    for (crp = *rp; crp; crp = crp->next)
        if (crp->ko == ko) {
            crp->r.o.x = min (r.o.x, r.c.x);
            crp->r.o.y = min (r.o.y, r.c.y);
            crp->r.c.x = max (r.o.x, r.c.x);
            crp->r.c.y = max (r.o.y, r.c.y);
            return;
        }
    if (!(crp = malloc (sizeof (gfxrect_t))))
        panic (POS, "rectinsert", "rect malloc failed");

    crp->ko = ko;
    crp->r.o.x = min (r.o.x, r.c.x);
    crp->r.o.y = min (r.o.y, r.c.y);
    crp->r.c.x = max (r.o.x, r.c.x);
    crp->r.c.y = max (r.o.y, r.c.y);
    crp->next = *rp;
    *rp = crp;
}

static void rectmerge (int ni, Tobj ko, Grect_t r) {
    gfxrect_t **rp;
    gfxrect_t *crp;

    rp = &gfxnodes[ni].rect[(unsigned long) ko % LISTSIZE];
    for (crp = *rp; crp; crp = crp->next)
        if (crp->ko == ko) {
            crp->r.o.x = min (crp->r.o.x, min (r.o.x, r.c.x));
            crp->r.o.y = min (crp->r.o.y, min (r.o.y, r.c.y));
            crp->r.c.x = max (crp->r.c.x, max (r.o.x, r.c.x));
            crp->r.c.y = max (crp->r.c.y, max (r.o.y, r.c.y));
            return;
        }
    if (!(crp = malloc (sizeof (gfxrect_t))))
        panic (POS, "rectmerge", "rect malloc failed");

    crp->ko = ko;
    crp->r.o.x = min (r.o.x, r.c.x);
    crp->r.o.y = min (r.o.y, r.c.y);
    crp->r.c.x = max (r.o.x, r.c.x);
    crp->r.c.y = max (r.o.y, r.c.y);
    crp->next = *rp;
    *rp = crp;
}

static Tobj rectfind (int ni, Gpoint_t p) {
    gfxrect_t **rp;
    gfxrect_t *crp;
    int li;

    for (li = 0; li < LISTSIZE; li++) {
        rp = &gfxnodes[ni].rect[li];
        for (crp = *rp; crp; crp = crp->next)
            if (crp->r.o.x <= p.x && crp->r.c.x >= p.x &&
                    crp->r.o.y <= p.y && crp->r.c.y >= p.y)
                return crp->ko;
    }
    return NULL;
}

static void rectdelete (int ni, Tobj ko) {
    gfxrect_t **rp;
    gfxrect_t *crp, *prp;

    rp = &gfxnodes[ni].rect[(unsigned long) ko % LISTSIZE];
    for (crp = *rp, prp = NULL; crp; prp = crp, crp = crp->next)
        if (crp->ko == ko) {
            if (crp == *rp)
                *rp = crp->next;
            else
                prp->next = crp->next;
            free (crp);
            return;
        }
}

static void rectprune (int ni) {
    gfxrect_t **rp;
    gfxrect_t *crp, *prp;
    int li;

    for (li = 0; li < LISTSIZE; li++) {
        rp = &gfxnodes[ni].rect[li];
        for (crp = *rp, prp = NULL; crp; ) {
            if (!M_AREAOF (crp->ko)) {
                if (crp == *rp)
                    *rp = crp->next, free (crp), crp = *rp;
                else
                    prp->next = crp->next, free (crp), crp = prp->next;
            } else
                prp = crp, crp = crp->next;
        }
    }
}

static void menuinsert (int ni, Tobj ko, long time, long mi) {
    gfxmenu_t **mp;
    gfxmenu_t *cmp;

    mp = &gfxnodes[ni].menu[(unsigned long) ko % LISTSIZE];
    for (cmp = *mp; cmp; cmp = cmp->next)
        if (cmp->ko == ko) {
            cmp->time = time, cmp->mi = mi;
            return;
        }
    if (!(cmp = malloc (sizeof (gfxmenu_t))))
        panic (POS, "menuinsert", "menu malloc failed");

    cmp->ko = ko;
    cmp->time = time;
    cmp->mi = mi;
    cmp->next = *mp;
    *mp = cmp;
}

static long menufind (int ni, Tobj ko, long time) {
    gfxmenu_t **mp;
    gfxmenu_t *cmp;

    mp = &gfxnodes[ni].menu[(unsigned long) ko % LISTSIZE];
    for (cmp = *mp; cmp; cmp = cmp->next)
        if (cmp->ko == ko && cmp->time == time)
            return cmp->mi;
    return -1;
}

static void menuprune (int ni) {
    gfxmenu_t **mp;
    gfxmenu_t *cmp, *pmp;
    int li;

    for (li = 0; li < LISTSIZE; li++) {
        mp = &gfxnodes[ni].menu[li];
        for (cmp = *mp, pmp = NULL; cmp; ) {
            if (!M_AREAOF (cmp->ko)) {
                if (cmp == *mp)
                    *mp = cmp->next, free (cmp), cmp = *mp;
                else
                    pmp->next = cmp->next, free (cmp), cmp = pmp->next;
            } else
                pmp = cmp, cmp = cmp->next;
        }
    }
}
