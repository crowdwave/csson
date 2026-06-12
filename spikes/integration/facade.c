#define _GNU_SOURCE
#include "quickjs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_js.h"
static JSRuntime *rt; static JSContext *ctx;
static void csson_init(void){
  rt=JS_NewRuntime(); ctx=JS_NewContext(rt);
  JSValue v=JS_Eval(ctx,(const char*)app_js,app_js_len,"<app>",JS_EVAL_TYPE_GLOBAL);
  if(JS_IsException(v)){JSValue e=JS_GetException(ctx);const char*s=JS_ToCString(ctx,e);fprintf(stderr,"init: %s\n",s?s:"?");JS_FreeCString(ctx,s);JS_FreeValue(ctx,e);}
  JS_FreeValue(ctx,v);
  JSContext *c1; int r; while((r=JS_ExecutePendingJob(rt,&c1))>0); (void)r;
}
char *csson_to_canonical_json(const char*src,size_t len,char**err){
  JSValue g=JS_GetGlobalObject(ctx),csson=JS_GetPropertyStr(ctx,g,"csson"),canon=JS_GetPropertyStr(ctx,csson,"canon");
  JSValue arg=JS_NewStringLen(ctx,src,len),r=JS_Call(ctx,canon,csson,1,&arg);
  char*out=NULL;
  if(JS_IsException(r)){JSValue e=JS_GetException(ctx);const char*m=JS_ToCString(ctx,e);if(err)*err=strdup(m?m:"err");JS_FreeCString(ctx,m);JS_FreeValue(ctx,e);}
  else{const char*s=JS_ToCString(ctx,r);if(s)out=strdup(s);JS_FreeCString(ctx,s);}
  JS_FreeValue(ctx,arg);JS_FreeValue(ctx,r);JS_FreeValue(ctx,canon);JS_FreeValue(ctx,csson);JS_FreeValue(ctx,g);
  return out;
}
int main(int argc,char**argv){
  csson_init();
  if(argc>1){FILE*f=fopen(argv[1],"rb");fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);
    char*buf=malloc(n+1);if(fread(buf,1,n,f)!=(size_t)n){};buf[n]=0;fclose(f);
    char*err=NULL,*out=csson_to_canonical_json(buf,n,&err);
    if(out){printf("%s\n",out);free(out);}else fprintf(stderr,"error: %s\n",err?err:"?");
    free(buf);free(err);}
  JS_FreeContext(ctx);JS_FreeRuntime(rt);return 0;
}
