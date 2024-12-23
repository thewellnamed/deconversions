/*
 * Copyright (c) AT&T Corp. 1994, 1995.
 * This code is licensed by AT&T Corp.  For the
 * terms and conditions of the license, see
 * http://www.research.att.com/orgs/ssr/book/reuse
 */

/*
 *  graphics code generator
 */

#include	"dot.h"

static graph_t		*G;
static int			N_pages, Page;		/* w.r.t. unrotated coords */
static int			Layer,Nlayers;
static char			**LayerID;
static point		First,Major,Minor;
static point		Pages;
static box			PB;		/* drawable region in device coords */
static pointf		GP;		/* graph page size, in graph coords */
static box			CB;		/* current page box, in graph coords */
static point		PFC;	/* device page box for centering */
static double	    Deffontsize;
static char			*Deffontname;
static char			Layerdelims[] = ":\t ";

emit_reset(g)
Agraph_t	*g;
{
	Agnode_t	*n;

	G = (graph_t *) 0;
	N_pages = Page = 0;
	Layer = Nlayers = 0;
	LayerID = (char **) 0;
	First.x = First.y = 0;
	Major.x = Major.y = 0;
	Minor.x = Minor.y = 0;
	Pages.x = Pages.y = 0;
	PB.LL.x = PB.LL.y = PB.UR.x = PB.UR.y = 0;
	GP.x = GP.y = 0;
	CB.LL.x = CB.LL.y = CB.UR.x = CB.UR.y = 0;
	PFC.x = PFC.y = 0;
	Deffontsize = 0;
	Deffontname = (char *) 0;

	for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
		n->u.state = 0;
	}

	if(CodeGen) CodeGen->reset();
}


emit_graph(g)
graph_t		*g;
{
	point	curpage;
	node_t	*n;
	edge_t	*e;

	emit_header(g);
	Layer = 1;
	do {
		if (Nlayers > 0) emit_layer(Layer);
		for (curpage = First; validpage(curpage); curpage = pageincr(curpage)) {
				setup_page(g,curpage);
				if (g->u.label) emit_label(g->u.label);
			emit_clusters(g);
			for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
				emit_node(n);
				for (e = agfstout(g,n); e; e = agnxtout(g,e)) {
					emit_node(e->head);
					emit_edge(e);
				}
			}
			CodeGen->end_page(curpage.x,curpage.y);
		}
		Layer++;
	} while (Layer <= Nlayers);
	emit_trailer();
}

emit_eof()
{
	if (Page > 0) CodeGen->end_job();
}

emit_clusters(g)
graph_t	*g;
{
	int			i,c,filled;
	graph_t		*subg;
	point		p,A[4];
	char		*str,**style;

	for (c = 1; c <= g->u.n_cluster; c++) {
		subg = g->u.clust[c];
		if (clust_in_layer(subg) == FALSE) continue;
		CodeGen->begin_context();
		filled = FALSE;
		if ((str = agget(subg,"style")) && str[0]) {
			CodeGen->set_style(style = parse_style(str));
			for (i = 0; style[i]; i++) 
				if (strcmp(style[i],"filled")==0) {filled = TRUE; break;}
		}
		if ((str = agget(subg,"color")) && str[0])
			CodeGen->set_color(str);
		A[0] = subg->u.bb.LL;
		A[2] = subg->u.bb.UR;
		A[1].x = A[2].x; A[1].y = A[0].y;
		A[3].x = A[0].x; A[3].y = A[2].y;
		CodeGen->polygon(A,4,filled);
		if (subg->u.label) emit_label(subg->u.label);
		CodeGen->end_context();
		emit_clusters(subg);
	}
}

emit_node(n)
node_t	*n;
{
	char	*comment,*code;

	if (n->u.shape == NULL) return;
	if (node_in_layer(n->graph,n) && node_in_CB(n) && (n->u.state != Page)) {
		n->u.shape->codefn(n,CodeGen);
		n->u.state = Page;

	}
}

