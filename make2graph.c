/* The MIT License

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

   contact: Pierre Lindenbaum PhD @yokofakun

History:
   * 2014 first commit
   * Sept 2014: fixed new format for GNU-Make v4. ( https://github.com/lindenb/makefile2graph/issues/1 )
   * Sept 2014: added long_opt , options basename and suffix
   * Nov  2014: added option to hide ROOT node

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

/* version */
#define M2G_VERSION "1.3"

#define OUT_OF_MEMORY do { fprintf(stderr,"%s: %d : OUT_OF_MEMORY.\n",__FILE__,__LINE__); exit(EXIT_FAILURE);} while(0)

/** a Target */
typedef struct target_t
	{
	/* target id */
	size_t id;
	/* filename */
	char* name;
	/* associated children, sorted by name */
	struct target_t** children;
	/* number of children */
	size_t n_children;
	/* target is dirty */
	int must_remake;
	}Target,*TargetPtr;

/** the Makefile graph */
typedef struct make2graph_t
	{
	/** all the targets , sorted by name */
	TargetPtr* targets;
	/** number of target */
	size_t target_count;
	/** root target */
	TargetPtr root;
	/** target id-generator */
	size_t id_generator;
	/** flag print only basename */
	int print_basename_only;
	/** flag print only extension */
	int print_suffix_only;
	/** show <root> node https://github.com/lindenb/makefile2graph/issues/3 */
	int show_root;
	}Graph,*GraphPtr;


/** compare target by name */
static int TargetCmp(const void * a, const void * b)
	{
	return strcmp((*(TargetPtr*)a)->name,(*(TargetPtr*)b)->name);
	}

/** creates a new target */
static TargetPtr TargetNew(GraphPtr graph,const char* name)
	{
	TargetPtr target=(TargetPtr)calloc(1,sizeof(Target));
	if(target==NULL) OUT_OF_MEMORY;
	target->id=(++graph->id_generator);
	target->name=strdup(name);
	if(target->name==NULL) OUT_OF_MEMORY;
	return target;
	}

/** add a children to the specified target */
static void TargetAddChildren(TargetPtr root, TargetPtr c)
	{
	TargetPtr* t=(TargetPtr*)bsearch((const void*)&c, (void*)root->children, root->n_children, sizeof(TargetPtr),TargetCmp);
        if(t!=0) return;
	root->children=realloc(root->children,sizeof(TargetPtr)*(root->n_children+1));
	if(root->children==NULL)  OUT_OF_MEMORY;
	root->children[root->n_children++]=c;
	qsort(root->children, root->n_children ,sizeof(TargetPtr) , TargetCmp);
	}

/** does string starts with substring */
static int startsWith(const char* str,const char* pre)
	{
    	size_t lenpre = strlen(pre), lenstr = strlen(str);
   	return lenstr < lenpre ? 0 : strncmp(pre, str, lenpre) == 0;
	}

/** extract filename between '`' and "'" 
 * Make v4.0 changed this: the first separator is now "'"
 */
static char* targetName(const char* line)
	{
	char* p;
	char* b=strchr(line,'`');
	if(b==NULL) b=strchr(line,'\'');//GNU make 4.0
      	char* e=( b==NULL ? NULL : strchr(b+1,'\'') );
      	if(b==NULL  || e==NULL || b>e)
      		{
      		fprintf(stderr,"Cannot get target name in \"%s\".\n",line);
      		exit(EXIT_FAILURE);
      		}
        p= strndup(b+1,(e-b)-1);
	if(p==NULL) OUT_OF_MEMORY;
	return p;
	}

/** get a label for this target name */
static char* targetLabel(GraphPtr g,const char* s)
	{
	char* p=(char*)s;
	if( g->print_basename_only )
		{
		char* slash=strrchr(p,'/');
		if(slash!=NULL) p=slash+1;
		}
	if( g->print_suffix_only )
		{
		char* slash=strrchr(p,'.');
		if(slash!=NULL) p=slash+1;
		}
	return p;
	}

/** read line character by character */
static char* readline(FILE* in)
	{
	int c;
	char* p=NULL;
	size_t len=0UL;
	if(feof(in)) return NULL;
	p=(char*)malloc(sizeof(char));
	if(p==NULL) OUT_OF_MEMORY;
	while((c=fgetc(in))!=-1 && c!='\n')
		{
		p=(char*)realloc((char*)p,sizeof(char)*(len+2));
		if(p==NULL) OUT_OF_MEMORY;
		if(!(len==0 && isspace(c))) /* trim on the fly */
			{
			p[len++]=c;
			}
		}
	p[len]=0;
	return p;
	}
 
 /** get target, create it it doesn't exist */
