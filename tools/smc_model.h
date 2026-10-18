#ifndef SRVROS_TEST_SMC_MODEL_H
#define SRVROS_TEST_SMC_MODEL_H
struct model { unsigned commands,args,reads,status_reads,since_write;uint8_t status,reply[32],command,key[4];unsigned length;bool early_data; };
static uint8_t model_status(void *ctx) { struct model *m=ctx;m->status_reads++;m->since_write++;return m->status; }
static uint8_t model_read(void *ctx) { struct model *m=ctx;assert(m->status==5 && m->reads<m->length);uint8_t value=m->reply[m->reads++];if(m->reads==m->length)m->status=0;return value; }
static void model_command(void *ctx,uint8_t value) { struct model *m=ctx;assert(!m->status && (value==0x10 || value==0x13));m->commands++;m->command=value;m->status=12;m->since_write=0; }
static void model_argument(void *ctx,uint8_t value) {
    struct model *m=ctx;assert(m->since_write>=2 && (m->status&4));m->since_write=0;
    if(m->args<4) { m->key[m->args]=value;m->status=m->early_data&&m->args==3?5:4; }
    else { assert(value>=1 && value<=32);m->status=m->length?5:0; }
    m->args++;
}

#endif
