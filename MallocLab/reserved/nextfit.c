/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "hdu",
    /* First member's full name */
    "NiC",
    /* First member's email address */
    "wuhu@qq.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define MINSIZE 16
#define CHUNKSIZE (1<<12) /* Extend heap by this amount */

#define MAX(x,y) (x)>(y)? x:y
#define MIN(x,y) (x)<(y)? x:y
/* Pack a size and allocated bit into a word */ 
#define PACK(size,alloc) ((size)|(alloc))

/* Read and write a word at address P */
#define GET(P) (*(unsigned int *)(P))
#define PUT(P,val) (*(unsigned int *)(P)=(val))

/* Read the size and allocted fields from address P */
#define GET_SIZE(P) (GET(P)&(~0X7))
#define GET_ALLOC(P) (GET(P)&(0X1))

/* Given block ptr bp,compute address of its header and footer */ 
#define HEAD(bp) ((char*)(bp)-WSIZE)
#define FOOT(bp) ((char*)(bp)+GET_SIZE(HEAD(bp))-DSIZE)

/* Given block ptr bp,compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp)+GET_SIZE(HEAD((char*)(bp))))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE((char *)(bp)-DSIZE))

static char *heap_listp; /* points to first vaild byte of heap */
static char *pre_listp;/* next_find中上次有效分配的指针 */
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *next_fit(size_t size);
static void place(void *bp,size_t size);
static void split_block(void *bp,size_t size);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* 分配一个初始块,两个序言块,一个结尾块 */
    if((heap_listp=mem_sbrk(4*WSIZE))==(void *)(-1))
        return -1;
    PUT(heap_listp,PACK(0,0));
    PUT(heap_listp+WSIZE,PACK(DSIZE,1));
    PUT(heap_listp+2*WSIZE,PACK(DSIZE,1));
    PUT(heap_listp+3*WSIZE,PACK(0,1));
    /* 让heap_listp 指向堆的可分配空间 */
    heap_listp+=2*WSIZE;
    pre_listp=heap_listp;
    /* 堆目前无空间可用,拓展 CHUNKSIZE bytes */
    if(extend_heap(CHUNKSIZE/WSIZE)==NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    char *bp;
    if(!size)
        return NULL;
    /* 头部尾部需要8字节,所以最小分配16字节 */
    if(size<=DSIZE)
        size=MINSIZE;
    else
        size=(ALIGN(size))+(2*DSIZE);
    /* first-fit 分配 */
    if((bp=next_fit(size))!=NULL){
        place(bp,size);
        return bp;
    }
    /* 分配失败 */
    size_t extendsize=MAX(size,CHUNKSIZE);
    if((bp=extend_heap(extendsize/WSIZE))==NULL)
        return NULL;
    place(bp,size);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size=GET_SIZE(HEAD(bp));
    PUT(HEAD(bp),PACK(size,0));
    PUT(FOOT(bp),PACK(size,0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr=ptr;
    void *newptr;
    size_t oldsize=GET_SIZE(HEAD(ptr));

    if(ptr==NULL)
        return mm_malloc(size);
    if(size==0){
        mm_free(ptr);
        return ptr;
    }
    if(oldsize>=size){
        newptr=mm_malloc(size);
        memcpy(newptr,oldptr,size);
        mm_free(oldptr);
    }
    else{
        newptr=mm_malloc(size);
        memcpy(newptr,oldptr,oldsize);
        mm_free(oldptr);
    }
    return newptr;
}

static void *extend_heap(size_t words)
{
    char *bp;
    /* 拓展size个字节,满足8字节对齐 */
    size_t size=ALIGN(words*WSIZE);
    /* bp 指向堆空间尚未使用的首字节 */
    if((bp=mem_sbrk(size))==(void *)-1)
        return NULL;
    PUT(HEAD(bp),PACK(size,0)); /* 设置 block header */
    PUT(FOOT(bp),PACK(size,0)); /* 设置 block footer */
    PUT(HEAD(NEXT_BLKP(bp)),PACK(0,1)); /* 设置 结尾块 */
    /* 合并前面的空闲块 */
    return coalesce(bp);
} 

static void *coalesce(void *bp)
{
    size_t prev_alloc=GET_ALLOC(HEAD(PREV_BLKP(bp)));
    size_t next_alloc=GET_ALLOC(HEAD(NEXT_BLKP(bp)));
    size_t size=GET_SIZE(HEAD(bp));
    if(prev_alloc&&next_alloc){ /* 前后都无空闲块 */
        //pre_listp=bp;
        return bp;
    }
    else if(prev_alloc&&!next_alloc){   /* 仅后面有空闲块 */
        size+=GET_SIZE(HEAD(NEXT_BLKP(bp)));
        PUT(HEAD(bp),PACK(size,0));
        /* 注意到这里的FOOT(bp)已经指向后面的空闲块尾部 */
        PUT(FOOT(bp),PACK(size,0));
    } 
    else if(!prev_alloc&&next_alloc){  /* 仅前面有空闲块 */
        size+=GET_SIZE(HEAD(PREV_BLKP(bp)));
        PUT(FOOT(bp),PACK(size,0));
        PUT(HEAD(PREV_BLKP(bp)),PACK(size,0));
        bp=PREV_BLKP(bp);
    }
    else{   /* 前后都有空闲块 */
        size+=(GET_SIZE(HEAD(NEXT_BLKP(bp)))+GET_SIZE(HEAD(PREV_BLKP(bp))));
        PUT(HEAD(PREV_BLKP(bp)),PACK(size,0));
        PUT(FOOT(NEXT_BLKP(bp)),PACK(size,0));
        bp=PREV_BLKP(bp);
    }
    /* 注意合并空闲块时需改变pre_listp */
    pre_listp=bp;
    return bp;
}


static void *next_fit(size_t size){
    for (char* bp = pre_listp; GET_SIZE(HEAD(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HEAD(bp)) && GET_SIZE(HEAD(bp)) >= size)
        {
            return bp;
        }
    }
    return NULL;
}

static void place(void *bp,size_t size){
    size_t oldsize=GET_SIZE(HEAD(bp));
    PUT(HEAD(bp),PACK(oldsize,1));
    PUT(FOOT(bp),PACK(oldsize,1));

    split_block(bp,size);
    
    pre_listp=bp;
}

static void split_block(void *bp,size_t size){
    size_t oldsize=GET_SIZE(HEAD(bp));
    if(oldsize-size>=MINSIZE)
    {
        PUT(HEAD(bp),PACK(size,1));
        PUT(FOOT(bp),PACK(size,1));
        size_t remain_size=oldsize-size;
        bp=NEXT_BLKP(bp);
        PUT(HEAD(bp),PACK(remain_size,0));  
        PUT(FOOT(bp),PACK(remain_size,0));
    }
}