emit_edge(e)
edge_t	*e;
{
	int		i;
	char	*color,*style;
	bezier	bz;
	boolean	saved = FALSE;
	double	scale;

	if ((edge_in_CB(e) == FALSE) || (edge_in_layer(e->head->graph,e) == FALSE))
		return;

	CodeGen->begin_edge(e);
	style = late_string(e,E_style,"");
	color = late_string(e,E_color,"");
	scale = late_float(e,E_arrowsz,1.0,0.0);
	if (color[0] || style [0]) {
		CodeGen->begin_context();
		if (style[0]) CodeGen->set_style(parse_style(style));
		if (color[0]) CodeGen->set_color(color);
		saved = TRUE;
	}
	if (e->u.spl) {
		for (i = 0; i < e->u.spl->size; i++) {
			bz = e->u.spl->list[i];
			CodeGen->beziercurve(bz.list,bz.size);
			if (bz.sflag) CodeGen->arrowhead(bz.sp,DEGREES(atan2pt(bz.list[0],bz.sp)),scale);
			if (bz.eflag) CodeGen->arrowhead(bz.ep,DEGREES(atan2pt(bz.list[bz.size-1],bz.ep)),scale);
		}
	}
	if (e->u.label) {
		emit_label(e->u.label);
		if (E_decorate) emit_attachment(e->u.label,e->u.spl);
	}
	if (saved) CodeGen->end_context();
	CodeGen->end_edge(e);
}

node_in_CB(n)
node_t	*n;
{
	box	nb;

	if (N_pages == 1) return TRUE;
	nb.LL.x = n->u.coord.x - n->u.lw;
	nb.LL.y = n->u.coord.y - n->u.ht/2;
	nb.UR.x = n->u.coord.x + n->u.rw;
	nb.UR.y = n->u.coord.y + n->u.ht/2;
	return rect_overlap(CB,nb);
}

node_in_layer(g,n)
graph_t		*g;
node_t		*n;
{
	char	*pn,*pe;
	edge_t	*e;

	if (Nlayers <= 0) return TRUE;
	pn = late_string(n,N_layer,"");
	if (selectedlayer(pn)) return TRUE;
	if (pn[0]) return FALSE;
	if ((e = agfstedge(g,n)) == NULL) return TRUE;
	for (e = agfstedge(g,n); e; e = agnxtedge(g,e,n)) {
		pe = late_string(e,E_layer,"");
		if ((pe[0] == '\0') || selectedlayer(pe)) return TRUE;
	}
	return FALSE;
}

edge_in_layer(g,e)
graph_t		*g;
edge_t		*e;
{
	char	*pe,*pn;
	int		cnt;

	if (Nlayers <= 0) return TRUE;
	pe = late_string(e,E_layer,"");
	if (selectedlayer(pe)) return TRUE;
	if (pe[0]) return FALSE;
	for (cnt = 0; cnt < 2; cnt++) {
		pn = late_string(cnt < 1? e->tail:e->head,N_layer,"");
		if ((pn[0] == '\0') || selectedlayer(pn)) return TRUE;
	}
	return FALSE;
}

clust_in_layer(subg)
graph_t		*subg;
{
	char		*pg;
	node_t		*n;

	if (Nlayers <= 0) return TRUE;
	pg = late_string(subg,agfindattr(subg,"layer"),"");
	if (selectedlayer(pg)) return TRUE;
	if (pg[0]) return FALSE;
	for (n = agfstnode(subg); n; n = agnxtnode(subg,n))
		if (node_in_layer(subg->root,n)) return TRUE;
	return FALSE;
}

edge_in_CB(e)
edge_t	*e;
{
	int		i,j,np;
	bezier	bz;
	point	*p,pp,sz;
	box		b;
	textlabel_t	*lp;

	if (N_pages == 1) return TRUE;
	if (e->u.spl == NULL) return FALSE;
	for (i = 0; i < e->u.spl->size; i++) {
		bz = e->u.spl->list[i];
		np = bz.size;
		p = bz.list;
		pp = p[0];
		for (j = 0; j < np; j++) {
			if (rect_overlap(CB,mkbox(pp,p[j]))) return TRUE;
			pp = p[j];
		}
	}
	if ((lp = e->u.label) == NULL) return FALSE;
	sz = cvt2pt(lp->dimen);
	b.LL.x = lp->p.x - sz.x / 2; b.UR.x = lp->p.x + sz.x / 2;
	b.LL.y = lp->p.y - sz.y / 2; b.UR.y = lp->p.y + sz.y / 2;
	return rect_overlap(CB,b);
}

