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

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#define OUT_OF_MEMORY do { fputs("OUT_OF_MEMORY.\n",stderr); exit(EXIT_FAILURE);} while(0)


typedef struct target_t
	{
	size_t id;
	char* name;
	struct target_t** children;
	size_t n_children;
	int must_remake;
	}Target,*TargetPtr;


typedef struct make2graph_t
	{
	TargetPtr* targets;
	size_t target_count;
	TargetPtr root;
	size_t id_generator;
	}Graph,*GraphPtr;

static int TargetCmp(const void * a, const void * b)
	{
	return strcmp((*(TargetPtr*)a)->name,(*(TargetPtr*)b)->name);
	}

static TargetPtr TargetNew(GraphPtr graph,const char* name)
	{
	TargetPtr target=(TargetPtr)calloc(1,sizeof(Target));
	if(target==NULL) OUT_OF_MEMORY;
	target->id=(++graph->id_generator);
	target->name=strdup(name);
	if(target->name==NULL) OUT_OF_MEMORY;
	return target;
	}

static void TargetAddChildren(TargetPtr root, TargetPtr c)
	{
	TargetPtr* t=(TargetPtr*)bsearch((const void*)&c, (void*)root->children, root->n_children, sizeof(TargetPtr),TargetCmp);
        if(t!=0) return;
	root->children=realloc(root->children,sizeof(TargetPtr)*(root->n_children+1));
	root->children[root->n_children++]=c;
	qsort(root->children, root->n_children ,sizeof(TargetPtr) , TargetCmp);
	}


static int startsWith(const char* str,const char* pre)
	{
    	size_t lenpre = strlen(pre), lenstr = strlen(str);
   	return lenstr < lenpre ? 0 : strncmp(pre, str, lenpre) == 0;
	}

static char* targetName(const char* line)
	{
	char* p;
	char* b=strchr(line,'`');
      	char* e=strchr(line,'\'');
      	if(b==NULL  || e==NULL || b>e)
      		{
      		fprintf(stderr,"Cannot get target name in \"%s\".\n",line);
      		exit(EXIT_FAILURE);
      		}
        p= strndup(b+1,(e-b)-1);
	if(p==NULL) OUT_OF_MEMORY;
	return p;
	}
	
	
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

static void GraphScan(GraphPtr graph,TargetPtr root,FILE* in)
	{
	char* line=NULL;

	while((line=readline(in))!=NULL)
		{
		 if(startsWith(line,"Considering target file"))
		        {
		        char* tName=targetName(line);

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
		       
		free(line);
		}
	}

void DumpGraphAsDot(GraphPtr g,FILE* out)
	{
	size_t i=0,j=0;
	
	fputs("digraph G {\n",out);


	for(i=0; i< g->target_count; ++i)
		{

		TargetPtr t= g->targets[i];
		
		
		fprintf(out,
			"n%d[label=\"%s\", color=\"%s\"];\n",
			t->id,
			t->name,
			(t->must_remake?"red":"green")
			);

		}
	for(i=0; i< g->target_count; ++i)
		{
		TargetPtr t= g->targets[i];
		for(j=0; j< t->n_children; ++j)
			{
			TargetPtr c = t->children[j];
			fprintf(out,"n%d -> n%d ; \n", c->id , t->id);
			}
		}
	fputs("}\n",out);
	}




void DumpGraphAsGexf(GraphPtr g,FILE* out)
	{
	size_t i=0,j=0,k=0UL;
	fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n",out);
	fputs("<gexf xmlns=\"http://www.gexf.net/1.2draft\" version=\"1.2\">\n",out);
	fputs("  <meta>\n",out);
	fputs("    <creator>Pierre Lindenbaum</creator>\n",out);
	fputs("    <description>Makefile Graph</description>\n",out);
	fputs("  </meta>\n",out);
	fputs("  <graph mode=\"static\" defaultedgetype=\"directed\">\n",out);
	fputs("    <attributes class=\"node\" mode=\"static\"/>\n",out);
	fputs("    <nodes>\n",out);
	for(i=0; i< g->target_count; ++i)
		{
		j=0UL;
		TargetPtr t= g->targets[i];
		
		
		fprintf(out,
			"      <node id=\"n%d\" label=\"", t->id);
		while(t->name[j]!=0)
			{
			switch(t->name[j])
				{
				case '<': fputs("&lt;",out);break;
				case '>': fputs("&gt;",out);break;
				case '\"': fputs("&apos;",out);break;
				case '\'': fputs("&quot;",out);break;
				case '&': fputs("&amp;",out);break;
				default: fputc(t->name[j],out);break;
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
		for(j=0; j< t->n_children; ++j)
			{
			TargetPtr c = t->children[j];
			fprintf(out,
				"      <edge id=\"E%d\" type=\"directed\" source=\"n%d\" target=\"n%d\"/>\n",
				++k,c->id , t->id);
			}
		}
	fputs("    </edges>\n",out);
	fputs("  </graph>\n",out);
	fputs("</gexf>\n",out);
	fflush(out);
	}

static void usage(FILE* out)
	{
	fputs(
		"Options:\n"
		" -h help (this screen)\n",out);
	fputs(" -x XML output (gexf)\n",out);
	fputs("\n",out);
	}

int main(int argc,char** argv)
	{
	int gexf_output=0;
	int c;
	GraphPtr app=NULL;

	while ((c = getopt (argc, argv, "xh")) != -1)
	 {
         switch (c)
           	{
           	case 'h': usage(stderr);return EXIT_SUCCESS;
           	case 'x': gexf_output=1;break;
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
