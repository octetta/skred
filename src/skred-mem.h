#ifndef _SKRED_SHM_H_
#define _SKRED_SHM_H_

typedef struct skred_mem_s skred_mem_t;

int skred_mem_create(skred_mem_t *sm, const char *name, size_t size);
int skred_mem_open(skred_mem_t *sm, const char *name, size_t size);
void skred_mem_close(skred_mem_t *sm);
skred_mem_t *skred_mem_new(void);
void *skred_mem_addr(skred_mem_t *s);

#endif