emit_attachment(lp,spl)
textlabel_t	*lp;
splines		*spl;
{
	point	sz,A[3];
	char	*s;

	for (s = lp->text; *s; s++) if (isspace(*s) == FALSE) break;
	if (*s == 0) return;

	sz = cvt2pt(lp->dimen);
	A[0] = pointof(lp->p.x + sz.x/2,lp->p.y - sz.y/2);
	A[1] = pointof(A[0].x - sz.x, A[0].y);
	A[2] = spline_at_y(spl,lp->p.y);
	CodeGen->polyline(A,3);
}

emit_header(g)
graph_t	*g;
{
	char	*user,buf[SMALLBUF];

	setup_graph(g);
	if (Page == 0) {
		user = username(buf);
		CodeGen->begin_job(Output_file,g,Lib,user,Version,Pages);
	}
	CodeGen->begin_graph(g,PB,PFC);
}

emit_trailer()
{
	CodeGen->end_graph();
}

point pagecode(c)
char	c;
{
	point		rv;
	rv.x = rv.y = 0;
	switch(c) {
		case 'T': First.y = Pages.y - 1; rv.y = -1; break;
		case 'B': rv.y = 1; break;
		case 'L': rv.x = 1; break;
		case 'R': First.x = Pages.x - 1; rv.x = -1; break;
	}
	return rv;
}

static set_pagedir(g)
graph_t		*g;
{
	char	*str;

	Major.x = Major.y = Minor.x = Minor.y = 0;
	str = agget(g,"pagedir");
	if (str && str[0]) {
		Major = pagecode(str[0]);
		Minor = pagecode(str[1]);
	}
	if ((abs(Major.x + Minor.x) != 1) || (abs(Major.y + Minor.y) != 1)) {
		Major.x = 0; Major.y = 1; Minor.x = 1; Minor.y = 0;
		First.x = First.y = 0;
		if (str) fprintf(stderr,"warning, pagedir=%s ignored\n",str);
	}
}

int validpage(page)
point	page;
{
	return ((page.x >= 0) && (page.x < Pages.x) 
		&& (page.y >= 0) && (page.y < Pages.y));
}

int layerindex(tok)
char	*tok;
{
	int		i;

	for (i = 1; i <= Nlayers; i++) 
		if (streq(tok,LayerID[i])) return i;
	return -1;
}

int is_natural_number(str)
char	*str;
{
	while (*str) if (NOT(isdigit(*str++))) return FALSE;
	return TRUE;
}

int layer_index(str,all)
char	*str;
int		all;
{
	int		i;

	if (streq(str,"all")) return all;
	if (is_natural_number(str)) return atoi(str);
	if (LayerID)
		for (i = 1; i <= Nlayers; i++)
			if (streq(str,LayerID[i])) return i;
	return -1;
}

int selectedlayer(spec)
char	*spec;
{
	int		n0,n1;
	char	buf[SMALLBUF], *w0, *w1;

	strcpy(buf,spec);
	w1 = w0 = strtok(buf,Layerdelims);
	if (w0) w1 = strtok(NULL,Layerdelims);
	switch((w0 != NULL) + (w1 != NULL)) {
	case 0: return FALSE;
	case 1: n0 = layer_index(w0,Layer);
		return (n0 == Layer);
	case 2: n0 = layer_index(w0,0);  n1 = layer_index(w1,Nlayers);
		if ((n0 < 0) || (n1 < 0)) return TRUE;
		if (n0 > n1) {int t = n0; n0 = n1; n1 = t;}
		return BETWEEN(n0,Layer,n1);
	}
}

point
pageincr(page)
point		page;
{
	page = add_points(page,Minor);
	if (validpage(page) == FALSE) {
		if (Major.y) page.x = First.x;
		else page.y = First.y;
		page = add_points(page,Major);
	}
	return page;
}

static point exch_xy(p)
point	p;
{
	int		t;
	t = p.x; p.x = p.y; p.y = t;
	return p;
}

static pointf exch_xyf(p)
pointf	p;
{
	double		t;
	t = p.x; p.x = p.y; p.y = t;
	return p;
}

