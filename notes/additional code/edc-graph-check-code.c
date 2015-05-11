#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils.h"

#define MAX_BUF 32
#define V_KEYWORD "vertices"
#define E_KEYWORD "edge"
#define C_KEYWORD "code"

#define PARSE_GRAPH_NEW        1
#define PARSE_GRAPH_KEY        2
#define PARSE_GRAPH_VERTEX     4
#define PARSE_GRAPH_EDGE       8
#define PARSE_GRAPH_WEIGHT    16
#define PARSE_GRAPH_CODE      32
#define PARSE_GRAPH_OTHER     64
#define PARSE_GRAPH_COMPLETE 128

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

#define    REMOVE(NODE)   (n[((NODE)/n_blen)] &= ~(((long long int)1)<<((NODE)%n_blen)))
#define IS_REMOVED(NODE) (((*(n+(((long long int)NODE)/(long long int)n_blen))) & (((long long int)1)<<(((long long int)NODE)%(long long int)n_blen))) == 0)

#define INIT_MASK 0xff


extern	char *__progname;

struct adj_list_node {
	long long int from;
	long long int to;
	double weight;
	struct adj_list_node *from_next;
	struct adj_list_node *to_next;
};

long long int *code;
long long int code_len;
long long int *n;
long long int vertices;
long long int initial_number_edges;
long long int n_len;
int c_len;
int n_blen;
double initial_weight;
struct adj_list_node **adj_from;
struct adj_list_node **adj_to;


static int           parse_graph(void);
static void          print_adj_matrix(void);
static void          print_code(void);
static void          usage(void);
static void          add_adj_list_entry(struct adj_list_node *new);
static void          del_adj_list_entry(long long int vertex);


/*
 * Calculate redundant check symbols from an error graph with a ranking
 * heuristic.
 */
int
main(int argc, char **argv)
{
	long long int i, j;
	int remove;

	vertices       = 0;
	initial_weight = 0;
	initial_number_edges = 0;
	adj_from       = NULL;
	adj_to         = NULL;

	if (argc > 1)
		usage();

	if (!parse_graph())
	{
		fprintf(stderr, "Parsing error: No graph found.\n");
		exit(EXIT_FAILURE);
	}

	print_adj_matrix();

	for (i=0; i<vertices; i++)
	{
		remove = 1;
		for (j=0; j<code_len; j++)
			if (code[j] == i)
				remove = 0;
		if (remove)
		{
			REMOVE(i);
			del_adj_list_entry(i);
		}
	}

	print_code();

	return 0;
}

