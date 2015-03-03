#ifndef UTILS_H
#define UTILS_H

#define MAX_BUF 32
#define KEYWORD "code"

#define PARSE_NEW       1
#define PARSE_KEY       2
#define PARSE_CODE      4
#define PARSE_OTHER     8
#define PARSE_COMPLETE 16

#define DATA_LEFT       1
#define DATA_RIGHT      2

#define encode(d) (code[(d)])
#define decode(c) ((align == DATA_LEFT) ? (((c) & data_mask)>>chkb_width) : ((c) & data_mask))


extern long long int *code;
extern long long int code_len;
extern long long int data_mask;
extern long long int chkb_mask;
extern int code_width;
extern int chkb_width;
extern int align;
extern int data_width;

extern long long int *summands;
extern int sum_len;

int         parse_code();
int         hamming_weight(long long int a, int len);
void        print_binary_vertex(long long int v, int len);
int         ring_sum_expansion(long long int *tt, int tt_len, int input_width, int pos, int length);
int         fprintsum(FILE *stream, long long int *sum, int sum_len, int input_width, int length);

#endif /* UTILS_H */