/* this isn't a pretty sight... */
setup_graph(g)
graph_t		*g;
{
	double		xscale,yscale,scale;
	char		*p;
	point		PFCLM;	/* page for centering less margins */
	point		DS;		/* device drawable region for a page of the graph */

	assert((g->u.bb.LL.x == 0) && (g->u.bb.LL.y == 0));

	if (LayerID) free(LayerID);
	if (p = agget(g,"layers")) Nlayers = parse_layers(p);
	else {LayerID = NULL; Nlayers = 0;}

	/* determine final drawing size and scale to apply. */
	/* N.B. magnification could be allowed someday in the next conditional */
	/* N.B. size given by user is not rotated by landscape mode */
	if ((g->u.drawing->size.x > 0)	/* was given by user... */
		&& ((g->u.drawing->size.x < g->u.bb.UR.x) /* drawing is too big... */
			|| (g->u.drawing->size.y < g->u.bb.UR.y))) {
		xscale = ((double)g->u.drawing->size.x) / g->u.bb.UR.x;
		yscale = ((double)g->u.drawing->size.y) / g->u.bb.UR.y;
		scale = MIN(xscale,yscale);
		g->u.drawing->scale = scale;
		g->u.drawing->size.x = scale * g->u.bb.UR.x;
		g->u.drawing->size.y = scale * g->u.bb.UR.y;
	}
	else {	/* go with "natural" size of layout */
		g->u.drawing->size = g->u.bb.UR;
		scale = g->u.drawing->scale = 1.0;
	}

	/* determine pagination */
	PB.LL = g->u.drawing->margin;
	if ((g->u.drawing->page.x > 0) && (g->u.drawing->page.y > 0)) {
			/* page was set by user */
		point	tp;
		PFC = g->u.drawing->page;
		PFCLM.x = PFC.x - 2*PB.LL.x; PFCLM.y = PFC.y - 2*PB.LL.y;
		GP.x = PFCLM.x ; GP.y = PFCLM.y;	/* convert to double */
		if (g->u.drawing->landscape) GP = exch_xyf(GP);
		GP.x = GP.x / scale; GP.y = GP.y / scale;
			/* we don't want graph page to exceed its bounding box */
		GP.x = MIN(GP.x,g->u.bb.UR.x); GP.y = MIN(GP.y,g->u.bb.UR.y);
		Pages.x = (GP.x > 0) ? ceil( ((double)g->u.bb.UR.x) / GP.x) : 1;
		Pages.y = (GP.y > 0) ? ceil( ((double)g->u.bb.UR.y) / GP.y) : 1;
		N_pages = Pages.x * Pages.y;

			/* find the drawable size in device coords */
		tp = g->u.drawing->size;
		if (g->u.drawing->landscape) tp = exch_xy(tp);
		DS.x = MIN(tp.x,PFCLM.x);
		DS.y = MIN(tp.y,PFCLM.y);
	}
	else {
			/* page not set by user, assume default when centering,
				but allow infinite page for any other interpretation */
		GP.x = g->u.bb.UR.x; GP.y = g->u.bb.UR.y;
		PFC.x = DEFAULT_PAGEWD; PFC.y = DEFAULT_PAGEHT;
		PFCLM.x = PFC.x - 2*PB.LL.x; PFCLM.y = PFC.y - 2*PB.LL.y;
		DS = g->u.drawing->size;
		if (g->u.drawing->landscape) DS = exch_xy(DS);
		Pages.x = Pages.y = N_pages = 1;
	}

	set_pagedir(g);
	/* determine page box including centering */
	if (g->u.drawing->centered) {
		point	extra;
		if ((extra.x = PFCLM.x - DS.x) < 0) extra.x = 0;
		if ((extra.y = PFCLM.y - DS.y) < 0) extra.y = 0;
		PB.LL.x += extra.x / 2; PB.LL.y += extra.y / 2;
	}
	PB.UR = add_points(PB.LL,DS);
	Deffontname = late_nnstring(g->proto->n,N_fontname,DEFAULT_FONTNAME);
	Deffontsize = late_float(g->proto->n,N_fontsize,DEFAULT_FONTSIZE,MIN_FONTSIZE);
}

/* even if this makes you cringe, at least it's short */
setup_page(g,page)
graph_t		*g;
point		page;
{
	point		offset;
	int			theta;

	Page++;
	theta = (g->u.drawing->landscape ? 90 : 0);

	/* establish current box in graph coordinates */
	CB.LL.x = page.x  * GP.x;	CB.LL.y = page.y * GP.y;
	CB.UR.x = CB.LL.x + GP.x;	CB.UR.y = CB.LL.y + GP.y;

	/* establish offset to be applied, in graph coordinates */
	if (g->u.drawing->landscape == FALSE) offset = pointof(-CB.LL.x,-CB.LL.y);
	else { offset.x = (page.y + 1) * GP.y; offset.y = -page.x * GP.x; }
	CodeGen->begin_page(page,g->u.drawing->scale,theta,offset);
	emit_defaults(g);
}