int
parse_graph()
{
	struct adj_list_node *new;
	long long int i;
	long long int edge[2];
	long long int max_cw;
	double weight;
	char buf[MAX_BUF];
	int  buf_cnt;
	int  c;
	int  state;
	int  data_width;

	/* initialize state and start parsing */
	state = PARSE_GRAPH_NEW;
	i = 0;
	buf_cnt = 0;
	adj_from = NULL;
	adj_to = NULL;
	new = NULL;
	code = NULL;
	code_len = 0;
	vertices = 0;
	max_cw = -1;
	data_width = 1;
	initial_weight = 0;
	initial_number_edges = 0;
	while ((c = getchar()) != EOF)
	{
		switch (state) {
		case PARSE_GRAPH_NEW:
			if (isspace(c))
				continue;

			if (isalpha(c))
			{
				state   = PARSE_GRAPH_KEY;
				buf[0]  = tolower(c);
				buf_cnt = 1;
			}
			else
				state     = PARSE_GRAPH_OTHER;
			break;
		case PARSE_GRAPH_KEY:
			if (c == '\n')
				state = PARSE_GRAPH_NEW;

			else if (isalpha(c))
			{
				if (buf_cnt < MAX_BUF)
					buf[buf_cnt++] = tolower(c);
				else
				{
					fprintf(stderr, "Parsing error: buffer to small.\n");
					exit(EXIT_FAILURE);
				}
			}

			else if (isspace(c) || ispunct(c))
			{
				if (buf_cnt == strlen(V_KEYWORD))
				{
					if (strncmp(buf, V_KEYWORD, strlen(V_KEYWORD)) == 0)
					{
						if (vertices == 0)
							state = PARSE_GRAPH_VERTEX;
						else
						{
							fprintf(stderr, "Parsing error: %s already parsed.\n", V_KEYWORD);
							exit(EXIT_FAILURE);
						}
					}
					else
						state = PARSE_GRAPH_OTHER;
				}
				else if (buf_cnt == strlen(E_KEYWORD))
				{
					if (strncmp(buf, E_KEYWORD, strlen(E_KEYWORD)) == 0)
					{
						if (vertices > 0)
							state = PARSE_GRAPH_EDGE;
						else
						{
							fprintf(stderr, "Parsing error: no %s keyword before %s parsed.\n", V_KEYWORD, E_KEYWORD);
							exit(EXIT_FAILURE);
						}
					}
					else if (strncmp(buf, C_KEYWORD, strlen(C_KEYWORD)) == 0)
					{
						state = PARSE_GRAPH_CODE;
					}
					else
						state = PARSE_GRAPH_OTHER;
				}
				else
					state = PARSE_GRAPH_OTHER;
				buf_cnt = 0;
			}
			break;
		case PARSE_GRAPH_VERTEX:
			if (isdigit(c))
			{
				if (buf_cnt < MAX_BUF)
					buf[buf_cnt++] = c;
				else
				{
					fprintf(stderr, "Parsing error: buffer to small.\n");
					exit(EXIT_FAILURE);
				}
			}
			else if (isspace(c))
			{
				/* parse number of vertices */
				if (buf_cnt == 0)
					break;

				buf[buf_cnt] = 0;
				sscanf(buf, "%lld", &vertices);
				buf_cnt = 0;
				if (hamming_weight(vertices, sizeof(long long int)*8) != 1)
				{
					fprintf(stderr, "Parsing error: number of vertices isn't a power of 2.\n");
					exit(EXIT_FAILURE);
				}
				c_len = 0;
				i = 0;
				while ((vertices & (((long long int)1)<<c_len)) == 0)
					c_len++;
				adj_from = malloc(vertices * sizeof(struct adj_list_node));
				adj_to   = malloc(vertices * sizeof(struct adj_list_node));
				if ((adj_from == NULL) || (adj_to == NULL))
				{
					fprintf(stderr, "Parsing error: Out of memory for %lld vertices.\n", vertices);
					exit(EXIT_FAILURE);
				}
				memset(adj_from, 0, vertices * sizeof(struct adj_list_node *));
				memset(adj_to,   0, vertices * sizeof(struct adj_list_node *));
				i = 0;

				if (c == '\n')
				{
					state = PARSE_GRAPH_NEW;
				}
				else
				{
					fprintf(stderr, "Parsing error: number of vertices.\n");
					exit(EXIT_FAILURE);
				}
			}
			break;
		case PARSE_GRAPH_EDGE:
			if (isdigit(c))
			{
				if (buf_cnt < MAX_BUF)
					buf[buf_cnt++] = c;
				else
				{
					fprintf(stderr, "Parsing error: buffer to small.\n");
					exit(EXIT_FAILURE);
				}
			}
			else if (isspace(c) || ispunct(c))
			{
				if (buf_cnt == 0)
					break;
				if (c == '\n')
				{
					fprintf(stderr, "Parsing error: edge isn't correct.\n");
					exit(EXIT_FAILURE);
				}

				/* parse the vertices of the edge */
				if ((i < 0) || (i > 1))
				{
					fprintf(stderr, "Parsing error: edge isn't correct.\n");
					exit(EXIT_FAILURE);
				}
				buf[buf_cnt] = 0;
				sscanf(buf, "%lld", &(edge[i]));
				if ((edge[i] >= vertices) || (edge[i] < 0))
				{
					fprintf(stderr, "Parsing error: a edge has a wrong vertex.\n");
					exit(EXIT_FAILURE);
				}
				i++;
				buf_cnt = 0;
				if (i == 2)
				{
					i=0;
					state = PARSE_GRAPH_WEIGHT;
				}
			}
			else
			{
				state = PARSE_GRAPH_OTHER;
				printf("state change: ignore line\n");
			}
			break;
		case PARSE_GRAPH_WEIGHT:
			if (isdigit(c) || (c == '.') || (c == '-') || (c == 'e'))
			{
				if (buf_cnt < MAX_BUF)
					buf[buf_cnt++] = c;
				else
				{
					fprintf(stderr, "Parsing error: buffer to small.\n");
					exit(EXIT_FAILURE);
				}
			}
			else if (isspace(c) || ispunct(c))
			{
				if (buf_cnt == 0)
				{
					if (c == '\n')
					{
						/* is the parsed edge weight ok? */
						if (i == 1)
						{
							new = malloc(sizeof(struct adj_list_node));
							if (new == NULL)
							{
								fprintf(stderr, "Parsing error: Out of memory for %lld vertices.\n", vertices);
								exit(EXIT_FAILURE);
							}
							new->from       = edge[0];
							new->to         = edge[1];
							new->weight     = weight;
							new->from_next  = NULL;
							new->to_next    = NULL;
							initial_weight += weight;
							initial_number_edges++;
							add_adj_list_entry(new);
							new = NULL;
							i = 0;
							state = PARSE_GRAPH_NEW;
						}
						else
						{
							fprintf(stderr, "Parsing error: edge isn't correct.\n");
							exit(EXIT_FAILURE);
						}
					}
					break;
				}

				/* parse weight */
				buf[buf_cnt] = 0;
				if (i != 0)
				{
					fprintf(stderr, "Parsing error: edge isn't correct.\n");
					exit(EXIT_FAILURE);
				}
				sscanf(buf, "%lg", &weight);
				i++;
				buf_cnt = 0;
				if (c == '\n')
				{
					new = malloc(sizeof(struct adj_list_node));
					if (new == NULL)
					{
						fprintf(stderr, "Parsing error: Out of memory for %lld vertices.\n", vertices);
						exit(EXIT_FAILURE);
					}
					new->from       = edge[0];
					new->to         = edge[1];
					new->weight     = weight;
					new->from_next  = NULL;
					new->to_next    = NULL;
					initial_weight += weight;
					initial_number_edges++;
					add_adj_list_entry(new);
					new = NULL;
					i = 0;
					state = PARSE_GRAPH_NEW;
				}
			}
			else
			{
				state = PARSE_GRAPH_OTHER;
				printf("state change: ignore line\n");
			}
			break;
		case PARSE_GRAPH_CODE:
			if (isdigit(c))
			{
				if (buf_cnt < MAX_BUF)
					buf[buf_cnt++] = c;
				else
				{
					fprintf(stderr, "Parsing error: buffer to small.\n");
					exit(EXIT_FAILURE);
				}
			}
			else if (isspace(c) || ispunct(c))
			{
				if (buf_cnt == 0)
				{
					if (c == '\n')
					{
						/* is the parsed code looks ok? */
						if (i == code_len)
						{
							printf("DEBUG code parsed with %lld elements\n", code_len);
							state = PARSE_GRAPH_NEW;
						}
						else
						{
							fprintf(stderr, "Parsing error: number of codewords isn't a power of 2.\n");
							exit(EXIT_FAILURE);
						}
					}
					break;
				}

				/* parse codeword */
				buf[buf_cnt] = 0;
				if (i >= code_len)
				{
					code_len = ((long long int) 1)<<(++data_width);
					if (code == NULL)
						code = malloc(code_len * sizeof(long long int));
					else
						code = realloc(code, code_len * sizeof(long long int));
					if (code == NULL)
					{
						fprintf(stderr, "Parsing error: Out of memory.\n");
						exit(EXIT_FAILURE);
					}
				}
				sscanf(buf, "%lld", &(code[i]));
				if (max_cw < code[i])
					max_cw = code[i];
				i++;
				buf_cnt = 0;
				if (c == '\n')
				{
					/* is the parsed code looks ok? */
					if (i == code_len)
					{
						printf("DEBUG code parsed with %lld elements\n", code_len);
						state = PARSE_GRAPH_NEW;
					}
					else
					{
						fprintf(stderr, "Parsing error: number of codewords isn't a power of 2.\n");
						exit(EXIT_FAILURE);
					}
				}
			}
			else
			{
				state = PARSE_GRAPH_OTHER;
				printf("state change: ignore line\n");
			}
			break;
		case PARSE_GRAPH_OTHER:
			/* ignore line */
			if (c == '\n')
			{
				state = PARSE_GRAPH_NEW;
			};
			break;
		default:
			fprintf(stderr, "Undefined parsing state.\n");
			exit(EXIT_FAILURE);
		}
	}

	if ((vertices > 0) && (max_cw < vertices) && (code_len > 1))
		state = PARSE_GRAPH_COMPLETE;

	n_blen = sizeof(long long int)*8;
	n_len = (vertices % n_blen == 0) ? vertices/n_blen : 1;
	n = (long long int*) malloc (n_len*sizeof(long long int));
	if (n == NULL)
	{
		fprintf(stderr, "Out of memory error.\n");
		exit(EXIT_FAILURE);
	}
	memset(n, INIT_MASK, sizeof(long long int)*n_len);

	return (state == PARSE_GRAPH_COMPLETE);
}

