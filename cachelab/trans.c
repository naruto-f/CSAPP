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
#include <math.h>
#include "cachelab.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {

}

char transpose_61x67_desc[] = "Transpose submission61x67";
void transpose_61x67_submit(int M, int N, int A[N][M], int B[M][N]) {
    int stride = 16;

    for (int kk = 0; kk < N; kk += stride) {
        for (int ll = 0; ll < M; ll += stride) {
            for (int i = kk; i < min(kk + stride, N); ++i) {
                for (int j = ll; j < min(ll + stride, M); ++j) {
                    B[j][i] = A[i][j];
                }
            }
        }
    }

}

char transpose_32x32_desc[] = "Transpose submission32x32";
void transpose_32x32_submit(int M, int N, int A[N][M], int B[M][N])
{
    int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;
    // int row_stride = 8, col_stride = 8;
    int stride = 8;

    for (int kk = 0; kk < N; kk += stride) {
        for (int ll = 0; ll < M; ll += stride) {
            for (int i = kk; i < kk + stride; ++i) {
                for (int j = ll; j < ll + stride; j += stride) {
                    /* 因为模拟缓存是使用直接映射，数据A和B位于对角线上的块由偏移相同所以会映射到同一个缓存块，所以位于数组B的读取可能会覆盖之前读取且需要使用的数组A内容的缓存行，所以可以使用8个局部变量缓存可能被覆盖的A的数据 */
                    tmp1 = A[i][j];
                    tmp2 = A[i][j + 1];
                    tmp3 = A[i][j + 2];
                    tmp4 = A[i][j + 3];
                    tmp5 = A[i][j + 4];
                    tmp6 = A[i][j + 5];
                    tmp7 = A[i][j + 6];
                    tmp8 = A[i][j + 7];

                    B[j][i] = tmp1;
                    B[j + 1][i] = tmp2;
                    B[j + 2][i] = tmp3;
                    B[j + 3][i] = tmp4;
                    B[j + 4][i] = tmp5;
                    B[j + 5][i] = tmp6;
                    B[j + 6][i] = tmp7;
                    B[j + 7][i] = tmp8;
                }
            }
        }
    }
}


char transpose_64x64_desc[] = "Transpose submission64*64";
void transpose_64x64_submit(int M, int N, int A[N][M], int B[M][N])
{
    int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;

    //将8x8的大块分成4x4的小块处理
    for (int kk = 0; kk < N; kk += 8) {
        for (int ll = 0; ll < M; ll += 8) {
            for (int i = kk; i < kk + 4; ++i) {
                //使用局部变量保存A当前在缓存中的行
                tmp1 = A[i][ll];
                tmp2 = A[i][ll + 1];
                tmp3 = A[i][ll + 2];
                tmp4 = A[i][ll + 3];
                tmp5 = A[i][ll + 4];
                tmp6 = A[i][ll + 5];
                tmp7 = A[i][ll + 6];
                tmp8 = A[i][ll + 7];

                //赋值给B的1和2块，核心还是要充分利用已经在缓存上的内容
                B[ll][i] = tmp1;
                B[ll + 1][i] = tmp2;
                B[ll + 2][i] = tmp3;
                B[ll + 3][i] = tmp4;
                B[ll][i + 4] = tmp5;
                B[ll + 1][i + 4] = tmp6;
                B[ll + 2][i + 4] = tmp7;
                B[ll + 3][i + 4] = tmp8;
            }

            for (int i = ll; i < ll + 4; ++i) {
                tmp1 = B[i][kk + 4];
                tmp2 = B[i][kk + 5];
                tmp3 = B[i][kk + 6];
                tmp4 = B[i][kk + 7];

                //转置A的第3块到B的第2块
                B[i][kk + 4] = A[kk + 4][i];
                B[i][kk + 5] = A[kk + 5][i];
                B[i][kk + 6] = A[kk + 6][i];
                B[i][kk + 7] = A[kk + 7][i];

                //将原先保存在B的第2块中的内容平移到B的第三块
                B[i + 4][kk + 0] = tmp1;
                B[i + 4][kk + 1] = tmp2;
                B[i + 4][kk + 2] = tmp3;
                B[i + 4][kk + 3] = tmp4;
            }
            for (int i = kk + 4; i < kk + 8; ++i) {
                //转置A的第4块到B的第4块，因为对角线上的数据块缓存映射冲突，所以先将A中的行保存到4个局部变量中。
                tmp1 = A[i][ll + 4];
                tmp2 = A[i][ll + 5];
                tmp3 = A[i][ll + 6];
                tmp4 = A[i][ll + 7];

                B[ll + 4][i] = tmp1;
                B[ll + 5][i] = tmp2;
                B[ll + 6][i] = tmp3;
                B[ll + 7][i] = tmp4;
            }
        }
    }
}



/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

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
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);

    registerTransFunction(transpose_64x64_submit, transpose_64x64_desc);
    registerTransFunction(transpose_32x32_submit, transpose_32x32_desc);
    registerTransFunction(transpose_61x67_submit, transpose_61x67_desc);
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

