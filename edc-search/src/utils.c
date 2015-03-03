#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include"utils.h"

long long int *code = NULL;
long long int code_len  = 0;
long long int data_mask = 0;
long long int chkb_mask = 0;
int code_width  = 0;
int chkb_width  = 0;
int align       = DATA_LEFT;
int data_width  = 0;

long long int *summands;
long long int data_mask;

int sum_len = 0;
int cmp_len;

int
hamming_weight(long long int a, int len)
{
    int i, cnt;
    cnt = 0;
    for (i=0; i < len; i++)
    {
        if ((a & (((long long int)1)<<i)) != 0)
            cnt++;
    }
    return cnt;
}

void
print_binary_vertex(long long int v, int len)
{
	int i;
	for (i=len-1; i>=0; i--)
	{
		if ((v & (((long long int)1)<<i)) != 0)
			printf("1");
		else
			printf("0");
		if (i%4==0)
			printf(" ");
	}
}

int
tt_cmp(const void *p, const void *q)
{
    return hamming_weight((*(long long int *)p) & data_mask, cmp_len)
            - hamming_weight((*(long long int *)q)&data_mask, cmp_len);
}

int
ring_sum_expansion(long long int *tt, int tt_len, int input_width, int pos, int length)
{
    long long int cb_mask;
    int c, i, j;

    if (sum_len != 0)
    {
        free(summands);
        sum_len = 0;
    }
    cmp_len = length;
    cb_mask = 1 << pos;
    data_mask = (1 << length) - 1 - ((1 << (length-input_width))-1);
    qsort(tt, tt_len, sizeof(long long int), tt_cmp);

/*
    // print sorted truth table
    printf(" tt = [ ");
    for (i=0; i<tt_len; i++)
        printf("%lli, ", tt[i]);
    printf("]\n");

    printf("tt_len: %d\n", tt_len);
    printf("cb_mas: %lld\n", cb_mask);
    printf("dd_mas: %lld\n", data_mask);
*/

    for (i=0; i<tt_len; i++)
    {
        c = 0;
        for (j=0; j<sum_len; j++)
            if ((tt[i] & summands[j]) == summands[j])
                c = c ^ 1;

//        printf("tt: %lld, dw: %lld, cb: %lld, c: %d\n", tt[i], (tt[i] & data_mask), (tt[i]&cb_mask), c);
        if (((tt[i]&cb_mask) >> pos) != c)
        {
            sum_len++;
            if (sum_len == 1)
                summands = malloc(sizeof(long long int));
            else
                summands = realloc(summands, sum_len*sizeof(long long int));
            if (summands == NULL)
            {
                fprintf(stderr, "Out of memory error.\n");
                exit(EXIT_FAILURE);
            }
            summands[sum_len-1] = tt[i] & data_mask;
        }
    }

/*
    printf("\nsum_len: %d\n", sum_len);
    // print the summands
    printf(" summands = [ ");
    for (i=0; i<sum_len; i++)
        printf("%lli, ", summands[i]);
    printf("]\n");
*/

    return sum_len;
}

int
fprintsum(FILE *stream, long long int *sum, int sum_len, int input_width, int length)
{
    int i, j, slen, len, chk_num;

    if (sum_len == 0)
        return 0;

    len = 0;
    chk_num = length - input_width;
    for (i=0; i<sum_len; i++)
    {
        slen = 0;
        if (sum[i] == 0)
        {
            slen += fprintf(stream, "(1");
        }
        else
        {
            for (j=0; j<length; j++)
            {
                if ((sum[i] & (1<<j)) != 0)
                {
                    if ((slen == 0) && (len == 0))
                        slen += fprintf(stream, "(data[s+%d]", j-chk_num);
                    else if (slen == 0)
                        slen += fprintf(stream, " ^ (data[s+%d]", j-chk_num);
                    else
                        slen += fprintf(stream, " & data[s+%d]", j-chk_num);
                }
            }
        }
        len += slen;
        len += fprintf(stream, ")");
        if ( ( i % 5 ) == 0 )
            len += fprintf(stream, "\n");
    }
    len += fprintf(stream, "\n");

    return len;
}

int
parse_code()
{
	long long int i;
	long long int max_cw;
	char buf[MAX_BUF];
	int  buf_cnt;
	int  c;
	int  state;

	/* initialize state and start parsing */
	state = PARSE_NEW;
	i = 0;
	buf_cnt = 0;
	code = NULL;
	code_len = 0;
	data_width = 1;
	max_cw = -1;
	while ((state != PARSE_COMPLETE) && ((c = getchar()) != EOF))
	{
		switch (state) {
		case PARSE_NEW:
			if (isspace(c))
				continue;

			if (isalpha(c))
			{
				state   = PARSE_KEY;
				buf[0]  = tolower(c);
				buf_cnt = 1;
			}
			else
				state     = PARSE_OTHER;
			break;
		case PARSE_KEY:
			if (c == '\n')
			{
				state = PARSE_NEW;
				break;
			};
			buf[buf_cnt++] = tolower(c);
			if (buf_cnt > 3)
			{
				if (strncmp(buf, KEYWORD, strlen(KEYWORD)) == 0)
					state = PARSE_CODE;
				else
					state = PARSE_OTHER;
				buf_cnt = 0;
			}
			break;
		case PARSE_CODE:
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
							if (i == 0)
								state = PARSE_NEW;
							else
								state = PARSE_COMPLETE;
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

#ifdef _WIN32
				sscanf_s(buf, "%lld", &(code[i]));
#else
				sscanf(buf, "%lld", &(code[i]));
#endif	
				
				if (max_cw < code[i])
					max_cw = code[i];
				else
					align = DATA_RIGHT;
				i++;
				buf_cnt = 0;
				if (c == '\n')
				{
					/* is the parsed code looks ok? */
					if (i == code_len)
					{
						state = PARSE_COMPLETE;
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
				state = PARSE_OTHER;
				printf("state change: ignore line\n");
			}
			break;
		case PARSE_OTHER:
			/* ignore line */
			if (c == '\n')
			{
				state = PARSE_NEW;
			};
			break;
		default:
			fprintf(stderr, "Undefined parsing state.\n");
			exit(EXIT_FAILURE);
		}
	}

	/* calculate code properies */
	if (state == PARSE_COMPLETE)
	{
		code_width = data_width + 1;
		while (max_cw > (((long long int) 1)<<code_width))
			code_width++;
		chkb_width = code_width - data_width;
		chkb_mask = (((long long int) 1)<<chkb_width) - 1;
		if (align == DATA_RIGHT)
			chkb_mask = chkb_mask<<data_width;
		data_mask = (((long long int) 1)<<data_width) - 1;
		if (align == DATA_LEFT)
			data_mask = data_mask<<chkb_width;
	}
	/* check encoding and decoding */
	for (i=0; i<code_len; i++)
	{
		if (decode(encode(i)) != i)
		{
			fprintf(stderr, "Input error: can't use code for decoding.\n");
			exit(EXIT_FAILURE);
		}
	}

	return (state == PARSE_COMPLETE);
}
