%{
/*
 * Written by Stephen North.  3/1/93 release.
 * graph parser.
 */

#include	"libgraph.h"

static char		Port[SMALLBUF],*Symbol;
static char		In_decl,In_edge_stmt;
static int		Current_class,Agraph_type;
static Agraph_t		*G;
static Agnode_t		*N;
static Agedge_t		*E;
static objstack_t	*SP;
static Agraph_t		*Gstack[32];
static int			GSP;

static void push_subg(g)
     Agraph_t *g;
{
	G = Gstack[GSP++] = g;
}

static Agraph_t *pop_subg()
{
	Agraph_t		*g;
	if (GSP == 0) {
		fprintf(stderr,"Gstack underflow in graph parser\n"); exit(1);
	}
	g = Gstack[--GSP];
	G = Gstack[GSP - 1];
	return g;
}

static objport_t pop_gobj()
{
	objport_t	rv;
	rv.obj = pop_subg();
	rv.port = NULL;
	return rv;
}

static void begin_graph(name)
     char *name;
{
	Agraph_t		*g;
	g = AG.parsed_g = agopen(name,Agraph_type);
	push_subg(g);
	In_decl = TRUE;
}

static void end_graph()
{
	pop_subg();
}

static Agnode_t *bind_node(name)
     char *name;
{
	Agnode_t	*n = agnode(G,name);
	In_decl = FALSE;
	return n;
}

static void anonsubg()
{
	static int		anon_id = 0;
	char			buf[SMALLBUF];
	Agraph_t			*subg;

	In_decl = FALSE;
	sprintf(buf,"_anonymous_%d",anon_id++);
	subg = agsubg(G,buf);
	push_subg(subg);
}

static int isanonsubg(g)
     Agraph_t *g;
{
	return (strncmp("_anonymous_",g->name,11) == 0);
}

static void begin_edgestmt(objp)
     objport_t objp;
{
	struct objstack_t	*new_sp;

	new_sp = NEW(objstack_t);
	new_sp->link = SP;
	SP = new_sp;
	SP->list = SP->last = NEW(objlist_t);
	SP->list->data  = objp;
	SP->list->link = NULL;
	SP->in_edge_stmt = In_edge_stmt;
	SP->subg = G;
	agpushproto(G);
	In_edge_stmt = TRUE;
}

static void mid_edgestmt(objp)
     objport_t objp;
{
	SP->last->link = NEW(objlist_t);
	SP->last = SP->last->link;
	SP->last->data = objp;
	SP->last->link = NULL;
}

static void end_edgestmt()
{
	objstack_t	*old_SP;
	objlist_t	*tailptr,*headptr,*freeptr;
	Agraph_t		*t_graph,*h_graph;
	Agnode_t	*t_node,*h_node,*t_first,*h_first;
	Agedge_t	*e;
	char		*tport,*hport;

	for (tailptr = SP->list; tailptr->link; tailptr = tailptr->link) {
		headptr = tailptr->link;
		tport = tailptr->data.port;
		hport = headptr->data.port;
		if (TAG_OF(tailptr->data.obj) == TAG_NODE) {
			t_graph = NULL;
			t_first = (Agnode_t*)(tailptr->data.obj);
		}
		else {
			t_graph = (Agraph_t*)(tailptr->data.obj);
			t_first = agfstnode(t_graph);
		}
		if (TAG_OF(headptr->data.obj) == TAG_NODE) {
			h_graph = NULL;
			h_first = (Agnode_t*)(headptr->data.obj);
		}
		else {
			h_graph = (Agraph_t*)(headptr->data.obj);
			h_first = agfstnode(h_graph);
		}

		for (t_node = t_first; t_node; t_node = t_graph ?
		  agnxtnode(t_graph,t_node) : NULL) {
			for (h_node = h_first; h_node; h_node = h_graph ?
			  agnxtnode(h_graph,h_node) : NULL ) {
				e = agedge(G,t_node,h_node);
				if (tport && tport[0]) agxset(e,TAILX,tport);
				if (hport && hport[0]) agxset(e,HEADX,hport);
			}
		}
	}
	tailptr = SP->list; 
	while (tailptr) {
		freeptr = tailptr;
		tailptr = tailptr->link;
		if (TAG_OF(freeptr->data.obj) == TAG_NODE)
		free(freeptr->data.port);
		free(freeptr);
	}
	if (G != SP->subg) abort();
	agpopproto(G);
	In_edge_stmt = SP->in_edge_stmt;
	old_SP = SP;
	SP = SP->link;
	In_decl = FALSE;
	free(old_SP);
	Current_class = TAG_GRAPH;
}

static Agraph_t *parent_of(g)
     Agraph_t *g;
{
	Agraph_t		*rv;
	rv = agusergraph(agfstin(g->meta_node->graph,g->meta_node)->tail);
	return rv;
}

static void attr_set(name, value)
     char *name;
     char *value;
{
	Agsym_t		*ap = NULL;
	char		*defval = "";

	if (In_decl && (G->root == G)) defval = value;
	switch (Current_class) {
		case TAG_NODE:
			ap = agfindattr(G->proto->n,name);
			if (ap == NULL)
				ap = agnodeattr(AG.parsed_g,name,defval);
			agxset(N,ap->index,value);
			break;
		case TAG_EDGE:
			ap = agfindattr(G->proto->e,name);
			if (ap == NULL)
				ap = agedgeattr(AG.parsed_g,name,defval);
			agxset(E,ap->index,value);
			break;
		case 0:		/* default */
		case TAG_GRAPH:
			ap = agfindattr(G,name);
			if (ap == NULL) 
				ap = agraphattr(AG.parsed_g,name,defval);
			agxset(G,ap->index,value);
			break;
	}
}

%}

