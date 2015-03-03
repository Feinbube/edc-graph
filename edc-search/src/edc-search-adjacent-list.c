#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <omp.h>

#ifdef _WIN32
#include "../windows/getopt.h"
#else
#include <unistd.h>
#endif

#include "utils.h"

#define PARALLEL

#define MAX_BUF 32
#define V_KEYWORD "vertices"
#define E_KEYWORD "edge"

#define DEFAULT_REDUNDANCY 2

#define PARSE_GRAPH_NEW       1
#define PARSE_GRAPH_KEY       2
#define PARSE_GRAPH_VERTEX    4
#define PARSE_GRAPH_EDGE      8
#define PARSE_GRAPH_WEIGHT   16
#define PARSE_GRAPH_OTHER    32
#define PARSE_GRAPH_COMPLETE 64

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

#define    REMOVE(NODE)   (n[((NODE)/n_blen)] &= ~(((long long int)1)<<((NODE)%n_blen)))
#define IS_REMOVED(NODE) (((*(n+(((long long int)NODE)/(long long int)n_blen))) & (((long long int)1)<<(((long long int)NODE)%(long long int)n_blen))) == 0)

#define INIT_MASK 0xff

#ifdef _WIN32
char *__progname;
#else
extern	char *__progname;
#endif

struct adj_list_node {
	long long int v1;
	long long int v2;
	double weight;
	struct adj_list_node *v1_next;
	struct adj_list_node *v2_next;
};

long long int *n;
long long int vertices;
long long int n_len;
long long int c_mask;
long long int d_mask;
int c_len;
int n_blen;
int r;
double initial_weight;
long long int initial_number_edges;
struct adj_list_node **adj_list;
int number_threads = 1;


static int           parse_graph(char *filename);
static void          print_adj_matrix(void);
static double        get_rank(long long int v);
static long long int select_codeword(void);
static void          remove_complementary(long long int c);
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
	int ch;
	long long int i, max;
	int silent;
	char filename[255];

#ifdef _WIN32
	__progname = argv[0];
#endif

	r              = DEFAULT_REDUNDANCY;
	vertices       = 0;
	adj_list       = NULL;
	initial_weight = 0;
	initial_number_edges = 0;
	silent = 0;
	while ((ch = getopt(argc, argv, "sr:f:t:")) != -1)
		switch(ch) {
		case 'r':
			r = atoi(optarg);
			if ((errno == ERANGE) || (r < 1))
			{
				fprintf(stderr,
				    "Redundancy check bit length out of range.\n");
				usage();
				/* NOTREACHED */
			}
			break;
		case 's':
			silent = 1;
			break;
		case 'f':
			strcpy(filename, optarg);
			break;
		case 't':
			{
			number_threads = atoi(optarg);
			break;
			}
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;
	c_mask = (((long long int) 1) << r) - 1;
	d_mask = ~c_mask;

	if (argc > 0)
		usage();
	
	parse_graph(filename);

	printf("rank(0):                     %lg\n", get_rank(0));
	printf("rank(%lld):                  %lg\n\n", (vertices-1), get_rank(vertices-1));

	if (!silent)
		print_adj_matrix();

	max = ((long long int)1)<<(c_len - r);
	for (i=0; i<max; i++)
		remove_complementary(select_codeword());

	print_code();

	return 0;
}

