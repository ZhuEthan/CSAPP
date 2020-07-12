/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
}

void printMatrix(int M, int N, int A[N][M]) {
	int i, j;
	for (i = 0; i < N; i++) {
		for (j = 0; j < M; j++) {
			printf("%3d", A[i][j]);
		}
		printf("\n");
	}
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

char trans_exp_desc_2[] = "transpose 32 * 32";
void trans_exp_2(int M, int N, int A[N][M], int B[M][N])
{
	//trans_help(M, N, 0, 8, N-8, N, A, B);
	//trans_help(M, N, 8, M, N-8, N, A, B);
	//trans_help(M, N, 0, 8, 0, N-8, A, B);
	//trans_help(M, N, 8, M, 0, N-8, A, B);
    int i, j, k, l, dia, flag;
	int blocksize = 8;

    for (i = 0; i < N; i += blocksize) {
        for (j = 0; j < M; j += blocksize) {
         	for (k = i; k < i+blocksize; k++) {
				flag = 0;
				for (l = j; l < j+blocksize; l++) {
					if (k == l) {
						dia = A[k][l];
						flag = 1;
					} else {
						B[l][k] = A[k][l];
					}
				}
				if (flag == 1) {
					B[k][k] = dia;
				}
			}	
        }
    }    
}



char trans_exp_desc_3[] = "transpose 64*64";
void trans_exp_3(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k, l;
	int blocksize = 4;
	//int kk, ll;
	//int line, lineFlag;
	int temp[4][2];

    for (i = 0; i < N; i += blocksize) {
        for (j = 0; j < M; j += blocksize) {
			if (i == j) {
				temp[0][0] = A[i][j];
				temp[0][1] = A[i][j+1];
				temp[1][0] = A[i+1][j];
				temp[1][1] = A[i+1][j+1];
			
				B[j][i+2] = A[i+2][j];
				B[j+1][i+2] = A[i+2][j+1];
				B[j][i+3] = A[i+3][j];
				B[j+1][i+3] = A[i+3][j+1];

				B[i][j]	= temp[0][0];
				B[i][j+1] = temp[1][0];
				B[i+1][j] = temp[0][1];
				B[i+1][i+1] = temp[1][1];
		
				temp[0][0] = A[i+2][j+2];
				temp[0][1] = A[i+2][j+3];
				temp[1][0] = A[i+3][j+2];
				temp[1][1] = A[i+3][j+3];

				B[j+2][i] = A[i][j+2];
				B[j+3][i] = A[i][j+3];
				B[j+2][i+1] = A[i+1][j+2];
				B[j+3][i+1] = A[i+1][j+3];

				B[i+2][j+2] = temp[0][0];
				B[i+2][j+3] = temp[1][0];
				B[i+3][j+2] = temp[0][1];
				B[i+3][i+3] = temp[1][1];
				
			} else if (((j == 4 || j == 12 || j == 20 || j == 28 || j == 36 || j == 44 || j == 52 || j == 60) && j-i==4) || ((j == 0 || j == 8 || j == 16 || j == 24 || j == 32 || j == 40 || j == 48 || j == 56) && i-j==4)) {

				temp[0][0] = A[i][j+2];
                temp[0][1] = A[i][j+3];
                temp[1][0] = A[i+1][j+2];
                temp[1][1] = A[i+1][j+3];
            
                temp[2][0] = A[i+2][j];
                temp[2][1] = A[i+2][j+1];
                temp[3][0] = A[i+3][j];
                temp[3][1] = A[i+3][j+1];
            
                B[j][i] = A[i][j];
                B[j+1][i] = A[i][j+1];
                B[j][i+1] = A[i+1][j];
                B[j+1][i+1] = A[i+1][j+1];
            
                B[j+2][i+2] = A[i+2][j+2];
                B[j+3][i+2] = A[i+2][j+3];
                B[j+2][i+3] = A[i+3][j+2];
                B[j+3][i+3] = A[i+3][j+3];
            
                B[j+2][i] = temp[0][0];
                B[j+3][i] = temp[0][1];
                B[j+2][i+1] = temp[1][0];
                B[j+3][i+1] = temp[1][1];
            
                B[j][i+2] = temp[2][0];
                B[j+1][i+2] = temp[2][1];
                B[j][i+3] = temp[3][0];
                B[j+1][i+3] = temp[3][1];

			} else {
         		for (k = i; k < i+blocksize; k++) {
					for (l = j; l < j+blocksize; l++) {
						B[l][k] = A[k][l];
					}
				}	
			}
        }
    }    

}


/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    //registerTransFunction(transpose_submit, transpose_submit_desc); 

    registerTransFunction(trans_exp_2, trans_exp_desc_2); 
    registerTransFunction(trans_exp_3, trans_exp_desc_3); 
    registerTransFunction(trans_exp_4, trans_exp_desc_4); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