void
print_adj_matrix()
{
	struct adj_list_node *it;
	long long int i, j;
	double row[vertices];

	it = NULL;
	printf("adjacent matrix:\n");
	printf("----------------\n\n");
	for (i=0; i < vertices; i++)
	{
		for (j=0; j < vertices; j++)
			row[j] = 0;

		it = adj_from[i];
		while (it != NULL)
		{
			if (it->from == i)
			{
				row[it->to] = it->weight;
				it = it->from_next;
			}
			else
			{
				fprintf(stderr, "Data structur error: invalid adj_from list.\n");
				exit(EXIT_FAILURE);
			}
		}

		for (j=0; j < vertices; j++)
		{
			if (i == 0)
			{
				if (j == 0)
				{
					for (j=0; j < vertices; j++)
					{
						if (j == 0)
							printf("from\\to");
						printf("\t%lld", j);
					}
					j=0;
					printf("\n");
				}
			}
			if (j == 0)
			{
				printf("%lld", i);
			}
			printf("\t%lg", row[j]);
		}
		printf("\n");
	}
}

/*
double
get_rank(long long int v)
{
	struct adj_list_node *it;
	double rank;
	long long int i, d;

	if (IS_REMOVED(v))
		return -1;

	rank = 0;
	d = v & d_mask;

	for (i=d; i<(d + c_mask + 1); i++)
	{
		it = adj_from[i];
		while (it != NULL)
		{
			if ((i == v) && !IS_REMOVED(i))
			{
				if ((it->to & d_mask) != (v & d_mask))
					rank -= it->weight;
			}
			else if (!IS_REMOVED(i) && ((it->to & d_mask) != (v & d_mask)))
				rank += it->weight;

			it = it->from_next;
		}
		it = adj_to[i];
		while (it != NULL)
		{
			if ((i == v) && !IS_REMOVED(i))
			{
				if ((it->from & d_mask) != (v & d_mask))
					rank -= it->weight;
			}
			else if (!IS_REMOVED(i) && ((it->from & d_mask) != (v & d_mask)))
				rank += it->weight;

			it = it->to_next;
		}
	}
	return rank;
}

long long int
select_codeword()
{
	long long int best, i;
	double best_rank, rank;

	best      = -1;
	best_rank = -1;

	for (i=0; i<vertices; i++)
	{
		rank = get_rank(i);
		if (rank > best_rank)
		{
			best = i;
			best_rank = rank;
		}
	}

	return best;
}

void
remove_complementary(long long int c)
{
	long long int i, d;
	d = c & d_mask;

	for (i=d; i<(d + c_mask + 1); i++)
	{
		if (i == c)
			continue;

		REMOVE(i);
		del_adj_list_entry(i);
	}
}
*/