%union	{
			int					i;
			char				*str;
			struct objport_t	obj;
			struct Agnode_t		*n;
}

%token		<i>	T_graph T_digraph T_subgraph T_strict
%token		<i>	T_node T_edge T_edgeop
%token		<str>	T_symbol
%type		<n>		node_name
%type		<obj>	node_id subg_stmt
%left T_subgraph	/* to avoid subgraph hdr shift/reduce conflict */
%left '{'
%%

file		:	graph_type T_symbol
				{begin_graph($2);}
			'{' stmt_list '}'
				{AG.accepting_state = TRUE; end_graph();}
		|	error
				{
					if (AG.parsed_g)
						agclose(AG.parsed_g);
					AG.parsed_g = NULL;
					/*exit(1);*/
				}
		|	/* empty*/  {AG.parsed_g = NULL;}
		;

	/* it is safe to change graph type and name before contents appear */
graph_type	:	T_graph
				{Agraph_type = AGRAPH; AG.edge_op = "--";}
		|	T_strict T_graph
				{Agraph_type = AGRAPHSTRICT; AG.edge_op = "--";}
		|	T_digraph
				{Agraph_type = AGDIGRAPH; AG.edge_op = "->";}
		|	T_strict T_digraph
				{Agraph_type = AGDIGRAPHSTRICT; AG.edge_op = "->";}
		;

attr_class	:	T_graph 
				{Current_class = TAG_GRAPH;}
		|	T_node
				{Current_class = TAG_NODE; N = G->proto->n;}
		|	T_edge
				{Current_class = TAG_EDGE; E = G->proto->e;}
		;

inside_attr_list :	attr_set optcomma inside_attr_list
		|			/* empty */
		;

optcomma	:	/* empty */ 
		|	','

attr_list	:	'[' inside_attr_list ']'

rec_attr_list	:	rec_attr_list attr_list
		|	/* empty */
		;

opt_attr_list	:	rec_attr_list
		;

attr_set	:	T_symbol {Symbol = strdup($1);} '=' T_symbol 
					{attr_set(Symbol,$4); free(Symbol);}
		;

stmt_list	:	stmt_list1
		|	/* empty */
		;

stmt_list1	:	stmt
		|	stmt_list1 stmt
		;

stmt		:	stmt1
		|	stmt1 ';'
		;

stmt1		:	node_stmt
		|	edge_stmt
		|	attr_stmt 	
		|	subg_stmt
		;

attr_stmt	:	attr_class attr_list
				{Current_class = TAG_GRAPH; /* reset */}
		|	attr_set
				{Current_class = TAG_GRAPH;}
		;

node_id		:	node_name node_port
				{
					objport_t		rv;
					rv.obj = $1;
					rv.port = strdup(Port);
					Port[0] = '\0';
					$$ = rv;
				} 
		;

node_name	:	T_symbol {$$ = bind_node($1);}
		;

node_port	:	/* empty */
		|	port_location 
		|	port_angle 			/* undocumented */
		|	port_angle port_location 	/* undocumented */
		|	port_location port_angle 	/* undocumented */
		;

port_location	:	':' T_symbol {strcat(Port,":"); strcat(Port,$2);}
		|	':' '(' T_symbol {Symbol = strdup($3);} ',' T_symbol ')'
				{	char buf[SMALLBUF];
					sprintf(buf,":(%s,%s)",Symbol,$6);
					strcat(Port,buf); free(Symbol);
				}
		;

port_angle	:	'@' T_symbol
				{	char buf[SMALLBUF];
					sprintf(buf,"@%s",$2);
					strcat(Port,buf);
				}
		;

node_stmt	:	node_id
				{Current_class = TAG_NODE; N = (Agnode_t*)($1.obj);}
			opt_attr_list
				{Current_class = TAG_GRAPH; /* reset */}
		;

edge_stmt	:	node_id
				{begin_edgestmt($1);}
			edgeRHS
				{ E = SP->subg->proto->e;
				  Current_class = TAG_EDGE; }
			opt_attr_list
				{end_edgestmt();}
		|	subg_stmt
				{begin_edgestmt($1);}
			edgeRHS
				{ E = SP->subg->proto->e;
				  Current_class = TAG_EDGE; }
			opt_attr_list
				{end_edgestmt();}
		;

edgeRHS		:	T_edgeop node_id {mid_edgestmt($2);}
		|	T_edgeop node_id
				{mid_edgestmt($2);}
			edgeRHS
		|	T_edgeop subg_stmt
				{mid_edgestmt($2);}
		|	T_edgeop subg_stmt
				{mid_edgestmt($2);}
			edgeRHS
		;


subg_stmt	:	subg_hdr '{' stmt_list '}'%prec '{' {$$ = pop_gobj();}
		|	'{' { anonsubg(); } stmt_list '}' {$$ = pop_gobj();}
		|	subg_hdr %prec T_subgraph {$$ = pop_gobj();}
		;

subg_hdr	:	T_subgraph T_symbol
				{ Agraph_t	 *subg;
				if (subg = agfindsubg(AG.parsed_g,$2))
				aginsert(G,subg);
				else subg = agsubg(G,$2); 
				push_subg(subg);
				In_decl = FALSE;
				}
		;
