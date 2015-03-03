#include <errno.h>
#include <limits.h>
//#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include "../windows/getopt.h"
#else
#include <unistd.h>
#endif

#define ERROR_PROB    0.0001
#define WO_ERROR_PROB 0.9999

#define DEFAULT_CODEWORD_LENGTH 2
#define DEFAULT_GALOIS_EXPONENT 2
#define ELEMENT(CW,EL) (((CW) & \
	    (( ((long long int)f_len) - 1) << ( (EL)*pp) )) >> ((EL)*pp) )

#ifdef _WIN32
char *__progname;
#else
extern	char *__progname;
#endif

static long long int error_from[]    = { 0,    1,    1,    2};
static long long int error_to[]      = { 1,    2,    3,    3};
static double        error_prob[]    = {0.46, 0.44, 0.05, 0.02};
static long long int err_gray_from[] = { 0,    1,    1,    3};
static long long int err_gray_to[]   = { 1,    3,    2,    2};
static double        err_gray_prob[] = {0.46, 0.44, 0.05, 0.02};
static int           error_num       = 4;

long long int c_max;
int b_len;
int c_len;
int f_len;
int pp;

static void usage(void);
static void all_cell_errors(void);


/*
 * Calculate an error graph representing multi-level NAND flash cells.
 * Error model based on experimental results from Cai, Haratsch, Mutlu and Mai
 * from IEEE DATE conference 2012.
 */
int
main(int argc, char **argv)
{
	int ch;

#ifdef _WIN32
	__progname = argv[0];
#endif

	c_len = DEFAULT_CODEWORD_LENGTH;
	pp    = DEFAULT_GALOIS_EXPONENT;
	while ((ch = getopt(argc, argv, "c:g")) != -1)
		switch(ch) {
		case 'c':
			c_len = atoi(optarg);
			if ((errno == ERANGE) || (c_len < 2))
			{
				fprintf(stderr,
				    "Codeword length out of range.\n");
				usage();
				/* NOTREACHED */
			}
			break;
		case 'g':
			memcpy(error_from, err_gray_from, error_num * sizeof(long long int));
			memcpy(error_to,   err_gray_to,   error_num * sizeof(long long int));
			memcpy(error_prob, err_gray_prob, error_num * sizeof(double));
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;
	f_len = (1<<pp);
	b_len = c_len * pp;
	c_max = ((long long int)1)<<b_len;

	if (argc > 0)
		usage();

	printf("vertices = %lld\n", c_max);
	all_cell_errors();

	return 0;
}

void
all_cell_errors()
{
	long long int err_pos_vec, *err_pos, err_comb, err_comb_max;
	long long int from, to, err_type, d_max, e_max, i, j;
	int err_pos_len;
	double prob;

	err_pos      = NULL;
	err_pos_len  = 0;
	e_max        = ((long long int)1)<<c_len;
	for (err_pos_vec=1; err_pos_vec<e_max; err_pos_vec++)
	{
		/* calculate positions of erroneous cells */
		for (i=0; i<c_len; i++)
		{
			if ( (err_pos_vec & (((long long int) 1)<<i)) != 0 )
			{
				if (err_pos_len == 0)
				{
					err_pos = malloc(sizeof(long long int));
					if (err_pos == NULL)
					{
						fprintf(stderr, "Error: Out of memory.\n");
						exit(EXIT_FAILURE);
					}
					err_pos_len = 1;
					err_pos[0] = i;
				}
				else
				{
					err_pos_len++;
					err_pos = realloc(err_pos, err_pos_len * sizeof(long long int));
					if (err_pos == NULL)
					{
						fprintf(stderr, "Error: Out of memory.\n");
						exit(EXIT_FAILURE);
					}
					err_pos[err_pos_len-1] = i;
				}
			}
		}


		/* calculate all possible combinations of errors for this error vector */
		// TODO: this is only valid for 4 types of single cell errors
		err_comb_max = ((long long int)1)<<(err_pos_len*2);
		for (err_comb=0; err_comb<err_comb_max; err_comb++)
		{
			/* calculate the part of the cells without errors */
			if (c_len == err_pos_len)
			{
				d_max = 0;
				prob  = pow(ERROR_PROB, err_pos_len);
				prob *= pow(WO_ERROR_PROB, (c_len-err_pos_len));
				prob *= pow(4, ((c_len-err_pos_len)*-1));
				from  = 0;
				to    = 0;
				for (j=0; j<err_pos_len; j++)
				{
					err_type = ((err_comb & (((long long int)3)<<(j*2)))>>(j*2));
					from = (from & ((((long long int)1)<<(pp*err_pos[j]))-1)) | (error_from[err_type]<<(pp*err_pos[j])) | ((from & ~((((long long int)1)<<(pp*err_pos[j]))-1))<<pp);
					to   = (to   & ((((long long int)1)<<(pp*err_pos[j]))-1)) | (error_to[err_type]<<(pp*err_pos[j]))   | ((to   & ~((((long long int)1)<<(pp*err_pos[j]))-1))<<pp);
					prob *= error_prob[err_type];
				}
				printf("edge %lld %lld %0.15lg\n", from, to, prob);
			}
			else
			{
				d_max = ((long long int)1)<<(b_len-(pp*err_pos_len));

				for (i=0; i<d_max; i++)
				{
					prob  = pow(ERROR_PROB, err_pos_len);
					prob *= pow(WO_ERROR_PROB, (c_len-err_pos_len));
					prob *= pow(4, ((c_len-err_pos_len)*-1));
					from  = i;
					to    = i;
					for (j=0; j<err_pos_len; j++)
					{
						err_type = ((err_comb & (((long long int)3)<<(j*2)))>>(j*2));
						from = (from & ((((long long int)1)<<(pp*err_pos[j]))-1)) | (error_from[err_type]<<(pp*err_pos[j])) | ((from & ~((((long long int)1)<<(pp*err_pos[j]))-1))<<pp);
						to   = (to   & ((((long long int)1)<<(pp*err_pos[j]))-1)) | (error_to[err_type]<<(pp*err_pos[j]))   | ((to   & ~((((long long int)1)<<(pp*err_pos[j]))-1))<<pp);
						prob *= error_prob[err_type];
					}
					printf("edge %lld %lld %0.15lg\n", from, to, prob);
				}
			}
		}

		/* Free allocated and now unused memory */
		err_pos_len = 0;
		free(err_pos);
		err_pos = NULL;
	}
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-c codeword length] [-g]\n",
	     __progname);
	exit(1);
}