void
print_code()
{
	long long int i, noe;
	double weight;
	struct adj_list_node *it;

	weight = 0;
	noe = 0;
	printf("\ncode = [");
	for (i=0; i<vertices; i++)
	{
		if (!IS_REMOVED(i))
		{
			if (i > 0)
				printf(", ");
			printf("%lld", i);
			it = adj_from[i];
			while (it != NULL)
			{
				if (!IS_REMOVED(it->to))
				{
					weight += it->weight;
					noe++;
				}
				it = it->from_next;
			}
		}
	}
	printf("]\n\n");

	printf("number of vertices:          %lld\n", vertices);
	printf("codewort bit length:         %d\n", c_len);
	printf("initial number of edges:     %lld\n", initial_number_edges);
	printf("remaining number of edges:   %lld\n", noe);
	printf("detection percentage:        %lf%%\n", (((double)(initial_number_edges-noe)*100)/((double)initial_number_edges)));
	printf("initial weights:             %lg\n", initial_weight);
	printf("remaining weights:           %lg\n\n", weight);
}

void
add_adj_list_entry(struct adj_list_node *new)
{
	struct adj_list_node *it, *pe;

	pe = NULL;
	it = adj_from[new->from];
	if (it == NULL)
		adj_from[new->from] = new;
	else
	{
		while (it != NULL)
		{
			pe = it;
			if (it->from < new->from)
				it = it->from_next;
			else if (it->from == new->from)
			{
				if (it->to < new->to)
					it = it->from_next;
				else if (it->to == new->to)
				{
					it->weight += new->weight;
					free(new);
					return;
				}
				else
					it = NULL;
			}
			else
				it = NULL;
		}
		if (pe->from_next == NULL)
		{
			pe->from_next = new;
		}
		else
		{
			new->from_next = pe->from_next;
			pe->from_next  = new;
		}
	}

	pe = NULL;
	it = adj_to[new->to];
	if (it == NULL)
		adj_to[new->to] = new;
	else
	{
		while (it != NULL)
		{
			pe = it;
			if (it->to < new->to)
				it = it->to_next;
			else if (it->to == new->to)
			{
				if (it->from < new->from)
					it = it->to_next;
				else
					it = NULL;
			}
			else
				it = NULL;
		}
		if (pe->to_next == NULL)
		{
			pe->to_next = new;
		}
		else
		{
			new->to_next = pe->to_next;
			pe->to_next  = new;
		}
	}
}

