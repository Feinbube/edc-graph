#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_CODEWORD_LENGTH 4
#define DEFAULT_GALOIS_EXPONENT 2
#define DEFAULT_REDUNDANCY 2
#define ELEMENT(CW,EL) (((CW) & \
	    (( ((long long int)f_len) - 1) << ( (EL)*pp) )) >> ((EL)*pp) )

extern	char *__progname;

long long int d_max;
long long int c_max;
long long int c_mask;
long long int d_mask;
int b_len;
int c_len;
int f_len;
int pp;
int k;
int r;

static void usage(void);
static void generate_code(void);


/*
 * Calculate an error detecting code based on the paper named "Systematic
 * t-Unidirectional Error-Detecting Codes over Zm" from Bose, Elmougy and
 * Tallini from 2007.
 */
int
main(int argc, char **argv)
{
	int ch;

	c_len = DEFAULT_CODEWORD_LENGTH;
	pp    = DEFAULT_GALOIS_EXPONENT;
	r     = DEFAULT_REDUNDANCY;
	while ((ch = getopt(argc, argv, "c:e:r:")) != -1)
		switch(ch) {
		case 'c':
			c_len = atoi(optarg);
			if ((errno == ERANGE) || (c_len < 3))
			{
				fprintf(stderr,
				    "Codeword length out of range.\n");
				usage();
				/* NOTREACHED */
			}
			break;
		case 'e':
			pp = atoi(optarg);
			if ((errno == ERANGE) || (pp < 0) || (pp > 62))
			{
				fprintf(stderr,
				    "Exponent for galois field out of range.\n");
				usage();
				/* NOTREACHED */
			}
			break;
		case 'r':
			r = atoi(optarg);
			if ((errno == ERANGE) || (r < 1))
			{
				fprintf(stderr,
				    "Redundancy out of range.\n");
				usage();
				/* NOTREACHED */
			}
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;
	k     = c_len - r;
	f_len = (1<<pp);
	b_len = c_len * pp;
	d_max = ((long long int)1)<<(k * pp);
	c_max = ((long long int)1)<<b_len;
	c_mask = (((long long int) 1) << (r*pp)) - 1;
	d_mask = ~c_mask;

	if (argc > 0)
		usage();

	generate_code();

	return 0;
}

void
generate_code()
{
	long long int i, j, chk, mod;

	if (r <= 2)
		mod = ((long long int) 1) << (r*pp);
	else
		mod = ((long long int) 1) << ((r-1)*pp);
	printf("code = [");
	for (i=0; i<d_max; i++)
	{
		chk = 0;
		for (j=0; j<k; j++)
		{
			chk += f_len - 1 - ELEMENT(i,j);
		}
		chk = chk % mod;
		if (r > 2)
			chk |= ((f_len - 1 - ELEMENT(chk, (r-2)))<<((r-1)*pp));
		if (i != 0)
			printf(", ");
		printf("%lld", ((i<<(r*pp)) | chk));
	}
	printf("]\n");
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-c codeword length] [-e field exponent] [-r redundant symbols]\n",
	     __progname);
	exit(1);
}