int
parse_graph(char *filename)
{
	struct adj_list_node *new;
	long long int i;
	long long int edge[2];
	double weight;
	char buf[MAX_BUF];
	int  buf_cnt;
	int  c;
	int  state;
	FILE* fd;

	/* initialize state and start parsing */
	state = PARSE_GRAPH_NEW;
	i = 0;
	buf_cnt = 0;
	adj_list = NULL;
	new = NULL;
	vertices = 0;
	initial_weight = 0;
	initial_number_edges = 0;
	fd = fopen(filename, "r");
//	while ((c = getchar()) != EOF)
	while((c = fgetc(fd)) != EOF)
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

#ifdef _WIN32
				sscanf_s(buf, "%lld", &vertices);
#else
				sscanf(buf, "%lld", &vertices);
#endif		

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
				if (c_len <= r)
				{
					fprintf(stderr, "Parsing error: bit length of vertices must be greater then redundancy.\n");
					exit(EXIT_FAILURE);
				}
				adj_list = malloc(vertices * sizeof(struct adj_list_node));
				if (adj_list == NULL)
				{
					fprintf(stderr, "Parsing error: Out of memory for %lld vertices.\n", vertices);
					exit(EXIT_FAILURE);
				}
				memset(adj_list, 0, vertices * sizeof(struct adj_list_node *));
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
				
#ifdef _WIN32
				sscanf_s(buf, "%lld", &(edge[i]));
#else
				sscanf(buf, "%lld", &(edge[i]));
#endif	

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
							if ((edge[0] & d_mask) != (edge[1] & d_mask))
							{
								new = malloc(sizeof(struct adj_list_node));
								if (new == NULL)
								{
									fprintf(stderr, "Parsing error: Out of memory for %lld vertices.\n", vertices);
									exit(EXIT_FAILURE);
								}
								new->v1      = MIN(edge[0], edge[1]);
								new->v2      = MAX(edge[0], edge[1]);
								new->weight  = weight;
								new->v1_next = NULL;
								new->v2_next = NULL;
								add_adj_list_entry(new);
								new = NULL;
							}
							initial_weight += weight;
							initial_number_edges++;
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

#ifdef _WIN32
				sscanf_s(buf, "%lg", &weight);
#else
				sscanf(buf, "%lg", &weight);
#endif	
				
				i++;
				buf_cnt = 0;
				if (c == '\n')
				{
					if ((edge[0] & d_mask) != (edge[1] & d_mask))
					{
						new = malloc(sizeof(struct adj_list_node));
						if (new == NULL)
						{
							fprintf(stderr, "Parsing error: Out of memory for %lld vertices.\n", vertices);
							exit(EXIT_FAILURE);
						}
						new->v1      = MIN(edge[0], edge[1]);
						new->v2      = MAX(edge[0], edge[1]);
						new->weight  = weight;
						new->v1_next = NULL;
						new->v2_next = NULL;
						add_adj_list_entry(new);
						new = NULL;
					}
					initial_weight += weight;
					initial_number_edges++;
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

	if ((vertices > 0) && (r < c_len) && (adj_list != NULL))
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

	fclose(fd);
	return (state == PARSE_GRAPH_COMPLETE);
}

void
print_adj_matrix()
{
	struct adj_list_node *it;
	long long int i, j;
	

#ifdef _WIN32
	double* row = (double*)malloc(sizeof(double) * vertices);
#else
	double row[vertices];
#endif	

	it = NULL;
	printf("optimized adjacent matrix:\n");
	printf("--------------------------\n\n");
	for (i=0; i < vertices; i++)
	{
		for (j=0; j < vertices; j++)
			row[j] = 0;

		it = adj_list[i];
		while (it != NULL)
		{
			if (it->v1 == i)
			{
				row[it->v2] = it->weight;
				it = it->v1_next;
			}
			else
			{
				row[it->v1] = it->weight;
				it = it->v2_next;
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
			printf("\t%0.3lg", row[j]);
		}
		printf("\n");
	}
}

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
		it = adj_list[i];
		while (it != NULL)
		{
			if ((i == v) && !IS_REMOVED(i))
				rank -= it->weight;
			else if (!IS_REMOVED(i))
				rank += it->weight;

			if (it->v1 == i)
				it = it->v1_next;
			else
				it = it->v2_next;
		}
	}
	return rank;
}

#ifdef PARALLEL

long long int select_codeword()
{
	long long int best, i;
	double best_rank, rank;
	double local_best, local_best_rank;

	best      = -1;
	best_rank = -1;

#pragma omp parallel  num_threads(number_threads) private(rank, local_best, local_best_rank, i) 
	{
		local_best = -1;
		local_best_rank = -1;

#pragma omp for schedule(guided)
		for (i = 0; i<vertices; i++) {
			rank = get_rank(i);
			if (rank > local_best_rank)	{
				local_best = i;
				local_best_rank = rank;
			}
		}
#pragma omp critical
		if (local_best_rank > best_rank) {
			best = local_best;
			best_rank = local_best_rank;
		}
	}

	return best;
}

#else 

long long int select_codeword()
{
	long long int best, i;
	double best_rank, rank;

	best = -1;
	best_rank = -1;

	for (i = 0; i<vertices; i++) {
		rank = get_rank(i);
		if (rank > best_rank) {
			best = i;
			best_rank = rank;
		}
	}

	return best;
}

#endif

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

void
print_code()
{
	long long int i, number_edges;
	struct adj_list_node *it;
	double weight;

	weight = 0;
	number_edges = 0;
	printf("\ncode = [");
	for (i=0; i<vertices; i++)
	{
		if (!IS_REMOVED(i))
		{
			if (i > c_mask)
				printf(", ");
			printf("%lld", i);
			it = adj_list[i];
			while (it != NULL)
			{
				if (!IS_REMOVED(it->v1) && !IS_REMOVED(it->v2))
				{
					weight += it->weight;
					number_edges++;
				}
				if (it->v1 == i)
					it = it->v1_next;
				else
					it = it->v2_next;
			}
		}
	}
	weight = weight / 2.0;
	number_edges = number_edges / 2;
	printf("]\n\n");

	printf("number of vertices:          %lld\n", vertices);
	printf("codewort bit length:         %d\n", c_len);
	printf("redundancy check bit length: %d\n", r);
	printf("checkbit mask:               0x%016llX\n", c_mask);
	printf("data mask:                   0x%016llX\n", d_mask);
	printf("initial number of edges:     %lld\n", initial_number_edges);
	printf("remaining edges:             %lld\n", number_edges);
	printf("percentage detected edges:   %0.15lf%%\n", (((double)(initial_number_edges-number_edges))/((double)initial_number_edges))*100);
	printf("initial weight:              %0.15lg\n", initial_weight);
	printf("remaining weights:           %0.15lg\n", weight);
}