static TargetPtr GraphGetTarget(GraphPtr graph,const char* name)
        {
        TargetPtr t;
	Target key;
	TargetPtr pkey=&key;
	key.name=(char*)name;
	TargetPtr* prev=(TargetPtr*)bsearch(
		(const void*) &pkey,
		(void*)graph->targets,
		graph->target_count,
		sizeof(TargetPtr),
		TargetCmp
		);
	
        if(prev!=0)
		{
		return *prev;
	    	}

        t=TargetNew(graph,name);
	graph->targets = (TargetPtr*)realloc(
		(void*)graph->targets,
		sizeof(TargetPtr)*(1+graph->target_count)
		);
	if(graph->targets==NULL) OUT_OF_MEMORY;
	graph->targets[ graph->target_count ] = t;
	graph->target_count++;
	qsort(graph->targets, graph->target_count ,sizeof(TargetPtr) , TargetCmp);

	return t;
        }

/** scan the makefile -nd output */
static void GraphScan(GraphPtr graph,TargetPtr root,FILE* in)
	{
	char* line=NULL;
	char* makefile_name=NULL;
	while((line=readline(in))!=NULL)
		{
		 if(startsWith(line,"Considering target file"))
		        {
		        char* tName=targetName(line);
		        if(!graph->show_root && 
		           makefile_name!=NULL &&
		           strcmp(tName,makefile_name)==0)
		        	{
		        	free(tName);
		        	free(line);
		        	//skip lines
		        	while((line=readline(in))!=NULL)
		        		{
		        		if(startsWith(line,"Finished prerequisites of target file "))
						{
						tName=targetName(line);
						free(line);
						if(strcmp(tName,makefile_name)==0)
							{
							free(tName);
							break;
							}
						free(tName);
						continue;
						}
					free(line);
					}
		        	continue;
		        	} 

		        TargetPtr child=GraphGetTarget(graph,tName);

		        free(tName);

		        TargetAddChildren(root,child);
		        GraphScan(graph,child,in);

		        }
		    else if(startsWith(line,"Must remake target "))
			     {
			     char* tName=targetName(line);
			     GraphGetTarget(graph,tName)->must_remake=1;
			     free(tName);
			     }
		    else if(startsWith(line,"Pruning file "))
			     {
			     char* tName=targetName(line);
			     TargetAddChildren(root,GraphGetTarget(graph,tName));
			     free(tName);
			     }
		    else if(startsWith(line,"Finished prerequisites of target file "))
			{
			char* tName=targetName(line);
			if(strcmp(tName,root->name)!=0)
				 {
				 fprintf(stderr,"expected %s got %s\n", root->name , line);
				 exit(EXIT_FAILURE);
				 }
			free(tName);
			free(line);
			break;
			}
		    else if(startsWith(line,"Reading makefile "))
			{
			free(makefile_name);
			makefile_name=targetName(line);
			}
		       
		free(line);
		}
	free(makefile_name);
	}

/** export a graphiz dot */
static void DumpGraphAsDot(GraphPtr g,FILE* out)
	{
	size_t i=0,j=0;
	
	fputs("digraph G {\n",out);


	for(i=0; i< g->target_count; ++i)
		{

		TargetPtr t= g->targets[i];
		if( !g->show_root && t==g->root ) continue;
		
		if(t==g->root)
			{
			fprintf(out,
				"n%zu[shape=point, label=\"\"];\n",
				t->id
				);
			}
		else
			{
			fprintf(out,
				"n%zu[label=\"%s\", color=\"%s\"];\n",
				t->id,
				targetLabel(g,t->name),
				(t->must_remake?"red":"green")
				);
			}

		}
	for(i=0; i< g->target_count; ++i)
		{
		TargetPtr t= g->targets[i];
		if( !g->show_root && t==g->root) continue;
		
		for(j=0; j< t->n_children; ++j)
			{
			TargetPtr c = t->children[j];
			fprintf(out,"n%zu -> n%zu ; \n", c->id , t->id);
			}
		}
	fputs("}\n",out);
	}



