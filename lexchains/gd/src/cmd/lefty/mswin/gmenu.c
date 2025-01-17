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

#define WMU widget->u.m

int GMcreatewidget (Gwidget_t *parent, Gwidget_t *widget,
        int attrn, Gwattr_t *attrp) {
    int ai;

    for (ai = 0; ai < attrn; ai++) {
        switch (attrp[ai].id) {
        case G_ATTRUSERDATA:
            widget->udata = attrp[ai].u.u;
            break;
        default:
            Gerr (POS, G_ERRBADATTRID, attrp[ai].id);
            return -1;
        }
    }
    if (!(widget->w = CreatePopupMenu ())) {
        Gerr (POS, G_ERRCANNOTCREATE);
        return -1;
    }
    WMU->count = 0;
    return 0;
}

int GMsetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    int ai;

    for (ai = 0; ai < attrn; ai++) {
        switch (attrp[ai].id) {
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

int GMgetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    int ai;

    for (ai = 0; ai < attrn; ai++) {
        switch (attrp[ai].id) {
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

int GMdestroywidget (Gwidget_t *widget) {
    DestroyWindow (widget->w);
    return 0;
}

int GMmenuaddentries (Gwidget_t *widget, int en, char **ep) {
    int ei;

    for (ei = 0; ei < en; ei++)
        AppendMenu (widget->w, MF_ENABLED | MF_STRING, WMU->count++, ep[ei]);
    return 0;
}

int GMmenudisplay (Gwidget_t *parent, Gwidget_t *widget) {
    MSG msg;
    POINT p;
    UINT flag;

    /* FIXME not only right button */
    menupoped = TRUE;
    menuselected = -1;
    GetCursorPos (&p);
    if (GetAsyncKeyState (VK_LBUTTON) < 0)
        flag = TPM_LEFTALIGN | TPM_LEFTBUTTON;
    else
        flag = TPM_LEFTALIGN | TPM_RIGHTBUTTON;
    TrackPopupMenu (widget->w, flag, p.x, p.y, 0, parent->w, NULL);
    PostMessage (parent->w, WM_COMMAND, 999, 0);
    if (!GetMessage(&msg, parent->w, WM_COMMAND, WM_COMMAND))
        panic (POS, "GMmenudisplay", "exit code in GetMessage");
    TranslateMessage(&msg);
    DispatchMessage(&msg);
    Gpopdownflag = TRUE;
    return menuselected;
}