void
del_adj_list_entry(long long int vertex)
{
	struct adj_list_node *it1, *it2, *pe;

	if ((adj_from[vertex] == NULL) && (adj_to[vertex] == NULL))
		return;

	it1 = adj_from[vertex];
	pe  = NULL;
	while (it1 != NULL)
	{
		/* the vertex it1->from is going to be removed so search */
		/* the edge entry in it1->to adj_to and remove it */
		it2 = adj_to[it1->to];
		if (it2->from == vertex)
			adj_to[it1->to] = it2->to_next;
		else
		{
			/* the edge entry isn't the first so we have */
			/* to go through the list of edges from it1->to */
			pe  = it2;
			it2 = it2->to_next;
			while (it2 != NULL)
			{
				/* if we have found the entry remove it */
				if (it2->from == vertex)
				{
					pe->to_next = it2->to_next;
					break;
				}
				/* go to the next list entry of it1->to */
				pe  = it2;
				it2 = it2->to_next;
			}
		}
		/* remove the edge and go to the next entry of it1->from */
		pe  = it1;
		it1 = it1->from_next;
		free(pe);
		pe  = NULL;
	}
	adj_from[vertex] = NULL;

	it1 = adj_to[vertex];
	pe  = NULL;
	while (it1 != NULL)
	{
		/* the vertex it1->to is going to be removed so search */
		/* the edge entry in it1->from adj_from and remove it */
		it2 = adj_from[it1->from];
		if (it2->to == vertex)
			adj_from[it1->from] = it2->from_next;
		else
		{
			/* the edge entry isn't the first so we have */
			/* to go through the list of edges from it1->from */
			pe  = it2;
			it2 = it2->from_next;
			while (it2 != NULL)
			{
				/* if we have found the entry remove it */
				if (it2->to == vertex)
				{
					pe->from_next = it2->from_next;
					break;
				}
				/* go to the next list entry of it1->to */
				pe  = it2;
				it2 = it2->from_next;
			}
		}
		/* remove the edge and go to the next entry of it1->from */
		pe  = it1;
		it1 = it1->to_next;
		free(pe);
		pe  = NULL;
	}
	adj_to[vertex] = NULL;
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: %s\n",
	     __progname);
	exit(1);
}
