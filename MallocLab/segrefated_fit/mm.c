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
#include "../src/memlib.h"

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
#define MAXLIST 16
#define MIN(a,b) (a)>(b)? (b):(a)
#define MAX(a,b) (a)>(b)? (a):(b)
/* Pack a size and allocated bit into a word */ 
#define PACK(size,alloc) ((size)|(alloc))

/* Read and write a word at address P */
#define GET(P) (*(unsigned int *)(P))
#define PUT(P,val) (*(unsigned int *)(P)=(val))
#define SET_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))
/* Read the size and allocted fields from address P */
#define GET_SIZE(P) (GET(P)&(~0X7))
#define GET_ALLOC(P) (GET(P)&(0X1))

/* Given block ptr bp,compute address of its header and footer */ 
#define HEAD(bp) ((char*)(bp)-WSIZE)
#define FOOT(bp) ((char*)(bp)+GET_SIZE(HEAD(bp))-DSIZE)

/* Given block ptr bp,compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp)+GET_SIZE(HEAD((char*)(bp))))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE((char *)(bp)-DSIZE))

#define PRED_PTR(bp) ((char *)(bp))
#define SUCC_PTR(bp) ((char *)(bp)+WSIZE)

#define PRED(ptr) (*(char **)(ptr))
#define SUCC(ptr) (*(char **)(SUCC_PTR(ptr)))


void *seg_free_lists[MAXLIST];
static char *heap_listp;

static void *coalesce(void *bp);
static int find_list(size_t size);

static void *find_free_block(size_t size);
static void *extend_heap(size_t size);
static void place(void *bp,size_t size);
static void insert_node(void *ptr,size_t size);
static void delete_node(void *ptr);
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
   int listNum=0;
   for(listNum=0;listNum<MAXLIST;listNum++)
        seg_free_lists[listNum]=NULL;
    /* 申请四字,初始化堆 */
    if((heap_listp=mem_sbrk(4*WSIZE))==(void *)(-1))
        return -1;
    PUT(heap_listp,PACK(0,1));
    PUT(heap_listp+WSIZE,PACK(DSIZE,1));
    PUT(heap_listp+2*WSIZE,PACK(DSIZE,1));
    PUT(heap_listp+3*WSIZE,PACK(0,1));
    heap_listp+=DSIZE;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

void *mm_malloc(size_t size)
{
    if (size == 0)
        return NULL;
    /* 内存对齐 */
    if (size <= DSIZE)
        size = 2 * DSIZE;
    else
        size = ALIGN(size + DSIZE);
    /* 首次适配寻找空闲块 */
    void *ptr = find_free_block(size);
    /* 失败则拓展堆 */
    if (ptr == NULL)
    {
        if ((ptr = extend_heap(MAX(size, CHUNKSIZE))) == NULL)
            return NULL;
    }

    /* 在空闲块中分配size大小的块并分割 */
    place(ptr, size);

    return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size=GET_SIZE(HEAD(bp));

    PUT(HEAD(bp),PACK(size,0)); 
    PUT(FOOT(bp),PACK(size,0));
    /* free后插入seg_free_lists中 */
    insert_node(bp,size);
    /* 别忘记free后合并 */
    coalesce(bp);
    //printf("%p bp succful free\n\n",bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *new_block = ptr;
    int remainder;

    if (size == 0)
        return NULL;
    /* 内存对齐 */
    if (size <= DSIZE)
        size = 2 * DSIZE;
    else
        size = ALIGN(size + DSIZE);

    /* 如果size小于原来块的大小，直接返回原来的块 */
    if ((remainder = GET_SIZE(HEAD(ptr)) - size) >= 0)
    {
        return ptr;
    }
    /* 否则先检查地址连续下一个块是否为free块或者该块是堆的结束块，因为我们要尽可能利用相邻的free块，以此减小“external fragmentation” */
    else if (!GET_ALLOC(HEAD(NEXT_BLKP(ptr))) || !GET_SIZE(HEAD(NEXT_BLKP(ptr))))
    {
        /* 即使加上后面连续地址上的free块空间也不够，需要扩展块 */
        if ((remainder = GET_SIZE(HEAD(ptr)) + GET_SIZE(HEAD(NEXT_BLKP(ptr))) - size) < 0)
        {
            if (extend_heap(MAX(-remainder, CHUNKSIZE)) == NULL)
                return NULL;
            remainder += MAX(-remainder, CHUNKSIZE);
        }

        /* 删除刚刚利用的free块并设置新块的头尾 */
        delete_node(NEXT_BLKP(ptr));
            PUT(HEAD(ptr), PACK(size + remainder, 1));
            PUT(FOOT(ptr), PACK(size + remainder, 1));
    }
    /* 没有可以利用的连续free块，而且size大于原来的块，这时只能申请新的不连续的free块、复制原块内容、释放原块 */
    else
    {
        new_block = mm_malloc(size);
        memcpy(new_block, ptr, GET_SIZE(HEAD(ptr)));
        mm_free(ptr);
    }

    return new_block;
}
/* 辅助函数部分 */
static void *extend_heap(size_t size)
{
    char *bp;
    /* 拓展size个字节,满足8字节对齐 */
    size=ALIGN(size);
    /* bp 指向堆空间尚未使用的首字节 */
    if((bp=mem_sbrk(size))==(void *)-1)
        return NULL;

    PUT(HEAD(bp),PACK(size,0)); /* 设置 block header */
    PUT(FOOT(bp),PACK(size,0)); /* 设置 block footer */
    PUT(HEAD(NEXT_BLKP(bp)),PACK(0,1)); /* 设置 结尾块 */

    insert_node(bp,size);
    /* 合并前面的空闲块 */
    return coalesce(bp);
} 
/* 寻找对应的链 */
static int find_list(size_t size)
{
    int listNumber=0;
    while((listNumber<MAXLIST-1 )){
        if(size>1){
            size>>=1;
            listNumber++;
        }
        else break;
    }
    return listNumber;
}
/* 首次适配寻找空闲块 */
static void* find_free_block(size_t size)
{
    int listNumber=find_list(size);
    void *ptr=NULL;
    while(listNumber<MAXLIST){
        ptr=seg_free_lists[listNumber];
        while ((ptr != NULL) && ((size > GET_SIZE(HEAD(ptr)))))
            {
                //ptr = PRED(ptr);
                ptr = SUCC(ptr);
            }
            /* 找到对应的free块 */
            if (ptr != NULL)
                return ptr;
        listNumber++;
    }
    return NULL;
}