emit_label(lp)
textlabel_t		*lp;
{
	int		i, left_x, center_x, right_x, width_x;
	point	ref;
	double	align;
	char	*p;

	width_x = POINTS(lp->dimen.x);
	center_x = lp->p.x;
	left_x = center_x - width_x/2;
	right_x = center_x + width_x/2;
	ref.y = lp->p.y + (POINTS(lp->dimen.y) - lp->fontsize)/2;

	if (lp->nlines < 1) return;
	CodeGen->begin_context();
		CodeGen->set_color(lp->fontcolor);
		CodeGen->set_font(lp->fontname,lp->fontsize);

		for (i = 0; i < lp->nlines; i++) {
			switch(lp->line[i].just) {
				case 'l':	align = 0.0;	ref.x = left_x;		break;
				case 'r':	align = -1.0;	ref.x = right_x;	break;
				default:
				case 'n':	align = -0.5;	ref.x =	center_x;	break;
			}
			CodeGen->textline(ref,lp->line[i].str,lp->line[i].width,
				lp->fontsize,align);
			ref.y = ref.y - lp->fontsize - 2;
		}

	CodeGen->end_context();
}

emit_defaults(g)
graph_t		*g;
{
	CodeGen->set_color(DEFAULT_COLOR);
	CodeGen->set_font(Deffontname,Deffontsize);
}

int parse_layers(p)
char	*p;
{
	int		ntok,c;
	char	*pcopy,*tok;

	ntok = strccnt(p,':')+1;
	pcopy = strdup(p);
	if (LayerID) free(LayerID);
	LayerID = N_NEW(ntok+2,char*);
	c = 1;
	for (tok = strtok(pcopy,Layerdelims); tok; tok = strtok(NULL,Layerdelims)) 
		LayerID[c++] = tok;
	return ntok;
}

emit_layer(n)
int	n;
{
	char	buf[BUFSIZ],*fake[2];
	char	null = 0;

	if (LayerID) {
		/* create compatible char array structure for set_style call */
		sprintf(buf,"setlayer%c%d%c%d%c",null,n,null,Nlayers,null);
		fake[0] = buf;
		fake[1] = NULL;
		CodeGen->set_style(fake);
	}
}

static style_delim(c)
int		c;
{
	switch (c) {
		case '(': case ')': case ',': case '\0': return TRUE;
		default : return FALSE;
	}
}

static char *
style_token(p,out)
char	*p,*out;
{
	while (*p && (isspace(*p) || (*p ==','))) p++;
	switch (*p) {
		case '\0': return NULL;
		case '(': case ')': *out++ = *p++; break;
		default: while (style_delim(*p) == FALSE) *out++ = *p++;
	}
	*out = '\0';
	return p;
}

char **
parse_style(s)
char	*s;
{
	static char	*parse[64];
	static char	outbuf[SMALLBUF];

	int			fun = 0;
	boolean		in_parens = FALSE;
	char		buf[SMALLBUF],*p,*q,*out;


	p = s;
	out = outbuf;
	while (p = style_token(p,buf)) {
		switch (buf[0]) {
		case '(':
			if (in_parens) {
				fprintf(stderr,"nesting not allowed in style: %s\n",s);
				parse[0] = (char*)0;
				return parse;
			}
			in_parens = TRUE;
			break;

		case ')':
			if (in_parens == FALSE) {
				fprintf(stderr,"unmatched ')' in style: %s\n",s);
				parse[0] = (char*)0;
				return parse;
			}
			in_parens = FALSE; 
			break;

		default:
			if (in_parens == FALSE) {
				*out++ = '\0';	/* terminate previous */
				parse[fun++] = out;
			}
			q = buf;
			while (*out++ = *q++);
		}
	}
	*out++ = '\0';

	if (in_parens) {
		fprintf(stderr,"unmatched '(' in style: %s\n",s);
		parse[0] = (char*)0;
		return parse;
	}
	parse[fun] = (char*)0;
	return parse;
}
