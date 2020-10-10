#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
typedef struct 
{
    int valid;
    unsigned tag;
    int LRU_count; // 0表示正在使用,数值越大表示离最近一次更新越远
}cache_line;

int s,E,b,S,vmode;
int hits,misses, evictions;
char *file_path=NULL;
cache_line **cache=NULL;

void init_cache();
void update(unsigned address);
void update_LRU(unsigned set_index);

int main(int argc,char **argv)
{
    int opt;
    while((opt=getopt(argc,argv,"vs:E:b:t:"))!=-1)
    {
        switch (opt)
        {
            case 'v':
                vmode=1;
                break;

            case 's':
                s=atoi(optarg);
                S=1<<s; 
                break;

            case 'E':
                E=atoi(optarg);
                break;

            case 'b':
                b=atoi(optarg);
                break;
            
            case 't':
                file_path=optarg;
                break;
            
            default:
                exit(1);
                break;
        }
    }
    init_cache();

    FILE* pFile=fopen(file_path,"r");
    if(pFile==NULL){
        printf("open the file failed.\n");
        exit(1);
    }
    char op;
    unsigned address;
    int size;
    while(fscanf(pFile," %c %x,%d",&op,&address,&size)>0)
    {
       if(vmode&&op!='I') printf(" %c %x,%d ",op,address,size);
       if(op=='L'||op=='S') update(address);
       else if(op=='M'){
           update(address);
           update(address);
       }
       if(op!='I')printf("\n");
    }
    fclose(pFile);

    for(int i=0;i<S;i++)
        free(cache[i]);
    free(cache);

    printSummary(hits,misses,evictions);
    return 0;
}

void init_cache()
{
    cache=(cache_line **)malloc(sizeof(cache_line)*S);
    for(int i=0;i<S;i++){
        cache[i]=(cache_line*)malloc(sizeof(cache_line)*E);
    }
}

void update(unsigned address)
{
    unsigned set_index=(address>>b)&((unsigned)(-1)>>(32-s));
    unsigned tag=address>>(s+b);
    int hit_flag=0;
    int valid_index=-1; // 第一个空组
    int LRU_index=0; // 牺牲行

    for(int i=0;i<E;i++){
        if(!cache[set_index][i].valid&&valid_index==-1) valid_index=i;   // 找是否还存在空组
        if(cache[set_index][i].valid&&cache[set_index][i].LRU_count>cache[set_index][LRU_index].LRU_count)
        LRU_index=i;   // 若不存在空组,找牺牲行(LRU_count最大)
        if(cache[set_index][i].valid&&cache[set_index][i].tag==tag){
            cache[set_index][i].LRU_count=0;
            hits++;
            hit_flag=1;
            if(vmode) printf(" hit");
            break;
        }
        
    }
    if(!hit_flag){
            misses++;
            if(vmode) printf(" miss");
            // 存在空组就直接填进去
            if(valid_index!=-1){
                cache[set_index][valid_index].valid=1;
                cache[set_index][valid_index].tag=tag;
                cache[set_index][valid_index].LRU_count=0;
                
            }
            else{
                evictions++;
                if(vmode) printf(" eviction");
                cache[set_index][LRU_index].tag=tag;
                cache[set_index][LRU_index].LRU_count=0;
            }
            
    }
    update_LRU(set_index);
}

void update_LRU(unsigned set_index)
{
    for(int i=0;i<E;i++){
        if(cache[set_index][i].valid) cache[set_index][i].LRU_count++;
    }
}