/** export a Gephi / Gexf */
void DumpGraphAsGexf(GraphPtr g,FILE* out)
	{
	size_t i=0,j=0,k=0UL;
	fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n",out);
	fputs("<gexf xmlns=\"http://www.gexf.net/1.2draft\" version=\"1.2\">\n",out);
	fputs("  <meta>\n",out);
	fputs("    <creator>https://github.com/lindenb/makefile2graph version:" M2G_VERSION "</creator>\n",out);
	fputs("    <description>Creates a graph from a Makefile</description>\n",out);
	fputs("  </meta>\n",out);
	fputs("  <graph mode=\"static\" defaultedgetype=\"directed\">\n",out);
	fputs("    <attributes class=\"node\" mode=\"static\"/>\n",out);
	fputs("    <nodes>\n",out);
	for(i=0; i< g->target_count; ++i)
		{
		j=0UL;
		TargetPtr t= g->targets[i];
		const char* label=targetLabel(g,t->name);
		if( !g->show_root && t==g->root ) continue;
		
		fprintf(out,
			"      <node id=\"n%zu\" label=\"", t->id);
		while(label[j]!=0)
			{
			switch(t->name[j])
				{
				case '<': fputs("&lt;",out);break;
				case '>': fputs("&gt;",out);break;
				case '\"': fputs("&apos;",out);break;
				case '\'': fputs("&quot;",out);break;
				case '&': fputs("&amp;",out);break;
				default: fputc(label[j],out);break;
				}
			++j;
			}
		fputs("\"/>\n",out);
		}
	fputs("    </nodes>\n",out);
	fputs("    <edges>\n",out);
	for(i=0; i< g->target_count; ++i)
		{
		TargetPtr t= g->targets[i];
		if( !g->show_root && t==g->root ) continue;
		
		for(j=0; j< t->n_children; ++j)
			{
			TargetPtr c = t->children[j];
			fprintf(out,
				"      <edge id=\"E%zu\" type=\"directed\" source=\"n%zu\" target=\"n%zu\"/>\n",
				++k,c->id , t->id);
			}
		}
	fputs("    </edges>\n",out);
	fputs("  </graph>\n",out);
	fputs("</gexf>\n",out);
	fflush(out);
	}

/** print usage */
static void usage(FILE* out)
	{
	fprintf(out,"Compilation:\n\t%s %s\n",__DATE__,__TIME__);
	fprintf(out,"Version:\n\t%s\n",M2G_VERSION);
	fputs("Author:\n\tPierre Lindenbaum PhD @yokofakun\n",out);
	fputs("WWW:\n\thttps://github.com/lindenb/makefile2graph\n",out);
	fputs("Usage:\n\tmake -Bnd | make2graph\n",out);
	fputs(
		"Options:\n"
		"\t-h|--help help (this screen)\n",out);
	fputs("\t-x|--xml|--gexf XML output (gexf)\n",out);
	fputs("\t-b|--basename only print file basename.\n",out);
	fputs("\t-s|--suffix only print file extension.\n",out);
	fputs("\t-r|--root show <ROOT> node.\n",out);
	fputs("\t-v|--version print version.\n",out);
	fputs("\n",out);
	}

int main(int argc,char** argv)
	{
	int gexf_output=0;
	int print_basename_only=0;
	int print_suffix_only=0;
	int show_root=0;
	GraphPtr app=NULL;
	for(;;)
		{
		static struct option long_options[] =
		     {
			{"xml",    no_argument ,0, 'x'},
			{"gexf",    no_argument ,0, 'x'},
			{"help",   no_argument, 0, 'h'},
			{"basename",   no_argument, 0, 'b'},
			{"suffix",   no_argument, 0, 's'},
			{"root",   no_argument, 0, 'r'},
			{"version",   no_argument, 0, 'v'},
		       {0, 0, 0, 0}
		     };
		int option_index = 0;
		int c = getopt_long (argc, argv, "xhbsrv",
		                    long_options, &option_index);
		if (c == -1) break;
		switch (c)
		   	{
			case 'v': printf("%s\n",M2G_VERSION); return EXIT_SUCCESS;
		   	case 'h': usage(stdout); return EXIT_SUCCESS;
		   	case 'x': gexf_output=1; break;
			case 'b': print_basename_only=1; break;
			case 's': print_suffix_only=1; break;
			case 'r': show_root=1; break;
			case '?':
		       		fprintf (stderr, "Unknown option `-%c'.\n", optopt);
		       		return EXIT_FAILURE;
	   	        default:
	   	        	fprintf (stderr, "Bad input.\n");
		       		return EXIT_FAILURE;
			}
		  
		}
	
	
	app=(GraphPtr)calloc(1,sizeof(Graph));
	
	if(app==NULL)
		{
		fputs("OUT OF MEMORY\n",stderr);
		exit(EXIT_FAILURE);
		}
	
	app-> print_basename_only =  print_basename_only ;
	app-> print_suffix_only =  print_suffix_only ;
	app-> show_root = show_root;

	app->root=GraphGetTarget(app,"<ROOT>");
	if(optind==argc)
		{
		GraphScan(app,app->root,stdin);
		}
	else if(optind+1==argc)
		{
		FILE* in=fopen(argv[optind],"r");
		if(in==NULL)
			{
			fprintf(stderr,"Cannot open \"%s\" : \"%s\".\n",argv[optind],strerror(errno));
			return EXIT_FAILURE;
			}
		GraphScan(app,app->root,in);
		fclose(in);
		}
	else
		{
		fprintf(stderr,"Illegal number of arguments.\n");
		return EXIT_FAILURE;
		}
	if(gexf_output)
		{
		DumpGraphAsGexf(app,stdout);
		}
	else
		{
		DumpGraphAsDot(app,stdout);
		}
	free(app);
	
	return EXIT_SUCCESS;
	}
