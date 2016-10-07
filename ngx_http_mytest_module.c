#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_config.h>

static char * ngx_http_mytest(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_mytest_handler(ngx_http_request_t *r);

/*
 *每个ngx_command_t结构体定义了自己感兴趣的一个配置项
 */
static ngx_command_t  ngx_http_mytest_commands[] = {
	{								
		ngx_string("mytest"),		//name
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LMT_CONF|NGX_CONF_NOARGS,	//type
		ngx_http_mytest,			//set 当出现了name指定的配置项后将会调用set方法处理该配置项的参数
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL
	},
	ngx_null_command
};

/*
 *如果我们的模块没有什么工作室必须要在HTTP框架初始化阶段完成的
 *就不必实现ngx_http_module_t的8个回调函数
 */
static ngx_http_module_t  ngx_http_mytest_module_ctx = {
	NULL,	/* preconfiguration */
	NULL,	/* postconfiguration */
	NULL,	/* create main configuration */
	NULL,	/* init main configuration */
	NULL,	/* create server configuration */
	NULL,	/* merge server configuration */
	NULL,	/* create location configuration */
	NULL	/* merge location configuration */
};

ngx_module_t ngx_http_mytest_module = {
	NGX_MODULE_V1,
	&ngx_http_mytest_module_ctx,	//module context
	ngx_http_mytest_commands,		//module directives
	NGX_HTTP_MODULE,				//module type
	NULL,							//init master
	NULL,							//init module
	NULL,							//init process
	NULL,							//init thread
	NULL,							//exit thread
	NULL,							//exit process
	NULL,							//exit master
	NGX_MODULE_V1_PADDING
};

static char * ngx_http_mytest(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	    ngx_http_core_loc_conf_t  *clcf;
		clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);//
		/*定义了mytest模块的请求处理方法。当nginx接受完HTTP请求的头部信息后
		 *就会调用HTTP框架来处理请求，HTTP框架共有11个阶段，在NGX_HTTP_CONTENT_PHASE
		 *阶段将有可能调用mytest模块来处理请求，也就是调用ngx_http_mytest_handler
		 *
		 *ngx_int_t (*ngx_http_handler_pt)(ngx_http_request_t *r)
		 *上面就是handler回调函数的原型，参数r是nginx的HTTP框架解析完用户的请求后
		 *填充的数据结构，可以从中直接获取请求相关的信息。
		 *返回值可以是ngx_http_request.h 72行开始的宏,也可以是ngx_core.h 36行开始的全局错误码
		 */
		 
		clcf->handler = ngx_http_mytest_handler;
		return NGX_CONF_OK;
}

static ngx_int_t ngx_http_mytest_handler(ngx_http_request_t *r)
{
	/*只支持GET 和 HEAD方法*/
	if(!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD)))
		return NGX_HTTP_NOT_ALLOWED;
	/*丢弃请求中的包体*/
	ngx_int_t rc = ngx_http_discard_request_body(r);
	if(rc != NGX_OK)
		return rc;

	/*设置HTTP头部，以及包体*/
	ngx_str_t type = ngx_string("text/plain");
	//ngx_str_t response = ngx_string("Hello World!");
	r->headers_out.status = NGX_HTTP_OK;
	//r->headers_out.content_length_n = response.len;
	r->headers_out.content_type = type;
	/*支持文件的断点续传*/
	r->allow_ranges = 1;
	/*构造ngx_buf_t准备发送包体,这次buf里面是文件*/
	ngx_buf_t *b = NULL;
	b = ngx_palloc(r->pool, sizeof(ngx_buf_t));
	u_char* filename = (u_char*)"/tmp/test.txt";
	b->in_file = 1;
	b->file = ngx_palloc(r->pool, sizeof(ngx_file_t));
	b->file->fd = ngx_open_file(filename, NGX_FILE_RDONLY|NGX_FILE_NONBLOCK, NGX_FILE_OPEN, 0);
	b->file->log = r->connection->log;
	b->file->name.data = filename;
	b->file->name.len = strlen((const char *)filename);
	b->file_pos = 0;
	b->file_last = b->file->info.st_size;
	if(b->file->fd <= 0)
		return NGX_HTTP_NOT_FOUND;
	if(ngx_file_info(filename, &b->file->info) == NGX_FILE_ERROR)
		return NGX_HTTP_INTERNAL_SERVER_ERROR;

	r->headers_out.content_length_n = b->file->info.st_size;
	b->last_buf = 1;
	/*构造发送包体时的ngx_chain_t结构体*/
	ngx_chain_t out;
	out.buf = b;
	out.next = NULL;
	
	/*发送HTTP头部*/
	rc = ngx_http_send_header(r);



	/*因为在应答消息body的缓冲区中有打开文件，所以在请求结束的时候一定
	 *要清理文件描述符，缓冲区是在内存池当中的，而内存池在生命周期结束的时候会
	 *调用它的要清理的对象的回调函数(类似于C++析构函数)，所以我们要做的就是实例化
	 *一个内存池清理对象，填充它的回掉函数为ngx_pool_cleanup_file(ngx实现的专门用
	 *来清理内存池中文件的函数)然后填充传递给回掉函数的参数。
	 */
	/*一定要理清楚清理工cleaner和和被清理的junk这两个对象的关系*/
	/*给内存池添加一个清理对象cleaner*/
	ngx_pool_cleanup_t *cleaner = ngx_pool_cleanup_add(r->pool, sizeof(ngx_pool_cleanup_file_t));
	/*填充清理对象的回掉函数为专门清理文件的ngx_pool_cleanup_file函数*/
	cleaner->handler = ngx_pool_cleanup_file;
	/*填充清理对象cleaner的回掉函数的参数，也就是等待被清理的对象junk*/
	ngx_pool_cleanup_file_t *junk = cleaner->data;
	/*填充等待被清理的对象junk*/
	junk->fd = b->file->fd;
	junk->log = r->pool->log;
	junk->name = b->file->name.data;

#if 0

	/*构造ngx_buf_t准备发送包体*/
	ngx_buf_t *body_buf = NULL;
	body_buf = ngx_create_temp_buf(r->pool, response.len);
	if(body_buf == NULL)
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	/*将包体内容拷贝到body_buf中去*/
	ngx_memcpy(body_buf->pos, response.data, response.len);
	/*pos 到 last之间表示缓冲区的内容，所以一定要设置好last，否则是发送不出去的*/
	body_buf->last = body_buf->pos + response.len;
	/*表示这是最后一块缓冲区*/
	body_buf->last_buf = 1;

#endif


	


	/*最后一步为发送包体， 发送结束后HTTP框架会调用ngx_http_finalize_request结束请求*/
	return ngx_http_output_filter(r, &out);
}








