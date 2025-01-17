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

#define WVU widget->u.v

int GVcreatewidget (Gwidget_t *parent, Gwidget_t *widget,
        int attrn, Gwattr_t *attrp) {
    PIXpoint_t po;
    PIXsize_t ps;
    XSizeHints hints;
    char *s;
    int haveorigin, ai;

    WVU->func = NULL;
    WVU->closing = FALSE;
    s = "LEFTY";
    ps.x = ps.y = MINVWSIZE;
    haveorigin = FALSE;
    RESETARGS;
    for (ai = 0; ai < attrn; ai++) {
        switch (attrp[ai].id) {
        case G_ATTRORIGIN:
            haveorigin = TRUE;
            GETORIGIN (attrp[ai].u.p, po);
            ADD2ARGS (XtNx, po.x);
            ADD2ARGS (XtNy, po.y);
            break;
        case G_ATTRSIZE:
            GETSIZE (attrp[ai].u.s, ps, MINVWSIZE);
            break;
        case G_ATTRNAME:
            s = attrp[ai].u.t;
            break;
        case G_ATTRZORDER:
            Gerr (POS, G_ERRCANNOTSETATTR1, "zorder");
            return -1;
        case G_ATTREVENTCB:
            WVU->func = attrp[ai].u.func;
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
    if (!(widget->w = XtAppCreateShell (s, "LEFTY",
            topLevelShellWidgetClass, Gdisplay, argp, argn))) {
        Gerr (POS, G_ERRCANNOTCREATE);
        return -1;
    }
    if (haveorigin) {
        hints.x = po.x, hints.y = po.y;
        hints.width = ps.x, hints.height = ps.y;
        hints.flags = USPosition;
        Glazyrealize (widget->w, TRUE, &hints);
    } else
        Glazyrealize (widget->w, FALSE, NULL);
    return 0;
}

int GVsetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    PIXpoint_t po;
    PIXsize_t ps;
    int ai;

    RESETARGS;
    for (ai = 0; ai < attrn; ai++) {
        switch (attrp[ai].id) {
        case G_ATTRORIGIN:
            GETORIGIN (attrp[ai].u.p, po);
            ADD2ARGS (XtNx, po.x);
            ADD2ARGS (XtNy, po.y);
            break;
        case G_ATTRSIZE:
            GETSIZE (attrp[ai].u.s, ps, MINVWSIZE);
            ADD2ARGS (XtNwidth, ps.x);
            ADD2ARGS (XtNheight, ps.y);
            break;
        case G_ATTRNAME:
            Gerr (POS, G_ERRCANNOTSETATTR2, "name");
            return -1;
        case G_ATTRZORDER:
            Gflushlazyq ();
            if (Strcmp (attrp[ai].u.t, "top") == 0)
                XRaiseWindow (Gdisplay, XtWindow (widget->w));
            else if (Strcmp (attrp[ai].u.t, "bottom") == 0)
                XLowerWindow (Gdisplay, XtWindow (widget->w));
            else {
                Gerr (POS, G_ERRBADATTRVALUE, attrp[ai].u.t);
                return -1;
            }
            break;
        case G_ATTREVENTCB:
            WVU->func = attrp[ai].u.func;
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

int GVgetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    Position x, y;
    Dimension width, height;
    int ai;

    for (ai = 0; ai < attrn; ai++) {
        RESETARGS;
        switch (attrp[ai].id) {
        case G_ATTRORIGIN:
            ADD2ARGS (XtNx, &x);
            ADD2ARGS (XtNy, &y);
            XtGetValues (widget->w, argp, argn);
            attrp[ai].u.p.x = x, attrp[ai].u.p.y = y;
            break;
        case G_ATTRSIZE:
            ADD2ARGS (XtNwidth, &width);
            ADD2ARGS (XtNheight, &height);
            XtGetValues (widget->w, argp, argn);
            attrp[ai].u.s.x = width, attrp[ai].u.s.y = height;
            break;
        case G_ATTRNAME:
            Gerr (POS, G_ERRNOTIMPLEMENTED);
            return -1;
        case G_ATTRZORDER:
            Gerr (POS, G_ERRNOTIMPLEMENTED);
            return -1;
        case G_ATTREVENTCB:
            attrp[ai].u.func = WVU->func;
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

int GVdestroywidget (Gwidget_t *widget) {
    WVU->closing = TRUE;
    XtDestroyWidget (widget->w);
    return 0;
}

void Gwmdelaction (Widget w, XEvent *evp, char **app, unsigned int *anp) {
    Gwidget_t *widget;
    Gevent_t gev;

    widget = findwidget ((unsigned long) w, G_VIEWWIDGET);
    if (!widget)
        exit (0);
    gev.type = 0, gev.code = 0, gev.data = 0;
    gev.wi = widget - &Gwidgets[0];
    if (WVU->func)
        (*WVU->func) (&gev);
    else
        exit (0);
}