static void insert_node(void *ptr,size_t size)
{
    /* 采用新来的插至表头方式 */
    int listNumber=find_list(size); /* 找到对应大小链 */
    /* 第一次插入 */

    if(seg_free_lists[listNumber]==NULL){
        SET_PTR(PRED_PTR(ptr),NULL);
        SET_PTR(SUCC_PTR(ptr),NULL);
        seg_free_lists[listNumber]=ptr;
    }
    else{
        /* 插入至表头 */
        void *old=seg_free_lists[listNumber];
        SET_PTR(PRED_PTR(old),ptr);
        SET_PTR(PRED_PTR(ptr),NULL);
        SET_PTR(SUCC_PTR(ptr),old);
        seg_free_lists[listNumber]=ptr;
    }
    return;

}


static void delete_node(void *ptr)
{
    size_t size=GET_SIZE(HEAD(ptr));
    int listNumber=find_list(size);/* 找到对应大小链 */
    if(PRED(ptr)!=NULL){
        if(SUCC(ptr)!=NULL){
            /* [ListNumber]->xxx->ptr->xxx */
            SET_PTR(PRED_PTR(SUCC(ptr)),PRED(ptr));
            SET_PTR(SUCC_PTR(PRED(ptr)),SUCC(ptr));
        }
        else{
            /* [ListNumber]->xxx->ptr */
            SET_PTR(SUCC_PTR(PRED(ptr)),NULL);
        }
    }
    else{
        if(SUCC(ptr)!=NULL){
            /* [ListNumber]->ptr->xxx */
            SET_PTR(PRED_PTR(SUCC(ptr)),NULL);
            seg_free_lists[listNumber]=SUCC(ptr);
        }
        else{
            /* [ListNumber]->ptr */
            seg_free_lists[listNumber]=NULL;
        }
    }
}


static void *coalesce(void *bp)
{
    size_t prev_alloc=GET_ALLOC(HEAD(PREV_BLKP(bp)));
    size_t next_alloc=GET_ALLOC(HEAD(NEXT_BLKP(bp)));
    size_t size=GET_SIZE(HEAD(bp));
    if(prev_alloc&&next_alloc){ /* 前后都无空闲块 */
        return bp;
    }
    else if(prev_alloc&&!next_alloc){   /* 仅后面有空闲块 */
        /* 删除连续空闲的旧节点 */
        delete_node(bp);
        delete_node(NEXT_BLKP(bp));

        size+=GET_SIZE(HEAD(NEXT_BLKP(bp)));
        PUT(HEAD(bp),PACK(size,0));
        /* 注意到这里的FOOT(bp)已经指向后面的空闲块尾部 */
        PUT(FOOT(bp),PACK(size,0));
    } 
    else if(!prev_alloc&&next_alloc){  /* 仅前面有空闲块 */
        /* 删除连续空闲的旧节点 */
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        
        size+=GET_SIZE(HEAD(PREV_BLKP(bp)));
        PUT(FOOT(bp),PACK(size,0));
        PUT(HEAD(PREV_BLKP(bp)),PACK(size,0));
        bp=PREV_BLKP(bp);

    }
    else{   /* 前后都有空闲块 */
        /* 删除连续空闲的旧节点 */
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        delete_node(NEXT_BLKP(bp));

        size+=(GET_SIZE(HEAD(NEXT_BLKP(bp)))+GET_SIZE(HEAD(PREV_BLKP(bp))));
        PUT(HEAD(PREV_BLKP(bp)),PACK(size,0));
        PUT(FOOT(NEXT_BLKP(bp)),PACK(size,0));
        bp=PREV_BLKP(bp);
    }
    insert_node(bp,size);
    return bp;
}

/* alloc 空闲块并进行分割 */
static void place(void *bp,size_t size){
    size_t oldsize=GET_SIZE(HEAD(bp));
    size_t remain_size=oldsize-size;
    
    delete_node(bp);
    if(remain_size<DSIZE*2){
        /* 小于4字,不分割 */
        PUT(HEAD(bp),PACK(oldsize,1));
        PUT(FOOT(bp),PACK(oldsize,1));
    }
    else{
        PUT(HEAD(bp),PACK(size,1));
        PUT(FOOT(bp),PACK(size,1));
        PUT(HEAD(NEXT_BLKP(bp)),PACK(remain_size,0));
        PUT(FOOT(NEXT_BLKP(bp)),PACK(remain_size,0));
        insert_node(NEXT_BLKP(bp),remain_size);
    }
}


















