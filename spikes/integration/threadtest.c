#define _GNU_SOURCE
#include "quickjs.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "app_js.h"
static const char *DOC="cssonv1{ --x: 1; dept{--name:\"Eng\";} }";
static const char *WANT="{\"dept\":[{\"name\":\"Eng\"}],\"x\":1}";
static int fails=0;
static void *worker(void *arg){
  long id=(long)arg;
  for(int i=0;i<50;i++){
    JSRuntime *rt=JS_NewRuntime(); JSContext *ctx=JS_NewContext(rt);   /* thread-local runtime */
    JS_FreeValue(ctx, JS_Eval(ctx,(const char*)app_js,app_js_len,"<a>",JS_EVAL_TYPE_GLOBAL));
    JSContext*c1; while(JS_ExecutePendingJob(rt,&c1)>0);
    JSValue g=JS_GetGlobalObject(ctx),cs=JS_GetPropertyStr(ctx,g,"csson"),fn=JS_GetPropertyStr(ctx,cs,"canon");
    JSValue a=JS_NewString(ctx,DOC), r=JS_Call(ctx,fn,cs,1,&a);
    const char*s=JS_ToCString(ctx,r);
    if(!s||strcmp(s,WANT)!=0){ __atomic_fetch_add(&fails,1,__ATOMIC_SEQ_CST); }
    JS_FreeCString(ctx,s);
    JS_FreeValue(ctx,a);JS_FreeValue(ctx,r);JS_FreeValue(ctx,fn);JS_FreeValue(ctx,cs);JS_FreeValue(ctx,g);
    JS_FreeContext(ctx); JS_FreeRuntime(rt);
  }
  (void)id; return NULL;
}
int main(void){
  pthread_t t[8];
  for(long i=0;i<8;i++) pthread_create(&t[i],NULL,worker,(void*)i);
  for(int i=0;i<8;i++) pthread_join(t[i],NULL);
  printf("threads: 8x50=400 concurrent canon calls, %d wrong results\n", fails);
  return fails?1:0;
}