void
add_adj_list_entry(struct adj_list_node *new)
{
	struct adj_list_node *it, *pe;

	pe = NULL;
	it = adj_list[new->v1];
	if (it == NULL)
		adj_list[new->v1] = new;
	else
	{
		while (it != NULL)
		{
			pe = it;
			if (it->v1 == new->v1)
			{
				if (it->v2 == new->v2)
				{
					it->weight += new->weight;
					free(new);
					return;
				}
				else
					it = it->v1_next;
			}
			else
				it = it->v2_next;
		}
		if (pe->v1 == new->v1)
			pe->v1_next = new;
		else
			pe->v2_next = new;
	}

	pe = NULL;
	it = adj_list[new->v2];
	if (it == NULL)
		adj_list[new->v2] = new;
	else
	{
		while (it != NULL)
		{
			pe = it;
			if (it->v1 == new->v2)
				it = it->v1_next;
			else
				it = it->v2_next;
		}
		if (pe->v1 == new->v2)
			pe->v1_next = new;
		else
			pe->v2_next = new;
	}
}

void
del_adj_list_entry(long long int vertex)
{
	struct adj_list_node *it1, *it2, *pe;

	if ((it1 = adj_list[vertex]) == NULL)
		return;

	pe = NULL;
	while (it1 != NULL)
	{
		if (it1->v1 == vertex)
		{
			/* the vertex it1->v1 is going to be removed so search */
			/* the edge entry in it1->v2 adj_list and remove it */
			it2 = adj_list[it1->v2];
			if (it2->v1 == vertex)
				adj_list[it1->v2] = it2->v2_next;
			else if (it2->v2 == vertex)
				adj_list[it1->v2] = it2->v1_next;
			else
			{
				/* the edge entry isn't the first so we have */
				/* to go through the list of edges from it1->v2 */
				pe  = it2;
				if (it2->v1 == it1->v2)
					it2 = it2->v1_next;
				else
					it2 = it2->v2_next;
				while (it2 != NULL)
				{
					/* if we have found the entry remove it */
					if (it2->v1 == vertex)
					{
						if (pe->v1 == it1->v2)
						{
							pe->v1_next = it2->v2_next;
							break;
						}
						else
						{
							pe->v2_next = it2->v2_next;
							break;
						}
					}
					else if (it2->v2 == vertex)
					{
						if (pe->v1 == it1->v2)
						{
							pe->v1_next = it2->v1_next;
							break;
						}
						else
						{
							pe->v2_next = it2->v1_next;
							break;
						}
					}
					/* go to the next list entry of it1->v2 */
					pe  = it2;
					if (it2->v1 == it1->v2)
						it2 = it2->v1_next;
					else
						it2 = it2->v2_next;
				}
			}
			/* remove the edge and go to the next entry of it1->v1 */
			pe  = it1;
			it1 = it1->v1_next;
			free(pe);
			pe  = NULL;
		}
		else
		{
			/* the vertex it1->v2 is going to be removed so search */
			/* the edge entry in it1->v1 adj_list and remove it */
			it2 = adj_list[it1->v1];
			if (it2->v1 == vertex)
				adj_list[it1->v1] = it2->v2_next;
			else if (it2->v2 == vertex)
				adj_list[it1->v1] = it2->v1_next;
			else
			{
				/* the edge entry isn't the first so we have */
				/* to go through the list of edges from it1->v1 */
				pe  = it2;
				if (it2->v1 == it1->v1)
					it2 = it2->v1_next;
				else
					it2 = it2->v2_next;
				while (it2 != NULL)
				{
					/* if we have found the entry remove it */
					if (it2->v1 == vertex)
					{
						if (pe->v1 == it1->v1)
						{
							pe->v1_next = it2->v2_next;
							break;
						}
						else
						{
							pe->v2_next = it2->v2_next;
							break;
						}
					}
					else if (it2->v2 == vertex)
					{
						if (pe->v1 == it1->v1)
						{
							pe->v1_next = it2->v1_next;
							break;
						}
						else
						{
							pe->v2_next = it2->v1_next;
							break;
						}
					}
					/* go to the next list entry of it1->v1 */
					pe  = it2;
					if (it2->v1 == it1->v1)
						it2 = it2->v1_next;
					else
						it2 = it2->v2_next;
				}
			}
			/* remove the edge and go to the next entry of it1->v2 */
			pe  = it1;
			it1 = it1->v2_next;
			free(pe);
			pe  = NULL;
		}
	}
	adj_list[vertex] = NULL;
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: %s [-s .. no graph output] [-r redundancy check bit length]\n",
	     __progname);
	exit(1);
}
